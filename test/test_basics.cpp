// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <doctest/doctest.h>

#include <modern-uuid/uuid.h>

#include <vector>
#include <sstream>
#include <iostream>
#include <map>
#include <unordered_map>

using namespace muuid;
using namespace std::literals;

TEST_SUITE("basics") {

static_assert(std::is_class_v<uuid>);
static_assert(std::is_trivially_copyable_v<uuid>);
static_assert(std::is_standard_layout_v<uuid>);
static_assert(std::has_unique_object_representations_v<uuid>);
static_assert(!std::is_trivially_default_constructible_v<uuid>);
static_assert(std::is_nothrow_default_constructible_v<uuid>);
static_assert(std::is_trivially_copy_constructible_v<uuid>);
static_assert(std::is_nothrow_copy_constructible_v<uuid>);
static_assert(std::is_trivially_move_constructible_v<uuid>);
static_assert(std::is_nothrow_move_constructible_v<uuid>);
static_assert(std::is_trivially_copy_assignable_v<uuid>);
static_assert(std::is_nothrow_copy_assignable_v<uuid>);
static_assert(std::is_trivially_move_assignable_v<uuid>);
static_assert(std::is_nothrow_move_assignable_v<uuid>);
static_assert(std::is_trivially_destructible_v<uuid>);
static_assert(std::is_nothrow_destructible_v<uuid>);
static_assert(std::equality_comparable<uuid>);
static_assert(std::totally_ordered<uuid>);
#if !defined(_LIBCPP_VERSION) || (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 140000)
static_assert(std::three_way_comparable<uuid>);
#endif
static_assert(std::regular<uuid>);

template<uuid U1> class some_class {};
some_class<uuid("bc961bfb-b006-42f4-93ae-206f02658810")> some_object;

std::map<uuid, std::string> m;
std::unordered_map<uuid, std::string> um;

TEST_CASE("nil") {

    constexpr uuid u;

    static_assert(u.get_variant() == uuid::variant::reserved_ncs);
    static_assert(u.get_type() == uuid::type::none);

    constexpr uint8_t null_bytes[16] = {};

    CHECK(memcmp(u.bytes.data(), null_bytes, u.bytes.size()) == 0);
}

TEST_CASE("bytes") {
    constexpr std::array<uint8_t, 16> buf1 = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    std::vector<uint8_t> buf2{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}};

    constexpr uuid u1(buf1);
    constexpr uuid u2{std::array<uint8_t, 16>{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}}};
    uuid u3{std::span<uint8_t, 16>{buf2}};
    uuid u4{std::span{buf2}.subspan<0, 16>()};

    CHECK(u1 == u2);
    CHECK(u1 != uuid());
    CHECK(uuid() < u2);
    CHECK(u2 == u3);

    CHECK(u2.bytes == buf1);
    CHECK(u3.bytes == buf1);
}

TEST_CASE("literals") {
    constexpr uuid us("7d444840-9dc0-11d1-b245-5ffdce74fad2");

    constexpr std::array<uint8_t, 16> expected = {0x7d,0x44,0x48, 0x40, 0x9d, 0xc0, 0x11, 0xd1, 0xb2, 0x45, 0x5f, 0xfd, 0xce, 0x74, 0xfa, 0xd2};
    CHECK(us.bytes == expected);
}

TEST_CASE("hash") {

    constexpr std::hash<uuid> hasher;
    
    CHECK(hasher(uuid()) != 0);

    constexpr uuid val("7d444840-9dc0-11d1-b245-5ffdce74fad2");
    CHECK(hasher(val) != 0);
    CHECK(hasher(val) != hasher(uuid()));
    CHECK(hasher(val) == hasher(val));
}

TEST_CASE("strings") {

    constexpr uuid us("7d444840-9dc0-11d1-b245-5ffdce74fad2");
    uuid us1 = uuid::from_chars("7d444840-9dc0-11d1-b245-5ffdce74fad2"s).value();
    constexpr uuid us2 = uuid::from_chars("7d444840-9dc0-11d1-b245-5ffdce74fad2").value();

    CHECK(us == us1);
    CHECK(us2 == us1);

    CHECK(us.to_chars() == std::array<char, 37>{"7d444840-9dc0-11d1-b245-5ffdce74fad2"});
    CHECK(us.to_chars(uuid::lowercase) == std::array<char, 37>{"7d444840-9dc0-11d1-b245-5ffdce74fad2"});
    CHECK(us.to_chars(uuid::uppercase) == std::array<char, 37>{"7D444840-9DC0-11D1-B245-5FFDCE74FAD2"});

    CHECK(us1.to_chars() == std::array<char, 37>{"7d444840-9dc0-11d1-b245-5ffdce74fad2"});
    CHECK(us1.to_chars(uuid::lowercase) == std::array<char, 37>{"7d444840-9dc0-11d1-b245-5ffdce74fad2"});
    CHECK(us1.to_chars(uuid::uppercase) == std::array<char, 37>{"7D444840-9DC0-11D1-B245-5FFDCE74FAD2"});

    CHECK(us2.to_string() == "7d444840-9dc0-11d1-b245-5ffdce74fad2");
}

#if MUUID_SUPPORTS_STD_FORMAT
TEST_CASE("format") {

    CHECK(std::format("{}", uuid()) == "00000000-0000-0000-0000-000000000000");
    CHECK(std::format("{}", uuid("7d444840-9dc0-11d1-b245-5ffdce74fad2")) == "7d444840-9dc0-11d1-b245-5ffdce74fad2");
    CHECK(std::format("{:l}", uuid("7d444840-9dc0-11d1-b245-5ffdce74fad2")) == "7d444840-9dc0-11d1-b245-5ffdce74fad2");
    CHECK(std::format("{:u}", uuid("7d444840-9dc0-11d1-b245-5ffdce74fad2")) == "7D444840-9DC0-11D1-B245-5FFDCE74FAD2");
}
#endif

