#pragma once

#include "Curve.h"

namespace pulsar
{

/** Note lengths of one ducking cycle. */
struct Rates
{
    static constexpr int count = 5;
    static const juce::StringArray& names();
    static double beats (int index) noexcept;       // in quarter notes
    enum { whole = 0, half, quarter, eighth, sixteenth };
};

struct Preset
{
    juce::String name;
    int rate = Rates::quarter;
    float mix = 75.0f;                  // percent
    Curve curve;
    bool factory = true;
    juce::File file;                    // user presets

    juce::var toVar() const;
    static Preset fromVar (const juce::var&);
};

/** Factory presets + user presets saved as .ltpreset files (JSON). */
class PresetLibrary
{
public:
    PresetLibrary();

    static std::vector<Preset> factory();
    static juce::File userFolder();

    void rescan();
    const std::vector<Preset>& all() const noexcept { return presets; }
    int indexOf (const juce::String& name) const;

    /** Saves (or overwrites) a user preset. Returns false on failure. */
    bool save (Preset);
    bool remove (const juce::String& name);

private:
    std::vector<Preset> presets;
};

} // namespace pulsar
