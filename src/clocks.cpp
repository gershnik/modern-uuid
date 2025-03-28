// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <modern-uuid/uuid.h>

#include "clocks.h"
#include "random_generator.h"
#include "fork_handler.h"
#include "threading.h"

#include <cstring>
#include <cassert>
#include <mutex>
#include <algorithm>

using namespace std::chrono;
using namespace std::literals;
using namespace muuid;
using namespace muuid::impl;

using hundred_nanoseconds = duration<int64_t, std::ratio<int64_t(1), int64_t(10'000'000)>>;

static atomic_if_multithreaded<clock_persistence *> g_clock_persistence_v1{};
static atomic_if_multithreaded<clock_persistence *> g_clock_persistence_v6{};
static atomic_if_multithreaded<clock_persistence *> g_clock_persistence_v7{};

template<class T>
static inline T detect_roundness_to_pow10_impl(T val) {
    T ret = 1;
    while (val != 0) {
        if (val % 10 != 0)
            break;
        val /= 10;
        ret *= 10;
    }
    return ret;
}

template<class T>
static inline T detect_roundness_to_pow10(T val) {
    //WASM passes time reading through some floating point shenanigans and they 
    //can be 1 off the real value (e.g. 999 for 1000)
    //We will take the maximum from val, val - 1 and val + 1
    //This should be harmless for normal integer readings
    T ret = detect_roundness_to_pow10_impl(val);
    if (val > 0)
        ret = std::max(ret, detect_roundness_to_pow10_impl(val - 1));
    ret = std::max(ret, detect_roundness_to_pow10_impl(val + 1));
    return ret;
}

static system_clock::duration get_clock_tick() {
    //we cannot rely system_clock::duration type to tell us the actual precision of the
    //clock. Some implementations lie. For example Emscripten says microseconds but in 
    //reality usually (always?) has millisecond granularity. Thus we need to discover
    //the precision at runtime
    static system_clock::duration max_adjustment = []() {
        system_clock::duration::rep diffs[3];
        system_clock::duration::rep base = system_clock::now().time_since_epoch().count();
        for (size_t i = 0; i < std::size(diffs); ++i) {
            for ( ; ; ) {
                auto val = system_clock::now().time_since_epoch().count();
                if (val != base) {
                    diffs[i] = val - base;
                    base = val;
                    break;
                }
            }
        }
        system_clock::duration::rep pows[std::size(diffs)];
        std::transform(std::begin(diffs), std::end(diffs), std::begin(pows), [](auto diff) {
            return detect_roundness_to_pow10(diff);
        });
        auto ret = *std::min_element(std::begin(pows), std::end(pows));
        return system_clock::duration(ret);
    }();
    return max_adjustment;
}

static inline system_clock::time_point next_distinct_now(system_clock::time_point prev) {
    for ( ; ; ) {
        auto now = system_clock::now();
        if (now != prev) {
            return now;
        }
    }
}

namespace {

    class persistence_holder {
    public:
        persistence_holder() noexcept = default;
        ~persistence_holder() noexcept {
            if (m_per_thread)
                m_per_thread->close();
            if (m_persistence)
                m_persistence->sub_ref();
        }
        persistence_holder(const persistence_holder &) noexcept = delete;
        persistence_holder & operator=(const persistence_holder &) noexcept = delete;

        bool set(clock_persistence * persistence) {
            if (m_persistence == persistence)
                return false;
            if (m_per_thread) {
                m_per_thread->close();
                m_per_thread = nullptr;
            }
            if (persistence)
                persistence->add_ref();
            if (m_persistence)
                m_persistence->sub_ref();
            m_persistence = persistence;
            if (m_persistence)
                m_per_thread = &m_persistence->get_for_current_thread();
            return true;
        }

        void lock() {
            if (m_per_thread)
                m_per_thread->lock();
        }
        void unlock() {
            if (m_per_thread)
                m_per_thread->unlock();
        }

        template<class Duration>
        void save(time_point<system_clock, Duration> time, 
                  uint16_t clock_seq,
                  typename Duration::rep adjustment) {

            assert(adjustment <= std::numeric_limits<int32_t>::max());
            if (!m_per_thread)
                return;
            
            clock_persistence::data data = { time_point_cast<nanoseconds>(time), clock_seq, int32_t(adjustment)};
            m_per_thread->store(data);
        }

