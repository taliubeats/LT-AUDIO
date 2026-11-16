#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_dsp/lt_dsp.h>
#include "NebulaEngine.h"

namespace nebula
{

namespace IDs
{
    // macros
    inline constexpr const char* motion = "motion";
    inline constexpr const char* texture = "texture";
    inline constexpr const char* pitch = "pitch";
    inline constexpr const char* space = "space";
    inline constexpr const char* dirt = "dirt";
    inline constexpr const char* mix = "mix";
    // pitch step
    inline constexpr const char* stepMode = "stepMode";
    inline constexpr const char* stepRate = "stepRate";
    inline constexpr const char* glide = "glide";
    inline constexpr const char* chance = "chance";
    inline constexpr const char* stepAmount = "stepAmount";
    inline constexpr const char* scaleLock = "scaleLock";
    inline constexpr const char* scaleKey = "scaleKey";
    // tape stop
    inline constexpr const char* tapeEvery = "tapeEvery";
    inline constexpr const char* tapeLength = "tapeLength";
    inline constexpr const char* tapeCurve = "tapeCurve";
    inline constexpr const char* tapeStart = "tapeStart";
    inline constexpr const char* tapeChance = "tapeChance";
    inline constexpr const char* tapePosition = "tapePosition";
    inline constexpr const char* tapeTrigger = "tapeTrigger";
    // grain engine
    inline constexpr const char* size = "size";
    inline constexpr const char* density = "density";
    inline constexpr const char* position = "position";
    inline constexpr const char* spray = "spray";
    inline constexpr const char* grainPitch = "grainPitch";
    inline constexpr const char* stereo = "stereo";
    inline constexpr const char* shape = "shape";
    inline constexpr const char* reverse = "reverse";
    inline constexpr const char* freeze = "freeze";
    // grain delay
    inline constexpr const char* gdTime = "gdTime";
    inline constexpr const char* gdFeedback = "gdFeedback";
    inline constexpr const char* gdHighPass = "gdHP";
    inline constexpr const char* gdLowPass = "gdLP";
    inline constexpr const char* gdMix = "gdMix";
    // fx + gain
    inline constexpr const char* particles = "fxParticles";
    inline constexpr const char* cloud = "fxCloud";
    inline constexpr const char* reverseFx = "fxReverse";
    inline constexpr const char* shimmer = "fxShimmer";
    inline constexpr const char* input = "input";
    inline constexpr const char* output = "output";

    /** step1 .. step8 */
    juce::String step (int i);
}

struct Choices
{
    static const juce::StringArray& stepModes();   // OFF +5 +7 +12 -12 CUSTOM
    static const juce::StringArray& stepRates();   // 1/16 .. 4 BAR
    static double stepRateBeats (int);
    static const juce::StringArray& tapeEvery();   // OFF 1 BAR 2 BAR 4 BAR
    static double tapeEveryBeats (int);
    static const juce::StringArray& scales();
    static const juce::StringArray& keys();
};

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** LT COSMOS: 50 factory presets. group = tags, comma separated (e.g. "PAD,KEYS"). */
std::vector<lt::presets::ParamPreset> factoryPresets();

} // namespace nebula
