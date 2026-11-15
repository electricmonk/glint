// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Knob.h"

#include "Theme.h"
#include "plugin/Parameters.h"

namespace glint::ui {

namespace {
constexpr float kDegToRad = juce::MathConstants<float>::pi / 180.0f;

class KnobValueInterface final : public juce::AccessibilityValueInterface {
public:
    explicit KnobValueInterface(Knob& k) : knob_(k) {}
    bool isReadOnly() const override { return false; }
    double getCurrentValue() const override { return knob_.value(); }
    void setValue(double v) override { knob_.setValueFromAccessibility(static_cast<float>(v)); }
    juce::String getCurrentValueAsString() const override { return knob_.valueText(); }
    void setValueAsString(const juce::String& s) override { setValue(s.retainCharacters("-0123456789.").getDoubleValue()); }
    AccessibleValueRange getRange() const override
    {
        const auto r = knob_.range();
        return { { r.start, r.end }, (r.end - r.start) / 100.0 };
    }

private:
    Knob& knob_;
};
} // namespace

Knob::Knob(juce::RangedAudioParameter& param, juce::String label, juce::Colour colour, Style style, bool bipolar, float hotAbove)
    : param_(param)
    , attachment_(param, [this](float v) { value_ = v; repaint(); })
    , label_(std::move(label))
    , colour_(colour)
    , style_(style)
    , bipolar_(bipolar)
    , hotAbove_(hotAbove)
{
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    setTitle(label_);
    setDescription(label_);
    setHelpText("Drag vertically, Shift for fine. Double-click to reset. Arrow keys step 1%, Shift 10%.");
    attachment_.sendInitialUpdate();
}

juce::String Knob::valueText() const
{
    return params::formatValue(param_.getParameterID(), value_);
}

juce::Rectangle<float> Knob::knobBounds() const
{
    if (style_.labelBelow)
        return { (static_cast<float>(getWidth()) - style_.size) * 0.5f, 0.0f, style_.size, style_.size };
    return { 0.0f, (static_cast<float>(getHeight()) - style_.size) * 0.5f, style_.size, style_.size };
}

void Knob::paint(juce::Graphics& g)
{
    const auto kb = knobBounds();
    const float c = style_.size * 0.5f;
    const float cx = kb.getX() + c, cy = kb.getY() + c;
    const auto range = param_.getNormalisableRange();
    const float n = juce::jlimit(0.0f, 1.0f, (value_ - range.start) / (range.end - range.start));
    const float ang = -135.0f + 270.0f * n;

    // Face.
    g.setColour(colour::knobFace);
    g.fillEllipse(cx - style_.inner, cy - style_.inner, style_.inner * 2.0f, style_.inner * 2.0f);
    g.setColour(colour::knobRim);
    g.drawEllipse(cx - style_.inner, cy - style_.inner, style_.inner * 2.0f, style_.inner * 2.0f, 1.0f);

    auto strokeArc = [&](float a0, float a1, juce::Colour col) {
        if (a1 - a0 < 0.5f)
            return;
        juce::Path p;
        p.addCentredArc(cx, cy, style_.radius, style_.radius, 0.0f, a0 * kDegToRad, a1 * kDegToRad, true);
        g.setColour(col);
        g.strokePath(p, juce::PathStrokeType(style_.stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    strokeArc(-135.0f, 135.0f, colour::knobTrack);
    if (bipolar_) {
        if (ang >= 0.0f) strokeArc(0.0f, ang, colour_);
        else strokeArc(ang, 0.0f, colour_);
    } else if (value_ > hotAbove_) {
        const float aH = -135.0f + 270.0f * ((hotAbove_ - range.start) / (range.end - range.start));
        strokeArc(-135.0f, aH, colour_);
        strokeArc(aH, ang, colour::hot);
    } else {
        strokeArc(-135.0f, ang, colour_);
    }

    // Pointer.
    const float rad = (ang - 90.0f) * kDegToRad;
    const juce::Point<float> p0 { cx + style_.pointer0 * std::cos(rad), cy + style_.pointer0 * std::sin(rad) };
    const juce::Point<float> p1 { cx + style_.pointer1 * std::cos(rad), cy + style_.pointer1 * std::sin(rad) };
    g.setColour(colour::text);
    juce::Path ptr;
    ptr.startNewSubPath(p0);
    ptr.lineTo(p1);
    g.strokePath(ptr, juce::PathStrokeType(style_.pointerWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Keyboard focus ring.
    if (hasKeyboardFocus(false)) {
        g.setColour(colour::ice);
        g.drawEllipse(kb.reduced(1.0f), 2.0f);
    }

    // Label + value.
    if (style_.labelBelow) {
        auto text = getLocalBounds().toFloat().withTrimmedTop(style_.size + 7.0f);
        g.setColour(colour::text);
        g.setFont(font(FontFace::Grotesk500, style_.labelSize).withExtraKerningFactor(0.14f));
        g.drawText(label_.toUpperCase(), text.removeFromTop(16.0f), juce::Justification::centred, false);
        g.setColour(colour_);
        g.setFont(font(FontFace::Mono400, style_.valueSize));
        g.drawText(valueText(), text.removeFromTop(15.0f), juce::Justification::centred, false);
    } else {
        auto text = getLocalBounds().toFloat().withTrimmedLeft(style_.size + 8.0f);
        text = text.withSizeKeepingCentre(text.getWidth(), 29.0f);
        g.setColour(colour::textSecondary);
        g.setFont(font(FontFace::Inter400, 11.0f).withExtraKerningFactor(0.14f));
        g.drawText(label_.toUpperCase(), text.removeFromTop(14.0f), juce::Justification::centredLeft, false);
        text.removeFromTop(1.0f);
        g.setColour(colour::text);
        g.setFont(font(FontFace::Mono400, 11.0f));
        g.drawText(valueText(), text.removeFromTop(14.0f), juce::Justification::centredLeft, false);
    }
}

void Knob::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    dragging_ = true;
    dragStartValue_ = value_;
    dragStartY_ = e.position.y;
    fineDrag_ = e.mods.isShiftDown();
    attachment_.beginGesture();
}

void Knob::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragging_)
        return;
    const auto range = param_.getNormalisableRange();
    const bool fine = e.mods.isShiftDown();
    // Re-anchor when Shift toggles so the knob doesn't jump.
    if (fine != fineDrag_) {
        fineDrag_ = fine;
        dragStartValue_ = value_;
        dragStartY_ = e.position.y;
    }
    const float k = fine ? 0.15f : 1.0f;
    const float v = juce::jlimit(range.start, range.end,
                                 dragStartValue_ + (dragStartY_ - e.position.y) / 170.0f * (range.end - range.start) * k);
    attachment_.setValueAsPartOfGesture(v);
}

void Knob::mouseUp(const juce::MouseEvent&)
{
    if (!dragging_)
        return;
    dragging_ = false;
    attachment_.endGesture();
}

void Knob::mouseDoubleClick(const juce::MouseEvent&)
{
    attachment_.setValueAsCompleteGesture(param_.convertFrom0to1(param_.getDefaultValue()));
}

void Knob::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto range = param_.getNormalisableRange();
    const float dy = (std::abs(w.deltaY) > std::abs(w.deltaX) ? w.deltaY : -w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    const float k = e.mods.isShiftDown() ? 0.15f : 1.0f;
    attachment_.setValueAsCompleteGesture(juce::jlimit(range.start, range.end, value_ + dy * 0.25f * (range.end - range.start) * k));
}

void Knob::stepBy(float fraction)
{
    const auto range = param_.getNormalisableRange();
    attachment_.setValueAsCompleteGesture(juce::jlimit(range.start, range.end, value_ + fraction * (range.end - range.start)));
}

bool Knob::keyPressed(const juce::KeyPress& key)
{
    const float step = key.getModifiers().isShiftDown() ? 0.10f : 0.01f;
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey) { stepBy(step); return true; }
    if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey) { stepBy(-step); return true; }
    if (code == juce::KeyPress::homeKey) { attachment_.setValueAsCompleteGesture(param_.getNormalisableRange().start); return true; }
    if (code == juce::KeyPress::endKey) { attachment_.setValueAsCompleteGesture(param_.getNormalisableRange().end); return true; }
    return false;
}

void Knob::setValueFromAccessibility(float plain)
{
    const auto range = param_.getNormalisableRange();
    attachment_.setValueAsCompleteGesture(juce::jlimit(range.start, range.end, plain));
}

std::unique_ptr<juce::AccessibilityHandler> Knob::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(
        *this, juce::AccessibilityRole::slider, juce::AccessibilityActions {},
        juce::AccessibilityHandler::Interfaces { std::make_unique<KnobValueInterface>(*this) });
}

} // namespace glint::ui
