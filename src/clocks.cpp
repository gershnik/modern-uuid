// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <modern-uuid/uuid.h>

#include "clocks.h"
#include "random_generator.h"
#include "fork_handler.h"

#include <chrono>
#if MUUID_MULTITHREADED
    #include <mutex>
#endif

using namespace std::chrono;
using namespace std::literals;

namespace muuid::impl {
    
    class clock_generator {
        friend reset_on_fork_singleton<clock_generator>;

    private:
        using hundred_nanoseconds = duration<int64_t, std::ratio<int64_t(1), int64_t(10'000'000)>>;

    public:
        static clock_generator & instance() {
            return reset_on_fork_singleton<clock_generator>::instance();
        }

        clock_result_v1 get_v1() {

            using hundred_nanoseconds = duration<int64_t, std::ratio<int64_t(1), int64_t(10'000'000)>>;

            system_clock::time_point now;
            int adjustment;
            uint16_t clock_seq;
            for ( ; ; ) {
                now = system_clock::now();
                {
                #if MUUID_MULTITHREADED
                    std::lock_guard guard{m_mutex};
                #endif

                    if (m_v1.adjust(now, adjustment, clock_seq))
                        break;
                }
            }

            uint64_t clock = duration_cast<hundred_nanoseconds>(now.time_since_epoch()).count();
            clock += adjustment;
            //gregorian offset of Unix epoch
            clock += ((uint64_t(0x01B21DD2)) << 32) + 0x13814000;

            return {clock, clock_seq};
        }

        clock_result_v7 get_v7() {

            system_clock::time_point now;
            int adjustment;
            uint16_t clock_seq;
            for ( ; ; ) {
                now = system_clock::now();
                {
                #if MUUID_MULTITHREADED
                    std::lock_guard guard{m_mutex};
                #endif

                    if (m_v7.adjust(now, adjustment, clock_seq))
                        break;
                }
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
                extra = 0;
            }
            return {clock, extra, clock_seq};
        }

    private:
        clock_generator() = default;

    #if MUUID_MULTITHREADED
        void prepare_fork_in_parent() {
            m_mutex.lock();
        }
        void after_fork_in_parent() {
            m_mutex.unlock();
        }
    #endif

    private:
        template<class UnitDuration, class MaxUnitDuration>
        struct state {
            state():
                m_last_time(round<MaxUnitDuration>(system_clock::now()) - 1s),
                m_clock_seq([](){
                    std::uniform_int_distribution<uint16_t> distrib;
                    return distrib(get_random_generator()) & 0x3FFF;
                }()),
                m_adjustment(0)
            {}

            bool adjust(system_clock::time_point & now, int & adjustment, uint16_t & clock_seq) {

                constexpr int max_adjustment = []() constexpr -> int64_t {
                    if constexpr (UnitDuration(1) < system_clock::duration(1)) {
                        return duration_cast<UnitDuration>(system_clock::duration(1)).count();
                    } else {
                        return 0;
                    }
                }();

                auto rounded_now = round<MaxUnitDuration>(now);
                
                if (rounded_now < m_last_time) {
                    m_clock_seq = (m_clock_seq + 1) & 0x3FFF;
                    m_adjustment = 0;
                    m_last_time = rounded_now;
                } else if (rounded_now == m_last_time) {
                    if constexpr (max_adjustment > 0) {
                        if (m_adjustment >= max_adjustment)
                            return false;
                        ++m_adjustment;
                    } else {
                        return false;
                    }
                } else {
                    m_adjustment = 0;
                    m_last_time = rounded_now;
                }
                adjustment = m_adjustment;
                clock_seq = m_clock_seq;
                now = time_point_cast<system_clock::duration>(rounded_now);
                return true;
            }

        private:
            time_point<system_clock, MaxUnitDuration> m_last_time;
            uint16_t m_clock_seq;
            int m_adjustment;
        };

    private:
        state<hundred_nanoseconds, hundred_nanoseconds> m_v1;
        state<milliseconds, microseconds> m_v7;
    #if MUUID_MULTITHREADED
        std::mutex m_mutex;
    #endif
    };

    

    clock_result_v1 get_clock_v1() {
        return clock_generator::instance().get_v1();
    }

    clock_result_v7 get_clock_v7() {
        return clock_generator::instance().get_v7();
    }
}
