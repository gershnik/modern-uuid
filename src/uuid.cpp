// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <modern-uuid/uuid.h>

#include "random_generator.h"
#include "node_id.h"
#include "clocks.h"

extern "C" {
    #include "external/md5.h"
    #include "external/sha1.h"
}

using namespace muuid;

auto uuid::generate_random() noexcept -> uuid {
    auto & gen = impl::get_random_generator();
    std::uniform_int_distribution<uint32_t> distrib;

    std::array<uint32_t, 4> buf;
    for(auto & b: buf) {
        b = distrib(gen);
    }
    uuid * ret = reinterpret_cast<uuid *>(&buf);
    
    ret->bytes[8] = (ret->bytes[8] & 0x3F) | 0x80;
    ret->bytes[6] = (ret->bytes[6] & 0x0F) | 0x40;

    return *ret;
}

auto uuid::generate_md5(uuid ns, std::string_view name) noexcept -> uuid {
	
	static_assert(sizeof(uuid) == MUUID_MD5LENGTH);
    
    MUUID_MD5_CTX ctx;
	muuid_MD5Init(&ctx);
	muuid_MD5Update(&ctx, ns.bytes.data(), ns.bytes.size());
	muuid_MD5Update(&ctx, (const uint8_t *)name.data(), unsigned(name.size()));
    uuid ret;
	muuid_MD5Final(ret.bytes.data(), &ctx);

	ret.bytes[8] = (ret.bytes[8] & 0x3F) | 0x80;
	ret.bytes[6] = (ret.bytes[6] & 0x0F) | 0x30;
	return ret;
}

auto uuid::generate_sha1(uuid ns, std::string_view name) noexcept -> uuid {
    
	static_assert(sizeof(uuid) < MUUID_SHA1LENGTH);

    struct buffer {
        uuid ret;
        uint8_t padding[MUUID_SHA1LENGTH - sizeof(uuid)];
    };

    static_assert(sizeof(buffer) == MUUID_SHA1LENGTH);
    
    MUUID_SHA1_CTX ctx;
	muuid_SHA1Init(&ctx);
	muuid_SHA1Update(&ctx, ns.bytes.data(), ns.bytes.size());
	muuid_SHA1Update(&ctx, (const uint8_t *)name.data(), unsigned(name.size()));
    buffer buf;
    muuid_SHA1Final(buf.ret.bytes.data(), &ctx);

    buf.ret.bytes[8] = (buf.ret.bytes[8] & 0x3F) | 0x80;
	buf.ret.bytes[6] = (buf.ret.bytes[6] & 0x0F) | 0x50;
    return buf.ret;
}

auto uuid::generate_time_based() -> uuid {    
    std::span<const uint8_t, 6> node_id = impl::get_node_id();
    auto [clock, clock_seq] = impl::get_clock_v1();

    uint32_t clock_high = uint32_t(clock >> 32);
    uint32_t clock_low = uint32_t(clock);

    uuid_parts parts;
    parts.time_low = clock_low;
    parts.time_mid = uint16_t(clock_high);
    parts.time_hi_and_version = ((clock_high >> 16) & 0x0FFF) | 0x1000;
    parts.clock_seq = clock_seq | 0x8000;
    memcpy(parts.node, node_id.data(), node_id.size());

    return uuid(parts);
}

auto uuid::generate_reordered_time_based() -> uuid {    
    std::span<const uint8_t, 6> node_id = impl::get_node_id();
    auto [clock, clock_seq] = impl::get_clock_v6();

    clock <<= 4;
    uint32_t clock_high = uint32_t(clock >> 32);
    uint32_t clock_low = uint32_t(clock);

    uuid_parts parts;
    parts.time_low = clock_high;
    parts.time_mid = uint16_t(clock_low >> 16);
    parts.time_hi_and_version = ((clock_low >> 4) & 0x0FFF) | 0x6000;
    parts.clock_seq = clock_seq | 0x8000;
    memcpy(parts.node, node_id.data(), node_id.size());

    return uuid(parts);
}

auto uuid::generate_unix_time_based() -> uuid {  
    auto [clock, extra, clock_seq] = impl::get_clock_v7();

    uuid_parts parts;
    parts.time_low = uint32_t(clock >> 16);
    parts.time_mid = uint16_t(clock);
    parts.time_hi_and_version = (extra & 0x0FFF) | 0x7000;
    parts.clock_seq = clock_seq | 0x8000;
    
    auto & gen = impl::get_random_generator();
    std::uniform_int_distribution<unsigned> distrib(0, 255);
    for (auto & b: parts.node)
        b = uint8_t(distrib(gen));
    
    return uuid(parts);
}

void muuid::set_time_based_persistence(clock_persistence_factory * f) {
    impl::g_clock_persistence_v1.set(f);
}

void muuid::set_reordered_time_based_persistence(clock_persistence_factory * f) {
    impl::g_clock_persistence_v6.set(f);
}

void muuid::set_unix_time_based_persistence(clock_persistence_factory * f) {
    impl::g_clock_persistence_v7.set(f);
}

