// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <doctest/doctest.h>

#include <modern-uuid/ulid.h>

#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>

using namespace muuid;
using namespace std::literals;


TEST_SUITE("ulid_basics") {

static_assert(std::is_class_v<ulid>);
static_assert(std::is_trivially_copyable_v<ulid>);
static_assert(std::is_standard_layout_v<ulid>);
static_assert(std::has_unique_object_representations_v<ulid>);
static_assert(!std::is_trivially_default_constructible_v<ulid>);
static_assert(std::is_nothrow_default_constructible_v<ulid>);
static_assert(std::is_trivially_copy_constructible_v<ulid>);
static_assert(std::is_nothrow_copy_constructible_v<ulid>);
static_assert(std::is_trivially_move_constructible_v<ulid>);
static_assert(std::is_nothrow_move_constructible_v<ulid>);
static_assert(std::is_trivially_copy_assignable_v<ulid>);
static_assert(std::is_nothrow_copy_assignable_v<ulid>);
static_assert(std::is_trivially_move_assignable_v<ulid>);
static_assert(std::is_nothrow_move_assignable_v<ulid>);
static_assert(std::is_trivially_destructible_v<ulid>);
static_assert(std::is_nothrow_destructible_v<ulid>);
static_assert(std::equality_comparable<ulid>);
static_assert(std::totally_ordered<ulid>);
#if !defined(_LIBCPP_VERSION) || (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 140000)
static_assert(std::three_way_comparable<ulid>);
#endif
static_assert(std::regular<ulid>);


namespace {
    template<ulid U1> class some_class {};
    [[maybe_unused]] some_class<ulid("01BX5ZZKBKACTAV9WEVGEMMVRY")> some_object;
    [[maybe_unused]] some_class<ulid(L"01BX5ZZKBKACTAV9WEVGEMMVRY")> some_objectw;
    [[maybe_unused]] some_class<ulid(u"01BX5ZZKBKACTAV9WEVGEMMVRY")> some_object16;
    [[maybe_unused]] some_class<ulid(U"01BX5ZZKBKACTAV9WEVGEMMVRY")> some_object32;
    [[maybe_unused]] some_class<ulid(u8"01BX5ZZKBKACTAV9WEVGEMMVRY")> some_object8;

    [[maybe_unused]] std::map<ulid, std::string> m;
    [[maybe_unused]] std::unordered_map<ulid, std::string> um;
}

TEST_CASE("nil") {

    constexpr ulid u;

    constexpr uint8_t null_bytes[16] = {};

    CHECK(memcmp(u.bytes.data(), null_bytes, u.bytes.size()) == 0);
}

TEST_CASE("max") {

    constexpr ulid u = ulid::max();

    constexpr uint8_t max_bytes[16] = {
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    };

    CHECK(memcmp(u.bytes.data(), max_bytes, u.bytes.size()) == 0);
}

TEST_CASE("bytes") {
    constexpr std::array<uint8_t, 16> buf1 = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    std::vector<uint8_t> buf2{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}};

    constexpr ulid u1(buf1);
    constexpr ulid u2{std::array<uint8_t, 16>{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}}};
    ulid u3{std::span<uint8_t, 16>{buf2}};
    ulid u4{std::span{buf2}.subspan<0, 16>()};

    CHECK(u1 == u2);
    CHECK(u1 != ulid());
    CHECK(ulid() < u2);
    CHECK(u2 == u3);

    CHECK(u2.bytes == buf1);
    CHECK(u3.bytes == buf1);
}

TEST_CASE("literals") {
    constexpr ulid us("01ARYZ6S41YYYYYYYYYYYYYYYY");
    constexpr ulid usw(L"01ARYZ6S41YYYYYYYYYYYYYYYY");
    constexpr ulid us16(u"01ARYZ6S41YYYYYYYYYYYYYYYY");
    constexpr ulid us32(U"01ARYZ6S41YYYYYYYYYYYYYYYY");
    constexpr ulid us8(u8"01ARYZ6S41YYYYYYYYYYYYYYYY");

    constexpr std::array<uint8_t, 16> expected = 
        {0x01,0x56,0x3d,0xf3,0x64,0x81, 0xF7,0xBD,0xEF,0x7B,0xDE,0xF7,0xBD,0xEF,0x7B,0xDE};
    
    CHECK(us.bytes == expected);
    CHECK(usw.bytes == expected);
    CHECK(us16.bytes == expected);
    CHECK(us32.bytes == expected);
    CHECK(us8.bytes == expected);
}

