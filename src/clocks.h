// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_CLOCKS_H_INCLUDED
#define HEADER_MODERN_UUID_CLOCKS_H_INCLUDED

#include <cstdint>

#if MUUID_MULTITHREADED
    #include <atomic>
#endif

namespace muuid::impl {

    struct clock_result_v1 {
        uint64_t value;
        uint16_t sequence;
    };
    using clock_result_v6 = clock_result_v1;

    struct clock_result_v7 {
        uint64_t value;
        uint16_t extra;
        uint16_t sequence;
    };

    clock_result_v1 get_clock_v1();
    clock_result_v6 get_clock_v6();
    clock_result_v7 get_clock_v7();

    #if MUUID_MULTITHREADED
        
        template<class T>
        class atomic_if_multithreaded {
        public:
            atomic_if_multithreaded(): m_value{T{}} {}
            atomic_if_multithreaded(T value): m_value{value} {}
            T get() { return this->m_value.load(std::memory_order_acquire); }
            void set(T val) { this->m_value.store(val, std::memory_order_release); }
        private:
            std::atomic<T> m_value;
        };

    #else

        template<class T>
        class atomic_if_multithreaded {
        public:
            atomic_if_multithreaded(): m_value{T{}} {}
            atomic_if_multithreaded(T value): m_value{value} {}
            T get() { return this->m_value; }
            void set(T val) { this->m_value = val; }
        private:
            T m_value;
        };

    #endif

    extern atomic_if_multithreaded<clock_persistence_factory *> g_clock_persistence_v1;
    extern atomic_if_multithreaded<clock_persistence_factory *> g_clock_persistence_v6;
    extern atomic_if_multithreaded<clock_persistence_factory *> g_clock_persistence_v7;
}

#endif
