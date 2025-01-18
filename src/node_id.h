// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_NODE_ID_H_INCLUDED
#define HEADER_MODERN_UUID_NODE_ID_H_INCLUDED

#include <cstdint>
#include <span>

namespace muuid::impl {

    std::span<const uint8_t, 6> get_node_id();
}

#endif
