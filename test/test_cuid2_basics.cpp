// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "test_util.h"

#include <modern-uuid/cuid2.h>

#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>

using namespace muuid;
using namespace std::literals;


TEST_SUITE("cuid2_basics") {

static_assert(std::is_class_v<cuid2>);
static_assert(std::is_trivially_copyable_v<cuid2>);
static_assert(std::is_standard_layout_v<cuid2>);
static_assert(std::has_unique_object_representations_v<cuid2>);
static_assert(!std::is_trivially_default_constructible_v<cuid2>);
static_assert(std::is_nothrow_default_constructible_v<cuid2>);
static_assert(std::is_trivially_copy_constructible_v<cuid2>);
static_assert(std::is_nothrow_copy_constructible_v<cuid2>);
static_assert(std::is_trivially_move_constructible_v<cuid2>);
static_assert(std::is_nothrow_move_constructible_v<cuid2>);
static_assert(std::is_trivially_copy_assignable_v<cuid2>);
static_assert(std::is_nothrow_copy_assignable_v<cuid2>);
static_assert(std::is_trivially_move_assignable_v<cuid2>);
static_assert(std::is_nothrow_move_assignable_v<cuid2>);
static_assert(std::is_trivially_destructible_v<cuid2>);
static_assert(std::is_nothrow_destructible_v<cuid2>);
static_assert(std::equality_comparable<cuid2>);
static_assert(std::totally_ordered<cuid2>);
#if !defined(_LIBCPP_VERSION) || (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 140000)
static_assert(std::three_way_comparable<cuid2>);
#endif
static_assert(std::regular<cuid2>);

namespace {
    template<cuid2 U> class some_class {};
    [[maybe_unused]] some_class<cuid2("tz4a98xxat96iws9zmbrgj3a")> some_object;
    [[maybe_unused]] some_class<cuid2(L"tz4a98xxat96iws9zmbrgj3a")> some_objectw;
    [[maybe_unused]] some_class<cuid2(u"tz4a98xxat96iws9zmbrgj3a")> some_object16;
    [[maybe_unused]] some_class<cuid2(U"tz4a98xxat96iws9zmbrgj3a")> some_object32;
    [[maybe_unused]] some_class<cuid2(u8"tz4a98xxat96iws9zmbrgj3a")> some_object8;

    [[maybe_unused]] std::map<cuid2, std::string> m;
    [[maybe_unused]] std::unordered_map<cuid2, std::string> um;
}


TEST_CASE("literals") {
    constexpr cuid2 us("pfh0haxfpzowht3oi213cqos");
    constexpr cuid2 usw(L"pfh0haxfpzowht3oi213cqos");
    constexpr cuid2 us16(u"pfh0haxfpzowht3oi213cqos");
    constexpr cuid2 us32(U"pfh0haxfpzowht3oi213cqos");
    constexpr cuid2 us8(u8"pfh0haxfpzowht3oi213cqos");

    constexpr std::array<uint8_t, 16> expected = 
        {0x0F,0x33,0x9f,0xf4,0xa4,0xfc,0xda,0xd1,0x63,0x48,0x2e,0x6e,0xc1,0xa3,0x39,0x1c};
    
    CHECK_EQUAL_SEQ(us.bytes, expected);
    CHECK_EQUAL_SEQ(usw.bytes, expected);
    CHECK_EQUAL_SEQ(us16.bytes, expected);
    CHECK_EQUAL_SEQ(us32.bytes, expected);
    CHECK_EQUAL_SEQ(us8.bytes, expected);
}

TEST_CASE("nil") {

    constexpr cuid2 u;

    constexpr uint8_t null_bytes[16] = {};

    CHECK_EQUAL_SEQ(u.bytes, null_bytes);
}

TEST_CASE("max") {

    constexpr cuid2 u = cuid2::max();

    constexpr uint8_t max_bytes[16] = {
        0x19,0x78,0x1d,0x7e,0x5f,0x7d,0xc6,0xf7,0x01,0x7e,0x3f,0xff,0xff,0xff,0xff,0xff
    };

    CHECK_EQUAL_SEQ(u.bytes, max_bytes);
}

TEST_CASE("bytes") {
    constexpr std::array<uint8_t, 16> buf1 = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    std::vector<uint8_t> buf2{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}};

    constexpr cuid2 u1 = cuid2::from_bytes(buf1).value();
    constexpr cuid2 u2 = cuid2::from_bytes(std::array<uint8_t, 16>{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}}).value();
    cuid2 u3 = cuid2::from_bytes(std::span<uint8_t, 16>{buf2}).value();
    cuid2 u4 = cuid2::from_bytes(std::span{buf2}.subspan<0, 16>()).value();

    CHECK(u1 == u2);
    CHECK(u1 != cuid2());
    CHECK(cuid2() < u2);
    CHECK(u2 == u3);
    CHECK(u4 == u3);

    CHECK(u2.bytes == buf1);
    CHECK(u3.bytes == buf1);

    CHECK(!cuid2::from_bytes(std::array<uint8_t, 16>{{255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255}}));
    CHECK(!cuid2::from_bytes(std::array<uint8_t, 16>{{0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255}}));
}

