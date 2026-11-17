// SPDX-License-Identifier: AGPL-3.0-or-later
// Phase 4/5: state round-trip, presets, bypass, sample-rate and block-size
// changes, editor reopen state and accessibility, plus UI snapshots.
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../tools/common/Analysis.h"
#include "../tools/common/Signals.h"
#include "plugin/Parameters.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

using namespace glint;

namespace {

std::unique_ptr<GlintAudioProcessor> makeProcessor(double sr = 48000.0, int block = 512)
{
    auto p = std::make_unique<GlintAudioProcessor>();
    p->setPlayConfigDetails(2, 2, sr, block);
    p->prepareToPlay(sr, block);
    return p;
}

void setPlain(GlintAudioProcessor& p, const char* id, float v)
{
    auto* prm = p.state().getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(v));
}

struct Rendered {
    std::vector<float> l, r;
};

Rendered run(GlintAudioProcessor& p, const std::vector<float>& in, int block, bool bypassSecondHalf = false)
{
    Rendered out { in, in };
    juce::MidiBuffer midi;
    for (size_t pos = 0; pos < in.size(); pos += static_cast<size_t>(block)) {
        const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(block), in.size() - pos));
        float* chans[2] = { out.l.data() + pos, out.r.data() + pos };
        juce::AudioBuffer<float> buf(chans, 2, n);
        const bool bypass = bypassSecondHalf && pos >= in.size() / 3 && pos < 2 * in.size() / 3;
        if (bypass) p.processBlockBypassed(buf, midi);
        else p.processBlock(buf, midi);
    }
    return out;
}

} // namespace

TEST_CASE("Plugin: the 11 macros plus 3 switches are host parameters, with frozen IDs")
{
    GlintAudioProcessor p;
    const juce::StringArray expected { "macro.bloom", "macro.space", "macro.density", "macro.drift", "macro.spread",
                                       "macro.stereoDensity", "macro.tone", "macro.shimmer", "macro.texture", "macro.mix",
                                       "macro.freeze", "macro.shimmerOn", "macro.grainsOn", "macro.direction" };
    juce::StringArray ids;
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm))
            ids.add(r->getParameterID());
    CHECK(ids == expected);
    // No forbidden names in parameter or preset names.
    for (auto* prm : p.getParameters())
        for (const char* bad : { "Mutable", "Beads", "Valhalla" })
            CHECK_FALSE(prm->getName(64).containsIgnoreCase(bad));
    for (int i = 0; i < p.getNumPrograms(); ++i)
        for (const char* bad : { "Mutable", "Beads", "Valhalla" })
            CHECK_FALSE(p.getProgramName(i).containsIgnoreCase(bad));
}

TEST_CASE("Plugin: state round-trips exactly")
{
    auto a = makeProcessor();
    juce::Random rng(1234);
    // Random legal values (bools/choices snapped to their steps).
    auto randomise = [&] {
        for (auto* prm : a->getParameters()) {
            auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm);
            r->setValueNotifyingHost(r->convertTo0to1(r->convertFrom0to1(rng.nextFloat())));
        }
    };
    randomise();
    a->setCurrentProgram(3);
    randomise();

    juce::MemoryBlock mb;
    a->getStateInformation(mb);
    auto b = makeProcessor();
    b->setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));

    const auto pa = a->getParameters(), pb = b->getParameters();
    REQUIRE(pa.size() == pb.size());
    for (int i = 0; i < pa.size(); ++i) {
        INFO(pa[i]->getName(32));
        CHECK(pa[i]->getValue() == pb[i]->getValue());
    }
    CHECK(b->getCurrentProgram() == 3);

    juce::MemoryBlock mb2;
    b->getStateInformation(mb2);
    CHECK(mb == mb2);
}

TEST_CASE("Plugin: 20 factory presets, starting with the five from the mockup")
{
    GlintAudioProcessor p;
    CHECK(p.getNumPrograms() == 20);
    CHECK(p.getProgramName(0) == "Halo");
    CHECK(p.getProgramName(1) == "Night Choir");
    CHECK(p.getProgramName(2) == "Cathedral Snow");
    CHECK(p.getProgramName(3) == "Tape Ghosts");
    CHECK(p.getProgramName(4) == "Dust Halo");
    p.setCurrentProgram(2);
    CHECK(p.state().getRawParameterValue(params::id::bloom)->load() == 104.0f);
    CHECK(static_cast<int>(p.state().getRawParameterValue(params::id::shimmer)->load()) == 2);
}

