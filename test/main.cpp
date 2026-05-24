// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

static bool wide_ctype_works() {
    auto& f = std::use_facet<std::ctype<wchar_t>>(std::locale());
    return f.is(std::ctype_base::space, L' ');
}

bool g_wide_ctype_works = false;

int main(int argc, char ** argv)
{
    #if defined(_WIN32)
        SetConsoleOutputCP(CP_UTF8);
    #endif

    g_wide_ctype_works = wide_ctype_works();

    return doctest::Context(argc, argv).run();
}
