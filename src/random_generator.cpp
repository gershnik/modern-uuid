// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "random_generator.h"
#include "fork_handler.h"

#include "external/randutils.hpp"

namespace muuid::impl {

    std::mt19937 & get_random_generator() {

        struct generator : std::mt19937 {
            generator():
                std::mt19937(randutils::auto_seed_128{}.base())
            {}
        };

        return reset_on_fork_thread_local<generator>::instance();
    }

}