TEST_CASE("Plugin: every preset renders finite audio below 0 dBFS")
{
    const double sr = 48000.0;
    const auto in = signals::pink(sr, 6.0, 0.5, 3);
    for (int i = 0; i < 20; ++i) {
        auto p = makeProcessor(sr, 256);
        p->setCurrentProgram(i);
        p->reset();
        const auto out = run(*p, in, 256);
        const double peak = std::max(analysis::peak(out.l), analysis::peak(out.r));
        INFO(p->getProgramName(i) << ": peak " << analysis::toDb(peak) << " dBFS");
        CHECK(analysis::countNonFinite(out.l) + analysis::countNonFinite(out.r) == 0);
        CHECK(peak < 1.0);
        CHECK(analysis::rms(out.l, static_cast<size_t>(sr)) > 1e-4);
    }
}

TEST_CASE("Plugin: sample-rate and block-size changes stay finite")
{
    auto p = makeProcessor(44100.0, 512);
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0, 48000.0 }) {
        for (int block : { 1, 32, 256, 1024, 4096 }) {
            p->setPlayConfigDetails(2, 2, sr, block);
            p->prepareToPlay(sr, block);
            const auto in = signals::pink(sr, 0.25, 0.5, 9);
            const auto out = run(*p, in, block);
            INFO(sr << " Hz, block " << block);
            CHECK(analysis::countNonFinite(out.l) + analysis::countNonFinite(out.r) == 0);
            CHECK(analysis::peak(out.l) < 1.0);
        }
    }
    // Random block sizes.
    p->prepareToPlay(48000.0, 4096);
    const auto in = signals::pink(48000.0, 2.0, 0.5, 4);
    std::vector<float> l = in, r = in;
    juce::Random rng(7);
    juce::MidiBuffer midi;
    for (size_t pos = 0; pos < in.size();) {
        const int n = std::min(1 + rng.nextInt(4096), static_cast<int>(in.size() - pos));
        float* chans[2] = { l.data() + pos, r.data() + pos };
        juce::AudioBuffer<float> buf(chans, 2, n);
        p->processBlock(buf, midi);
        pos += static_cast<size_t>(n);
    }
    CHECK(analysis::countNonFinite(l) == 0);
}

TEST_CASE("Plugin: mono in / stereo out works")
{
    auto p = std::make_unique<GlintAudioProcessor>();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(p->setBusesLayout(layout));
    p->prepareToPlay(48000.0, 256);
    const auto in = signals::pink(48000.0, 2.0, 0.5, 4);
    std::vector<float> l = in, r(in.size(), 0.0f);
    juce::MidiBuffer midi;
    for (size_t pos = 0; pos < in.size(); pos += 256) {
        float* chans[2] = { l.data() + pos, r.data() + pos };
        juce::AudioBuffer<float> buf(chans, 2, 256);
        p->processBlock(buf, midi);
    }
    CHECK(analysis::rms(r, 48000) > 1e-3);
    CHECK(analysis::countNonFinite(r) == 0);
}

TEST_CASE("Plugin: bypass in and out is click-free")
{
    const double sr = 48000.0;
    auto p = makeProcessor(sr, 256);
    const auto in = signals::sine(sr, 6.0, 220.0, 0.25);
    const auto out = run(*p, in, 256, true);
    const auto clicks = analysis::detectClicks(out.l, sr);
    INFO("clicks " << clicks.clicks << ", worst ratio " << clicks.worstRatio << " at " << clicks.worstIndex / sr << " s");
    CHECK(clicks.clicks == 0);
}

TEST_CASE("Plugin: restoring state snaps bools and choices left at unsnapped values")
{
    // What pluginval does: save, set raw normalised values, restore.
    auto p = makeProcessor();
    setPlain(*p, params::id::shimmerOn, 1.0f);
    setPlain(*p, params::id::freeze, 0.0f);
    juce::MemoryBlock mb;
    p->getStateInformation(mb);
    for (auto* prm : p->getParameters()) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm);
        if (r->getParameterID() == params::id::shimmerOn) r->setValueNotifyingHost(0.67f);
        if (r->getParameterID() == params::id::freeze) r->setValueNotifyingHost(0.3f);
    }
    p->setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    for (auto* prm : p->getParameters()) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm);
        if (r->getParameterID() == params::id::shimmerOn) CHECK(r->getValue() == 1.0f);
        if (r->getParameterID() == params::id::freeze) CHECK(r->getValue() == 0.0f);
    }
}

