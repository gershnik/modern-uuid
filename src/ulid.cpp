// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <modern-uuid/ulid.h>

#include "clocks.h"


using namespace muuid;

auto ulid::generate() -> ulid {
    auto [clock, tail_low, tail_high] = impl::get_clock_ulid();
    uint32_t time_high = uint32_t(clock >> 16);
    uint16_t time_low = uint16_t(clock);
    std::array<uint8_t, 16> buf;
    auto data = buf.data();
    data = impl::write_bytes(time_high, data);
    data = impl::write_bytes(time_low, data);
    data = impl::write_bytes(tail_high, data);
    data = impl::write_bytes(tail_low, data);

    ulid * ret = reinterpret_cast<ulid *>(&buf);
    return *ret;
}
