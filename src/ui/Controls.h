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
    enum class Kind { Chip, Freeze, Arrow, GroupToggle };
    // Draws an icon in place of the text (the text stays the accessible name).
    using Icon = void (*)(juce::Graphics&, juce::Rectangle<float>);
    Pill(const juce::String& text, Kind kind);
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;
    void setAccent(juce::Colour c) { accent_ = c; repaint(); }
    void setIcon(Icon icon) { icon_ = icon; repaint(); }
    bool hasIcon() const noexcept { return icon_ != nullptr; }

private:
    Kind kind_;
    juce::Colour accent_;
    Icon icon_ = nullptr;
};

// Direction icons: right arrow, two-way arrow, left arrow.
void drawArrowRight(juce::Graphics&, juce::Rectangle<float>);
void drawArrowBoth(juce::Graphics&, juce::Rectangle<float>);
void drawArrowLeft(juce::Graphics&, juce::Rectangle<float>);

// Row of chips bound to a choice parameter (radio behaviour).
class ChipGroup : public juce::Component {
public:
    // An empty title draws no heading (a GroupToggle can sit there instead);
    // the accessible title is always set.
    ChipGroup(juce::RangedAudioParameter& param, const juce::StringArray& names, const juce::String& title,
              bool showTitle = true);
    void resized() override;
    void paint(juce::Graphics&) override;
    int preferredWidth() const;
    void setIcons(std::initializer_list<Pill::Icon> icons);

private:
    juce::ParameterAttachment attachment_;
    juce::String title_;
    bool showTitle_ = true;
    std::vector<std::unique_ptr<Pill>> chips_;
    void select(int index);
    int chipWidth(const Pill&) const;
};

// Section on/off switch drawn as a heading: a dot (filled when on) and the
// section name. Accessible as a toggle button.
class GroupToggle : public Pill {
public:
    GroupToggle(juce::RangedAudioParameter& param, const juce::String& name, juce::Colour accent);
    int preferredWidth() const;

private:
    juce::ParameterAttachment attachment_;
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
