// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Theme.h"

#include <array>

#include "BinaryData.h"

namespace glint::ui {

namespace {
juce::Typeface::Ptr load(FontFace face)
{
    switch (face) {
    case FontFace::Grotesk500: return juce::Typeface::createSystemTypefaceFor(BinaryData::spacegrotesk500_ttf, BinaryData::spacegrotesk500_ttfSize);
    case FontFace::Grotesk600: return juce::Typeface::createSystemTypefaceFor(BinaryData::spacegrotesk600_ttf, BinaryData::spacegrotesk600_ttfSize);
    case FontFace::Inter400: return juce::Typeface::createSystemTypefaceFor(BinaryData::inter400_ttf, BinaryData::inter400_ttfSize);
    case FontFace::Inter500: return juce::Typeface::createSystemTypefaceFor(BinaryData::inter500_ttf, BinaryData::inter500_ttfSize);
    case FontFace::Inter600: return juce::Typeface::createSystemTypefaceFor(BinaryData::inter600_ttf, BinaryData::inter600_ttfSize);
    case FontFace::Mono400: return juce::Typeface::createSystemTypefaceFor(BinaryData::jetbrainsmono400_ttf, BinaryData::jetbrainsmono400_ttfSize);
    case FontFace::Mono500: return juce::Typeface::createSystemTypefaceFor(BinaryData::jetbrainsmono500_ttf, BinaryData::jetbrainsmono500_ttfSize);
    }
    return {};
}
} // namespace

FontLibrary::FontLibrary()
{
    for (size_t i = 0; i < faces.size(); ++i)
        faces[i] = load(static_cast<FontFace>(i));
}

juce::Font font(FontFace face, float height)
{
    FontHandle lib;
    const auto& tf = lib->faces[static_cast<size_t>(face)];
    // Heights in the spec are CSS px font-size; JUCE's height is ascent+descent.
    return juce::Font(juce::FontOptions(tf).withPointHeight(height * 0.75f));
}

} // namespace glint::ui
