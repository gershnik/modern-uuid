// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <modern-uuid/uuid.h>

#include "random_generator.h"
#include "fork_handler.h"

#include <chrono>
#if MUUID_MULTITHREADED
    #include <mutex>
#endif

using namespace std::chrono;
using namespace std::literals;

namespace muuid::impl {
    
    struct clock_generator {
        friend reset_on_fork_singleton<clock_generator>;

        static clock_generator & instance() {
            return reset_on_fork_singleton<clock_generator>::instance();
        }

        std::pair<uint64_t, uint16_t> get_v1() {

            using hundred_nanoseconds = duration<int64_t, std::ratio<int64_t(1), int64_t(10'000'000)>>;

            constexpr int max_adjustment = []() constexpr -> int64_t {
                if constexpr (hundred_nanoseconds(1) < system_clock::duration(1)) {
                    return duration_cast<hundred_nanoseconds>(system_clock::duration(1)).count();
                } else {
                    return 0;
                }
            }();
            

            system_clock::time_point now;
            int adjustment;
            for ( ; ; ) {
                now = system_clock::now();
                {
                #if MUUID_MULTITHREADED
                    std::lock_guard guard{m_mutex};
                #endif

                    if (now < m_last_time) {
                        m_clock_seq = (m_clock_seq + 1) & 0x3FFF;
                        m_adjustment_v1 = 0;
                        m_last_time = now;
                    } else if (now == m_last_time) {
                        if (m_adjustment_v1 >= max_adjustment)
                            continue;
                        ++m_adjustment_v1;
                    } else {
                        m_adjustment_v1 = 0;
                        m_last_time = now;
                    }
                    adjustment = m_adjustment_v1;
                }
                break;
            }

            uint64_t clock = duration_cast<hundred_nanoseconds>(now.time_since_epoch()).count();
            clock += adjustment;
            //gregorian offset of Unix epoch
            clock += ((uint64_t(0x01B21DD2)) << 32) + 0x13814000;

            return {clock, m_clock_seq};
        }

        std::pair<uint64_t, uint16_t> get_v7() {

            constexpr int max_adjustment = []() constexpr -> int64_t {
                if constexpr (milliseconds(1) < system_clock::duration(1)) {
                    return duration_cast<milliseconds>(system_clock::duration(1)).count();
                } else {
                    return 0;
                }
            }();

            system_clock::time_point now;
            int adjustment;
            for ( ; ; ) {
                now = system_clock::now();
                {
                #if MUUID_MULTITHREADED
                    std::lock_guard guard{m_mutex};
                #endif

                    if (now < m_last_time) {
                        m_clock_seq = (m_clock_seq + 1) & 0x3FFF;
                        m_adjustment_v7 = 0;
                        m_last_time = now;
                    } else if (now == m_last_time) {
                        if (m_adjustment_v7 >= max_adjustment)
                            continue;
                        ++m_adjustment_v7;
                    } else {
                        m_adjustment_v7 = 0;
                        m_last_time = now;
                    }
                    adjustment = m_adjustment_v7;
                }
                break;
            }

            auto interval = now.time_since_epoch();
            auto interval_ms = duration_cast<milliseconds>(interval);
            uint64_t clock = interval_ms.count();
            clock += adjustment;

            uint16_t extra;
            if constexpr (milliseconds(1) > system_clock::duration(1)) {
                const uint64_t ticks_in_ms = uint64_t(duration_cast<system_clock::duration>(milliseconds(1)).count());
                uint64_t remainder = (interval - duration_cast<system_clock::duration>(interval_ms)).count();

                uint64_t frac = remainder * 4096;
                extra = uint16_t(frac / ticks_in_ms);
                if (frac % ticks_in_ms >= ticks_in_ms / 2)
                    ++extra;
            } else {
                std::uniform_int_distribution<uint16_t> distrib;
                extra = distrib(get_random_generator()) & 0x3FFF;
            }
            return {clock, extra};
        }

    private:
        clock_generator():
            m_clock_seq([](){
                std::uniform_int_distribution<uint16_t> distrib;
                return distrib(get_random_generator()) & 0x3FFF;
            }()),
            m_last_time(system_clock::now() - 1s)
        {}

    #if MUUID_MULTITHREADED
        void prepare_fork_in_parent() {
            m_mutex.lock();
        }
        void after_fork_in_parent() {
            m_mutex.unlock();
        }
    #endif

    private:
        uint16_t m_clock_seq;
        system_clock::time_point m_last_time;
        int m_adjustment_v1 = 0;
        int m_adjustment_v7 = 0;
    #if MUUID_MULTITHREADED
        std::mutex m_mutex;
    #endif
    };

    

    std::pair<uint64_t, uint16_t> get_clock_v1() {
        return clock_generator::instance().get_v1();
    }

    std::pair<uint64_t, uint16_t> get_clock_v7() {
        return clock_generator::instance().get_v7();
    }
}
