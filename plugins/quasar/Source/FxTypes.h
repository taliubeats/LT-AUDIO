#pragma once

#include <juce_core/juce_core.h>

namespace quasar
{

/** The effects that can go in a bus slot (index 0 = empty slot). */
enum class FxType
{
    empty = 0, saturation, compressor, chorus, flanger, phaser, delay, reverb, equalizer, filter,
    distortion, bitcrush, tape, stereoWidth, utility, count
};

/**
    Description of the 8 generic knobs of an effect slot. A slot stores p1..p8 normalised (0..1);
    each effect maps them to real values with this table, so the host, the UI and the DSP agree.
*/
struct FxParamSpec
{
    const char* name = "";
    float min = 0, max = 1, def = 0.5f;
    enum Kind { linear, log, choice, toggle } kind = linear;
    const char* unit = "";
    juce::StringArray choices = {};

    float toReal (float norm) const noexcept
    {
        norm = juce::jlimit (0.0f, 1.0f, norm);
        switch (kind)
        {
            case log:    return min * std::pow (max / min, norm);
            case choice: return (float) juce::jlimit (0, juce::jmax (0, choices.size() - 1), (int) std::floor (norm * (float) choices.size() * 0.9999f));
            case toggle: return norm >= 0.5f ? 1.0f : 0.0f;
            case linear: default: return min + (max - min) * norm;
        }
    }
    float toNorm (float real) const noexcept
    {
        switch (kind)
        {
            case log:    return juce::jlimit (0.0f, 1.0f, std::log (juce::jmax (min, real) / min) / std::log (max / min));
            case choice: return choices.size() <= 1 ? 0.0f : ((float) juce::roundToInt (real) + 0.5f) / (float) choices.size();
            case toggle: return real >= 0.5f ? 1.0f : 0.0f;
            case linear: default: return juce::jlimit (0.0f, 1.0f, (real - min) / (max - min));
        }
    }
    juce::String format (float norm) const;
};

struct FxInfo
{
    const char* name;              // "REVERB"
    int numParams;                 // used knobs (<= 8)
    std::array<FxParamSpec, 8> params;
};

const FxInfo& fxInfo (int type);
const juce::StringArray& fxNames();    // "EMPTY", "SATURATION", ...

} // namespace quasar
