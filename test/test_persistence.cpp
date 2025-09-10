// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "test_util.h"

#include <modern-uuid/uuid.h>

#if !MUUID_MULTITHREADED
    #error Tests currently require threading support
#endif

#include <filesystem>
#include <thread>
#include <cassert>

#if __has_include(<unistd.h>) && __has_include(<sys/file.h>) && !defined(__MINGW32__)
    #include <unistd.h>
    #include <sys/file.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #define USE_POSIX_PERSISTENCE
    #define sys_close ::close
    #define sys_read ::read
    #define sys_write ::write
    #define sys_lseek ::lseek
    #define sys_ftruncate ::ftruncate
    #define posix_category system_category
#elif defined(_WIN32) 
    #include <Windows.h>
    #include <io.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #define USE_WIN32_PERSISTENCE
    #define sys_close ::_close
    #define sys_read ::_read
    #define sys_write ::_write
    #define sys_lseek ::_lseek
    #define sys_ftruncate ::_chsize
    #define posix_category generic_category
#else
    #error "Don't know how to access files"
#endif

using namespace muuid;
using namespace std::literals;

class file_clock_persistence final : public uuid_clock_persistence {
private:
    class per_thread final : public uuid_clock_persistence::per_thread {
    public:
        per_thread(const std::filesystem::path & path) {
        #ifdef USE_POSIX_PERSISTENCE
            auto fd = ::open(path.c_str(), O_RDWR|O_CREAT|O_CLOEXEC, S_IRUSR|S_IWUSR);
        #else 
            HANDLE h = CreateFileW(path.c_str(), 
                                   GENERIC_READ | GENERIC_WRITE, 
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 
                                   nullptr, 
                                   OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h == INVALID_HANDLE_VALUE)
                throw std::system_error(std::error_code(GetLastError(), std::system_category()));
            auto fd = _open_osfhandle(intptr_t(h), _O_RDWR);
        #endif
            
            if (fd < 0)
                throw std::system_error(std::error_code(errno, std::posix_category()));

            m_desc = fd;
        }
        ~per_thread() {
            if (m_desc != -1)
                sys_close(m_desc);
        }

        void close() noexcept override 
            { delete this; }

        void lock() override {
        #ifdef USE_POSIX_PERSISTENCE
            while (flock(m_desc, LOCK_EX) < 0) {
                int err = errno;
                if ((err == EAGAIN) || (err == EINTR))
                    continue;
                throw std::system_error(std::error_code(err, std::system_category()));
            }
        #else
            auto h = HANDLE(_get_osfhandle(m_desc));
            OVERLAPPED ovl{};
            if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, 4096, 0, &ovl))
                throw std::system_error(std::error_code(GetLastError(), std::system_category()));
        #endif
        }
        void unlock() override {
        #ifdef USE_POSIX_PERSISTENCE
            if (flock(m_desc, LOCK_UN) < 0)
                std::terminate();
        #else
            auto h = HANDLE(_get_osfhandle(m_desc));
            OVERLAPPED ovl{};
            if (!UnlockFileEx(h, 0, 4096, 0, &ovl))
                std::terminate();
        #endif
        }

        bool load(data & d) override {
            using when_type = data::time_point_t::rep;

            uint8_t buf[sizeof(when_type) + sizeof(d.seq) + sizeof(d.adjustment)];
            for (size_t read = 0; read < sizeof(buf); ) {
                auto byte_count = sys_read(m_desc, buf + read, unsigned(sizeof(buf) - read));
                if (byte_count == 0)
                {
                    if (read < sizeof(buf))
                        return false;
                    break;
                }
                if (byte_count < 0)
                    throw std::system_error(std::error_code(errno, std::posix_category()));
                read += byte_count;
            }
            
            uint8_t * current = buf;
            
            when_type count;
            memcpy(&count, current, sizeof(count));
            d.when = data::time_point_t(data::time_point_t::duration(count));
            current += sizeof(count);
            memcpy(&d.seq, current, sizeof(d.seq));
            current += sizeof(d.seq);
            memcpy(&d.adjustment, current, sizeof(d.adjustment));
            return true;
        }

        void store(const data & d) override {
            using when_type = data::time_point_t::rep;
            
            uint8_t buf[sizeof(when_type) + sizeof(d.seq) + sizeof(d.adjustment)];
            uint8_t * current = buf;

            when_type count = d.when.time_since_epoch().count();
            memcpy(current, &count, sizeof(count));
            current += sizeof(count);
            memcpy(current, &d.seq, sizeof(d.seq));
            current += sizeof(d.seq);
            memcpy(current, &d.adjustment, sizeof(d.adjustment));
            
            if (sys_ftruncate(m_desc, 0) < 0)
                throw std::system_error(std::error_code(errno, std::system_category()));
            if (sys_lseek(m_desc, SEEK_SET, 0) < 0)
                throw std::system_error(std::error_code(errno, std::system_category()));
            for (size_t written = 0; written < sizeof(buf); ) {
                auto byte_count = sys_write(m_desc, buf + written, unsigned(sizeof(buf) - written));
                if (byte_count < 0)
                    throw std::system_error(std::error_code(errno, std::posix_category()));
                written += byte_count;
            }
        }
    private:
        int m_desc = -1;
    };

