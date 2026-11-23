#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_dsp/lt_dsp.h>
#include "MeteorEngine.h"

namespace meteor
{

namespace IDs
{
    inline constexpr const char* revOn = "revOn";
    inline constexpr const char* revSize = "revSize";
    inline constexpr const char* revMix = "revMix";
    inline constexpr const char* dblOn = "dblOn";
    inline constexpr const char* dblVoices = "dblVoices";
    inline constexpr const char* dblDetune = "dblDetune";
    inline constexpr const char* dblWidth = "dblWidth";
    inline constexpr const char* dblMix = "dblMix";
    inline constexpr const char* chopOn = "chopOn";
    inline constexpr const char* chopRate = "chopRate";
    inline constexpr const char* chopPattern = "chopPattern";
    inline constexpr const char* chopDepth = "chopDepth";
    inline constexpr const char* glitchOn = "glitchOn";
    inline constexpr const char* glitchRate = "glitchRate";
    inline constexpr const char* glitchChance = "glitchChance";
    inline constexpr const char* glitchRepeats = "glitchRepeats";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* output = "output";
    inline constexpr const char* strokeLength = "strokeLength";
    inline constexpr const char* strokeIntensity = "strokeIntensity";
    inline constexpr const char* strokeTone = "strokeTone";

    inline const std::array<const char*, 18> live { revOn, revSize, revMix, dblOn, dblVoices, dblDetune, dblWidth, dblMix,
                                                     chopOn, chopRate, chopPattern, chopDepth, glitchOn, glitchRate, glitchChance,
                                                     glitchRepeats, mix, output };
    inline const std::array<const char*, 3> strokes { strokeLength, strokeIntensity, strokeTone };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
std::vector<lt::presets::ParamPreset> factoryPresets();

} // namespace meteor
