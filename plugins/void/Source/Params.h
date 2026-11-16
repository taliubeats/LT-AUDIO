#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_dsp/lt_dsp.h>
#include "ReverbTypes.h"

namespace lt_void
{

namespace IDs
{
    inline constexpr const char* type = "type";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* preDelay = "predelay";
    inline constexpr const char* preSync = "predelaySync";
    inline constexpr const char* preNote = "predelayNote";
    inline constexpr const char* size = "size";
    inline constexpr const char* decay = "decay";
    inline constexpr const char* highFreq = "highFreq";
    inline constexpr const char* lowFreq = "lowFreq";
    inline constexpr const char* attack = "attack";
    inline constexpr const char* diffusion = "diffusion";
    inline constexpr const char* modRate = "modRate";
    inline constexpr const char* modDepth = "modDepth";
    inline constexpr const char* lowCut = "lowCut";
    inline constexpr const char* highCut = "highCut";
    inline constexpr const char* width = "width";
    inline constexpr const char* duck = "duck";
    inline constexpr const char* freeze = "freeze";
}

struct Notes
{
    static const juce::StringArray& names();
    static double beats (int index);   // quarter notes
};

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** 3 factory presets per reverb type. */
std::vector<lt::presets::ParamPreset> factoryPresets();

} // namespace lt_void