        template<class Duration>
        bool load(time_point<system_clock, Duration> & time, 
                  uint16_t & clock_seq, 
                  typename Duration::rep & adjustment) {

            if (!m_per_thread)
                return false;
            clock_persistence::data data;
            if (!m_per_thread->load(data))
                return false;
            
            time = std::chrono::time_point_cast<Duration>(data.when);
            clock_seq = data.seq;
            adjustment = data.adjustment;
            return true;
        }
    private:
        clock_persistence * m_persistence = nullptr;
        clock_persistence::per_thread * m_per_thread = nullptr;
    };

    template<class Derived, class UnitDuration, class MaxUnitDuration>
    class clock_state_base {
    public:
        using unit_duration = UnitDuration;
        using max_unit_duration = MaxUnitDuration;

    protected:
        clock_state_base():
            m_max_adjustment(clock_state_base::get_max_adjustment()) 
        {}

        ~clock_state_base() {
            if (this->m_locked_for_fork)
                this->m_persistance.unlock();
        }

        void set_persistence(clock_persistence * pers) {
            
            if (this->m_persistance.set(pers) || !m_initialized) {
                std::lock_guard guard{this->m_persistance};

                if (!m_persistance.load(this->m_last_time, this->m_clock_seq, this->m_adjustment))
                {
                    static_cast<Derived *>(this)->init_new();
                    this->m_persistance.save(this->m_last_time, this->m_clock_seq, this->m_adjustment);
                } else {
                    this->m_clock_seq &= 0x3FFF;
                    if (this->m_adjustment < 0)
                        this->m_adjustment = 0;
                }

                m_initialized = true;
            }
        }

        void prepare_fork_in_parent() {
            this->m_persistance.lock();
            this->m_locked_for_fork = true;
        }
        void after_fork_in_parent() {
            this->m_persistance.unlock();
            this->m_locked_for_fork = false;
        }

    private:
        static typename max_unit_duration::rep get_max_adjustment() {
            auto val = get_clock_tick();
            typename max_unit_duration::rep ret = round<max_unit_duration>(system_clock::duration(val)).count();
            return ret > 1 ? ret : 0;
        }
    protected:
        time_point<system_clock, max_unit_duration> m_last_time;
        uint16_t m_clock_seq;
        typename max_unit_duration::rep m_adjustment = 0;
        typename max_unit_duration::rep m_max_adjustment;
        persistence_holder m_persistance;
        bool m_initialized = false;
        bool m_locked_for_fork = false;
    };

