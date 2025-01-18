// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_CLOCKS_H_INCLUDED
#define HEADER_MODERN_UUID_CLOCKS_H_INCLUDED

#include <cstdint>
#include <utility>

namespace muuid::impl {

    std::pair<uint64_t, uint16_t> get_clock_v1();
    std::pair<uint64_t, uint16_t> get_clock_v7();
}

#endif
