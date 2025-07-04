// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <doctest/doctest.h>

#include <fmt/format.h>
#include <modern-uuid/uuid.h>

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

}
