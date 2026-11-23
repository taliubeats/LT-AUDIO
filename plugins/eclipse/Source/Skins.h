#pragma once

#include <juce_graphics/juce_graphics.h>

namespace eclipse
{

/** Drum families. The order is also the order of the 8 extra stereo outputs. */
enum class Family { kick = 0, snare, clap, hat, openHat, tom, perc, cymbal, sample };
constexpr int numFamilies = 8;
const char* familyName (Family);            // "KICK", "SNARE"...
const char* familyLabel (Family);           // "Kick", "Snare"... for menus

/** Character of the VIBE saturation. Each skin picks one. */
enum class Vibe { tape = 0, warmTape, hardClip, tube, exciter, console, darkTape, heavyTube,
                  sweet, silk, fatClip, bright, fuzz, glue };
constexpr int numVibes = 14;
const char* vibeName (Vibe);                // "TAPE", "WARM TAPE"...

/**
    A skin: every colour the UI uses, the family colours and the VIBE character.
    Neutral skins are tuned by hand; artist skins are generated from a base colour and an accent
    with the same formulas as design/eclipse/eclipse-ui.html.
*/
struct Skin
{
    juce::String id, name, caption;   // caption = VIBE character in words
    bool light = false, neutral = false;
    Vibe vibe = Vibe::tape;

    juce::Colour room, chassis1, chassis2, panel1, panel2, hi, lo;
    juce::Colour tx, dim, lcd1, lcd2, lcdLine;
    juce::Colour key1, key2, keyEdge, keyTx;
    juce::Colour pad1, pad2, padEdge, padEmpty;
    juce::Colour cap1, cap2, skirt1, skirt2, ringOff, acc, screw;
    juce::Colour fam[numFamilies];

    juce::Colour family (Family f) const { return f == Family::sample ? dim : fam[(int) f]; }
};

const std::vector<Skin>& allSkins();
const Skin& skinById (const juce::String& id);   // falls back to NEGRA
int skinIndex (const juce::String& id);

} // namespace eclipse