TEST_CASE("hash") {

    constexpr std::hash<cuid2> hasher;
    
    CHECK(hasher(cuid2()) != 0);

    constexpr cuid2 val("nc6bzmkmd014706rfda898to");
    CHECK(hasher(val) != 0);
    CHECK(hasher(val) != hasher(cuid2()));
    CHECK(hasher(val) == hasher(val));
}

TEST_CASE("strings") {

    constexpr cuid2 us("NC6BZMKMD014706RFDA898TO");
    cuid2 us1 = cuid2::from_chars("NC6BZMKMD014706RFDA898TO"s).value();
    constexpr cuid2 us2 = cuid2::from_chars("NC6BZMKMD014706RFDA898TO").value();

    CHECK(us == us1);
    CHECK(us2 == us1);

    CHECK_EQUAL_SEQ(us.to_chars(), "nc6bzmkmd014706rfda898to"sv);
    CHECK_EQUAL_SEQ(us.to_chars(cuid2::lowercase), "nc6bzmkmd014706rfda898to"sv);
    CHECK_EQUAL_SEQ(us.to_chars(cuid2::uppercase), "NC6BZMKMD014706RFDA898TO"sv);
    std::array<char, 24> buf;
    us.to_chars(buf);
    CHECK(buf == us.to_chars());
    us.to_chars(buf, cuid2::lowercase);
    CHECK(buf == us.to_chars(cuid2::lowercase));
    us.to_chars(buf, cuid2::uppercase);
    CHECK(buf == us.to_chars(cuid2::uppercase));

    CHECK_EQUAL_SEQ(cuid2{}.to_chars(), "a00000000000000000000000"sv);
    CHECK_EQUAL_SEQ(cuid2{}.to_chars(cuid2::lowercase), "a00000000000000000000000"sv);
    CHECK_EQUAL_SEQ(cuid2{}.to_chars(cuid2::uppercase), "A00000000000000000000000"sv);

    CHECK_EQUAL_SEQ(cuid2::max().to_chars(), "zzzzzzzzzzzzzzzzzzzzzzzz"sv);
    CHECK_EQUAL_SEQ(cuid2::max().to_chars(cuid2::lowercase), "zzzzzzzzzzzzzzzzzzzzzzzz"sv);
    CHECK_EQUAL_SEQ(cuid2::max().to_chars(cuid2::uppercase), "ZZZZZZZZZZZZZZZZZZZZZZZZ"sv);

    CHECK(us2.to_string() == "nc6bzmkmd014706rfda898to");

    constexpr auto cchars = us.to_chars();
    CHECK_EQUAL_SEQ(cchars, "nc6bzmkmd014706rfda898to"sv);
}

TEST_CASE("stringsw") {

    constexpr cuid2 us(L"NC6BZMKMD014706RFDA898TO");
    cuid2 us1 = cuid2::from_chars(L"NC6BZMKMD014706RFDA898TO"s).value();
    constexpr cuid2 us2 = cuid2::from_chars(L"NC6BZMKMD014706RFDA898TO").value();

    CHECK(us == us1);
    CHECK(us2 == us1);

    CHECK_EQUAL_SEQ(us.to_chars<wchar_t>(), L"nc6bzmkmd014706rfda898to"sv);
    CHECK_EQUAL_SEQ(us.to_chars<wchar_t>(cuid2::lowercase), L"nc6bzmkmd014706rfda898to"sv);
    CHECK_EQUAL_SEQ(us.to_chars<wchar_t>(cuid2::uppercase), L"NC6BZMKMD014706RFDA898TO"sv);
    std::array<wchar_t, 24> buf;
    us.to_chars(buf);
    CHECK(buf == us.to_chars<wchar_t>());
    us.to_chars(buf, cuid2::lowercase);
    CHECK(buf == us.to_chars<wchar_t>(cuid2::lowercase));
    us.to_chars(buf, cuid2::uppercase);
    CHECK(buf == us.to_chars<wchar_t>(cuid2::uppercase));

    CHECK_EQUAL_SEQ(cuid2{}.to_chars<wchar_t>(), L"a00000000000000000000000"sv);
    CHECK_EQUAL_SEQ(cuid2{}.to_chars<wchar_t>(cuid2::lowercase), L"a00000000000000000000000"sv);
    CHECK_EQUAL_SEQ(cuid2{}.to_chars<wchar_t>(cuid2::uppercase), L"A00000000000000000000000"sv);

    CHECK_EQUAL_SEQ(cuid2::max().to_chars<wchar_t>(), L"zzzzzzzzzzzzzzzzzzzzzzzz"sv);
    CHECK_EQUAL_SEQ(cuid2::max().to_chars<wchar_t>(cuid2::lowercase), L"zzzzzzzzzzzzzzzzzzzzzzzz"sv);
    CHECK_EQUAL_SEQ(cuid2::max().to_chars<wchar_t>(cuid2::uppercase), L"ZZZZZZZZZZZZZZZZZZZZZZZZ"sv);

    CHECK(us2.to_string<wchar_t>() == L"nc6bzmkmd014706rfda898to");

    constexpr auto cchars = us.to_chars<wchar_t>();
    CHECK_EQUAL_SEQ(cchars, L"nc6bzmkmd014706rfda898to"sv);
}

