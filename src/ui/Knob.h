// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace glint::ui {

// Rotary knob per UI-SPEC: 270 degree arc, vertical drag (Shift = fine),
// double-click = default, arrows step 1% (Shift 10%), wheel, host gestures,
// keyboard focus and an accessibility handler with name + formatted value.
class Knob : public juce::Component {
public:
    struct Style {
        float size = 72.0f, radius = 30.0f, inner = 21.0f, stroke = 5.0f;
        float pointer0 = 4.0f, pointer1 = 15.0f, pointerWidth = 3.0f;
        float labelSize = 12.0f, valueSize = 12.0f;
        bool labelBelow = true; // false: label/value to the right (footer minis)
    };

    Knob(juce::RangedAudioParameter& param, juce::String label, juce::Colour colour, Style style,
         bool bipolar = false, float hotAbove = std::numeric_limits<float>::max());

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    float value() const noexcept { return value_; }
    juce::String valueText() const;
    const juce::String& label() const noexcept { return label_; }
    void setValueFromAccessibility(float plain);
    juce::NormalisableRange<float> range() const { return param_.getNormalisableRange(); }

private:
    juce::Rectangle<float> knobBounds() const;
    void stepBy(float fraction);

    juce::RangedAudioParameter& param_;
    juce::ParameterAttachment attachment_;
    juce::String label_;
    juce::Colour colour_;
    Style style_;
    bool bipolar_;
    float hotAbove_;
    float value_ = 0.0f;
    float dragStartValue_ = 0.0f;
    float dragStartY_ = 0.0f;
    bool dragging_ = false;
    bool fineDrag_ = false;
};

} // namespace glint::ui