TEST_CASE("output") {
    std::ostringstream obuf;

    obuf << uuid();
    CHECK(obuf.str() == "00000000-0000-0000-0000-000000000000");
    obuf.str("");

    obuf << uuid("7d444840-9dc0-11d1-b245-5ffdce74fad2");
    CHECK(obuf.str() == "7d444840-9dc0-11d1-b245-5ffdce74fad2");
    obuf.str("");

    obuf << std::uppercase << uuid("7d444840-9dc0-11d1-b245-5ffdce74fad2");
    CHECK(obuf.str() == "7D444840-9DC0-11D1-B245-5FFDCE74FAD2");
    obuf.str("");
}

TEST_CASE("input") {
    std::istringstream ibuf;
    uuid val;
    
    ibuf.str("00000000-0000-0000-0000-000000000000");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == uuid());

    ibuf.clear();
    ibuf.str("7d444840-9dc0-11d1-b245-5ffdce74fad2");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == uuid("7d444840-9dc0-11d1-b245-5ffdce74fad2"));

    ibuf.clear();
    ibuf.str("7D444840-9DC0-11D1-B245-5FFDCE74FAD2");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == uuid("7d444840-9dc0-11d1-b245-5ffdce74fad2"));

    ibuf.clear();
    ibuf.str("7D444840-9DC0-11D1-B245-5FFDCE74FAD");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(ibuf.eof());

    ibuf.clear();
    ibuf.str("7D4448409DC0-11D1-B245-5FFDCE74FAD2 ");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(!ibuf.eof());
}

TEST_CASE("random") {
    uuid u1 = uuid::generate_random();
    uuid u2 = uuid::generate_random();

    CHECK(u1.get_variant() == uuid::variant::standard);
    CHECK(u1.get_type() == uuid::type::random);
    CHECK(u2.get_variant() == uuid::variant::standard);
    CHECK(u2.get_type() == uuid::type::random);

    CHECK(u1 != u2);
    CHECK(u1 != uuid());
    CHECK(u2 != uuid());
    CHECK(u1 < uuid::max());
    CHECK(u2 < uuid::max());
}

TEST_CASE("md5") {
    uuid u1 = uuid::generate_md5(uuid::namespaces::dns, "www.widgets.com");
    CHECK(u1 == uuid("3d813cbb-47fb-32ba-91df-831e1593ac29"));
    
    CHECK(u1.get_variant() == uuid::variant::standard);
    CHECK(u1.get_type() == uuid::type::name_based_md5);
    CHECK(u1 < uuid::max());
}

TEST_CASE("sha1") {
    uuid u1 = uuid::generate_sha1(uuid::namespaces::dns, "www.widgets.com");
    CHECK(u1 == uuid("21f7f8de-8051-5b89-8680-0195ef798b6a"));
    
    CHECK(u1.get_variant() == uuid::variant::standard);
    CHECK(u1.get_type() == uuid::type::name_based_sha1);
    CHECK(u1 < uuid::max());
}

TEST_CASE("time_based") {
    uuid u1 = uuid::generate_time_based();
    uuid u2 = uuid::generate_time_based();

    CHECK(u1.get_variant() == uuid::variant::standard);
    CHECK(u1.get_type() == uuid::type::time_based);
    CHECK(u2.get_variant() == uuid::variant::standard);
    CHECK(u2.get_type() == uuid::type::time_based);

    CHECK(u1 != u2);
    CHECK(u1 != uuid());
    CHECK(u2 != uuid());
    CHECK(u1 < uuid::max());
    CHECK(u2 < uuid::max());

    std::cout << "v1: " << u1 << '\n';
    std::cout << "v1: " << u2 << '\n';
}

TEST_CASE("reordered_time_based") {
    uuid u1 = uuid::generate_reordered_time_based();
    uuid u2 = uuid::generate_reordered_time_based();
    uuid u3 = uuid::generate_reordered_time_based();
    
    CHECK(u1.get_variant() == uuid::variant::standard);
    CHECK(u1.get_type() == uuid::type::reordered_time_based);
    CHECK(u2.get_variant() == uuid::variant::standard);
    CHECK(u2.get_type() == uuid::type::reordered_time_based);
    CHECK(u3.get_variant() == uuid::variant::standard);
    CHECK(u3.get_type() == uuid::type::reordered_time_based);

    CHECK(uuid() < u1);
    CHECK(u1 < u2);
    CHECK(u2 < u3);
    CHECK(u3 < uuid::max());
    
    std::cout << "v6: " << u1 << '\n';
    std::cout << "v6: " << u2 << '\n';
    std::cout << "v6: " << u3 << '\n';
}

TEST_CASE("unix_time_based") {
    uuid u1 = uuid::generate_unix_time_based();
    uuid u2 = uuid::generate_unix_time_based();
    uuid u3 = uuid::generate_unix_time_based();
    
    CHECK(u1.get_variant() == uuid::variant::standard);
    CHECK(u1.get_type() == uuid::type::unix_time_based);
    CHECK(u2.get_variant() == uuid::variant::standard);
    CHECK(u2.get_type() == uuid::type::unix_time_based);
    CHECK(u3.get_variant() == uuid::variant::standard);
    CHECK(u3.get_type() == uuid::type::unix_time_based);

    CHECK(uuid() < u1);
    CHECK(u1 < u2);
    CHECK(u2 < u3);
    CHECK(u3 < uuid::max());
    
    std::cout << "v7: " << u1 << '\n';
    std::cout << "v7: " << u2 << '\n';
    std::cout << "v7: " << u3 << '\n';
}

}
