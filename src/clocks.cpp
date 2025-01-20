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

            time_point<system_clock, hundred_nanoseconds> adjusted_now;
            uint16_t clock_seq;
            for ( ; ; ) {
                auto now = system_clock::now();
                {
                #if MUUID_MULTITHREADED
                    std::lock_guard guard{m_mutex};
                #endif

                    if (m_v1.adjust(now, adjusted_now, clock_seq))
                        break;
                }
            }

            uint64_t clock = adjusted_now.time_since_epoch().count();
            //gregorian offset of Unix epoch
            clock += ((uint64_t(0x01B21DD2)) << 32) + 0x13814000;

            return {clock, clock_seq};
        }

        clock_result_v7 get_v7() {

            time_point<system_clock, microseconds> adjusted_now;
            uint16_t clock_seq;
            for ( ; ; ) {
                auto now = system_clock::now();
                {
                #if MUUID_MULTITHREADED
                    std::lock_guard guard{m_mutex};
                #endif

                    if (m_v7.adjust(now, adjusted_now, clock_seq))
                        break;
                }
            }

            auto interval = adjusted_now.time_since_epoch();
            auto interval_ms = duration_cast<milliseconds>(interval);
            
            uint64_t clock = interval_ms.count();
            uint64_t remainder = (interval - duration_cast<microseconds>(interval_ms)).count();
            uint64_t frac = remainder * 4096;
            uint16_t extra = uint16_t(frac / 1000) + uint16_t(frac % 1000 >= 500);
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

        template<class Duration>
        static typename Duration::rep get_max_adjustment() {
            //we cannot rely system_clock::duration type to tell us the actual precision of the
            //clock. Some implementations lie. For example Emscripten says microseconds but in 
            //reality usually (always?) has millisecond granularity. Thus we need to discover
            //the precision at runtime
            static typename Duration::rep max_adjustment = []() {
                typename Duration::rep samples[] = {
                    round<Duration>(system_clock::now()).time_since_epoch().count(),
                    round<Duration>(system_clock::now()).time_since_epoch().count(),
                    round<Duration>(system_clock::now()).time_since_epoch().count(),
                    round<Duration>(system_clock::now()).time_since_epoch().count()
                };
                typename Duration::rep ret = 1;
                for ( ; ; ) {
                    bool done = true;
                    for(auto & sample: samples) {
                        if (sample % 10 != 0) {
                            done = true;
                            break;
                        }
                        sample /= 10;
                        if (sample != 0)
                            done = false;
                    }
                    if (done)
                        break;
                    ret *= 10;
                }
                return ret != 1 ? ret : 0;
            }();
            return max_adjustment;
        }

    private:
        template<class UnitDuration, class MaxUnitDuration>
        class state {
        public:
            state() {
                m_last_time = round<MaxUnitDuration>(system_clock::now()) - 1s;
                auto & gen = get_random_generator();
                std::uniform_int_distribution<uint16_t> clock_seq_distrib(0, 0x3FFF);
                m_clock_seq = clock_seq_distrib(gen);
                m_max_adjustment = get_max_adjustment<MaxUnitDuration>();
                reset_adjustment();
            }

            bool adjust(system_clock::time_point now, time_point<system_clock, MaxUnitDuration> & adjusted, uint16_t & clock_seq) {

                adjusted = round<MaxUnitDuration>(now);
                
                if (adjusted < m_last_time) {
                    m_clock_seq = (m_clock_seq + 1) & 0x3FFF;
                    reset_adjustment();
                    m_last_time = adjusted;
                } else if (adjusted == m_last_time) {
                    if (m_adjustment >= m_max_adjustment)
                        return false;
                    ++m_adjustment;
                } else {
                    reset_adjustment();
                    m_last_time = adjusted;
                }
                adjusted += MaxUnitDuration(m_adjustment);
                clock_seq = m_clock_seq;
                return true;
            }
        private:
            void reset_adjustment() {
                auto & gen = get_random_generator();
                std::uniform_int_distribution<typename MaxUnitDuration::rep> adjustment_distrib(0, m_max_adjustment);
                m_adjustment = adjustment_distrib(gen);
            }
        private:
            time_point<system_clock, MaxUnitDuration> m_last_time;
            uint16_t m_clock_seq;
            typename MaxUnitDuration::rep m_adjustment;
            typename MaxUnitDuration::rep m_max_adjustment;
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
