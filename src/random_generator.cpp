// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "random_generator.h"
#include "fork_handler.h"

#include "external/randutils.hpp"

namespace muuid::impl {

    prng & get_random_generator() {

        struct generator : prng {
            generator():
                prng(randutils::auto_seed_128{}.base())
            {}
        };

        return reset_on_fork_thread_local<generator>::instance();
    }

}

