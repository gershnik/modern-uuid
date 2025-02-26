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

using namespace std::chrono;
using namespace std::literals;
using namespace muuid;
using namespace muuid::impl;

using hundred_nanoseconds = duration<int64_t, std::ratio<int64_t(1), int64_t(10'000'000)>>;

static atomic_if_multithreaded<clock_persistence *> g_clock_persistence_v1{};
static atomic_if_multithreaded<clock_persistence *> g_clock_persistence_v6{};
static atomic_if_multithreaded<clock_persistence *> g_clock_persistence_v7{};

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

namespace {

    #if MUUID_MULTITHREADED

        template<int PersistanceId>
        class mutex_persistence final : public clock_persistence, clock_persistence::per_thread {
        public:
            clock_persistence::per_thread & get_for_current_thread() override {
                return *this;
            }

            void add_ref() noexcept override 
            {}
            void sub_ref() noexcept override 
            {}

            void close() noexcept override 
            {}

            void lock() override
                { m_mutex.lock(); }
            void unlock() override
                { m_mutex.unlock(); }

            bool load(data & d) override
                { return false; }
            void store(const data & d) override
            {}
        private:
            mutex_persistence() = default;
            ~mutex_persistence() = default;
            mutex_persistence(const mutex_persistence &) noexcept = delete;
            mutex_persistence & operator=(const mutex_persistence &) noexcept = delete;
        private:
            std::mutex m_mutex{};

        public:
            static mutex_persistence instance;
        };
        template<int PersistanceId>
        mutex_persistence<PersistanceId> mutex_persistence<PersistanceId>::instance{};

        template<int PersistanceId>
        clock_persistence * const g_synchronized_persistence = &mutex_persistence<PersistanceId>::instance;

    #else
        template<int PersistanceId>
        clock_persistence * const g_synchronized_persistence = nullptr;
    #endif

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

    template<class UnitDuration, class MaxUnitDuration>
    class clock_state {
        friend muuid::impl::singleton_holder<clock_state>;
        friend muuid::impl::reset_on_fork_thread_local<clock_state, 1>;
        friend muuid::impl::reset_on_fork_thread_local<clock_state, 6>;
        friend muuid::impl::reset_on_fork_thread_local<clock_state, 7>;
                            
    public:
        template<int PersistanceId>
        static clock_state & instance(clock_persistence * pers) { 
            clock_state & ret = reset_on_fork_thread_local<clock_state, PersistanceId>::instance(); 
            ret.set_persistence(pers);
            return ret;
        }
        
        void get(time_point<system_clock, MaxUnitDuration> & adjusted_now, uint16_t & clock_seq) {
            for ( ; ; ) {
                auto now = system_clock::now();
                {
                    if (this->adjust(now, adjusted_now, clock_seq))
                        break;
                }
            }
        }

    private:
        clock_state():
            m_max_adjustment(get_max_adjustment<MaxUnitDuration>()) 
        {}

        ~clock_state() {
            if (this->m_locked_for_fork)
                this->m_persistance.unlock();
        }

        void init_new() {
            this->m_last_time = round<MaxUnitDuration>(system_clock::now()) - 1s;
            auto & gen = get_random_generator();
            std::uniform_int_distribution<uint16_t> clock_seq_distrib(0, 0x3FFF);
            this->m_clock_seq = clock_seq_distrib(gen);
            reset_adjustment();
        }

        void set_persistence(clock_persistence * pers) {
            
            if (this->m_persistance.set(pers)) {
                std::lock_guard guard{this->m_persistance};

                if (!m_persistance.load(this->m_last_time, this->m_clock_seq, this->m_adjustment))
                {
                    this->init_new();
                    this->m_persistance.save(this->m_last_time, this->m_clock_seq, this->m_adjustment);
                } else {
                    this->m_clock_seq &= 0x3FFF;
                    if (this->m_adjustment < 0)
                        this->m_adjustment = 0;
                }
            }
        }

        bool adjust(system_clock::time_point now, time_point<system_clock, MaxUnitDuration> & adjusted, uint16_t & clock_seq) {

            std::lock_guard guard{m_persistance};

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
                reset_adjustment();
                this->m_last_time = adjusted;
            } else if (adjusted == this->m_last_time) {
                if (this->m_adjustment >= this->m_max_adjustment) {
                    this->m_persistance.save(this->m_last_time, this->m_clock_seq, this->m_adjustment);
                    return false;
                }
                ++this->m_adjustment;
            } else {
                reset_adjustment();
                this->m_last_time = adjusted;
            }
            
            this->m_persistance.save(this->m_last_time, this->m_clock_seq, this->m_adjustment);
            adjusted += MaxUnitDuration(this->m_adjustment);
            clock_seq = this->m_clock_seq;
            return true;
        }

        void reset_adjustment() {
            if (this->m_max_adjustment != 0) {
                auto & gen = get_random_generator();
                std::uniform_int_distribution<typename MaxUnitDuration::rep> adjustment_distrib(0, this->m_max_adjustment - 2);
                this->m_adjustment = adjustment_distrib(gen);
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
        time_point<system_clock, MaxUnitDuration> m_last_time;
        uint16_t m_clock_seq;
        typename MaxUnitDuration::rep m_adjustment = 0;
        typename MaxUnitDuration::rep m_max_adjustment;
        persistence_holder m_persistance;
        bool m_locked_for_fork = false;
    };
}

static clock_result_v1 get_clock(clock_state<hundred_nanoseconds, hundred_nanoseconds> & state) {
    time_point<system_clock, hundred_nanoseconds> adjusted_now;
    uint16_t clock_seq;
    state.get(adjusted_now, clock_seq);

    uint64_t clock = adjusted_now.time_since_epoch().count();
    //gregorian offset of Unix epoch
    clock += ((uint64_t(0x01B21DD2)) << 32) + 0x13814000;

    return {clock, clock_seq};
}

static clock_result_v7 get_unix_clock(clock_state<milliseconds, microseconds> & state) {
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

    auto * pers = g_clock_persistence_v1.get();

    auto & state = clock_state<hundred_nanoseconds, hundred_nanoseconds>::instance<1>(pers);
    return get_clock(state);
}

clock_result_v6 muuid::impl::get_clock_v6() {
    auto * pers = g_clock_persistence_v6.get();
    auto & state = clock_state<hundred_nanoseconds, hundred_nanoseconds>::instance<6>(pers ? pers : g_synchronized_persistence<6>);
    return get_clock(state);
}

clock_result_v7 muuid::impl::get_clock_v7() {

    auto * pers = g_clock_persistence_v7.get();
    auto & state = clock_state<milliseconds, microseconds>::instance<7>(pers ? pers : g_synchronized_persistence<7>);
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

