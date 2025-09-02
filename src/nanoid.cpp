// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <modern-uuid/nanoid.h>

#include "random_generator.h"

using namespace muuid;

void muuid::impl::generate_nanoid(std::span<uint8_t> dest, uint8_t max) {

    auto & gen = impl::get_random_generator();
    std::uniform_int_distribution<unsigned> distrib(0, max);

    for (auto & b: dest)
        b = distrib(gen);
}
