#include "Scales.h"

namespace tune
{

const juce::StringArray& Keys::names()
{
    static const juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}

juce::String Keys::noteName (int pc)
{
    return names()[((pc % 12) + 12) % 12];
}

const juce::StringArray& Scales::names()
{
    static const juce::StringArray n { "Chromatic", "Major", "Minor", "Harmonic Minor",
                                       "Pentatonic Major", "Pentatonic Minor", "Blues", "Custom" };
    return n;
}

NoteMask Scales::mask (int key, int scale)
{
    static const std::vector<int> steps[] = {
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },  // chromatic
        { 0, 2, 4, 5, 7, 9, 11 },                  // major
        { 0, 2, 3, 5, 7, 8, 10 },                  // natural minor
        { 0, 2, 3, 5, 7, 8, 11 },                  // harmonic minor
        { 0, 2, 4, 7, 9 },                         // pentatonic major
        { 0, 3, 5, 7, 10 },                        // pentatonic minor
        { 0, 3, 5, 6, 7, 10 },                     // blues
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },  // custom (edited in EDIT NOTES)
    };
    NoteMask m {};
    for (auto s : steps[juce::jlimit (0, count - 1, scale)])
        m[(size_t) ((key + s) % 12)] = true;
    return m;
}

juce::String Scales::display (int key, int scale)
{
    if (scale == chromatic || scale == custom) return names()[scale];
    return Keys::noteName (key) + " " + names()[scale];
}

const juce::StringArray& Voices::names()
{
    static const juce::StringArray n { "Soprano", "Alto", "Tenor", "Baritone", "Bass" };
    return n;
}

float Voices::minHz (int v)
{
    static constexpr float f[] = { 175.0f, 130.0f, 100.0f, 80.0f, 60.0f };
    return f[juce::jlimit (0, count - 1, v)];
}

float Voices::maxHz (int v)
{
    static constexpr float f[] = { 1200.0f, 900.0f, 700.0f, 520.0f, 400.0f };
    return f[juce::jlimit (0, count - 1, v)];
}

} // namespace tune
