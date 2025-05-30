// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "test_util.h"

#include <modern-uuid/uuid.h>

using namespace muuid;

TEST_SUITE("system") {


TEST_CASE("apple") {

#ifdef __APPLE__

    CFUUIDRef uuidobj = CFUUIDCreate(nullptr);
    auto bytes = CFUUIDGetUUIDBytes(uuidobj);
    CFStringRef strobj = CFUUIDCreateString(nullptr, uuidobj);

    std::array<char, 37> str;

    REQUIRE(CFStringGetCString(strobj, str.data(), CFIndex(str.size()), kCFStringEncodingUTF8));

    const uuid u(bytes);

    auto bytes1 = u.to_CFUUIDBytes();
    CHECK(memcmp(&bytes, &bytes1, sizeof(CFUUIDBytes)) == 0);

    std::array<char, 37> str1;
    u.to_chars(str1, uuid::uppercase);
    str1[36] = 0;
    CHECK(str == str1);

    const uuid u1(uuidobj);
    u1.to_chars(str1, uuid::uppercase);
    CHECK(str == str1);

    CFUUIDRef uuidobj1 = u1.to_CFUUID();
    CHECK(CFEqual(uuidobj, uuidobj1));

    CFRelease(uuidobj1);
    CFRelease(strobj);
    CFRelease(uuidobj);
#endif
}

TEST_CASE("windows") {

#ifdef _WIN32

    GUID guid = { /* 0d1be41b-f035-4f89-b404-bc0641afae59 */
        0x0d1be41b,
        0xf035,
        0x4f89,
        {0xb4, 0x04, 0xbc, 0x06, 0x41, 0xaf, 0xae, 0x59}
    };

    const uuid u = guid;

    CHECK(u.to_string() == "0d1be41b-f035-4f89-b404-bc0641afae59");

    GUID guid1 = u.to_GUID();
    CHECK(IsEqualGUID(guid, guid1));

#if defined(_MSC_VER) || defined(__clang__)

    class __declspec(uuid("06f9ea87-da78-47aa-8a21-682260ed8b65")) foo;

    constexpr auto uuid_of_foo = uuidof<foo>;

    CHECK(uuid_of_foo.to_string() == "06f9ea87-da78-47aa-8a21-682260ed8b65");

#endif
#endif

}

}

namespace {
    class foo;
}
MUUID_ASSIGN_UUID(foo, "6cd966c0-1a04-486d-b642-7fda4cdcf1a4");

TEST_CASE("portable_uuidof") {
    constexpr auto uuid_of_foo = uuidof<foo>;
    CHECK(uuid_of_foo.to_string() == "6cd966c0-1a04-486d-b642-7fda4cdcf1a4");

}
