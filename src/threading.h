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

        #if __cpp_lib_atomic_flag_test >= 201907L
            class atomic_flag {
            public:
                atomic_flag() noexcept = default;

                bool test(std::memory_order order=std::memory_order_seq_cst) const volatile noexcept
                    { return m_impl.test(order); }
                bool test(std::memory_order order=std::memory_order_seq_cst) const noexcept
                    { return m_impl.test(order); }
                bool test_and_set(std::memory_order order=std::memory_order_seq_cst) volatile noexcept
                    { return m_impl.test_and_set(order); }
                bool test_and_set(std::memory_order order=std::memory_order_seq_cst) noexcept
                    { return m_impl.test_and_set(order); }
                void clear(std::memory_order order=std::memory_order_seq_cst) volatile noexcept
                    { m_impl.clear(); }
                void clear(std::memory_order order=std::memory_order_seq_cst) noexcept
                    { m_impl.clear(); }
            private:
                std::atomic_flag m_impl = ATOMIC_FLAG_INIT;
            };

        #else
            
        class atomic_flag {
            public:
                atomic_flag(): m_impl(false)
                {}

                bool test(std::memory_order order = std::memory_order_seq_cst) const volatile noexcept
                    { return m_impl.load(order); }
                bool test(std::memory_order order = std::memory_order_seq_cst) const noexcept
                    { return m_impl.load(order); }
                bool test_and_set(std::memory_order order=std::memory_order_seq_cst) volatile noexcept
                    { return m_impl.exchange(true, order); }
                bool test_and_set(std::memory_order order=std::memory_order_seq_cst) noexcept
                    { return m_impl.exchange(true, order); }
                void clear(std::memory_order order=std::memory_order_seq_cst) volatile noexcept
                    { m_impl.store(false); }
                void clear(std::memory_order order=std::memory_order_seq_cst) noexcept
                    { m_impl.store(false); }
            private:
                std::atomic<bool> m_impl;
            };
        #endif

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
