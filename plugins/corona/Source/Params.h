#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_dsp/lt_dsp.h>

namespace corona
{

namespace IDs
{
    inline constexpr const char* input     = "input";
    inline constexpr const char* threshold = "threshold";
    inline constexpr const char* attack    = "attack";
    inline constexpr const char* release   = "release";
    inline constexpr const char* debump    = "debump";
    inline constexpr const char* filter    = "filter";
    inline constexpr const char* makeup    = "makeup";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* power     = "power";
    inline constexpr const char* autoGain  = "autoGain";
    inline constexpr const char* mode      = "mode";
    inline constexpr const char* ovs       = "ovs";
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
std::vector<lt::presets::ParamPreset> factoryPresets();

} // namespace corona
