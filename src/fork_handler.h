// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_FORK_HANDLER_H_INCLUDED
#define HEADER_MODERN_UUID_FORK_HANDLER_H_INCLUDED

#include <modern-uuid/uuid.h>

#if __has_include(<unistd.h>)
    #include <unistd.h>
    #include <pthread.h>
    #include <signal.h>

    #define MUUID_HANDLE_FORK 1

#else 
    #define MUUID_HANDLE_FORK 0
#endif

#if MUUID_MULTITHREADED
    #include <atomic>
    #include <mutex>
#endif

#include <new>

namespace muuid::impl {

    template<class T>
    class object_buffer {
    public:
        template<std::invocable<void *> Constructor>
        object_buffer(Constructor && constructor) {
            std::forward<Constructor>(constructor)(&m_buf[0]);
        }
        ~object_buffer() {
            reinterpret_cast<T *>(&m_buf[0])->~T();
        }
        object_buffer(const object_buffer &) = delete;
        object_buffer operator=(const object_buffer &) = delete;

        template<std::invocable<void *> Constructor>
        void reset(Constructor && constructor) {
            reinterpret_cast<T *>(&m_buf[0])->~T();
            std::forward<Constructor>(constructor)(&m_buf[0]);
        }

        operator T&() {
            return *std::launder(reinterpret_cast<T *>(&m_buf[0]));
        }
    private:
        alignas(alignof(T)) static inline uint8_t m_buf[sizeof(T)];
    };

    
#if MUUID_HANDLE_FORK

    template<class T>
    concept fork_aware_static = requires(T & obj) {
        obj.prepare_fork_in_parent();
        obj.after_fork_in_parent();
    };

    template<class T, int Disambiguator = 0>
    class reset_on_fork_singleton {
    public:
        template<class... Args>
        static T & instance(Args &&... args) {
            return instance_with_constructor([&](void * addr) {
                new (addr) T{std::forward<Args>(args)...};
            });
        }

        template<std::invocable<void *> Constructor>
        static T & instance_with_constructor(Constructor && constructor) {
        #if MUUID_MULTITHREADED 
            if (!s_inst.m_initialized_in_this_process.test(std::memory_order_acquire)) {
                std::lock_guard<std::mutex> lock(s_inst.m_mutex);
                if (!s_inst.m_initialized_in_this_process.test(std::memory_order_relaxed)) {

                    if (s_inst.m_memory_initialized)
                        reinterpret_cast<T *>(&s_inst.m_buf[0])->~T();
                    s_inst.m_memory_initialized = false;
                    
                    std::forward<Constructor>(constructor)(&s_inst.m_buf[0]);

                    s_inst.m_memory_initialized = true;
                    s_inst.m_initialized_in_this_process.test_and_set(std::memory_order_release);
                }
            }
        #else
            if (!s_inst.m_initialized_in_this_process) {
                
                if (s_inst.m_memory_initialized)
                    reinterpret_cast<T *>(&s_inst.m_buf[0])->~T();
                s_inst.m_memory_initialized = false;
                
                std::forward<Constructor>(constructor)(&s_inst.m_buf[0]);

                s_inst.m_memory_initialized = true;
                s_inst.m_initialized_in_this_process = true;
            }
        #endif
            return *std::launder(reinterpret_cast<T *>(&s_inst.m_buf[0]));
        }
    
    private:
        static void prepare_fork_in_parent() {
            #if MUUID_MULTITHREADED
                s_inst.m_mutex.lock();
            #endif
            if constexpr (fork_aware_static<T>) {
                if (s_inst.m_memory_initialized)
                    reinterpret_cast<T *>(&s_inst.m_buf[0])->prepare_fork_in_parent();
            }
        }

        static void after_fork_in_parent() {
            if constexpr (fork_aware_static<T>) {
                if (s_inst.m_memory_initialized)
                    reinterpret_cast<T *>(&s_inst.m_buf[0])->after_fork_in_parent();
            }
            #if MUUID_MULTITHREADED
                s_inst.m_mutex.unlock();
            #endif
        }

        static void after_fork_in_child() {
            //NOTE 1: only signal safe functions can be called here!
            //NOTE 2: only one thread is running here
            
            #if MUUID_MULTITHREADED
                //mutex creation is constexpr and so signal safe
                new (&s_inst.m_mutex) std::mutex;
                s_inst.m_initialized_in_this_process.clear(std::memory_order_relaxed);
            #else
                s_inst.m_initialized_in_this_process = false;
            #endif
        }

    private:
        reset_on_fork_singleton() {
            if (pthread_atfork(prepare_fork_in_parent, after_fork_in_parent, after_fork_in_child) != 0)
                std::terminate();
        }
        ~reset_on_fork_singleton() {
            if (s_inst.m_memory_initialized)
                reinterpret_cast<T *>(&s_inst.m_buf[0])->~T();
        }
        

        alignas(alignof(T)) uint8_t m_buf[sizeof(T)];
        bool m_memory_initialized = false;
        
    #if MUUID_MULTITHREADED
        std::atomic_flag m_initialized_in_this_process = ATOMIC_FLAG_INIT;
        std::mutex m_mutex{};
    #else
        volatile sig_atomic_t m_initialized_in_this_process = false;
    #endif
        
        static inline reset_on_fork_singleton s_inst{};
    };

    template<class T, int Disambiguator = 0>
    class reset_on_fork_thread_local {
    public:
        template<class... Args>
        static T & instance(Args &&... args) {
            return instance_with_constructor([&](void * addr) {
                new (addr) T{std::forward<Args>(args)...};
            });
        }

        template<std::invocable<void *> Constructor>
        static T & instance_with_constructor(Constructor && constructor) {

            [[maybe_unused]]
            static int registered = []() {
                if (pthread_atfork(nullptr, nullptr, after_fork_in_child) != 0)
                    std::terminate();
                return 1;
            }();
            
            thread_local object_buffer<T> buf{std::forward<Constructor>(constructor)};

            if (s_need_to_reset) {
                buf.reset(std::forward<Constructor>(constructor));
            }
            

            return buf;
        }
    private:
        static void after_fork_in_child() {
            //NOTE 1: only signal safe functions can be called here!
            //NOTE 2: only one thread is running here
            s_need_to_reset = true;
        }

    private:
        static inline volatile sig_atomic_t s_need_to_reset = false;
    };


#else //!MUUID_HANDLE_FORK

    template<class T, int Disambiguator = 0>
    class reset_on_fork_singleton {
    public:
        template<class... Args>
        static T & instance(Args &&... args) {
            return instance_with_constructor([&](void * addr) {
                new (addr) T{std::forward<Args>(args)...};
            });
        }

        template<std::invocable<void *> Constructor>
        static T & instance_with_constructor(Constructor && constructor) {
            
            static object_buffer<T> buf{std::forward<Constructor>(constructor)};
            return buf;
        }
    };

    template<class T, int Disambiguator = 0>
    class reset_on_fork_thread_local {
    public:
        template<class... Args>
        static T & instance(Args &&... args) {
            return instance_with_constructor([&](void * addr) {
                new (addr) T{std::forward<Args>(args)...};
            });
        }

        template<std::invocable<void *> Constructor>
        static T & instance_with_constructor(Constructor && constructor) {

            thread_local object_buffer<T> buf{std::forward<Constructor>(constructor)};
            return buf;
        }
    };


#endif //MUUID_HANDLE_FORK

}


#endif
