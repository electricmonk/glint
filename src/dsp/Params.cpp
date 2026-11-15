// SPDX-License-Identifier: AGPL-3.0-or-later
#include "dsp/Params.h"

namespace glint {

bool EngineParams::set(std::string_view n, double v) noexcept
{
    const float f = static_cast<float>(v);
    struct Entry { std::string_view name; float EngineParams::*field; };
    static constexpr Entry entries[] = {
        { "inputGainDb", &EngineParams::inputGainDb },
        { "feedback", &EngineParams::feedback },
        { "grainDensity", &EngineParams::grainDensity },
        { "grainRateHz", &EngineParams::grainRateHz },
        { "spread", &EngineParams::spread },
        { "stereoDensity", &EngineParams::stereoDensity },
        { "time", &EngineParams::time },
        { "sizeMs", &EngineParams::sizeMs },
        { "pitchSt", &EngineParams::pitchSt },
        { "shape", &EngineParams::shape },
        { "randTime", &EngineParams::randTime },
        { "randSize", &EngineParams::randSize },
        { "randPitch", &EngineParams::randPitch },
        { "pitchJitterSt", &EngineParams::pitchJitterSt },
        { "reverse", &EngineParams::reverse },
        { "grainFeedback", &EngineParams::grainFeedback },
        { "directToReverb", &EngineParams::directToReverb },
        { "shimmerBlend", &EngineParams::shimmerBlend },
        { "shiftA", &EngineParams::shiftA },
        { "shiftB", &EngineParams::shiftB },
        { "shiftBLevel", &EngineParams::shiftBLevel },
        { "loopHighCutHz", &EngineParams::loopHighCutHz },
        { "loopLowCutHz", &EngineParams::loopLowCutHz },
        { "reverbSize", &EngineParams::reverbSize },
        { "decayS", &EngineParams::decayS },
        { "preDelayMs", &EngineParams::preDelayMs },
        { "diffusion", &EngineParams::diffusion },
        { "dampingHz", &EngineParams::dampingHz },
        { "modDepth", &EngineParams::modDepth },
        { "modRateHz", &EngineParams::modRateHz },
        { "grainsInLoop", &EngineParams::grainsInLoop },
        { "grainsLevelDb", &EngineParams::grainsLevelDb },
        { "reverbLevelDb", &EngineParams::reverbLevelDb },
        { "mix", &EngineParams::mix },
        { "width", &EngineParams::width },
        { "outputGainDb", &EngineParams::outputGainDb },
    };
    for (const auto& e : entries) {
        if (e.name == n) {
            this->*(e.field) = f;
            return true;
        }
    }
    if (n == "freeze") { freeze = v != 0.0; return true; }
    if (n == "grainsOn") { grainsOn = v != 0.0; return true; }
    if (n == "shimmerOn") { shimmerOn = v != 0.0; return true; }
    if (n == "color") { color = static_cast<Texture>(static_cast<int>(v)); return true; }
    if (n == "routing") { routing = static_cast<Routing>(static_cast<int>(v)); return true; }
    return false;
}

bool EngineParams::setString(std::string_view n, std::string_view v) noexcept
{
    if (n == "color") {
        if (v == "clean") color = Texture::Clean;
        else if (v == "digital") color = Texture::Digital;
        else if (v == "tape") color = Texture::Tape;
        else if (v == "dust") color = Texture::Dust;
        else return false;
        return true;
    }
    if (n == "routing") {
        if (v == "serial") routing = Routing::Serial;
        else if (v == "parallel") routing = Routing::Parallel;
        else if (v == "grains-in-loop") routing = Routing::GrainsInLoop;
        else return false;
        return true;
    }
    return false;
}

} // namespace glint
