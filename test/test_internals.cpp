// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "test_util.h"

#include <modern-uuid/uuid.h>

#include <array>

using namespace muuid;

static constexpr auto make_buf() {
    std::array<uint8_t, sizeof(size_t)> buf;
    for(size_t i = 0; i < buf.size(); ++i)
        buf[i] = uint8_t(i);
    return buf;
}

static constexpr size_t do_reinterpret_bytes() {

    constexpr auto buf = make_buf();
    size_t ret;
    impl::reinterpret_bytes(&buf[0], ret);
    return ret;
}

template<size_t Offset, size_t Size>
static constexpr size_t do_reinterpret_bytes_partial() {

    constexpr auto buf = make_buf();
    size_t ret;
    impl::reinterpret_bytes_partial<Offset, Size>(&buf[0], ret);
    return ret;
}

static size_t do_memcpy_partial(size_t offset, size_t size) {

    constexpr auto buf = make_buf();
    size_t ret = 0;
    memcpy((uint8_t*)&ret + offset, buf.data(), size);
    return ret;
}


TEST_SUITE("internals") {

TEST_CASE( "reinterpret_bytes" ) {
    constexpr auto buf = make_buf();
    size_t comp;
    memcpy(&comp, buf.data(), buf.size());

    CHECK(do_reinterpret_bytes() == comp);

    CHECK(do_reinterpret_bytes_partial<0, 1>() == do_memcpy_partial(0, 1));
    CHECK(do_reinterpret_bytes_partial<0, 2>() == do_memcpy_partial(0, 2));
    CHECK(do_reinterpret_bytes_partial<2, 3>() == do_memcpy_partial(2, 3));
    CHECK(do_reinterpret_bytes_partial<1, 3>() == do_memcpy_partial(1, 3));
    
}

}