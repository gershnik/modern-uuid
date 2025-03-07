// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_RANDOM_GENERATOR_H_INCLUDED
#define HEADER_MODERN_UUID_RANDOM_GENERATOR_H_INCLUDED

#include <random>
#include "external/chacha20.hpp"


namespace muuid::impl {

    using prng = chacha20_12;

    prng & get_random_generator();
}

#endif
