// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_PERSISTENCE_H_INCLUDED
#define HEADER_PERSISTENCE_H_INCLUDED

#include <filesystem>
#include <exception>

#if MUUID_MULTITHREADED
    #include <atomic>
    #include <mutex>
#endif

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

template<class Data>
class per_thread_file_clock_persistence : public muuid::generic_clock_persistence<Data>::per_thread {
public:
    per_thread_file_clock_persistence(const std::filesystem::path & path) {
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
    virtual ~per_thread_file_clock_persistence() {
        if (m_desc != -1)
            sys_close(m_desc);
    }

    void close() noexcept final 
        { delete this; }

    void lock() final {
    #if defined(__EMSCRIPTEN__)
        s_file_mutex.lock();
    #elif defined(USE_POSIX_PERSISTENCE)
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
    void unlock() final {
    #if defined(__EMSCRIPTEN__)
        s_file_mutex.unlock();
    #elif defined(USE_POSIX_PERSISTENCE)
        if (flock(m_desc, LOCK_UN) < 0)
            std::terminate();
    #else
        auto h = HANDLE(_get_osfhandle(m_desc));
        OVERLAPPED ovl{};
        if (!UnlockFileEx(h, 0, 4096, 0, &ovl))
            std::terminate();
    #endif
    }

protected:
    template<size_t N>
    bool read(uint8_t (&buf)[N]) {
        if (sys_lseek(m_desc, SEEK_SET, 0) < 0)
            throw std::system_error(std::error_code(errno, std::system_category()));
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
        return true;
    }

    template<size_t N>
    void write(uint8_t (&buf)[N]) {
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
#if defined(__EMSCRIPTEN__) //flock doesn't work on emscripten
    static inline std::mutex s_file_mutex{};
#endif
};

template<class PerThread>
class file_clock_persistence final : public muuid::generic_clock_persistence<typename PerThread::data> {
public:
    file_clock_persistence(const std::filesystem::path & path): m_path(path) {}

    void add_ref() noexcept override 
        { ++m_ref_count; }
    void sub_ref() noexcept override 
        { --m_ref_count; }

    int ref_count() const 
        { return m_ref_count; }

    PerThread & get_for_current_thread() override 
        { return *new PerThread(m_path); }
private:
    std::filesystem::path m_path;
#if MUUID_MULTITHREADED
    std::atomic<int> m_ref_count = 0;
#else
    int m_ref_count = 0;
#endif
};


#endif 