    template<class UnitDuration, class MaxUnitDuration>
    class non_repeatable_clock_state :  public clock_state_base<non_repeatable_clock_state<UnitDuration, MaxUnitDuration>, 
                                                                UnitDuration, MaxUnitDuration> {
        friend clock_state_base<non_repeatable_clock_state, UnitDuration, MaxUnitDuration>;
        friend muuid::impl::singleton_holder<non_repeatable_clock_state>;
        friend muuid::impl::reset_on_fork_thread_local<non_repeatable_clock_state, 1>;
                            
    public:
        template<int PersistanceId>
        static non_repeatable_clock_state & instance(clock_persistence * pers) { 
            auto & ret = reset_on_fork_thread_local<non_repeatable_clock_state, PersistanceId>::instance(); 
            ret.set_persistence(pers);
            return ret;
        }
        
        void get(time_point<system_clock, MaxUnitDuration> & adjusted_now, uint16_t & clock_seq) {
            std::lock_guard guard{this->m_persistance};
            for (auto now = system_clock::now(); ; now = next_distinct_now(now)) {
                if (adjust(now, adjusted_now, clock_seq))
                    break;
            }
            this->m_persistance.save(this->m_last_time, this->m_clock_seq, this->m_adjustment);
        }

    private:
        non_repeatable_clock_state() = default;
        ~non_repeatable_clock_state() = default;

        void init_new() {
            this->m_last_time = round<MaxUnitDuration>(system_clock::now()) - 1s;
            auto & gen = get_random_generator();
            std::uniform_int_distribution<uint16_t> clock_seq_distrib(0, 0x3FFF);
            this->m_clock_seq = clock_seq_distrib(gen);
            this->m_adjustment = 0;
        }

        bool adjust(system_clock::time_point now, time_point<system_clock, MaxUnitDuration> & adjusted, uint16_t & clock_seq) {

            adjusted = round<MaxUnitDuration>(now);
            //on a miniscule change that we have misdetected m_max_adjustment let's make sure
            //that adjusted is rounded on the m_max_adjustment boundary to avoid spillover
            if (this->m_max_adjustment != 0) {
                auto val = adjusted.time_since_epoch();
                val = (val / this->m_max_adjustment) * this->m_max_adjustment;
                adjusted = time_point<system_clock, MaxUnitDuration>(val);
            }
            
            if (adjusted < this->m_last_time) {
                this->m_clock_seq = (this->m_clock_seq + 1) & 0x3FFF;
                this->m_adjustment = 0;
                this->m_last_time = adjusted;
            } else if (adjusted == this->m_last_time) {
                if (this->m_adjustment >= this->m_max_adjustment)
                    return false;
                ++this->m_adjustment;
            } else {
                this->m_adjustment = 0;
                this->m_last_time = adjusted;
            }
            
            adjusted += MaxUnitDuration(this->m_adjustment);
            clock_seq = this->m_clock_seq;
            return true;
        }
    };

    template<class UnitDuration, class MaxUnitDuration>
    class monotonic_clock_state : public clock_state_base<monotonic_clock_state<UnitDuration, MaxUnitDuration>, 
                                                          UnitDuration, MaxUnitDuration> {
        friend clock_state_base<monotonic_clock_state, UnitDuration, MaxUnitDuration>;
        friend muuid::impl::singleton_holder<monotonic_clock_state>;
        friend muuid::impl::reset_on_fork_thread_local<monotonic_clock_state, 6>;
        friend muuid::impl::reset_on_fork_thread_local<monotonic_clock_state, 7>;                    
    public:
        template<int PersistanceId>
        static monotonic_clock_state & instance(clock_persistence * pers) { 
            auto & ret = reset_on_fork_thread_local<monotonic_clock_state, PersistanceId>::instance(); 
            ret.set_persistence(pers);
            return ret;
        }
        
        void get(time_point<system_clock, MaxUnitDuration> & adjusted_now, uint16_t & clock_seq) {

            std::lock_guard guard{this->m_persistance};
            auto now = system_clock::now();
            if (!adjust(now, adjusted_now, clock_seq, false)) {
                do {
                    now = next_distinct_now(now);
                } while (!adjust(now, adjusted_now, clock_seq, true));
            }
            this->m_persistance.save(this->m_last_time, this->m_clock_seq, this->m_adjustment);
        }

    private:
        monotonic_clock_state() = default;
        ~monotonic_clock_state() = default; 

        void init_new() {
            this->m_last_time = round<MaxUnitDuration>(system_clock::now()) - 1s;
            auto & gen = get_random_generator();
            std::uniform_int_distribution<uint16_t> clock_seq_distrib(0, 0x3FFF / 2);
            this->m_clock_seq = clock_seq_distrib(gen);
            this->m_adjustment = 0;
        }

        bool adjust(system_clock::time_point now, time_point<system_clock, MaxUnitDuration> & adjusted, uint16_t & clock_seq,
                    bool after_wait) {

            adjusted = round<MaxUnitDuration>(now);
            //on a miniscule change that we have misdetected m_max_adjustment let's make sure
            //that adjusted is rounded on the m_max_adjustment boundary to avoid spillover
            if (this->m_max_adjustment != 0) {
                auto val = adjusted.time_since_epoch();
                val = (val / this->m_max_adjustment) * this->m_max_adjustment;
                adjusted = time_point<system_clock, MaxUnitDuration>(val);
            }
            
            if (adjusted < this->m_last_time) {
                //we lost monotonicity
                //reset everything to current time and base state
                auto & gen = get_random_generator();
                std::uniform_int_distribution<uint16_t> clock_seq_distrib(0, 0x3FFF / 2);
                this->m_clock_seq = clock_seq_distrib(gen);
                this->m_adjustment = 0;
                this->m_last_time = adjusted;
            } else if (adjusted == this->m_last_time) {
                if (this->m_adjustment >= this->m_max_adjustment) {
                    uint16_t new_clock_seq = (this->m_clock_seq + 1) & 0x3FFF;
                    if (new_clock_seq == 0)
                        return false;
                    this->m_clock_seq = new_clock_seq;
                } else {
                    ++this->m_adjustment;
                }
            } else {
                this->m_adjustment = 0;
                this->m_last_time = adjusted;
                if (after_wait) {
                    auto & gen = get_random_generator();
                    std::uniform_int_distribution<uint16_t> clock_seq_distrib(0, 0x3FFF / 2);
                    this->m_clock_seq = clock_seq_distrib(gen);
                }
            }
            
            adjusted += MaxUnitDuration(this->m_adjustment);
            clock_seq = this->m_clock_seq;
            return true;
        }
    };

}

