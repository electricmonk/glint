// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "dsp/GlintEngine.h"

TEST_CASE("Engine prepares and processes any block size")
{
    glint::GlintEngine engine;
    engine.prepare(48000.0, 512);

    for (int block : {1, 32, 256, 1024, 4096}) {
        std::vector<float> l(static_cast<size_t>(block), 0.25f), r(static_cast<size_t>(block), -0.25f);
        engine.process(l.data(), r.data(), block);
        for (int i = 0; i < block; ++i) {
            REQUIRE(std::isfinite(l[static_cast<size_t>(i)]));
            REQUIRE(std::isfinite(r[static_cast<size_t>(i)]));
        }
    }
}
