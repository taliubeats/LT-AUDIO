#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

namespace lt_void
{

/** The 12 reverb engines of LT VOID. Each one changes the sound and the colours. */
enum class ReverbType { cloud = 0, plate, hall, room, chamber, vintage, spring, shimmer, reverse, gated, lofi, voidType, count };

/** How a type shapes the shared reverb engine. */
struct TypeCharacter
{
    float sizeScale = 1.0f;     // multiplies the delay lengths
    float erGain = 0.0f;        // early reflections level
    float erSpread = 1.0f;      // early reflections spacing
    float modScale = 1.0f;      // modulation depth multiplier
    float bloom = 1.0f;         // ATTACK time multiplier
    float maxDamp = 20000.0f;   // extra high damping inside the tank
    float loopLowCut = 0.0f;    // extra low cut inside the tank (0 = none)
    float dispersion = 0.0f;    // spring "boing" (allpass chirp inside the tank)
    float shimmer = 0.0f;       // +12 st feedback amount
    int crush = 0;              // 0 none, 1 vintage (12 bit, /2), 2 lo-fi (8 bit, /4, hiss)
    bool gated = false;
    bool reverse = false;
    float outputGain = 1.0f;
};

struct TypeInfo
{
    static constexpr int count = (int) ReverbType::count;
    static const juce::StringArray& names();
    static juce::String description (int type);
    static TypeCharacter character (int type);

    /** Colours (see design/void/all-types.webp). */
    struct Colours { juce::Colour accent, nebulaA, nebulaB, background; bool light = false; };
    static Colours colours (int type);
};

} // namespace lt_void
