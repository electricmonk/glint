// SPDX-License-Identifier: AGPL-3.0-or-later
// glint-render: offline renderer and benchmark for the Glint engine.
// Phase 0 stub; grows in Phase 1.
#include <cstdio>

#include "dsp/GlintEngine.h"

int main()
{
    glint::GlintEngine engine;
    engine.prepare(48000.0, 256);
    std::puts("glint-render: engine ready");
    return 0;
}
