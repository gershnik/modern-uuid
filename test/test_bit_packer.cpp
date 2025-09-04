// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "test_util.h"

#include <modern-uuid/bit_packer.h>

using namespace muuid::impl;
using namespace std::literals;

#define ARR(...) std::array<uint8_t, std::size({__VA_ARGS__})>{{__VA_ARGS__}}
#define SPN(...) std::span<const uint8_t, std::size({__VA_ARGS__})>(std::array<const uint8_t, std::size({__VA_ARGS__})>{{__VA_ARGS__}})


TEST_SUITE("bit_packer") {

TEST_CASE("2 bits") {
    {
        auto dest = ARR(1);
        bit_packer<2, 1>::pack_bits(SPN(3, 3, 3, 3), dest);
        CHECK_EQUAL_SEQ(dest, ARR(0xFF));

        auto src = ARR(1, 1, 1, 1);
        bit_packer<2, 1>::unpack_bits(std::span(dest), src);
        CHECK_EQUAL_SEQ(src, ARR(3, 3, 3, 3));
    }
    {
        auto dest = ARR(1, 1);
        bit_packer<2, 2>::pack_bits(SPN(3, 3, 3, 3, 3, 3, 3, 3), dest);
        CHECK_EQUAL_SEQ(dest, ARR(0xFF, 0xFF));

        auto src = ARR(1, 1, 1, 1, 1, 1, 1, 1);
        bit_packer<2, 2>::unpack_bits(std::span(dest), src);
        CHECK_EQUAL_SEQ(src, ARR(3, 3, 3, 3, 3, 3, 3, 3));
    }
}

TEST_CASE("3 bits") {
    {
        auto dest = ARR(1);
        bit_packer<3, 1>::pack_bits(SPN(3, 7, 7), dest);
        CHECK_EQUAL_SEQ(dest, ARR(0xFF));

        auto src = ARR(1, 1, 1);
        bit_packer<3, 1>::unpack_bits(std::span(dest), src);
        CHECK_EQUAL_SEQ(src, ARR(3, 7, 7));
    }
    {
        auto dest = ARR(1, 1);
        bit_packer<3, 2>::pack_bits(SPN(1, 7, 7, 7, 7, 7), dest);
        CHECK_EQUAL_SEQ(dest, ARR(0xFF, 0xFF));

        auto src = ARR(1, 1, 1, 1, 1, 1);
        bit_packer<3, 2>::unpack_bits(std::span(dest), src);
        CHECK_EQUAL_SEQ(src, ARR(1, 7, 7, 7, 7, 7));
    }
}

TEST_CASE("4 bits") {
    {
        auto dest = ARR(1);
        bit_packer<4, 1>::pack_bits(SPN(0xF, 0xF), dest);
        CHECK_EQUAL_SEQ(dest, ARR(0xFF));

        auto src = ARR(1, 1);
        bit_packer<4, 1>::unpack_bits(std::span(dest), src);
        CHECK_EQUAL_SEQ(src, ARR(0xF, 0xF));
    }
    {
        auto dest = ARR(1, 1);
        bit_packer<4, 2>::pack_bits(SPN(0xF, 0xF, 0xF, 0xF), dest);
        CHECK_EQUAL_SEQ(dest, ARR(0xFF, 0xFF));

        auto src = ARR(1, 1, 1, 1);
        bit_packer<4, 2>::unpack_bits(std::span(dest), src);
        CHECK_EQUAL_SEQ(src, ARR(0xF, 0xF, 0xF, 0xF));
    }
}

TEST_CASE("5 bits") {
    {
        auto dest = ARR(1);
        bit_packer<5, 1>::pack_bits(SPN(0x7, 0x1F), dest);
        CHECK_EQUAL_SEQ(dest, ARR(0xFF));

        auto src = ARR(1, 1);
        bit_packer<5, 1>::unpack_bits(std::span(dest), src);
        CHECK_EQUAL_SEQ(src, ARR(0x7, 0x1F));
    }
    {
        auto dest = ARR(1, 1);
        bit_packer<5, 2>::pack_bits(SPN(0x1, 0x1F, 0x1F, 0x1F), dest);
        CHECK_EQUAL_SEQ(dest, ARR(0xFF, 0xFF));

        auto src = ARR(1, 1, 1, 1);
        bit_packer<5, 2>::unpack_bits(std::span(dest), src);
        CHECK_EQUAL_SEQ(src, ARR(0x1, 0x1F, 0x1F, 0x1F));
    }
}

TEST_CASE("6 bits") {
    {
        auto dest = ARR(1);
        bit_packer<6, 1>::pack_bits(SPN(0x3, 0x3F), dest);
        CHECK_EQUAL_SEQ(dest, ARR(0xFF));

        auto src = ARR(1, 1);
        bit_packer<6, 1>::unpack_bits(std::span(dest), src);
        CHECK_EQUAL_SEQ(src, ARR(0x3, 0x3F));
    }
    {
        auto dest = ARR(1, 1);
        bit_packer<6, 2>::pack_bits(SPN(0xF, 0x3F, 0x3F), dest);
        CHECK_EQUAL_SEQ(dest, ARR(0xFF, 0xFF));

        auto src = ARR(1, 1, 1);
        bit_packer<6, 2>::unpack_bits(std::span(dest), src);
        CHECK_EQUAL_SEQ(src, ARR(0xF, 0x3F, 0x3F));
    }
}

TEST_CASE("7 bits") {
    {
        auto dest = ARR(1);
        bit_packer<7, 1>::pack_bits(SPN(0x1, 0x7F), dest);
        CHECK_EQUAL_SEQ(dest, ARR(0xFF));

        auto src = ARR(1, 1);
        bit_packer<7, 1>::unpack_bits(std::span(dest), src);
        CHECK_EQUAL_SEQ(src, ARR(0x1, 0x7F));
    }
    {
        auto dest = ARR(1, 1);
        bit_packer<7, 2>::pack_bits(SPN(0x3, 0x7F, 0x7F), dest);
        CHECK_EQUAL_SEQ(dest, ARR(0xFF, 0xFF));

        auto src = ARR(1, 1, 1);
        bit_packer<7, 2>::unpack_bits(std::span(dest), src);
        CHECK_EQUAL_SEQ(src, ARR(0x3, 0x7F, 0x7F));
    }
}


}