TEST_CASE("Plugin: version-1 state loads with the switches on and Direction forward")
{
    // Save a state, then rewrite it as version 1 without the new parameters.
    auto a = makeProcessor();
    setPlain(*a, params::id::bloom, 77.0f);
    juce::MemoryBlock mb;
    a->getStateInformation(mb);
    auto xml = juce::AudioProcessor::getXmlFromBinary(mb.getData(), static_cast<int>(mb.getSize()));
    REQUIRE(xml != nullptr);
    xml->setAttribute("stateVersion", 1);
    for (const char* pid : { params::id::shimmerOn, params::id::grainsOn, params::id::direction })
        if (auto* child = xml->getChildByAttribute("id", pid))
            xml->removeChildElement(child, true);
    juce::MemoryBlock v1;
    juce::AudioProcessor::copyXmlToBinary(*xml, v1);

    auto b = makeProcessor();
    setPlain(*b, params::id::shimmerOn, 0.0f);
    setPlain(*b, params::id::grainsOn, 0.0f);
    setPlain(*b, params::id::direction, 100.0f);
    b->setStateInformation(v1.getData(), static_cast<int>(v1.getSize()));
    CHECK(b->state().getRawParameterValue(params::id::bloom)->load() == 77.0f);
    CHECK(b->state().getRawParameterValue(params::id::shimmerOn)->load() == 1.0f);
    CHECK(b->state().getRawParameterValue(params::id::grainsOn)->load() == 1.0f);
    CHECK(b->state().getRawParameterValue(params::id::direction)->load() == 0.0f);
}

TEST_CASE("Plugin: version-2 state maps the Direction choice to a reverse percentage")
{
    // Version 2 stored Direction as 0 Forward / 1 Both / 2 Reverse.
    for (auto [choice, percent] : { std::pair { 0.0, 0.0f }, std::pair { 1.0, 50.0f }, std::pair { 2.0, 100.0f } }) {
        auto a = makeProcessor();
        juce::MemoryBlock mb;
        a->getStateInformation(mb);
        auto xml = juce::AudioProcessor::getXmlFromBinary(mb.getData(), static_cast<int>(mb.getSize()));
        REQUIRE(xml != nullptr);
        xml->setAttribute("stateVersion", 2);
        auto* dir = xml->getChildByAttribute("id", params::id::direction);
        REQUIRE(dir != nullptr);
        dir->setAttribute("value", choice);
        juce::MemoryBlock v2;
        juce::AudioProcessor::copyXmlToBinary(*xml, v2);
        auto b = makeProcessor();
        b->setStateInformation(v2.getData(), static_cast<int>(v2.getSize()));
        INFO("choice " << choice);
        CHECK(b->state().getRawParameterValue(params::id::direction)->load() == percent);
    }
}

TEST_CASE("Editor: reopens with the current state, controls are accessible")
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto p = makeProcessor();
    setPlain(*p, params::id::bloom, 104.0f);
    setPlain(*p, params::id::tone, -30.0f);
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
        ed->setVisible(true);
    }
    setPlain(*p, params::id::space, 20.0f);
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    ed->addToDesktop(0); // accessibility handlers need a peer

    // Walk the tree: every slider-role control has a title and a value.
    int sliders = 0, buttons = 0;
    std::function<void(juce::Component&)> walk = [&](juce::Component& c) {
        if (auto* h = c.getAccessibilityHandler()) {
            if (h->getRole() == juce::AccessibilityRole::slider) {
                ++sliders;
                CHECK(h->getTitle().isNotEmpty());
                REQUIRE(h->getValueInterface() != nullptr);
                CHECK(h->getValueInterface()->getCurrentValueAsString().isNotEmpty());
                if (h->getTitle() == "Bloom")
                    CHECK(h->getValueInterface()->getCurrentValueAsString() == "bloom 104%");
                if (h->getTitle() == "Space")
                    CHECK(h->getValueInterface()->getCurrentValueAsString() == "2.2 s tail"); // Space 20%
                if (h->getTitle() == "Direction")
                    CHECK(h->getValueInterface()->getCurrentValueAsString() == "forward");
                if (h->getTitle() == "Tone")
                    CHECK(h->getValueInterface()->getCurrentValueAsString() == "dark 30");
            }
            if (h->getRole() == juce::AccessibilityRole::button || h->getRole() == juce::AccessibilityRole::toggleButton)
                ++buttons;
        }
        if (c.getWantsKeyboardFocus())
            CHECK(c.getTitle().isNotEmpty());
        for (auto* child : c.getChildren())
            walk(*child);
    };
    walk(*ed);
    CHECK(sliders == 9);           // 6 row knobs + Direction + Tone + Mix
    CHECK(buttons >= 2 + 1 + 4 + 3 + 2); // preset arrows, freeze, chips, switches

    // The section switches are reachable by name and toggle their parameter.
    std::function<juce::Button*(juce::Component&, const juce::String&)> findButton =
        [&](juce::Component& c, const juce::String& title) -> juce::Button* {
        if (auto* b = dynamic_cast<juce::Button*>(&c); b && b->getTitle() == title) return b;
        for (auto* child : c.getChildren())
            if (auto* r = findButton(*child, title)) return r;
        return nullptr;
    };
    for (const char* name : { "Shimmer", "Grains" }) {
        auto* b = findButton(*ed, name);
        REQUIRE(b != nullptr);
        const char* pid = juce::String(name) == "Shimmer" ? params::id::shimmerOn : params::id::grainsOn;
        CHECK(b->getToggleState());
        REQUIRE(b->onClick);
        b->onClick(); // what a click (or the accessibility press action) runs
        CHECK(p->state().getRawParameterValue(pid)->load() == 0.0f);
    }

    // Every button receives a mouse click at its centre (no sibling drawn on
    // top of it swallows the click).
    int hit = 0;
    ed->setVisible(true); // getComponentAt only searches visible components
    std::function<void(juce::Component&)> hitTest = [&](juce::Component& c) {
        if (auto* b = dynamic_cast<juce::Button*>(&c); b && b->isVisible()) {
            const auto centre = ed->getLocalPoint(b, b->getLocalBounds().getCentre());
            auto* under = ed->getComponentAt(centre);
            INFO("button '" << b->getTitle() << "' at " << centre.toString() << " is covered by '"
                            << (under ? under->getTitle() + " / " + under->getName() : juce::String("nothing")) << "'");
            CHECK((under == b || (under != nullptr && b->isParentOf(under))));
            ++hit;
        }
        for (auto* child : c.getChildren())
            hitTest(*child);
    };
    hitTest(*ed);
    CHECK(hit >= 12);

    // Accessible value change goes to the parameter.
    std::function<juce::AccessibilityHandler*(juce::Component&, const juce::String&)> find =
        [&](juce::Component& c, const juce::String& title) -> juce::AccessibilityHandler* {
        if (auto* h = c.getAccessibilityHandler(); h && h->getTitle() == title) return h;
        for (auto* child : c.getChildren())
            if (auto* r = find(*child, title)) return r;
        return nullptr;
    };
    auto* mix = find(*ed, "Mix");
    REQUIRE(mix != nullptr);
    mix->getValueInterface()->setValue(80.0);
    CHECK(p->state().getRawParameterValue(params::id::mix)->load() == 80.0f);
}

