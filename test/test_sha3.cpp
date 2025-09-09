// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "test_util.h"

#include "../src/external/sha3.h"

#include <charconv>
#include <vector>

using namespace std::literals;

TEST_SUITE("sha3") {

TEST_CASE("simple") {

    constexpr char input[] = 
            "3A3A819C48EFDE2AD914FBF00E18AB6BC4F14513AB27D0C178A188B61431E7F5"
            "623CB66B23346775D386B50E982C493ADBBFC54B9A3CD383382336A1A0B2150A"
            "15358F336D03AE18F666C7573D55C4FD181C29E6CCFDE63EA35F0ADF5885CFC0"
            "A3D84A2B2E4DD24496DB789E663170CEF74798AA1BBCD4574EA0BBA40489D764"
            "B2F83AADC66B148B4A0CD95246C127D5871C4F11418690A5DDF01246A0C80A43"
            "C70088B6183639DCFDA4125BD113A8F49EE23ED306FAAC576C3FB0C1E256671D"
            "817FC2534A52F5B439F72E424DE376F4C565CCA82307DD9EF76DA5B7C4EB7E08"
            "5172E328807C02D011FFBF33785378D79DC266F6A5BE6BB0E4A92ECEEBAEB1";
    constexpr char output[] = 
            "6E8B8BD195BDD560689AF2348BDC74AB7CD05ED8B9A57711E9BE71E9726FDA45"
            "91FEE12205EDACAF82FFBBAF16DFF9E702A708862080166C2FF6BA379BC7FFC2";

    std::vector<uint8_t> buf;
    for(size_t i = 0; i != std::size(input) - 1; i += 2) {
        uint8_t val;
        auto result = std::from_chars(input + i, input + i + 2, val, 16);
        REQUIRE(result.ec == std::errc{});
        buf.push_back(val);
    }

    std::vector<uint8_t> expected;
    for(size_t i = 0; i != std::size(output) - 1; i += 2) {
        uint8_t val;
        auto result = std::from_chars(output + i, output + i + 2, val, 16);
        REQUIRE(result.ec == std::errc{});
        expected.push_back(val);
    }

    muuid_sha3_ctx ctx;
    muuid_sha3_512_init(&ctx);
    muuid_sha3_update(&ctx, buf.data(), buf.size());
    
    std::array<uint8_t, 64> hash;
    muuid_sha3_final(&ctx, hash.data());

    CHECK_EQUAL_SEQ(hash, expected);
}
}