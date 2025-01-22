// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <modern-uuid/uuid.h>

#include "clocks.h"
#include "random_generator.h"
#include "fork_handler.h"

#include <chrono>
#include <mutex>

using namespace std::chrono;
using namespace std::literals;

using hundred_nanoseconds = duration<int64_t, std::ratio<int64_t(1), int64_t(10'000'000)>>;

static constexpr bool g_synchronize_monotonic = 
    #if MUUID_MULTITHREADED
        true;
    #else
        false;
    #endif

namespace {
    struct null_mutex {
        void lock() {}
        bool try_lock() { return true; }
        void unlock() {}
    };

    #if MUUID_MULTITHREADED
        using mutex = std::mutex;
    #else
        using mutex = null_mutex;
    #endif
}

namespace muuid::impl {

    template<class Duration>
    static typename Duration::rep get_max_adjustment() {
        //we cannot rely system_clock::duration type to tell us the actual precision of the
        //clock. Some implementations lie. For example Emscripten says microseconds but in 
        //reality usually (always?) has millisecond granularity. Thus we need to discover
        //the precision at runtime
        static typename Duration::rep max_adjustment = []() {
            typename Duration::rep samples[3];
            samples[0] = round<Duration>(system_clock::now()).time_since_epoch().count();
            for (size_t i = 1; i < std::size(samples); ++i) {
                for ( ; ; ) {
                    auto val = round<Duration>(system_clock::now()).time_since_epoch().count();
                    if (val != samples[i - 1]) {
                        samples[i] = val;
                        break;
                    }
                }
            }
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

    template<class UnitDuration, class MaxUnitDuration, bool Synchronized>
    class clock_state {
        friend std::conditional_t<Synchronized, reset_on_fork_singleton<clock_state>, reset_on_fork_thread_local<clock_state>>;
    public:
        static clock_state & instance() {
            if constexpr (Synchronized)
                return reset_on_fork_singleton<clock_state>::instance();
            else
                return reset_on_fork_thread_local<clock_state>::instance();
        }

        bool adjust(system_clock::time_point now, time_point<system_clock, MaxUnitDuration> & adjusted, uint16_t & clock_seq) {

            std::lock_guard guard{m_mutex};

            adjusted = round<MaxUnitDuration>(now);
            //on a miniscule change that we have misdetected m_max_adjustment let's make sure
            //that adjusted is rounded on the m_max_adjustment boundary to avoid spillover
            if (m_max_adjustment != 0) {
                auto val = adjusted.time_since_epoch();
                val = (val / m_max_adjustment) * m_max_adjustment;
                adjusted = time_point<system_clock, MaxUnitDuration>(val);
            }
            
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
        clock_state() {
            m_last_time = round<MaxUnitDuration>(system_clock::now()) - 1s;
            auto & gen = get_random_generator();
            std::uniform_int_distribution<uint16_t> clock_seq_distrib(0, 0x3FFF);
            m_clock_seq = clock_seq_distrib(gen);
            m_max_adjustment = get_max_adjustment<MaxUnitDuration>();
            reset_adjustment();
        }

        void reset_adjustment() {
            if (m_max_adjustment != 0) {
                auto & gen = get_random_generator();
                std::uniform_int_distribution<typename MaxUnitDuration::rep> adjustment_distrib(0, m_max_adjustment - 2);
                m_adjustment = adjustment_distrib(gen);
            }
        }

        void prepare_fork_in_parent() requires(Synchronized) {
            m_mutex.lock();
        }
        void after_fork_in_parent() requires(Synchronized) {
            m_mutex.unlock();
        }
    private:
        time_point<system_clock, MaxUnitDuration> m_last_time;
        uint16_t m_clock_seq;
        typename MaxUnitDuration::rep m_adjustment = 0;
        typename MaxUnitDuration::rep m_max_adjustment;
        std::conditional_t<Synchronized, mutex, null_mutex> m_mutex;
    };


    clock_result_v1 get_clock_v1() {

        auto & state = clock_state<hundred_nanoseconds, hundred_nanoseconds, false>::instance();

        time_point<system_clock, hundred_nanoseconds> adjusted_now;
        uint16_t clock_seq;
        for ( ; ; ) {
            auto now = system_clock::now();
            {
                if (state.adjust(now, adjusted_now, clock_seq))
                    break;
            }
        }

        uint64_t clock = adjusted_now.time_since_epoch().count();
        //gregorian offset of Unix epoch
        clock += ((uint64_t(0x01B21DD2)) << 32) + 0x13814000;

        return {clock, clock_seq};
    }

    clock_result_v6 get_clock_v6() {

        auto & state = clock_state<hundred_nanoseconds, hundred_nanoseconds, g_synchronize_monotonic>::instance();

        time_point<system_clock, hundred_nanoseconds> adjusted_now;
        uint16_t clock_seq;
        for ( ; ; ) {
            auto now = system_clock::now();
            {
                if (state.adjust(now, adjusted_now, clock_seq))
                    break;
            }
        }

        uint64_t clock = adjusted_now.time_since_epoch().count();
        //gregorian offset of Unix epoch
        clock += ((uint64_t(0x01B21DD2)) << 32) + 0x13814000;

        return {clock, clock_seq};
    }

    clock_result_v7 get_clock_v7() {
        auto & state = clock_state<milliseconds, microseconds, g_synchronize_monotonic>::instance();

        time_point<system_clock, microseconds> adjusted_now;
        uint16_t clock_seq;
        for ( ; ; ) {
            auto now = system_clock::now();
            {
                if (state.adjust(now, adjusted_now, clock_seq))
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
}