TEST_CASE("Editor: snapshots at 100% and 150%", "[snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto p = makeProcessor();
    p->setCurrentProgram(2); // Cathedral Snow (Bloom 104%)
    // Feed audio so the particle cloud has grains.
    const auto in = signals::pink(48000.0, 3.0, 0.5, 5);
    run(*p, in, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    // Drain one frame of grain events into the particle cloud.
    std::function<void(juce::Component&)> poll = [&](juce::Component& c) {
        if (auto* cloud = dynamic_cast<ui::ParticleCloud*>(&c)) cloud->poll();
        for (auto* child : c.getChildren()) poll(*child);
    };
    // ~3 s of audio at 60 UI frames per second.
    juce::AudioBuffer<float> buf(2, 800);
    juce::MidiBuffer midi;
    for (int i = 0; i < 180; ++i) {
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < 800; ++s) buf.setSample(ch, s, in[static_cast<size_t>((i * 800 + s) % static_cast<int>(in.size()))]);
        p->processBlock(buf, midi);
        poll(*ed);
    }
    const auto outDir = juce::File::getCurrentWorkingDirectory();
    for (float scale : { 1.0f, 1.5f }) {
        ed->setSize(juce::roundToInt(820 * scale), juce::roundToInt(560 * scale));
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile("glint-ui-" + juce::String(juce::roundToInt(scale * 100)) + ".png");
        f.deleteFile();
        juce::FileOutputStream os(f);
        CHECK(png.writeImageToStream(img, os));
        CHECK(img.getWidth() == juce::roundToInt(820 * scale));
    }
}

int main(int argc, char* argv[])
{
    return Catch::Session().run(argc, argv);
}

TEST_CASE("Plugin: Direction reads as a reverse percentage and accepts words")
{
    auto p = makeProcessor();
    auto* d = p->state().getParameter(params::id::direction);
    REQUIRE(d != nullptr);
    struct Case { float plain; const char* text; };
    for (auto c : { Case { 0.0f, "forward" }, Case { 30.0f, "30% reverse" }, Case { 50.0f, "50/50" },
                    Case { 100.0f, "reverse" } })
        CHECK(d->getText(d->convertTo0to1(c.plain), 32) == c.text);
    CHECK(d->convertFrom0to1(d->getValueForText("reverse")) == 100.0f);
    CHECK(d->convertFrom0to1(d->getValueForText("forward")) == 0.0f);
    CHECK(d->convertFrom0to1(d->getValueForText("25")) == 25.0f);
}
