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
#else
    MESSAGE("skipping - not an Apple platform");
#endif
}

}
