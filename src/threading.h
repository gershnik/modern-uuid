// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_THREADING_H_INCLUDED
#define HEADER_MODERN_UUID_THREADING_H_INCLUDED

#include <modern-uuid/uuid.h>

#if MUUID_MULTITHREADED
    #include <atomic>
    #include <mutex>
#endif


namespace muuid::impl {

    struct null_mutex {
        void lock() {}
        bool try_lock() { return true; }
        void unlock() {}
    };

    #if MUUID_MULTITHREADED
        
        template<class T>
        class atomic_if_multithreaded {
        public:
            atomic_if_multithreaded(): m_value{T{}} {}
            atomic_if_multithreaded(T value): m_value{value} {}
            T get() { return this->m_value.load(std::memory_order_acquire); }
            void set(T val) { this->m_value.store(val, std::memory_order_release); }
            T exchange(T val) { return this->m_value.exchange(val, std::memory_order_acq_rel); }
        private:
            std::atomic<T> m_value;
        };

        using mutex_if_multithreaded = std::mutex;

    #else

        template<class T>
        class atomic_if_multithreaded {
        public:
            atomic_if_multithreaded(): m_value{T{}} {}
            atomic_if_multithreaded(T value): m_value{value} {}
            T get() { return this->m_value; }
            void set(T val) { this->m_value = val; }
            T exchange(T val) { 
                T ret = m_value;
                m_value = val;
                return ret;
            }
        private:
            T m_value;
        };

        using mutex_if_multithreaded = null_mutex;

    #endif
}

#endif
