#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_dsp/lt_dsp.h>
#include "NovaEngine.h"

namespace nova
{

namespace IDs
{
    inline constexpr const char* type = "type";
    inline constexpr const char* drive = "drive";
    inline constexpr const char* tone = "tone";
    inline constexpr const char* warmth = "warmth";
    inline constexpr const char* air = "air";
    inline constexpr const char* character = "character";
    inline constexpr const char* input = "input";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* output = "output";
    inline constexpr const char* autoGain = "autoGain";
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
std::vector<lt::presets::ParamPreset> factoryPresets();

} // namespace nova
