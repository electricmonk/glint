// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace glint::ui {

// Pill-shaped button drawn per the mockup; accessible as a (toggle) button.
class Pill : public juce::Button {
public:
    enum class Kind { Chip, Freeze, Arrow };
    Pill(const juce::String& text, Kind kind);
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;
    void setAccent(juce::Colour c) { accent_ = c; repaint(); }

private:
    Kind kind_;
    juce::Colour accent_;
};

// Row of chips bound to a choice parameter (radio behaviour).
class ChipGroup : public juce::Component {
public:
    ChipGroup(juce::RangedAudioParameter& param, const juce::StringArray& names, const juce::String& title);
    void resized() override;
    void paint(juce::Graphics&) override;
    int preferredWidth() const;

private:
    juce::ParameterAttachment attachment_;
    juce::String title_;
    std::vector<std::unique_ptr<Pill>> chips_;
    void select(int index);
};

// FREEZE toggle bound to a bool parameter.
class FreezeButton : public Pill {
public:
    explicit FreezeButton(juce::RangedAudioParameter& param);

private:
    juce::ParameterAttachment attachment_;
};

// "‹ name ›" preset selector.
class PresetPill : public juce::Component {
public:
    PresetPill(std::function<juce::String()> name, std::function<void(int delta)> step);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh() { repaint(); label_.setTitle(name_()); }

private:
    std::function<juce::String()> name_;
    std::function<void(int)> step_;
    Pill prev_ { juce::String::fromUTF8("\xe2\x80\xb9"), Pill::Kind::Arrow };
    Pill next_ { juce::String::fromUTF8("\xe2\x80\xba"), Pill::Kind::Arrow };
    juce::Component label_;
};

} // namespace glint::ui
