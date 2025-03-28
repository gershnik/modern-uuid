// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_TEST_UTIL_H_INCLUDED
#define HEADER_TEST_UTIL_H_INCLUDED

#include <doctest/doctest.h>

#include <iterator>

template<class T>
auto get_ends(const T & seq) {
    return std::make_pair(std::begin(seq), std::end(seq));
}

#define CHECK_EQUAL_SEQ(seq1, seq2) {\
    auto [c1, e1] = get_ends(seq1); \
    auto [c2, e2] = get_ends(seq2); \
    \
    for (size_t i = 0; ; ++i, ++c1, ++c2) {\
        if (c1 == e1) { \
            if (c2 == e2) \
                break; \
            \
            FAIL_CHECK("First sequence is shorter (length: ", i, ") than second, first: ", seq1, ", second: ", seq2); \
            break; \
        } \
        if (c2 == e2) { \
            FAIL_CHECK("Second sequence is shorter (length: ", i, ") than first, first: ", seq1, ", second: ", seq2); \
            break; \
        } \
        \
        if (*c1 != *c2) { \
            FAIL_CHECK("Discrepancy at index ", i, ", first: ", seq1, ", second: ", seq2);\
            break;\
        }\
    }\
}

#define CHECK_UNEQUAL_SEQ(seq1, seq2) { \
    auto [c1, e1] = get_ends(seq1); \
    auto [c2, e2] = get_ends(seq2); \
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
    if (equal) \
        FAIL_CHECK("Sequences are equal, first: ", seq1, ", second: ", seq2); \
}

#endif
