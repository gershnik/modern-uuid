// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_TEST_UTIL_H_INCLUDED
#define HEADER_TEST_UTIL_H_INCLUDED

#include <doctest/doctest.h>

#include <iterator>
#include <array>

template<class T>
auto get_ends(const T & seq) {
    return std::make_pair(std::begin(seq), std::end(seq));
}

namespace std {
    
    template<class T, size_t N>
    doctest::String toString(const std::array<T, N> & arr) {
        using doctest::toString;

        if constexpr (std::is_same_v<std::remove_const_t<T>, char>) {
            return toString(std::string_view(arr.data(), arr.size()));
        } else {
            doctest::String ret = "[";
            for (size_t i = 0; i < N; ++i) {
                if (i > 0)
                    ret += ", ";
                ret += toString(arr[i]);
            }
            ret += "]";
            return ret;
        }
    }
}

#define CHECK_EQUAL_SEQ(seq1, seq2) {\
    const auto & s1 = seq1; \
    const auto & s2 = seq2; \
    auto [c1, e1] = get_ends(s1); \
    auto [c2, e2] = get_ends(s2); \
    \
    for (size_t i = 0; ; ++i, ++c1, ++c2) {\
        if (c1 == e1) { \
            if (c2 == e2) \
                break; \
            \
            FAIL_CHECK("First sequence is shorter (length: ", i, ") than second, first: ", s1, ", second: ", s2); \
            break; \
        } \
        if (c2 == e2) { \
            FAIL_CHECK("Second sequence is shorter (length: ", i, ") than first, first: ", s1, ", second: ", s2); \
            break; \
        } \
        \
        if (*c1 != *c2) { \
            FAIL_CHECK("Discrepancy at index ", i, ", first: ", s1, ", second: ", s2);\
            break;\
        }\
    }\
    CHECK(true); \
}

#define CHECK_UNEQUAL_SEQ(seq1, seq2) { \
    const auto & s1 = seq1; \
    const auto & s2 = seq2; \
    auto [c1, e1] = get_ends(s1); \
    auto [c2, e2] = get_ends(s2); \
    bool equal = true; \
    for ( ; ; ++c1, ++c2) { \
        if (c1 == e1) { \
            equal = (c2 == e2); \
            break; \
        } \
        if (c2 == e2 || *c1 != *c2) { \
            equal = false; \
            break; \
        } \
    } \
    CHECK_MESSAGE(!equal, "Sequences are equal, first: ", s1, ", second: ", s2); \
}

#endif