TEST_CASE("hash") {

    constexpr std::hash<ulid> hasher;
    
    CHECK(hasher(ulid()) != 0);

    constexpr ulid val("01BX5ZZKBKACTAV9WEVGEMMVRY");
    CHECK(hasher(val) != 0);
    CHECK(hasher(val) != hasher(ulid()));
    CHECK(hasher(val) == hasher(val));
}

TEST_CASE("strings") {

    constexpr ulid us("01BX5ZZKBKACTAV9WEVGEMMVRY");
    ulid us1 = ulid::from_chars("01BX5ZZKBKACTAV9WEVGEMMVRY"s).value();
    constexpr ulid us2 = ulid::from_chars("01BX5ZZKBKACTAV9WEVGEMMVRY").value();

    CHECK(us == us1);
    CHECK(us2 == us1);

    auto match = [](const std::array<char, 26> & lhs, std::string_view rhs) {
        return std::string_view(lhs.data(), lhs.size()) == rhs;
    };

    CHECK(match(us.to_chars(), "01bx5zzkbkactav9wevgemmvry"));
    CHECK(match(us.to_chars(ulid::lowercase), "01bx5zzkbkactav9wevgemmvry"));
    CHECK(match(us.to_chars(ulid::uppercase), "01BX5ZZKBKACTAV9WEVGEMMVRY"));
    std::array<char, 26> buf;
    us.to_chars(buf);
    CHECK(buf == us.to_chars());
    us.to_chars(buf, ulid::lowercase);
    CHECK(buf == us.to_chars(ulid::lowercase));
    us.to_chars(buf, ulid::uppercase);
    CHECK(buf == us.to_chars(ulid::uppercase));

    CHECK(match(ulid{}.to_chars(), "00000000000000000000000000"));
    CHECK(match(ulid{}.to_chars(ulid::lowercase), "00000000000000000000000000"));
    CHECK(match(ulid{}.to_chars(ulid::uppercase), "00000000000000000000000000"));

    CHECK(match(ulid::max().to_chars(), "7zzzzzzzzzzzzzzzzzzzzzzzzz"));
    CHECK(match(ulid::max().to_chars(ulid::lowercase), "7zzzzzzzzzzzzzzzzzzzzzzzzz"));
    CHECK(match(ulid::max().to_chars(ulid::uppercase), "7ZZZZZZZZZZZZZZZZZZZZZZZZZ"));



    CHECK(us2.to_string() == "01bx5zzkbkactav9wevgemmvry");
}

TEST_CASE("stringsw") {

    constexpr ulid us(L"01BX5ZZKBKACTAV9WEVGEMMVRY");
    ulid us1 = ulid::from_chars(L"01BX5ZZKBKACTAV9WEVGEMMVRY"s).value();
    constexpr ulid us2 = ulid::from_chars(L"01BX5ZZKBKACTAV9WEVGEMMVRY").value();

    CHECK(us == us1);
    CHECK(us2 == us1);

    auto match = [](const std::array<wchar_t, 26> & lhs, std::wstring_view rhs) {
        return std::wstring_view(lhs.data(), lhs.size()) == rhs;
    };

    CHECK(match(us.to_chars<wchar_t>(), L"01bx5zzkbkactav9wevgemmvry"));
    CHECK(match(us.to_chars<wchar_t>(ulid::lowercase), L"01bx5zzkbkactav9wevgemmvry"));
    CHECK(match(us.to_chars<wchar_t>(ulid::uppercase), L"01BX5ZZKBKACTAV9WEVGEMMVRY"));
    std::array<wchar_t, 26> buf;
    us.to_chars(buf);
    CHECK(buf == us.to_chars<wchar_t>());
    us.to_chars(buf, ulid::lowercase);
    CHECK(buf == us.to_chars<wchar_t>(ulid::lowercase));
    us.to_chars(buf, ulid::uppercase);
    CHECK(buf == us.to_chars<wchar_t>(ulid::uppercase));

    CHECK(match(ulid{}.to_chars<wchar_t>(), L"00000000000000000000000000"));
    CHECK(match(ulid{}.to_chars<wchar_t>(ulid::lowercase), L"00000000000000000000000000"));
    CHECK(match(ulid{}.to_chars<wchar_t>(ulid::uppercase), L"00000000000000000000000000"));

    CHECK(match(ulid::max().to_chars<wchar_t>(), L"7zzzzzzzzzzzzzzzzzzzzzzzzz"));
    CHECK(match(ulid::max().to_chars<wchar_t>(ulid::lowercase), L"7zzzzzzzzzzzzzzzzzzzzzzzzz"));
    CHECK(match(ulid::max().to_chars<wchar_t>(ulid::uppercase), L"7ZZZZZZZZZZZZZZZZZZZZZZZZZ"));



    CHECK(us2.to_string<wchar_t>() == L"01bx5zzkbkactav9wevgemmvry");
}

