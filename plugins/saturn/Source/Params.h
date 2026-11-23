#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_dsp/lt_dsp.h>
#include "SaturnEngine.h"

namespace saturn
{

namespace IDs
{
    inline constexpr const char* tape = "tape";
    inline constexpr const char* age = "age";
    inline constexpr const char* wow = "wow";
    inline constexpr const char* flutter = "flutter";
    inline constexpr const char* drift = "drift";
    inline constexpr const char* saturation = "saturation";
    inline constexpr const char* tone = "tone";
    inline constexpr const char* bandwidth = "bandwidth";
    inline constexpr const char* hiss = "hiss";
    inline constexpr const char* ensemble = "ensemble";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* output = "output";

    inline const std::array<const char*, 12> all { tape, age, wow, flutter, drift, saturation, tone, bandwidth, hiss, ensemble, mix, output };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
std::vector<lt::presets::ParamPreset> factoryPresets();

} // namespace saturn
