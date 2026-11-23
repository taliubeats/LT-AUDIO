#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_dsp/lt_dsp.h>
#include "AirEngine.h"

namespace air
{

namespace IDs
{
    inline constexpr const char* bodyAmount = "bodyAmount";
    inline constexpr const char* bodyModel = "bodyModel";
    inline constexpr const char* midAmount = "midAmount";
    inline constexpr const char* midModel = "midModel";
    inline constexpr const char* highAmount = "highAmount";
    inline constexpr const char* highModel = "highModel";
    inline constexpr const char* link = "link";
    inline constexpr const char* trim = "trim";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* output = "output";
    inline constexpr const char* bypass = "bypass";
    inline constexpr const char* autoGain = "autoGain";

    inline const std::array<const char*, 3> amount { bodyAmount, midAmount, highAmount };
    inline const std::array<const char*, 3> model { bodyModel, midModel, highModel };
    /** Everything a preset stores (bypass stays with the session). */
    inline const std::array<const char*, 10> presetParams { bodyAmount, bodyModel, midAmount, midModel, highAmount, highModel,
                                                            link, trim, mix, output };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
std::vector<lt::presets::ParamPreset> factoryPresets();
void setParam (juce::AudioProcessorValueTreeState&, const juce::String& id, float realValue);

} // namespace air