#if MUUID_SUPPORTS_STD_FORMAT
TEST_CASE("format") {

    CHECK(std::format("{}", ulid()) == "00000000000000000000000000");
    CHECK(std::format("{}", ulid("01BX5ZZKBKACTAV9WEVGEMMVRY")) == "01bx5zzkbkactav9wevgemmvry");
    CHECK(std::format("{:l}", ulid("01BX5ZZKBKACTAV9WEVGEMMVRY")) == "01bx5zzkbkactav9wevgemmvry");
    CHECK(std::format("{:u}", ulid("01BX5ZZKBKACTAV9WEVGEMMVRY")) == "01BX5ZZKBKACTAV9WEVGEMMVRY");

    CHECK(std::format(L"{}", ulid()) == L"00000000000000000000000000");
    CHECK(std::format(L"{}", ulid("01BX5ZZKBKACTAV9WEVGEMMVRY")) == L"01bx5zzkbkactav9wevgemmvry");
    CHECK(std::format(L"{:l}", ulid("01BX5ZZKBKACTAV9WEVGEMMVRY")) == L"01bx5zzkbkactav9wevgemmvry");
    CHECK(std::format(L"{:u}", ulid("01BX5ZZKBKACTAV9WEVGEMMVRY")) == L"01BX5ZZKBKACTAV9WEVGEMMVRY");
}
#endif

TEST_CASE("output") {
    std::ostringstream obuf;

    obuf << ulid();
    CHECK(obuf.str() == "00000000000000000000000000");
    obuf.str("");

    obuf << ulid("01bx5zzkbkactav9wevgemmvry");
    CHECK(obuf.str() == "01bx5zzkbkactav9wevgemmvry");
    obuf.str("");

    obuf << std::uppercase << ulid("01bx5zzkbkactav9wevgemmvry");
    CHECK(obuf.str() == "01BX5ZZKBKACTAV9WEVGEMMVRY");
    obuf.str("");
}

TEST_CASE("outputw") {
    std::wostringstream obuf;

    obuf << ulid();
    CHECK(obuf.str() == L"00000000000000000000000000");
    obuf.str(L"");

    obuf << ulid("01bx5zzkbkactav9wevgemmvry");
    CHECK(obuf.str() == L"01bx5zzkbkactav9wevgemmvry");
    obuf.str(L"");

    obuf << std::uppercase << ulid("01bx5zzkbkactav9wevgemmvry");
    CHECK(obuf.str() == L"01BX5ZZKBKACTAV9WEVGEMMVRY");
    obuf.str(L"");
}

TEST_CASE("input") {
    std::istringstream ibuf;
    ulid val;
    
    ibuf.str("00000000000000000000000000");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == ulid());

    ibuf.clear();
    ibuf.str("01bx5zzkbkactav9wevgemmvry");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == ulid("01bx5zzkbkactav9wevgemmvry"));

    ibuf.clear();
    ibuf.str("01BX5ZZKBKACTAV9WEVGEMMVRY");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == ulid("01bx5zzkbkactav9wevgemmvry"));

    ibuf.clear();
    ibuf.str("01BX5ZZKBKACTAV9WEVGEMMVR");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(ibuf.eof());

    ibuf.clear();
    ibuf.str("01B X5ZZKBKACTAV9WEVGEMMVRY");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(!ibuf.eof());
}

TEST_CASE("inputw") {
    std::wistringstream ibuf;
    ulid val;
    
    ibuf.str(L"00000000000000000000000000");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == ulid());

    ibuf.clear();
    ibuf.str(L"01bx5zzkbkactav9wevgemmvry");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == ulid("01bx5zzkbkactav9wevgemmvry"));

    ibuf.clear();
    ibuf.str(L"01BX5ZZKBKACTAV9WEVGEMMVRY");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == ulid("01bx5zzkbkactav9wevgemmvry"));

    ibuf.clear();
    ibuf.str(L"01BX5ZZKBKACTAV9WEVGEMMVR");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(ibuf.eof());

    ibuf.clear();
    ibuf.str(L"01B X5ZZKBKACTAV9WEVGEMMVRY");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(!ibuf.eof());
}

TEST_CASE("generate") {
    ulid u1 = ulid::generate();
    ulid u2 = ulid::generate();
    ulid u3 = ulid::generate();
    
    CHECK(ulid() < u1);
    CHECK(u1 < u2);
    CHECK(u2 < u3);
    CHECK(u3 < ulid::max());
    
    std::cout << "ulid: " << u1 << '\n';
    std::cout << "ulid: " << u2 << '\n';
    std::cout << "ulid: " << u3 << '\n';
}

}