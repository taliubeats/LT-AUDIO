#pragma once

namespace lt::ui
{

/** Montserrat (SIL Open Font License), embedded in every LT product. */
struct Fonts
{
    enum class Weight { light, regular, semiBold, bold };

    static juce::Font get (Weight weight, float height);

    static juce::Font light (float h)    { return get (Weight::light, h); }
    static juce::Font regular (float h)  { return get (Weight::regular, h); }
    static juce::Font semiBold (float h) { return get (Weight::semiBold, h); }
    static juce::Font bold (float h)     { return get (Weight::bold, h); }

    /** Draws text with extra letter spacing (the "G R A I N" style used for titles). */
    static void drawSpaced (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                            juce::Font font, float extraSpacing, juce::Justification just);
};

} // namespace lt::ui
