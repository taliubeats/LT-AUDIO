#pragma once

namespace lt::ui
{

/** "LT" + product name logotypes and the small product icons (see design/brand). */
struct Logo
{
    enum class Glyph { none, orbitRing, planetRing, blackHole, star, galaxy, key };

    struct Spec
    {
        juce::String name;           // "VOID"
        Glyph glyph = Glyph::none;   // special letter
        int glyphIndex = -1;         // which letter of `name` is replaced/decorated
        bool boldName = false;       // PULSAR keeps its bold italic style
    };

    static Spec specFor (const juce::String& productId);
    static juce::String displayNameFor (const juce::String& productId);

    /** Draws the logotype fitted into `area`, aligned by `just`. Returns the width used. */
    static float draw (juce::Graphics&, juce::Rectangle<float> area, const Spec&, const Palette&,
                       juce::Justification just = juce::Justification::centred);

    /** Rounded-square app icon for a product. */
    static void drawIcon (juce::Graphics&, juce::Rectangle<float> area, const juce::String& productId,
                          const Palette&, bool selected = false);

    /** Just the symbol (no tile), e.g. for buttons. */
    static void drawSymbol (juce::Graphics&, juce::Rectangle<float> area, Glyph, const Palette&);
};

} // namespace lt::ui
