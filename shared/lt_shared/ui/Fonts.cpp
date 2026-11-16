#include "LTFontsData.h"

namespace lt::ui
{

juce::Font Fonts::get (Weight weight, float height)
{
    static juce::Typeface::Ptr faces[4] = {
        juce::Typeface::createSystemTypefaceFor (LTFontsData::MontserratLight_ttf,    LTFontsData::MontserratLight_ttfSize),
        juce::Typeface::createSystemTypefaceFor (LTFontsData::MontserratRegular_ttf,  LTFontsData::MontserratRegular_ttfSize),
        juce::Typeface::createSystemTypefaceFor (LTFontsData::MontserratSemiBold_ttf, LTFontsData::MontserratSemiBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor (LTFontsData::MontserratBold_ttf,     LTFontsData::MontserratBold_ttfSize)
    };

    auto face = faces[(int) weight];
    if (face == nullptr)
        return juce::Font (juce::FontOptions (height));

    return juce::Font (juce::FontOptions (face).withHeight (height));
}

void Fonts::drawSpaced (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                        juce::Font font, float extraSpacing, juce::Justification just)
{
    font.setExtraKerningFactor (extraSpacing);
    g.setFont (font);
    g.drawText (text, area, just, false);
}

} // namespace lt::ui
