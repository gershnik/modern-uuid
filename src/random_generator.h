// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_RANDOM_GENERATOR_H_INCLUDED
#define HEADER_MODERN_UUID_RANDOM_GENERATOR_H_INCLUDED

#include <random>

namespace muuid::impl {

    std::mt19937 & get_random_generator();
}

#endif