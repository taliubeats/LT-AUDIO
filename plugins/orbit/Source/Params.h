#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_dsp/lt_dsp.h>
#include "OrbitEngine.h"

namespace orbit
{

namespace IDs
{
    inline constexpr const char* mode = "mode";
    inline constexpr const char* sync = "sync";
    inline constexpr const char* note = "note";
    inline constexpr const char* ms = "ms";
    inline constexpr const char* feel = "feel";
    inline constexpr const char* feedback = "feedback";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* freeze = "freeze";
    inline constexpr const char* lowCut = "lowCut";
    inline constexpr const char* highCut = "highCut";
    inline constexpr const char* width = "width";
    inline constexpr const char* duck = "duck";
    inline constexpr const char* wobble = "wobble";
    inline constexpr const char* drive = "drive";
}

struct Notes
{
    static const juce::StringArray& names();          // 1/64 .. 1/1
    static double beats (int index, int feel);        // feel: 0 normal, 1 dotted, 2 triplet
};

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
std::vector<lt::presets::ParamPreset> factoryPresets();

} // namespace orbit
