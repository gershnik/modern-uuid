// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "test_util.h"

#include <modern-uuid/nanoid.h>

#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>

using namespace muuid;
using namespace std::literals;


TEST_SUITE("nanoid_basics") {

static_assert(std::is_class_v<nanoid>);
static_assert(std::is_trivially_copyable_v<nanoid>);
static_assert(std::is_standard_layout_v<nanoid>);
static_assert(std::has_unique_object_representations_v<nanoid>);
static_assert(!std::is_trivially_default_constructible_v<nanoid>);
static_assert(std::is_nothrow_default_constructible_v<nanoid>);
static_assert(std::is_trivially_copy_constructible_v<nanoid>);
static_assert(std::is_nothrow_copy_constructible_v<nanoid>);
static_assert(std::is_trivially_move_constructible_v<nanoid>);
static_assert(std::is_nothrow_move_constructible_v<nanoid>);
static_assert(std::is_trivially_copy_assignable_v<nanoid>);
static_assert(std::is_nothrow_copy_assignable_v<nanoid>);
static_assert(std::is_trivially_move_assignable_v<nanoid>);
static_assert(std::is_nothrow_move_assignable_v<nanoid>);
static_assert(std::is_trivially_destructible_v<nanoid>);
static_assert(std::is_nothrow_destructible_v<nanoid>);
static_assert(std::equality_comparable<nanoid>);
static_assert(std::totally_ordered<nanoid>);
#if !defined(_LIBCPP_VERSION) || (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 140000)
static_assert(std::three_way_comparable<nanoid>);
#endif
static_assert(std::regular<nanoid>);


namespace {
    template<nanoid U1> class some_class {};
    [[maybe_unused]] some_class<nanoid("V1StGXR8_Z5jdHi6B-myT")> some_object;
    [[maybe_unused]] some_class<nanoid(L"V1StGXR8_Z5jdHi6B-myT")> some_objectw;
    [[maybe_unused]] some_class<nanoid(u"V1StGXR8_Z5jdHi6B-myT")> some_object16;
    [[maybe_unused]] some_class<nanoid(U"V1StGXR8_Z5jdHi6B-myT")> some_object32;
    [[maybe_unused]] some_class<nanoid(u8"V1StGXR8_Z5jdHi6B-myT")> some_object8;

    [[maybe_unused]] std::map<nanoid, std::string> m;
    [[maybe_unused]] std::unordered_map<nanoid, std::string> um;
}

TEST_CASE("nil") {

    constexpr nanoid u;

    constexpr uint8_t null_bytes[16] = {};

    CHECK_EQUAL_SEQ(u.bytes, null_bytes);
}

TEST_CASE("max") {

    constexpr nanoid u = nanoid::max();

    constexpr uint8_t max_bytes[16] = {
        0x3F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    };

    CHECK_EQUAL_SEQ(u.bytes, max_bytes);
}

TEST_CASE("bytes") {
    constexpr std::array<uint8_t, 16> buf1 = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    std::vector<uint8_t> buf2{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}};

    constexpr nanoid u1(buf1);
    constexpr nanoid u2{std::array<uint8_t, 16>{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}}};
    nanoid u3{std::span<uint8_t, 16>{buf2}};
    nanoid u4{std::span{buf2}.subspan<0, 16>()};

    CHECK(u1 == u2);
    CHECK(u1 != nanoid());
    CHECK(nanoid() < u2);
    CHECK(u2 == u3);

    CHECK(u2.bytes == buf1);
    CHECK(u3.bytes == buf1);

    //constexpr nanoid invalid{std::array<uint8_t, 16>{{255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255}}};
    //CHECK(invalid == nanoid::max());
}

TEST_CASE("literals") {
    constexpr nanoid us("Uakgb_J5m9g-0JDMbcJqL");
    constexpr nanoid usw(L"Uakgb_J5m9g-0JDMbcJqL");
    constexpr nanoid us16(u"Uakgb_J5m9g-0JDMbcJqL");
    constexpr nanoid us32(U"Uakgb_J5m9g-0JDMbcJqL");
    constexpr nanoid us8(u8"Uakgb_J5m9g-0JDMbcJqL");

    constexpr std::array<uint8_t, 16> expected = 
        {0x25, 0x0f, 0x5c, 0xb0, 0xb1, 0x85, 0x47, 0x37, 0x22, 0x11, 0x62, 0x38, 0x30, 0xf9, 0x8d, 0xea};
    
    CHECK_EQUAL_SEQ(us.bytes, expected);
    CHECK(usw.bytes == expected);
    CHECK(us16.bytes == expected);
    CHECK(us32.bytes == expected);
    CHECK(us8.bytes == expected);
}

TEST_CASE("hash") {

    constexpr std::hash<nanoid> hasher;
    
    CHECK(hasher(nanoid()) != 0);

    constexpr nanoid val("Uakgb_J5m9g-0JDMbcJqL");
    CHECK(hasher(val) != 0);
    CHECK(hasher(val) != hasher(nanoid()));
    CHECK(hasher(val) == hasher(val));
}