template<class State>
requires(std::is_same_v<typename State::unit_duration, hundred_nanoseconds> && 
         std::is_same_v<typename State::max_unit_duration, hundred_nanoseconds>)
static clock_result_v1 get_clock(State & state) {
    time_point<system_clock, hundred_nanoseconds> adjusted_now;
    uint16_t clock_seq;
    state.get(adjusted_now, clock_seq);

    uint64_t clock = adjusted_now.time_since_epoch().count();
    //gregorian offset of Unix epoch
    clock += ((uint64_t(0x01B21DD2)) << 32) + 0x13814000;

    return {clock, clock_seq};
}


template<class State>
requires(std::is_same_v<typename State::unit_duration, milliseconds> && 
         std::is_same_v<typename State::max_unit_duration, microseconds>)
static clock_result_v7 get_unix_clock(State & state) {
    time_point<system_clock, microseconds> adjusted_now;
    uint16_t clock_seq;
    state.get(adjusted_now, clock_seq);

    auto interval = adjusted_now.time_since_epoch();
    auto interval_ms = duration_cast<milliseconds>(interval);
    
    uint64_t clock = interval_ms.count();
    uint64_t remainder = (interval - duration_cast<microseconds>(interval_ms)).count();
    uint64_t frac = remainder * 4096;
    uint16_t extra = uint16_t(frac / 1000) + uint16_t(frac % 1000 >= 500);
    return {clock, extra, clock_seq};
}

clock_result_v1 muuid::impl::get_clock_v1() {
    using state_type = non_repeatable_clock_state<hundred_nanoseconds, hundred_nanoseconds>;

    auto * pers = g_clock_persistence_v1.get();
    auto & state = state_type::instance<1>(pers);
    return get_clock(state);
}

clock_result_v6 muuid::impl::get_clock_v6() {
    using state_type = monotonic_clock_state<hundred_nanoseconds, hundred_nanoseconds>;

    auto * pers = g_clock_persistence_v6.get();
    auto & state = state_type::instance<6>(pers);
    return get_clock(state);
}

clock_result_v7 muuid::impl::get_clock_v7() {
    using state_type = monotonic_clock_state<milliseconds, microseconds>;

    auto * pers = g_clock_persistence_v7.get();
    auto & state = state_type::instance<7>(pers);
    return get_unix_clock(state);
}

void muuid::set_time_based_persistence(clock_persistence * pers) {
    if (pers)
        pers->add_ref();
    auto old = g_clock_persistence_v1.exchange(pers);
    if (old)
        old->sub_ref();
}

void muuid::set_reordered_time_based_persistence(clock_persistence * pers) {
    if (pers)
        pers->add_ref();
    auto old = g_clock_persistence_v6.exchange(pers);
    if (old)
        old->sub_ref();
}

void muuid::set_unix_time_based_persistence(clock_persistence * pers) {
    if (pers)
        pers->add_ref();
    auto old = g_clock_persistence_v7.exchange(pers);
    if (old)
        old->sub_ref();
}

