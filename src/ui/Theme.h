// SPDX-License-Identifier: AGPL-3.0-or-later
// Colours and fonts from docs/ui/UI-SPEC.md / glint-ui-v2.dc.html.
#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

namespace glint::ui {

namespace colour {
inline const juce::Colour background { 0xff0e1014 };
inline const juce::Colour well { 0xff0a0c0f };
inline const juce::Colour hairline { 0xff1c2026 };
inline const juce::Colour hairline2 { 0xff252a31 };
inline const juce::Colour text { 0xffeceae6 };
inline const juce::Colour textSecondary { 0xff9ca3ab };
inline const juce::Colour textDim { 0xff8a929b };
inline const juce::Colour gold { 0xffebc77a };
inline const juce::Colour ice { 0xff9ed8e6 };
inline const juce::Colour soft { 0xffc9ced6 };
inline const juce::Colour hot { 0xfffff1d0 };
inline const juce::Colour knobFace { 0xff171a20 };
inline const juce::Colour knobRim { 0xff2a3038 };
inline const juce::Colour knobTrack { 0xff1e2329 };
inline const juce::Colour chipOnBg { 0xff232830 };
inline const juce::Colour chipOnBorder { 0xff3a414b };
inline const juce::Colour centreLine { 0xff1f242b };
} // namespace colour

enum class FontFace { Grotesk500, Grotesk600, Inter400, Inter500, Inter600, Mono400, Mono500 };

// Bundled OFL fonts (BinaryData). The typefaces live as long as at least one
// FontLibrary handle exists (the editor holds one), so nothing outlives JUCE.
struct FontLibrary {
    FontLibrary();
    std::array<juce::Typeface::Ptr, 7> faces;
};
using FontHandle = juce::SharedResourcePointer<FontLibrary>;

juce::Font font(FontFace face, float height);

} // namespace glint::ui