#if MUUID_SUPPORTS_STD_FORMAT
TEST_CASE("format") {

    CHECK(std::format("{}", cuid2()) == "a00000000000000000000000");
    CHECK(std::format("{}", cuid2("NC6BZMKMD014706RFDA898TO")) == "nc6bzmkmd014706rfda898to");
    CHECK(std::format("{:l}", cuid2("NC6BZMKMD014706RFDA898TO")) == "nc6bzmkmd014706rfda898to");
    CHECK(std::format("{:u}", cuid2("NC6BZMKMD014706RFDA898TO")) == "NC6BZMKMD014706RFDA898TO");

    CHECK(std::format(L"{}", cuid2()) == L"a00000000000000000000000");
    CHECK(std::format(L"{}", cuid2("NC6BZMKMD014706RFDA898TO")) == L"nc6bzmkmd014706rfda898to");
    CHECK(std::format(L"{:l}", cuid2("NC6BZMKMD014706RFDA898TO")) == L"nc6bzmkmd014706rfda898to");
    CHECK(std::format(L"{:u}", cuid2("NC6BZMKMD014706RFDA898TO")) == L"NC6BZMKMD014706RFDA898TO");
}
#endif

TEST_CASE("output") {
    std::ostringstream obuf;

    obuf << cuid2();
    CHECK(obuf.str() == "a00000000000000000000000");
    obuf.str("");

    obuf << cuid2("pfh0haxfpzowht3oi213cqos");
    CHECK(obuf.str() == "pfh0haxfpzowht3oi213cqos");
    obuf.str("");

    obuf << std::uppercase << cuid2("pfh0haxfpzowht3oi213cqos");
    CHECK(obuf.str() == "PFH0HAXFPZOWHT3OI213CQOS");
    obuf.str("");
}

TEST_CASE("outputw") {
    std::wostringstream obuf;

    obuf << cuid2();
    CHECK(obuf.str() == L"a00000000000000000000000");
    obuf.str(L"");

    obuf << cuid2("pfh0haxfpzowht3oi213cqos");
    CHECK(obuf.str() == L"pfh0haxfpzowht3oi213cqos");
    obuf.str(L"");

    obuf << std::uppercase << cuid2("pfh0haxfpzowht3oi213cqos");
    CHECK(obuf.str() == L"PFH0HAXFPZOWHT3OI213CQOS");
    obuf.str(L"");
}

TEST_CASE("input") {
    std::istringstream ibuf;
    cuid2 val;
    
    ibuf.str("a00000000000000000000000");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == cuid2());

    ibuf.clear();
    ibuf.str("pfh0haxfpzowht3oi213cqos");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == cuid2("pfh0haxfpzowht3oi213cqos"));

    ibuf.clear();
    ibuf.str("PFH0HAXFPZOWHT3OI213CQOS");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == cuid2("pfh0haxfpzowht3oi213cqos"));

    ibuf.clear();
    ibuf.str("1fh0haxfpzowht3oi213cqos");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(!ibuf.eof());

    ibuf.clear();
    ibuf.str("pfh0haxfpzowht3oi213cqo_");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(!ibuf.eof());
}

TEST_CASE("inputw") {
    std::wistringstream ibuf;
    cuid2 val;
    
    ibuf.str(L"a00000000000000000000000");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == cuid2());

    ibuf.clear();
    ibuf.str(L"pfh0haxfpzowht3oi213cqos");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == cuid2("pfh0haxfpzowht3oi213cqos"));

    ibuf.clear();
    ibuf.str(L"PFH0HAXFPZOWHT3OI213CQOS");
    ibuf >> val;
    CHECK(ibuf);
    CHECK(val == cuid2("pfh0haxfpzowht3oi213cqos"));

    ibuf.clear();
    ibuf.str(L"1fh0haxfpzowht3oi213cqos");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(!ibuf.eof());

    ibuf.clear();
    ibuf.str(L"pfh0haxfpzowht3oi213cqo_");
    ibuf >> val;
    CHECK(!ibuf);
    CHECK(ibuf.fail());
    CHECK(!ibuf.eof());
}

TEST_CASE("generate") {
    cuid2 u1 = cuid2::generate();
    cuid2 u2 = cuid2::generate();
    
    CHECK(u1 != u2);
    CHECK(u1 != cuid2());
    CHECK(u2 != cuid2());
    CHECK(u1 < cuid2::max());
    CHECK(u2 < cuid2::max());
    
    std::cout << "cuid2: " << u1 << '\n';
    std::cout << "cuid2: " << u2 << '\n';
}

}

