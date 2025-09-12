// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "test_util.h"

#include <modern-uuid/uuid.h>

#include "persistence.h"

#if MUUID_MULTITHREADED

#include <thread>

using namespace muuid;
using namespace std::literals;

class uuid_per_thread : public per_thread_file_clock_persistence<uuid_persistence_data> {
    using super = per_thread_file_clock_persistence<uuid_persistence_data>;
public:
    using data = uuid_persistence_data;
public:
    uuid_per_thread(const std::filesystem::path & path):
        super(path)
    {}

    bool load(data & d) override {
        using when_type = data::time_point_t::rep;

        uint8_t buf[sizeof(when_type) + sizeof(d.seq) + sizeof(d.adjustment)];
        if (!super::read(buf))
            return false;
        
        uint8_t * current = buf;
        
        when_type count;
        memcpy(&count, current, sizeof(count));
        d.when = data::time_point_t(data::time_point_t::duration(count));
        current += sizeof(count);
        memcpy(&d.seq, current, sizeof(d.seq));
        current += sizeof(d.seq);
        memcpy(&d.adjustment, current, sizeof(d.adjustment));
        return true;
    }

    void store(const data & d) override {
        using when_type = data::time_point_t::rep;
        
        uint8_t buf[sizeof(when_type) + sizeof(d.seq) + sizeof(d.adjustment)];
        uint8_t * current = buf;

        when_type count = d.when.time_since_epoch().count();
        memcpy(current, &count, sizeof(count));
        current += sizeof(count);
        memcpy(current, &d.seq, sizeof(d.seq));
        current += sizeof(d.seq);
        memcpy(current, &d.adjustment, sizeof(d.adjustment));
        
        super::write(buf);
    }
};


static auto path = std::filesystem::path("pers.bin");
static file_clock_persistence<uuid_per_thread> pers(path);

TEST_SUITE("persistence") {

TEST_CASE("clock time_based") {

    {
        struct restore_pers {
            ~restore_pers() {
                set_time_based_persistence(nullptr);
            }
        } restore_pers;

        remove(path);
        set_time_based_persistence(&pers);

        CHECK(pers.ref_count() != 0);

        uuid u1 = uuid::generate_time_based();
        uuid u2;

        std::thread([&]() {
            u2 = uuid::generate_time_based();
        }).join();

        auto parts1 = u1.to_parts();
        auto parts2 = u2.to_parts();

        CHECK(parts1.clock_seq == parts2.clock_seq);
        CHECK(parts1.time_hi_and_version == parts2.time_hi_and_version);
        CHECK(parts1.time_mid == parts2.time_mid);
        CHECK(parts1.time_low < parts2.time_low);

        set_time_based_persistence(nullptr);

        std::thread([&]() {
            u2 = uuid::generate_time_based();
        }).join();

        parts2 = u2.to_parts();
        CHECK(parts1.clock_seq != parts2.clock_seq);

        set_time_based_persistence(&pers);
        std::thread([&]() {
            u2 = uuid::generate_time_based();
        }).join();

        parts2 = u2.to_parts();
        CHECK(parts1.clock_seq == parts2.clock_seq);
    }
    uuid::generate_time_based();
    CHECK(pers.ref_count() == 0);
}

TEST_CASE("clock reordered_time_based") {

    {
        struct restore_pers {
            ~restore_pers() {
                set_reordered_time_based_persistence(nullptr);
            }
        } restore_pers;

        remove(path);
        
        set_reordered_time_based_persistence(&pers);

        CHECK(pers.ref_count() != 0);

        uuid u1 = uuid::generate_reordered_time_based();
        uuid u2;

        std::thread([&]() {
            u2 = uuid::generate_reordered_time_based();
        }).join();

        auto parts1 = u1.to_parts();
        auto parts2 = u2.to_parts();

        CHECK(parts1.clock_seq == parts2.clock_seq);
        
        set_reordered_time_based_persistence(nullptr);

        std::thread([&]() {
            u2 = uuid::generate_reordered_time_based();
        }).join();

        parts2 = u2.to_parts();
        CHECK(parts1.clock_seq != parts2.clock_seq);

        set_reordered_time_based_persistence(&pers);
        std::thread([&]() {
            u2 = uuid::generate_reordered_time_based();
        }).join();

        parts2 = u2.to_parts();
        CHECK(parts1.clock_seq == parts2.clock_seq);
    }
    uuid::generate_reordered_time_based();
    CHECK(pers.ref_count() == 0);
}

TEST_CASE("clock unix_time_based") {

    {
        struct restore_pers {
            ~restore_pers() {
                set_unix_time_based_persistence(nullptr);
            }
        } restore_pers;

        remove(path);
        
        set_unix_time_based_persistence(&pers);

        CHECK(pers.ref_count() != 0);

        uuid u1 = uuid::generate_unix_time_based();
        uuid u2;

        std::thread([&]() {
            u2 = uuid::generate_unix_time_based();
        }).join();

        auto parts1 = u1.to_parts();
        auto parts2 = u2.to_parts();

        CAPTURE(u1);
        CAPTURE(u2);
        if (parts1.time_hi_and_version != parts2.time_hi_and_version || parts1.time_mid != parts2.time_mid)
            CHECK(parts1.clock_seq == parts2.clock_seq); //THIS MIGHT supuriously fail once in a while
        else
            CHECK(parts1.clock_seq != parts2.clock_seq); //THIS MIGHT supuriously fail once in a while
        
        set_unix_time_based_persistence(nullptr);

        std::thread([&]() {
            u2 = uuid::generate_unix_time_based();
        }).join();

        parts2 = u2.to_parts();
        CHECK(parts1.clock_seq != parts2.clock_seq); //THIS MIGHT supuriously fail once in a while

        set_unix_time_based_persistence(&pers);
        std::thread([&]() {
            u2 = uuid::generate_unix_time_based();
        }).join();

        parts2 = u2.to_parts();
        if (parts1.time_hi_and_version != parts2.time_hi_and_version || parts1.time_mid != parts2.time_mid)
            CHECK(parts1.clock_seq == parts2.clock_seq); //THIS MIGHT supuriously fail once in a while
        else
            CHECK(parts1.clock_seq != parts2.clock_seq); //THIS MIGHT supuriously fail once in a while
    }
    uuid::generate_unix_time_based();
    CHECK(pers.ref_count() == 0);
}

TEST_CASE("node time_based") {

    struct restore {
        ~restore() {
            set_node_id(node_id::detect_system);
        }
    } restore;

    uuid u1 = uuid::generate_time_based();
    auto parts1 = u1.to_parts();
    
    auto calculated = set_node_id(node_id::generate_random);
    
    uuid u2 = uuid::generate_time_based();
    auto parts2 = u2.to_parts();

    CHECK_UNEQUAL_SEQ(parts1.node, parts2.node);
    CHECK_EQUAL_SEQ(parts2.node, calculated);

    uuid u3 = uuid::generate_time_based();
    auto parts3 = u3.to_parts();

    CHECK_EQUAL_SEQ(parts2.node, parts3.node);
    
    calculated = set_node_id(node_id::detect_system);

    u3 = uuid::generate_time_based();
    parts3 = u3.to_parts();

    CHECK_EQUAL_SEQ(parts3.node, calculated);

    uint8_t dummy[6] = {1, 2, 3, 4, 5, 6};
    set_node_id(dummy);

    u3 = uuid::generate_time_based();
    parts3 = u3.to_parts();

    CHECK_EQUAL_SEQ(parts3.node, dummy);
}

}

#endif
