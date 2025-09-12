// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "test_util.h"

#include <modern-uuid/ulid.h>

#include "persistence.h"

#if MUUID_MULTITHREADED

#include <thread>
#include <latch>

using namespace muuid;
using namespace std::literals;

class ulid_per_thread : public per_thread_file_clock_persistence<ulid_persistence_data> {
    using super = per_thread_file_clock_persistence<ulid_persistence_data>;
public:
    using data = ulid_persistence_data;
public:
    ulid_per_thread(const std::filesystem::path & path):
        super(path)
    {}

    bool load(data & d) override {
        using when_type = data::time_point_t::rep;

        uint8_t buf[sizeof(when_type) + sizeof(d.adjustment) + sizeof(d.random)];
        if (!super::read(buf))
            return false;
        
        uint8_t * current = buf;
        
        when_type count;
        memcpy(&count, current, sizeof(count));
        d.when = data::time_point_t(data::time_point_t::duration(count));
        current += sizeof(count);
        memcpy(&d.adjustment, current, sizeof(d.adjustment));
        current += sizeof(d.adjustment);
        memcpy(&d.random, current, sizeof(d.random));
        return true;
    }

    void store(const data & d) override {
        using when_type = data::time_point_t::rep;
        
        uint8_t buf[sizeof(when_type) + sizeof(d.adjustment) + sizeof(d.random)];
        uint8_t * current = buf;

        when_type count = d.when.time_since_epoch().count();
        memcpy(current, &count, sizeof(count));
        current += sizeof(count);
        memcpy(current, &d.adjustment, sizeof(d.adjustment));
        current += sizeof(d.adjustment);
        memcpy(current, &d.random, sizeof(d.random));
        
        super::write(buf);
    }
};


static auto g_path = std::filesystem::path("pers.bin");
static file_clock_persistence<ulid_per_thread> pers(g_path);

TEST_SUITE("ulid persistence") {

TEST_CASE("basic") {

    {
        struct restore_pers {
            ~restore_pers() {
                set_ulid_persistence(nullptr);
            }
        } restore_pers;

        remove(g_path);
        set_ulid_persistence(&pers);

        CHECK(pers.ref_count() != 0);

        ulid::generate();

        int check_count = 0;
        for(int i = 0; i < 50; ++i) {

            ulid u1,u2;

            std::latch start{1};

            std::thread t1([&]() {
                start.wait();
                u1 = ulid::generate();
            });
            std::thread t2([&]() {
                start.wait();
                u2 = ulid::generate();
            });

            start.count_down();

            t1.join();
            t2.join();

            auto time1 = std::span(u1.bytes).subspan(0, 6);
            auto time2 = std::span(u2.bytes).subspan(0, 6);

            if (std::equal(time1.begin(), time1.end(), time2.begin(), time2.end())) {
                CHECK(u1 != u2);
                uint64_t ran1_high = (uint64_t(u1.bytes[6 ]) << 56) | (uint64_t(u1.bytes[7 ]) << 48) |
                                     (uint64_t(u1.bytes[8 ]) << 40) | (uint64_t(u1.bytes[9 ]) << 32) |
                                     (uint64_t(u1.bytes[10]) << 24) | (uint64_t(u1.bytes[11]) << 16) |
                                     (uint64_t(u1.bytes[12]) << 8 ) |           u1.bytes[13];
                uint16_t ran1_low = (u1.bytes[14] << 8) | u1.bytes[15];

                uint64_t ran2_high = (uint64_t(u2.bytes[6 ]) << 56) | (uint64_t(u2.bytes[7 ]) << 48) |
                                     (uint64_t(u2.bytes[8 ]) << 40) | (uint64_t(u2.bytes[9 ]) << 32) |
                                     (uint64_t(u2.bytes[10]) << 24) | (uint64_t(u2.bytes[11]) << 16) |
                                     (uint64_t(u2.bytes[12]) << 8 ) |           u2.bytes[13];
                uint16_t ran2_low = (u2.bytes[14] << 8) | u2.bytes[15];

                if (ran1_high != ran2_high)
                    CHECK(abs(int64_t(ran1_high) - int64_t(ran1_high)) == 1);
                CHECK(abs(int16_t(ran1_low) - int16_t(ran2_low)) == 1);
                ++check_count;
            }

        }
        if (!check_count) {
            WARN("No ulid persistence checks made - generation too slow");
        }

        set_ulid_persistence(nullptr);
    }
    ulid::generate();
    CHECK(pers.ref_count() == 0);
}

}

#endif
