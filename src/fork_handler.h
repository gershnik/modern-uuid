// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_FORK_HANDLER_H_INCLUDED
#define HEADER_MODERN_UUID_FORK_HANDLER_H_INCLUDED

#include <modern-uuid/uuid.h>

#include "threading.h"

#if __has_include(<unistd.h>)
    #include <unistd.h>
    #include <pthread.h>
    #include <signal.h>

    #define MUUID_HANDLE_FORK 1

#else 
    #define MUUID_HANDLE_FORK 0
#endif

#include <new>

namespace muuid::impl {

    template<class T>
    class singleton_holder;
    
#if MUUID_HANDLE_FORK

    template<class T>
    concept fork_aware_static = requires(T & obj) {
        obj.prepare_fork_in_parent();
        obj.after_fork_in_parent();
    };

    template<class T>
    class singleton_holder {
    public:
        singleton_holder() = default;
        ~singleton_holder() {
            if (m_memory_initialized)
                reinterpret_cast<T *>(&m_buf[0])->~T();
        }
        singleton_holder(const singleton_holder &) = delete;
        singleton_holder operator=(const singleton_holder &) = delete;

        void reset() {
            if (m_memory_initialized)
                reinterpret_cast<T *>(&m_buf[0])->~T();
            
            m_memory_initialized = false;
            //this can throw
            new (&m_buf[0]) T;
            m_memory_initialized = true;
        }

        operator bool() const 
            { return m_memory_initialized; }

        T & operator*() 
            { return *std::launder(reinterpret_cast<T *>(&m_buf[0])); }

        
    private:
        alignas(alignof(T)) uint8_t m_buf[sizeof(T)];
        bool m_memory_initialized = false;
    };

    template<class T, int Disambiguator = 0>
    class reset_on_fork_singleton {
    public:
        static T & instance() {

            [[maybe_unused]]
            static int registered = []() {
                if (pthread_atfork(prepare_fork_in_parent, after_fork_in_parent, after_fork_in_child) != 0)
                    std::terminate();
                return 1;
            }();

        #if MUUID_MULTITHREADED 
            if (!s_inst.m_initialized_in_this_process.test(std::memory_order_acquire)) {
                std::lock_guard<std::mutex> lock(s_inst.m_mutex);
                if (!s_inst.m_initialized_in_this_process.test(std::memory_order_relaxed)) {

                    s_inst.m_obj.reset();
                    
                    s_inst.m_initialized_in_this_process.test_and_set(std::memory_order_release);
                }
            }
        #else
            if (!s_inst.m_initialized_in_this_process) {
                
                s_inst.m_obj.reset();

                s_inst.m_initialized_in_this_process = true;
            }
        #endif
            return *s_inst.m_obj;
        }
    
    private:
        static void prepare_fork_in_parent() {
            #if MUUID_MULTITHREADED
                s_inst.m_mutex.lock();
            #endif
            if constexpr (fork_aware_static<T>) {
                if (s_inst.m_obj)
                    (*s_inst.m_obj).prepare_fork_in_parent();
            }
        }

        static void after_fork_in_parent() {
            if constexpr (fork_aware_static<T>) {
                if (s_inst.m_obj)
                    (*s_inst.m_obj).after_fork_in_parent();
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
        reset_on_fork_singleton() = default;
        ~reset_on_fork_singleton() = default;
        reset_on_fork_singleton(const reset_on_fork_singleton &) = delete;
        reset_on_fork_singleton & operator=(const reset_on_fork_singleton &) = delete;
        
    private:
        singleton_holder<T> m_obj;
        
    #if MUUID_MULTITHREADED
        volatile atomic_flag m_initialized_in_this_process;
        std::mutex m_mutex{};
    #else
        volatile sig_atomic_t m_initialized_in_this_process = false;
    #endif
        
        static inline reset_on_fork_singleton s_inst{};
    };

    template<class T, int Disambiguator = 0>
    class reset_on_fork_thread_local {
    private:
        using sig_atomic_counter = std::make_unsigned_t<sig_atomic_t>;
    public:
        static T & instance() {

            [[maybe_unused]]
            static int registered = []() {
                if (pthread_atfork(prepare_fork_in_parent, after_fork_in_parent, after_fork_in_child) != 0)
                    std::terminate();
                return 1;
            }();

            if (tl_inst.m_generation != s_generation) {
                tl_inst.m_obj.reset();
                tl_inst.m_generation = s_generation;
            }
            
            return *tl_inst.m_obj;
        }
    private:
        static void prepare_fork_in_parent() {
            if constexpr (fork_aware_static<T>) {
                if (tl_inst.m_obj)
                    (*tl_inst.m_obj).prepare_fork_in_parent();
            }
        }
        static void after_fork_in_parent() {
            if constexpr (fork_aware_static<T>) {
                if (tl_inst.m_obj)
                    (*tl_inst.m_obj).after_fork_in_parent();
            }
        }
        static void after_fork_in_child() {
            //NOTE 1: only signal safe functions can be called here!
            //NOTE 2: only one thread is running here
            s_generation = s_generation + 1;
        }

        reset_on_fork_thread_local():
            m_generation(s_generation - 1)
        {}
        ~reset_on_fork_thread_local() = default;
        reset_on_fork_thread_local(const reset_on_fork_thread_local &) = delete;
        reset_on_fork_thread_local & operator=(const reset_on_fork_thread_local &) = delete;

    private:
        sig_atomic_counter m_generation;
        singleton_holder<T> m_obj;
        
        static inline volatile sig_atomic_counter s_generation = 0;
        static thread_local inline reset_on_fork_thread_local tl_inst{};
    };


#else //!MUUID_HANDLE_FORK

    template<class T, int Disambiguator = 0>
    class reset_on_fork_singleton {
    public:
        static T & instance() {
            
            static T obj;
            return obj;
        }
    private:
        reset_on_fork_singleton() = delete;
        ~reset_on_fork_singleton() = delete;
        reset_on_fork_singleton(const reset_on_fork_singleton &) = delete;
        reset_on_fork_singleton & operator=(const reset_on_fork_singleton &) = delete;
    };

    template<class T, int Disambiguator = 0>
    class reset_on_fork_thread_local {
    public:
        static T & instance() {

            thread_local T obj;
            return obj;
        }
    private:
        reset_on_fork_thread_local() = delete;
        ~reset_on_fork_thread_local() = delete;
        reset_on_fork_thread_local(const reset_on_fork_thread_local &) = delete;
        reset_on_fork_thread_local & operator=(const reset_on_fork_thread_local &) = delete;
    };


#endif //MUUID_HANDLE_FORK

}


#endif