TEST_CASE("strings") {

    constexpr nanoid us("Uakgb_J5m9g-0JDMbcJqL");
    nanoid us1 = nanoid::from_chars("Uakgb_J5m9g-0JDMbcJqL"s).value();
    constexpr nanoid us2 = nanoid::from_chars("Uakgb_J5m9g-0JDMbcJqL").value();

    CHECK(us == us1);
    CHECK(us2 == us1);

    CHECK_EQUAL_SEQ(us.to_chars(), "Uakgb_J5m9g-0JDMbcJqL"sv);
    std::array<char, 21> buf;
    us.to_chars(buf);
    CHECK_EQUAL_SEQ(buf, us.to_chars());
    
    CHECK_EQUAL_SEQ(nanoid{}.to_chars(), "uuuuuuuuuuuuuuuuuuuuu"sv);
    
    CHECK_EQUAL_SEQ(nanoid::max().to_chars(), "ttttttttttttttttttttt"sv);
    
    CHECK(us2.to_string() == "Uakgb_J5m9g-0JDMbcJqL");

    constexpr auto cchars = us.to_chars();
    CHECK_EQUAL_SEQ(cchars, "Uakgb_J5m9g-0JDMbcJqL"sv);
}

TEST_CASE("stringsw") {
    constexpr nanoid us(L"Uakgb_J5m9g-0JDMbcJqL");
    nanoid us1 = nanoid::from_chars(L"Uakgb_J5m9g-0JDMbcJqL"s).value();
    constexpr nanoid us2 = nanoid::from_chars(L"Uakgb_J5m9g-0JDMbcJqL").value();

    CHECK(us == us1);
    CHECK(us2 == us1);

    CHECK_EQUAL_SEQ(us.to_chars(), L"Uakgb_J5m9g-0JDMbcJqL"sv);
    std::array<wchar_t, 21> buf;
    us.to_chars(buf);
    CHECK_EQUAL_SEQ(buf, us.to_chars<wchar_t>());
    
    CHECK_EQUAL_SEQ(nanoid{}.to_chars(), L"uuuuuuuuuuuuuuuuuuuuu"sv);
    
    CHECK_EQUAL_SEQ(nanoid::max().to_chars<wchar_t>(), L"ttttttttttttttttttttt"sv);

    CHECK(us2.to_string<wchar_t>() == L"Uakgb_J5m9g-0JDMbcJqL");
}

#if MUUID_SUPPORTS_STD_FORMAT
TEST_CASE("format") {

    CHECK(std::format("{}", nanoid()) == "uuuuuuuuuuuuuuuuuuuuu");
    CHECK(std::format("{}", nanoid("Uakgb_J5m9g-0JDMbcJqL")) == "Uakgb_J5m9g-0JDMbcJqL");
    
    CHECK(std::format(L"{}", nanoid()) == L"uuuuuuuuuuuuuuuuuuuuu");
    CHECK(std::format(L"{}", nanoid("Uakgb_J5m9g-0JDMbcJqL")) == L"Uakgb_J5m9g-0JDMbcJqL");
}
#endif

TEST_CASE("output") {
    std::ostringstream obuf;

    obuf << nanoid();
    CHECK(obuf.str() == "uuuuuuuuuuuuuuuuuuuuu");
    obuf.str("");

    obuf << nanoid("Uakgb_J5m9g-0JDMbcJqL");
    CHECK(obuf.str() == "Uakgb_J5m9g-0JDMbcJqL");
    obuf.str("");
}

TEST_CASE("outputw") {
    std::wostringstream obuf;

    obuf << nanoid();
    CHECK(obuf.str() == L"uuuuuuuuuuuuuuuuuuuuu");
    obuf.str(L"");

    obuf << nanoid("Uakgb_J5m9g-0JDMbcJqL");
    CHECK(obuf.str() == L"Uakgb_J5m9g-0JDMbcJqL");
    obuf.str(L"");
}

TEST_CASE("input") {
    std::istringstream ibuf;
    nanoid val;
    
    ibuf.str("uuuuuuuuuuuuuuuuuuuuu");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == nanoid());

    ibuf.clear();
    ibuf.str("Uakgb_J5m9g-0JDMbcJqL");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == nanoid("Uakgb_J5m9g-0JDMbcJqL"));

    ibuf.clear();
    ibuf.str("Uakgb_J5m9g-0JDMbcJq");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(ibuf.eof());

    ibuf.clear();
    ibuf.str("Uakgb!J5m9g-0JDMbcJqL");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(!ibuf.eof());
}

TEST_CASE("inputw") {
    std::wistringstream ibuf;
    nanoid val;
    
    ibuf.str(L"uuuuuuuuuuuuuuuuuuuuu");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == nanoid());

    ibuf.clear();
    ibuf.str(L"Uakgb_J5m9g-0JDMbcJqL");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == nanoid("Uakgb_J5m9g-0JDMbcJqL"));

    ibuf.clear();
    ibuf.str(L"Uakgb_J5m9g-0JDMbcJq");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(ibuf.eof());

    ibuf.clear();
    ibuf.str(L"Uakgb!J5m9g-0JDMbcJqL");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(!ibuf.eof());
}

TEST_CASE("generate") {
    nanoid u1 = nanoid::generate();
    nanoid u2 = nanoid::generate();
    
    CHECK(u1 != u2);
    CHECK(u1 != nanoid());
    CHECK(u2 != nanoid());
    CHECK(u1 < nanoid::max());
    CHECK(u2 < nanoid::max());
    
    std::cout << "nanoid: " << u1 << '\n';
    std::cout << "nanoid: " << u2 << '\n';
}


}
