// SPDX-License-Identifier: AGPL-3.0-or-later
// Factory presets. Values are in host units (percent; Shimmer/Texture/Direction
// indices). Direction and the switches default to Forward / on.
#pragma once

#include <array>

namespace glint::presets {

struct Preset {
    const char* name;
    float bloom, space, density, drift, spread, stereoDensity, tone, mix;
    int shimmer; // 0 Octave, 1 Fifth, 2 Oct + 5th, 3 Sub + Oct
    int texture; // 0 Clean, 1 Tape, 2 Dust
    int direction = 0; // 0 Forward, 1 Both, 2 Reverse
    bool shimmerOn = true;
    bool grainsOn = true;
};

// Defaults: bloom 60, space 65, density 45, drift 35, spread 70, stereo 0, tone 0, mix 35.
inline constexpr std::array<Preset, 20> kFactory { {
    // The five from the v2 mockup.
    { "Halo",             60, 65, 45, 35,  70,   0,   0, 35, 0, 0 },
    { "Night Choir",      85, 85, 30, 50,  80, -30, -30, 50, 3, 0 },
    { "Cathedral Snow",  104, 95, 70, 40, 100,  60,  25, 60, 2, 0 },
    { "Tape Ghosts",      45, 55, 55, 70,  75,  20, -45, 45, 0, 1, 1 },
    { "Dust Halo",        70, 75, 85, 60,  90,  40, -15, 40, 1, 2 },
    // Grown to 20.
    { "Glass Harp",       75, 70, 25, 20,  60, -20,  45, 40, 0, 0, 0, true, false }, // pure shimmer reverb
    { "Slow Aurora",      95, 90, 35, 45,  85,  30,  10, 55, 2, 0 },
    { "Frost Bloom",     108, 85, 60, 30,  95,  50,  35, 55, 0, 0 },
    { "Low Cloud",        80, 80, 40, 40,  70, -40, -60, 50, 3, 1 },
    { "Starlit Vault",    90, 100, 50, 35, 100,  70,  20, 50, 2, 0 },
    { "Whisper Room",     30, 40, 60, 50,  55,   0, -20, 30, 0, 0, 1, false, true }, // grains + plain reverb
    { "Choir Of Wires",   88, 75, 90, 25,  80,  20,  40, 45, 1, 0 },
    { "Lantern",          55, 60, 20, 15,  50, -50, -10, 35, 1, 1 },
    { "Drift Ice",        65, 70, 50, 85,  90,  60,  15, 45, 0, 2, 1 },
    { "Old Film",         50, 55, 65, 75,  60,  10, -55, 40, 0, 2, 2 },
    { "Bright Halo",      70, 60, 45, 30,  75,   0,  60, 35, 0, 0 },
    { "Deep Field",      100, 100, 30, 55, 100,  80, -35, 60, 3, 0 },
    { "Mist Pads",        78, 88, 75, 60,  85, -10, -25, 65, 2, 1 },
    { "Ember Choir",     102, 80, 55, 40,  70, -30, -40, 50, 3, 2 },
    { "Ghost Note",       40, 50, 10, 20,  40, -70,   5, 30, 0, 0, 2 },
} };

} // namespace glint::presets
