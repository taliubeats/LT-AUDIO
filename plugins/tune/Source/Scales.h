#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace tune
{

using NoteMask = std::array<bool, 12>;   // index 0 = C ... 11 = B

struct Keys
{
    static const juce::StringArray& names();                 // C, C#, ... B
    static juce::String noteName (int pitchClass);           // "F#"
};

struct Scales
{
    enum { chromatic = 0, major, minor, harmonicMinor, pentatonicMajor, pentatonicMinor, blues, custom, count };
    static const juce::StringArray& names();                 // "Chromatic", "Major", ...
    static NoteMask mask (int key, int scale);               // notes of a key + scale (custom = all)
    static juce::String display (int key, int scale);        // "A Minor", "Chromatic", "Custom"
};

/** Singer ranges: they set the pitch detection range (and so the latency). */
struct Voices
{
    enum { soprano = 0, alto, tenor, baritone, bass, count };
    static const juce::StringArray& names();
    static float minHz (int voice);
    static float maxHz (int voice);
};

} // namespace tune
