// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include <modern-uuid/cuid2.h>

#include "random_generator.h"
#include "fork_handler.h"

extern "C" {
    #include "external/sha3.h"
}

using namespace muuid;
using namespace std::chrono;

namespace {

    class cuid2_state {
    public:
        cuid2_state() {
            auto & gen = impl::get_random_generator();
            std::uniform_int_distribution<uint32_t> dist;
            m_counter = dist(gen);
            for (auto & val: m_fingerprint)
                val = dist(gen);
        }

        uint32_t count() {
            return m_counter++;
        }

        std::span<const uint8_t, 16> fingerprint() const {
            return std::span<const uint8_t, 16>(reinterpret_cast<const uint8_t *>(m_fingerprint), 16);
        }

    private:
        uint32_t m_counter;
        uint32_t m_fingerprint[4];
    };

}

static cuid2_state & get_state() {
    return impl::reset_on_fork_thread_local<cuid2_state>::instance();
}

auto cuid2::generate() -> cuid2 {
    auto & gen = impl::get_random_generator();

    std::uniform_int_distribution<unsigned> first_letter_dist(0, 25);
    auto first = uint8_t(first_letter_dist(gen));

    auto time = system_clock::now().time_since_epoch().count();

    auto & state = get_state();
    uint32_t count = state.count();
    auto fingerprint = state.fingerprint();
    
    std::uniform_int_distribution<uint32_t> salt_dist;
    uint32_t salt[] = {salt_dist(gen), salt_dist(gen), salt_dist(gen), salt_dist(gen)};
    
    muuid_sha3_ctx ctx;
    muuid_sha3_512_init(&ctx);
    muuid_sha3_update(&ctx, (const unsigned char *)&time, sizeof(time));
    muuid_sha3_update(&ctx, (const unsigned char *)salt, sizeof(salt));
    muuid_sha3_update(&ctx, (const unsigned char *)&count, sizeof(count));
    muuid_sha3_update(&ctx, (const unsigned char *)fingerprint.data(), fingerprint.size());

    std::array<uint8_t, 64> hash;
    muuid_sha3_final(&ctx, hash.data());

    impl::cuid2_repr repr_out;
    static_assert(sizeof(repr_out) < 63);
    memcpy(&repr_out, &hash[1], sizeof(repr_out));

    impl::cuid2_repr repr_in;
    for(size_t i = 0; i < char_length - 1; ++i) {
        auto val = repr_out.pop_base36_digit();
        repr_in.push_base36_digit(val);
    }
    
    uint8_t ret[16];
    ret[0] = first;
    repr_in.get_bytes(std::span<uint8_t, 15>(&ret[1], 15));
    return *reinterpret_cast<cuid2 *>(&ret);
}
