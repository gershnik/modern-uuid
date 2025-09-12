// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <modern-uuid/uuid.h>
#include <modern-uuid/ulid.h>

#include "clocks.h"
#include "random_generator.h"
#include "fork_handler.h"
#include "threading.h"

#include <cstring>
#include <cassert>
#include <algorithm>

using namespace std::chrono;
using namespace std::literals;
using namespace muuid;
using namespace muuid::impl;

using hundred_nanoseconds = duration<int64_t, std::ratio<int64_t(1), int64_t(10'000'000)>>;

static atomic_if_multithreaded<uuid_clock_persistence *> g_clock_persistence_v1{};
static atomic_if_multithreaded<uuid_clock_persistence *> g_clock_persistence_v6{};
static atomic_if_multithreaded<uuid_clock_persistence *> g_clock_persistence_v7{};
static atomic_if_multithreaded<ulid_clock_persistence *> g_clock_persistence_ulid{};

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

    template<class Data>
    class persistence_holder {
    public:
        persistence_holder() noexcept = default;
        ~persistence_holder() noexcept {
            if (this->m_per_thread)
                this->m_per_thread->close();
            if (this->m_persistence)
                this->m_persistence->sub_ref();
        }
        persistence_holder(const persistence_holder &) noexcept = delete;
        persistence_holder & operator=(const persistence_holder &) noexcept = delete;

        bool set(generic_clock_persistence<Data> * persistence) {
            if (this->m_persistence == persistence)
                return false;
            if (this->m_per_thread) {
                this->m_per_thread->close();
                this->m_per_thread = nullptr;
            }
            if (persistence)
                persistence->add_ref();
            if (this->m_persistence)
                this->m_persistence->sub_ref();
            this->m_persistence = persistence;
            if (this->m_persistence)
                this->m_per_thread = &this->m_persistence->get_for_current_thread();
            return true;
        }

        void lock() {
            if (this->m_per_thread)
                this->m_per_thread->lock();
        }
        void unlock() {
            if (this->m_per_thread)
                this->m_per_thread->unlock();
        }

        void save(const Data & data) {
            if (this->m_per_thread)
                this->m_per_thread->store(data);
        }

        bool load(Data & data) {
            return this->m_per_thread && this->m_per_thread->load(data);
        }
        
    private:
        generic_clock_persistence<Data> * m_persistence = nullptr;
        typename generic_clock_persistence<Data>::per_thread * m_per_thread = nullptr;
    };

    template<class Derived, class PersData, class UnitDuration, class MaxUnitDuration>
    class clock_state_base {
    public:
        using unit_duration = UnitDuration;
        using max_unit_duration = MaxUnitDuration;

    public:

        void set_persistence(generic_clock_persistence<PersData> * pers) {

            if constexpr (Derived::load_once) {
                if (this->m_holder.set(pers) || !this->m_initialized) {
                    std::lock_guard guard{this->m_holder};

                    PersData data;
                    if (!m_holder.load(data)) {
                        static_cast<Derived *>(this)->init_new(data);
                        this->m_holder.save(data);
                    } else {
                        static_cast<Derived *>(this)->load_existing(data);
                    }

                    this->m_initialized = true;
                }
            } else {
                this->m_holder.set(pers);
                if (!this->m_initialized) {
                    PersData data;
                    static_cast<Derived *>(this)->init_new(data);
                    //do not save!
                    m_initialized = true;
                }
            }
        }

    protected:
        clock_state_base():
            m_max_adjustment(clock_state_base::get_max_adjustment()) 
        {}

    #if MUUID_HANDLE_FORK
        ~clock_state_base() {
            if (this->m_locked_for_fork)
                this->m_holder.unlock();
        }

        void prepare_fork_in_parent() {
            this->m_holder.lock();
            this->m_locked_for_fork = true;
        }
        void after_fork_in_parent() {
            this->m_holder.unlock();
            this->m_locked_for_fork = false;
        }
    #else
        ~clock_state_base() = default;
    #endif

    protected:
        template<class Func>
        void mutate(Func && func) {
            std::lock_guard guard{this->m_holder};
            PersData data;
            if constexpr (!Derived::load_once) {
                if (m_holder.load(data))
                    static_cast<Derived *>(this)->load_existing(data);
            }
            func(data);
            this->m_holder.save(data);
        }

    private:
        static typename max_unit_duration::rep get_max_adjustment() {
            auto val = get_clock_tick();
            typename max_unit_duration::rep ret = round<max_unit_duration>(system_clock::duration(val)).count();
            return ret > 1 ? ret : 0;
        }
    protected:
        time_point<system_clock, max_unit_duration> m_last_time;
        typename max_unit_duration::rep m_adjustment = 0;
        const typename max_unit_duration::rep m_max_adjustment;

    private:
        persistence_holder<PersData> m_holder;
        bool m_initialized = false;
    #if MUUID_HANDLE_FORK
        bool m_locked_for_fork = false;
    #endif
    };

    template<class UnitDuration, class MaxUnitDuration>
    class non_repeatable_clock_state :  public clock_state_base<non_repeatable_clock_state<UnitDuration, MaxUnitDuration>, 
                                                                uuid_persistence_data,
                                                                UnitDuration, MaxUnitDuration> {
        friend clock_state_base<non_repeatable_clock_state, uuid_persistence_data, UnitDuration, MaxUnitDuration>;
        friend muuid::impl::singleton_holder<non_repeatable_clock_state>;
        friend muuid::impl::reset_on_fork_thread_local<non_repeatable_clock_state, 1>;

        static constexpr bool load_once = true;
                            
    public:
        void get(time_point<system_clock, MaxUnitDuration> & adjusted_now, uint16_t & clock_seq) {
            this->mutate([&](uuid_persistence_data & data) {
                for (auto now = system_clock::now(); ; now = next_distinct_now(now)) {
                    if (this->adjust(now, adjusted_now, clock_seq))
                        break;
                }
                assert(this->m_adjustment <= std::numeric_limits<int32_t>::max());
                data = { time_point_cast<nanoseconds>(this->m_last_time), this->m_clock_seq, int32_t(this->m_adjustment)};
            });
        }

    private:
        non_repeatable_clock_state() = default;
        ~non_repeatable_clock_state() = default;

        void init_new(uuid_persistence_data & data) {
            this->m_last_time = round<MaxUnitDuration>(system_clock::now()) - 1s;
            auto & gen = get_random_generator();
            std::uniform_int_distribution<uint16_t> clock_seq_distrib(0, 0x3FFF);
            this->m_clock_seq = clock_seq_distrib(gen);
            this->m_adjustment = 0;
            data = { time_point_cast<nanoseconds>(this->m_last_time), this->m_clock_seq, int32_t(this->m_adjustment)};  
        }

        void load_existing(const uuid_persistence_data & data) {
            this->m_last_time = std::chrono::time_point_cast<MaxUnitDuration>(data.when);
            this->m_clock_seq = data.seq & 0x3FFF;
            if (data.adjustment < 0)
                this->m_adjustment = 0;
            else
                this->m_adjustment = data.adjustment;
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
    private:
        uint16_t m_clock_seq = 0;
    };

    template<class UnitDuration, class MaxUnitDuration>
    class monotonic_clock_state : public clock_state_base<monotonic_clock_state<UnitDuration, MaxUnitDuration>,
                                                          uuid_persistence_data,
                                                          UnitDuration, MaxUnitDuration> {
        friend clock_state_base<monotonic_clock_state, uuid_persistence_data, UnitDuration, MaxUnitDuration>;
        friend muuid::impl::singleton_holder<monotonic_clock_state>;
        friend muuid::impl::reset_on_fork_thread_local<monotonic_clock_state, 6>;
        friend muuid::impl::reset_on_fork_thread_local<monotonic_clock_state, 7>;
        
        
        static constexpr bool load_once = true;
    public:
        template<int PersistanceId>
        static monotonic_clock_state & instance(uuid_clock_persistence * pers) { 
            auto & ret = reset_on_fork_thread_local<monotonic_clock_state, PersistanceId>::instance(); 
            ret.set_persistence(pers);
            return ret;
        }
        
        void get(time_point<system_clock, MaxUnitDuration> & adjusted_now, uint16_t & clock_seq) {

            this->mutate([&](uuid_persistence_data & data) {
                auto now = system_clock::now();
                if (!this->adjust(now, adjusted_now, clock_seq, false)) {
                    do {
                        now = next_distinct_now(now);
                    } while (!adjust(now, adjusted_now, clock_seq, true));
                }
                assert(this->m_adjustment <= std::numeric_limits<int32_t>::max());
                data = { time_point_cast<nanoseconds>(this->m_last_time), this->m_clock_seq, int32_t(this->m_adjustment)};
            });
        }

    private:
        monotonic_clock_state() = default;
        ~monotonic_clock_state() = default; 

        void init_new(uuid_persistence_data & data) {
            this->m_last_time = round<MaxUnitDuration>(system_clock::now()) - 1s;
            auto & gen = get_random_generator();
            std::uniform_int_distribution<uint16_t> clock_seq_distrib(0, 0x3FFF / 2);
            this->m_clock_seq = clock_seq_distrib(gen);
            this->m_adjustment = 0;
            data = { time_point_cast<nanoseconds>(this->m_last_time), this->m_clock_seq, int32_t(this->m_adjustment)};  
        }

        void load_existing(const uuid_persistence_data & data) {
            this->m_last_time = std::chrono::time_point_cast<MaxUnitDuration>(data.when);
            this->m_clock_seq = data.seq & 0x3FFF;
            if (data.adjustment < 0)
                this->m_adjustment = 0;
            else
                this->m_adjustment = data.adjustment;
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
    private:
        uint16_t m_clock_seq = 0;
    };

    class ulid_clock_state : public clock_state_base<ulid_clock_state,
                                                     ulid_persistence_data,
                                                     milliseconds, milliseconds> {
        friend clock_state_base<ulid_clock_state, ulid_persistence_data, milliseconds, milliseconds>;
        friend muuid::impl::singleton_holder<ulid_clock_state>;
        friend muuid::impl::reset_on_fork_thread_local<ulid_clock_state>;

        static constexpr bool load_once = false;
    public:
        static ulid_clock_state & instance(ulid_clock_persistence * pers) { 
            auto & ret = reset_on_fork_thread_local<ulid_clock_state>::instance(); 
            ret.set_persistence(pers);
            return ret;
        }

        void get(time_point<system_clock, milliseconds> & adjusted_now, uint64_t & tail_low, uint16_t & tail_high) {
            this->mutate([&](ulid_persistence_data & data) {
                auto now = system_clock::now();
                adjust(now, adjusted_now);
                tail_low = m_tail.low;
                tail_high = m_tail.high;
                data.when = m_last_time;
                assert(m_adjustment <= std::numeric_limits<int32_t>::max());
                data.adjustment = int32_t(this->m_adjustment);
                memcpy(data.random, &m_tail, sizeof(m_tail));
            });
        }
    private:
        ulid_clock_state() = default;
        ~ulid_clock_state() = default; 

        void init_new(ulid_persistence_data & data) {
            m_last_time = round<milliseconds>(system_clock::now()) - 1s;
            m_tail.fill_random();
            m_adjustment = 0;
            data.when = m_last_time;
            data.adjustment = int32_t(m_adjustment);
            memcpy(data.random, &m_tail, sizeof(m_tail));
        }

        void load_existing(const ulid_persistence_data & data) {
            m_last_time = data.when;
            memcpy(&m_tail, data.random, sizeof(m_tail));
            if (data.adjustment < 0)
                m_adjustment = 0;
            else
                m_adjustment = data.adjustment;
        }

        void adjust(system_clock::time_point now, time_point<system_clock, milliseconds> & adjusted) {

            adjusted = round<milliseconds>(now);
            //on a miniscule change that we have misdetected m_max_adjustment let's make sure
            //that adjusted is rounded on the m_max_adjustment boundary to avoid spillover
            if (m_max_adjustment != 0) {
                auto val = adjusted.time_since_epoch();
                val = (val / m_max_adjustment) * m_max_adjustment;
                adjusted = time_point<system_clock, milliseconds>(val);
            }
            
            if (adjusted < m_last_time) {
                //we lost monotonicity
                //reset everything to current time and base state
                m_tail.fill_random();
                m_adjustment = 0;
                m_last_time = adjusted;
            } else if (adjusted == m_last_time) {
                if (m_adjustment >= m_max_adjustment) {
                    m_tail.increment();
                } else {
                    ++m_adjustment;
                    m_tail.fill_random();
                }
            } else {
                m_adjustment = 0;
                m_last_time = adjusted;
                m_tail.fill_random();
            }
            
            adjusted += milliseconds(this->m_adjustment);
        }
    private:
        struct tail_t {
            uint64_t low = 0;
            uint16_t high = 0;

            void fill_random() {
                auto & gen = get_random_generator();
                std::uniform_int_distribution<unsigned> distrib(0, 255);
                auto * dest = reinterpret_cast<uint8_t *>(this);
                for (size_t i = 0; i < sizeof(*this); ++i)
                    *dest++ = uint8_t(distrib(gen));
            }
            void increment() {
                if (++low == 0) {
                    ++high;
                }
            }
        } m_tail;
    };

}


clock_result_v1 muuid::impl::get_clock_v1() {
    using state_type = non_repeatable_clock_state<hundred_nanoseconds, hundred_nanoseconds>;

    auto * current_pers = g_clock_persistence_v1.get();
    auto & per_thread_state = reset_on_fork_thread_local<state_type, 1>::instance();
    per_thread_state.set_persistence(current_pers);

    time_point<system_clock, hundred_nanoseconds> adjusted_now;
    uint16_t clock_seq;
    per_thread_state.get(adjusted_now, clock_seq);

    uint64_t clock = adjusted_now.time_since_epoch().count();
    //gregorian offset of Unix epoch
    clock += ((uint64_t(0x01B21DD2)) << 32) + 0x13814000;

    return {clock, clock_seq};
}

clock_result_v6 muuid::impl::get_clock_v6() {
    using state_type = monotonic_clock_state<hundred_nanoseconds, hundred_nanoseconds>;

    auto * current_pers = g_clock_persistence_v6.get();
    auto & per_thread_state = reset_on_fork_thread_local<state_type, 6>::instance();
    per_thread_state.set_persistence(current_pers);

    time_point<system_clock, hundred_nanoseconds> adjusted_now;
    uint16_t clock_seq;
    per_thread_state.get(adjusted_now, clock_seq);

    uint64_t clock = adjusted_now.time_since_epoch().count();
    //gregorian offset of Unix epoch
    clock += ((uint64_t(0x01B21DD2)) << 32) + 0x13814000;

    return {clock, clock_seq};
}

clock_result_v7 muuid::impl::get_clock_v7() {
    using state_type = monotonic_clock_state<milliseconds, microseconds>;

    auto * current_pers = g_clock_persistence_v7.get();
    auto & per_thread_state = reset_on_fork_thread_local<state_type, 7>::instance();
    per_thread_state.set_persistence(current_pers);

    time_point<system_clock, microseconds> adjusted_now;
    uint16_t clock_seq;
    per_thread_state.get(adjusted_now, clock_seq);

    auto interval = adjusted_now.time_since_epoch();
    auto interval_ms = duration_cast<milliseconds>(interval);

    uint64_t clock = interval_ms.count();
    uint64_t remainder = (interval - duration_cast<microseconds>(interval_ms)).count();
    uint64_t frac = remainder * 4096;
    uint16_t extra = uint16_t(frac / 1000) + uint16_t(frac % 1000 >= 500);
    return {clock, extra, clock_seq};
}

clock_result_ulid muuid::impl::get_clock_ulid() {

    auto * current_pers = g_clock_persistence_ulid.get();
    auto & per_thread_state = reset_on_fork_thread_local<ulid_clock_state>::instance();
    per_thread_state.set_persistence(current_pers);

    time_point<system_clock, milliseconds> adjusted_now;
    uint64_t tail_low;
    uint16_t tail_high;
    per_thread_state.get(adjusted_now, tail_low, tail_high);

    auto interval = adjusted_now.time_since_epoch();
    return {uint64_t(interval.count()), tail_low, tail_high};
}

void muuid::set_time_based_persistence(uuid_clock_persistence * pers) {
    if (pers)
        pers->add_ref();
    auto old = g_clock_persistence_v1.exchange(pers);
    if (old)
        old->sub_ref();
}

void muuid::set_reordered_time_based_persistence(uuid_clock_persistence * pers) {
    if (pers)
        pers->add_ref();
    auto old = g_clock_persistence_v6.exchange(pers);
    if (old)
        old->sub_ref();
}

void muuid::set_unix_time_based_persistence(uuid_clock_persistence * pers) {
    if (pers)
        pers->add_ref();
    auto old = g_clock_persistence_v7.exchange(pers);
    if (old)
        old->sub_ref();
}

void muuid::set_ulid_persistence(ulid_clock_persistence * pers) {

    if (pers)
        pers->add_ref();
    auto old = g_clock_persistence_ulid.exchange(pers);
    if (old)
        old->sub_ref();
}

namespace muuid {

    //ABI compat functions

// The [[deprecated]] attribute on functions silences deprecation warnings for type usage
// on most compilers these days.
// Except for very old gccs and MSVC (of course)
 #ifdef __GNUC__
     #pragma GCC diagnostic push
     #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
 #elif defined(_MSC_VER)
     #pragma warning(push)
     #pragma warning(disable: 4996)
 #endif

    [[deprecated]] MUUID_EXPORTED void set_time_based_persistence(clock_persistence * persistence) {
        return set_time_based_persistence(static_cast<uuid_clock_persistence *>(persistence));
    }
    [[deprecated]] MUUID_EXPORTED void set_reordered_time_based_persistence(clock_persistence * persistence) {
        return set_reordered_time_based_persistence(static_cast<uuid_clock_persistence *>(persistence));
    }
    [[deprecated]] MUUID_EXPORTED void set_unix_time_based_persistence(clock_persistence * persistence) {
        return set_unix_time_based_persistence(static_cast<uuid_clock_persistence *>(persistence));
    }

 #ifdef __GNUC__
     #pragma GCC diagnostic pop
 #elif defined(_MSC_VER)
     #pragma warning(pop)
 #endif

}
