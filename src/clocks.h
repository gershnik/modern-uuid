// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_CLOCKS_H_INCLUDED
#define HEADER_MODERN_UUID_CLOCKS_H_INCLUDED

#include <cstdint>

namespace muuid::impl {

    struct clock_result_v1 {
        uint64_t value;
        uint16_t sequence;
    };

    struct clock_result_v7 {
        uint64_t value;
        uint16_t extra;
        uint16_t sequence;
    };

    clock_result_v1 get_clock_v1();
    clock_result_v7 get_clock_v7();
}

#endif
