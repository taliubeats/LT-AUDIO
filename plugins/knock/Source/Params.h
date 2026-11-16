#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_dsp/lt_dsp.h>
#include "KnockEngine.h"

namespace knock
{

namespace IDs
{
    inline constexpr const char* knock = "knock";
    inline constexpr const char* mode = "mode";          // DRUMS | MELODY
    inline constexpr const char* input = "input";
    inline constexpr const char* output = "output";
    inline constexpr const char* punch = "punch";
    inline constexpr const char* saturation = "saturation";
    inline constexpr const char* distortion = "distortion";
    inline constexpr const char* distType = "distType";
    inline constexpr const char* bright = "bright";
    inline constexpr const char* low = "low";
    inline constexpr const char* glue = "glue";
    inline constexpr const char* width = "width";
    inline constexpr const char* crush = "crush";
    inline constexpr const char* lofi = "lofi";
    inline constexpr const char* stutter = "stutter";
    inline constexpr const char* filter = "filter";
    inline constexpr const char* ring = "ring";
    inline constexpr const char* reverse = "reverse";
    inline constexpr const char* reverb = "reverb";
    inline constexpr const char* delay = "delay";
    inline constexpr const char* delayTime = "delayTime";
}

/** The three CHAOS sections, with the parameters CHAOS may change and their musical ranges. */
struct ChaosRange { const char* id; float lo, hi; };
const std::vector<ChaosRange>& chaosTone();
const std::vector<ChaosRange>& chaosFx();
const std::vector<ChaosRange>& chaosSpace();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
std::vector<lt::presets::ParamPreset> factoryPresets();

} // namespace knock
