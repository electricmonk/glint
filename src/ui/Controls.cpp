// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Controls.h"

#include "Theme.h"

namespace glint::ui {

Pill::Pill(const juce::String& text, Kind kind)
    : juce::Button(text)
    , kind_(kind)
    , accent_(colour::ice)
{
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTitle(text);
}

void Pill::paintButton(juce::Graphics& g, bool highlighted, bool)
{
    auto b = getLocalBounds().toFloat().reduced(0.5f);
    const float radius = b.getHeight() * 0.5f;
    const bool on = getToggleState();

    switch (kind_) {
    case Kind::Chip: {
        g.setColour(on ? colour::chipOnBg : juce::Colours::transparentBlack);
        g.fillRoundedRectangle(b, radius);
        g.setColour(on ? colour::chipOnBorder : (highlighted ? colour::knobRim : colour::hairline2));
        g.drawRoundedRectangle(b, radius, 1.0f);
        g.setColour(on ? colour::text : colour::textSecondary);
        g.setFont(font(FontFace::Inter400, 12.0f));
        g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred, false);
        break;
    }
    case Kind::Freeze: {
        g.setColour(on ? accent_ : juce::Colours::transparentBlack);
        g.fillRoundedRectangle(b, radius);
        g.setColour(on ? accent_ : (highlighted ? colour::chipOnBorder : colour::knobRim));
        g.drawRoundedRectangle(b, radius, 1.0f);
        g.setColour(on ? colour::background : colour::soft);
        g.setFont(font(FontFace::Inter600, 12.0f).withExtraKerningFactor(0.16f));
        g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred, false);
        break;
    }
    case Kind::Arrow:
        g.setColour(highlighted ? colour::text : colour::textSecondary);
        g.setFont(font(FontFace::Inter400, 17.0f));
        g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred, false);
        break;
    }

    if (hasKeyboardFocus(false)) {
        g.setColour(colour::ice);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), radius, 2.0f);
    }
}

// ---------------------------------------------------------------------------

ChipGroup::ChipGroup(juce::RangedAudioParameter& param, const juce::StringArray& names, const juce::String& title)
    : attachment_(param, [this](float v) {
        const int idx = juce::roundToInt(v);
        for (size_t i = 0; i < chips_.size(); ++i)
            chips_[i]->setToggleState(static_cast<int>(i) == idx, juce::dontSendNotification);
    })
    , title_(title)
{
    setTitle(title);
    setFocusContainerType(FocusContainerType::focusContainer);
    for (int i = 0; i < names.size(); ++i) {
        auto chip = std::make_unique<Pill>(names[i], Pill::Kind::Chip);
        chip->setClickingTogglesState(false);
        chip->setRadioGroupId(0);
        chip->setDescription(title + ": " + names[i]);
        chip->onClick = [this, i] { select(i); };
        addAndMakeVisible(*chip);
        chips_.push_back(std::move(chip));
    }
    attachment_.sendInitialUpdate();
}

void ChipGroup::select(int index)
{
    attachment_.setValueAsCompleteGesture(static_cast<float>(index));
}

int ChipGroup::preferredWidth() const
{
    const auto f = font(FontFace::Inter400, 12.0f);
    int w = 0;
    for (const auto& c : chips_)
        w += juce::roundToInt(juce::GlyphArrangement::getStringWidth(f, c->getButtonText())) + 24 + 4;
    return w - 4;
}

void ChipGroup::resized()
{
    const auto f = font(FontFace::Inter400, 12.0f);
    int x = 0;
    for (auto& c : chips_) {
        const int w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(f, c->getButtonText())) + 24;
        c->setBounds(x, getHeight() - 26, w, 26);
        x += w + 4;
    }
}

void ChipGroup::paint(juce::Graphics& g)
{
    g.setColour(colour::textSecondary);
    g.setFont(font(FontFace::Inter400, 11.0f).withExtraKerningFactor(0.14f));
    g.drawText(title_.toUpperCase(), 0, 0, getWidth(), 14, juce::Justification::centredLeft, false);
}

// ---------------------------------------------------------------------------

FreezeButton::FreezeButton(juce::RangedAudioParameter& param)
    : Pill("FREEZE", Kind::Freeze)
    , attachment_(param, [this](float v) { setToggleState(v >= 0.5f, juce::dontSendNotification); })
{
    setTitle("Freeze");
    setClickingTogglesState(false);
    onClick = [this] { attachment_.setValueAsCompleteGesture(getToggleState() ? 0.0f : 1.0f); };
    attachment_.sendInitialUpdate();
}

// ---------------------------------------------------------------------------

PresetPill::PresetPill(std::function<juce::String()> name, std::function<void(int)> step)
    : name_(std::move(name))
    , step_(std::move(step))
{
    prev_.setTitle("Previous preset");
    next_.setTitle("Next preset");
    prev_.onClick = [this] { step_(-1); refresh(); };
    next_.onClick = [this] { step_(+1); refresh(); };
    addAndMakeVisible(prev_);
    addAndMakeVisible(next_);
    label_.setInterceptsMouseClicks(false, false);
    label_.setAccessible(true);
    label_.setTitle(name_());
    addAndMakeVisible(label_);
    setTitle("Preset");
    setFocusContainerType(FocusContainerType::focusContainer);
}

void PresetPill::resized()
{
    auto b = getLocalBounds();
    prev_.setBounds(b.removeFromLeft(34));
    next_.setBounds(b.removeFromRight(34));
    label_.setBounds(b);
}

void PresetPill::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(colour::hairline2);
    g.drawRoundedRectangle(b, b.getHeight() * 0.5f, 1.0f);
    g.setColour(colour::text);
    g.setFont(font(FontFace::Inter500, 13.0f));
    g.drawText(name_(), getLocalBounds().reduced(34, 0), juce::Justification::centred, true);
}

} // namespace glint::ui
