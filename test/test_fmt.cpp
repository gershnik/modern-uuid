// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <doctest/doctest.h>

#include <fmt/format.h>
#include <modern-uuid/uuid.h>
#include <modern-uuid/ulid.h>
#include <modern-uuid/nanoid.h>
#include <modern-uuid/cuid2.h>

using namespace muuid;
using namespace std::literals;

static_assert(MUUID_SUPPORTS_FMT_FORMAT);

TEST_SUITE("fmt") {

TEST_CASE("format") {

    CHECK(fmt::format("{}", uuid()) == "00000000-0000-0000-0000-000000000000");
    CHECK(fmt::format("{}", uuid("7d444840-9dc0-11d1-b245-5ffdce74fad2")) == "7d444840-9dc0-11d1-b245-5ffdce74fad2");
    CHECK(fmt::format("{:l}", uuid("7d444840-9dc0-11d1-b245-5ffdce74fad2")) == "7d444840-9dc0-11d1-b245-5ffdce74fad2");
    CHECK(fmt::format("{:u}", uuid("7d444840-9dc0-11d1-b245-5ffdce74fad2")) == "7D444840-9DC0-11D1-B245-5FFDCE74FAD2");
}

TEST_CASE("format ulid") {

    CHECK(fmt::format("{}", ulid()) == "00000000000000000000000000");
    CHECK(fmt::format("{}", ulid("01BX5ZZKBKACTAV9WEVGEMMVRY")) == "01bx5zzkbkactav9wevgemmvry");
    CHECK(fmt::format("{:l}", ulid("01BX5ZZKBKACTAV9WEVGEMMVRY")) == "01bx5zzkbkactav9wevgemmvry");
    CHECK(fmt::format("{:u}", ulid("01BX5ZZKBKACTAV9WEVGEMMVRY")) == "01BX5ZZKBKACTAV9WEVGEMMVRY");
}

TEST_CASE("format nanoid") {

    CHECK(fmt::format("{}", nanoid()) == "uuuuuuuuuuuuuuuuuuuuu");
    CHECK(fmt::format("{}", nanoid("Uakgb_J5m9g-0JDMbcJqL")) == "Uakgb_J5m9g-0JDMbcJqL");
}

TEST_CASE("format cuid2") {

    CHECK(fmt::format("{}", cuid2()) == "a00000000000000000000000");
    CHECK(fmt::format("{}", cuid2("NC6BZMKMD014706RFDA898TO")) == "nc6bzmkmd014706rfda898to");
    CHECK(fmt::format("{:l}", cuid2("NC6BZMKMD014706RFDA898TO")) == "nc6bzmkmd014706rfda898to");
    CHECK(fmt::format("{:u}", cuid2("NC6BZMKMD014706RFDA898TO")) == "NC6BZMKMD014706RFDA898TO");
}

}