public:
    file_clock_persistence(const std::filesystem::path & path): m_path(path) {}

    per_thread & get_for_current_thread() override 
        { return *new per_thread(m_path); }

    void add_ref() noexcept override 
        { ++m_ref_count; }
    void sub_ref() noexcept override 
        { --m_ref_count; }

    int ref_count() const 
        { return m_ref_count; }
private:
    std::filesystem::path m_path;
#if MUUID_MULTITHREADED
    std::atomic<int> m_ref_count = 0;
#else
    int m_ref_count = 0;
#endif
};

auto path = std::filesystem::path("pers.bin");
file_clock_persistence pers(path);

TEST_SUITE("persistence") {

TEST_CASE("clock time_based") {

    {
        struct restore_pers {
            ~restore_pers() {
                set_time_based_persistence(nullptr);
            }
        } restore_pers;

        remove(path);
        set_time_based_persistence(&pers);

        CHECK(pers.ref_count() != 0);

        uuid u1 = uuid::generate_time_based();
        uuid u2;

        std::thread([&]() {
            std::this_thread::sleep_for(1ms);
            u2 = uuid::generate_time_based();
        }).join();

        auto parts1 = u1.to_parts();
        auto parts2 = u2.to_parts();

        CHECK(parts1.clock_seq == parts2.clock_seq);
        CHECK(parts1.time_hi_and_version == parts2.time_hi_and_version);
        CHECK(parts1.time_mid == parts2.time_mid);
        CHECK(parts1.time_low < parts2.time_low);

        set_time_based_persistence(nullptr);

        std::thread([&]() {
            std::this_thread::sleep_for(1ms);
            u2 = uuid::generate_time_based();
        }).join();

        parts2 = u2.to_parts();
        CHECK(parts1.clock_seq != parts2.clock_seq);

        set_time_based_persistence(&pers);
        std::thread([&]() {
            std::this_thread::sleep_for(1ms);
            u2 = uuid::generate_time_based();
        }).join();

        parts2 = u2.to_parts();
        CHECK(parts1.clock_seq == parts2.clock_seq);
    }
    uuid::generate_time_based();
    CHECK(pers.ref_count() == 0);
}

TEST_CASE("clock reordered_time_based") {

    {
        struct restore_pers {
            ~restore_pers() {
                set_reordered_time_based_persistence(nullptr);
            }
        } restore_pers;

        remove(path);
        
        set_reordered_time_based_persistence(&pers);

        CHECK(pers.ref_count() != 0);

        uuid u1 = uuid::generate_reordered_time_based();
        uuid u2;

        std::thread([&]() {
            std::this_thread::sleep_for(1ms);
            u2 = uuid::generate_reordered_time_based();
        }).join();

        auto parts1 = u1.to_parts();
        auto parts2 = u2.to_parts();

        CHECK(parts1.clock_seq == parts2.clock_seq);
        
        set_reordered_time_based_persistence(nullptr);

        std::thread([&]() {
            std::this_thread::sleep_for(1ms);
            u2 = uuid::generate_reordered_time_based();
        }).join();

        parts2 = u2.to_parts();
        CHECK(parts1.clock_seq != parts2.clock_seq);

        set_reordered_time_based_persistence(&pers);
        std::thread([&]() {
            std::this_thread::sleep_for(1ms);
            u2 = uuid::generate_reordered_time_based();
        }).join();

        parts2 = u2.to_parts();
        CHECK(parts1.clock_seq == parts2.clock_seq);
    }
    uuid::generate_reordered_time_based();
    CHECK(pers.ref_count() == 0);
}

TEST_CASE("clock unix_time_based") {

    {
        struct restore_pers {
            ~restore_pers() {
                set_unix_time_based_persistence(nullptr);
            }
        } restore_pers;

        remove(path);
        
        set_unix_time_based_persistence(&pers);

        CHECK(pers.ref_count() != 0);

        uuid u1 = uuid::generate_unix_time_based();
        uuid u2;

        std::thread([&]() {
            u2 = uuid::generate_unix_time_based();
        }).join();

        auto parts1 = u1.to_parts();
        auto parts2 = u2.to_parts();

        CAPTURE(u1);
        CAPTURE(u2);
        if (parts1.time_hi_and_version != parts2.time_hi_and_version || parts1.time_mid != parts2.time_mid)
            CHECK(parts1.clock_seq == parts2.clock_seq); //THIS MIGHT supuriously fail once in a while
        else
            CHECK(parts1.clock_seq != parts2.clock_seq); //THIS MIGHT supuriously fail once in a while
        
        set_unix_time_based_persistence(nullptr);

        std::thread([&]() {
            u2 = uuid::generate_unix_time_based();
        }).join();

        parts2 = u2.to_parts();
        CHECK(parts1.clock_seq != parts2.clock_seq); //THIS MIGHT supuriously fail once in a while

        set_unix_time_based_persistence(&pers);
        std::thread([&]() {
            u2 = uuid::generate_unix_time_based();
        }).join();

        parts2 = u2.to_parts();
        if (parts1.time_hi_and_version != parts2.time_hi_and_version || parts1.time_mid != parts2.time_mid)
            CHECK(parts1.clock_seq == parts2.clock_seq); //THIS MIGHT supuriously fail once in a while
        else
            CHECK(parts1.clock_seq != parts2.clock_seq); //THIS MIGHT supuriously fail once in a while
    }
    uuid::generate_unix_time_based();
    CHECK(pers.ref_count() == 0);
}

TEST_CASE("node time_based") {

    struct restore {
        ~restore() {
            set_node_id(node_id::detect_system);
        }
    } restore;

    uuid u1 = uuid::generate_time_based();
    auto parts1 = u1.to_parts();
    
    auto calculated = set_node_id(node_id::generate_random);
    
    uuid u2 = uuid::generate_time_based();
    auto parts2 = u2.to_parts();

    CHECK_UNEQUAL_SEQ(parts1.node, parts2.node);
    CHECK_EQUAL_SEQ(parts2.node, calculated);

    uuid u3 = uuid::generate_time_based();
    auto parts3 = u3.to_parts();

    CHECK_EQUAL_SEQ(parts2.node, parts3.node);
    
    calculated = set_node_id(node_id::detect_system);

    u3 = uuid::generate_time_based();
    parts3 = u3.to_parts();

    CHECK_EQUAL_SEQ(parts3.node, calculated);

    uint8_t dummy[6] = {1, 2, 3, 4, 5, 6};
    set_node_id(dummy);

    u3 = uuid::generate_time_based();
    parts3 = u3.to_parts();

    CHECK_EQUAL_SEQ(parts3.node, dummy);
}

}
