// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <doctest/doctest.h>

#include <modern-uuid/uuid.h>

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
    #include <windows.h>
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

class file_persistence_factory final : public clock_persistence_factory {
private:
    class file_persistence final : public clock_persistence {
    public:
        file_persistence(const std::filesystem::path & path) {
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
        ~file_persistence() {
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
    file_persistence_factory(const std::filesystem::path & path): m_path(path) {}

    clock_persistence & get() override 
        { return *new file_persistence(m_path); }
private:
    std::filesystem::path m_path;
};

TEST_SUITE("persistence") {

TEST_CASE("basics time_based") {

    struct restore_pers {
        ~restore_pers() {
            muuid::set_time_based_persistence(nullptr);
        }
    } restore_pers;

    auto path = std::filesystem::path("pers.bin");
    remove(path);
    file_persistence_factory pers(path);

    muuid::set_time_based_persistence(&pers);

    uuid u1 = uuid::generate_time_based();
    uuid u2;

    std::thread([&]() {
        u2 = uuid::generate_time_based();
    }).join();

    auto parts1 = u1.to_parts();
    auto parts2 = u2.to_parts();

    CHECK(parts1.clock_seq == parts2.clock_seq);
    CHECK(parts1.time_hi_and_version == parts2.time_hi_and_version);
    CHECK(parts1.time_mid == parts2.time_mid);
    CHECK(parts1.time_low < parts2.time_low);

    muuid::set_time_based_persistence(nullptr);

    std::thread([&]() {
        u2 = uuid::generate_time_based();
    }).join();

    parts2 = u2.to_parts();
    CHECK(parts1.clock_seq != parts2.clock_seq);

    muuid::set_time_based_persistence(&pers);
    std::thread([&]() {
        u2 = uuid::generate_time_based();
    }).join();

    parts2 = u2.to_parts();
    CHECK(parts1.clock_seq == parts2.clock_seq);
}

TEST_CASE("basics reordered_time_based") {

    struct restore_pers {
        ~restore_pers() {
            muuid::set_reordered_time_based_persistence(nullptr);
        }
    } restore_pers;

    auto path = std::filesystem::path("pers.bin");
    remove(path);
    file_persistence_factory pers(path);

    muuid::set_reordered_time_based_persistence(&pers);

    uuid u1 = uuid::generate_reordered_time_based();
    uuid u2;

    std::thread([&]() {
        u2 = uuid::generate_reordered_time_based();
    }).join();

    auto parts1 = u1.to_parts();
    auto parts2 = u2.to_parts();

    CHECK(parts1.clock_seq == parts2.clock_seq);
    
    muuid::set_reordered_time_based_persistence(nullptr);

    std::thread([&]() {
        u2 = uuid::generate_reordered_time_based();
    }).join();

    parts2 = u2.to_parts();
    CHECK(parts1.clock_seq != parts2.clock_seq);

    muuid::set_reordered_time_based_persistence(&pers);
    std::thread([&]() {
        u2 = uuid::generate_reordered_time_based();
    }).join();

    parts2 = u2.to_parts();
    CHECK(parts1.clock_seq == parts2.clock_seq);
}

TEST_CASE("basics unix_time_based") {

    struct restore_pers {
        ~restore_pers() {
            muuid::set_unix_time_based_persistence(nullptr);
        }
    } restore_pers;

    auto path = std::filesystem::path("pers.bin");
    remove(path);
    file_persistence_factory pers(path);

    muuid::set_unix_time_based_persistence(&pers);

    uuid u1 = uuid::generate_unix_time_based();
    uuid u2;

    std::thread([&]() {
        u2 = uuid::generate_unix_time_based();
    }).join();

    auto parts1 = u1.to_parts();
    auto parts2 = u2.to_parts();

    CHECK(parts1.clock_seq == parts2.clock_seq);
    
    muuid::set_unix_time_based_persistence(nullptr);

    std::thread([&]() {
        u2 = uuid::generate_unix_time_based();
    }).join();

    parts2 = u2.to_parts();
    CHECK(parts1.clock_seq != parts2.clock_seq);

    muuid::set_unix_time_based_persistence(&pers);
    std::thread([&]() {
        u2 = uuid::generate_unix_time_based();
    }).join();

    parts2 = u2.to_parts();
    CHECK(parts1.clock_seq == parts2.clock_seq);
}

}
