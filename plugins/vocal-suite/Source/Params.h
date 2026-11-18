#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_dsp/lt_dsp.h>
#include "VocalChain.h"

namespace vocal
{

/** One parameter of the chain, described once and used by the host, the UI, presets and the AI assistant. */
struct ParamInfo
{
    const char* id;
    const char* name;
    const char* module;       // TUNE, CLEANUP, TONE, DYNAMICS, SPACE, FX, OUTPUT
    float min, max, def;
    const char* unit;         // "%", "dB", "Hz", "ms", "s", "" ; "choice" / "bool" for those kinds
    const char* help;         // what it does (shown to the AI and as a tooltip)
};

const std::vector<ParamInfo>& paramInfos();
const ParamInfo* findParam (const juce::String& id);

namespace IDs
{
    inline constexpr const char* tuneOn = "tuneOn";
    inline constexpr const char* key = "key";
    inline constexpr const char* scale = "scale";
    inline constexpr const char* voice = "voice";
    inline constexpr const char* retune = "retune";
    inline constexpr const char* humanize = "humanize";
    inline constexpr const char* flex = "flex";
    inline constexpr const char* tuneMix = "tuneMix";
    inline constexpr const char* lowCut = "lowCut";
    inline constexpr const char* gate = "gate";
    inline constexpr const char* deEss = "deEss";
    inline constexpr const char* deEssFreq = "deEssFreq";
    inline constexpr const char* body = "body";
    inline constexpr const char* mud = "mud";
    inline constexpr const char* presence = "presence";
    inline constexpr const char* air = "air";
    inline constexpr const char* comp = "comp";
    inline constexpr const char* attack = "attack";
    inline constexpr const char* release = "release";
    inline constexpr const char* satType = "satType";
    inline constexpr const char* satDrive = "satDrive";
    inline constexpr const char* satMix = "satMix";
    inline constexpr const char* doubler = "doubler";
    inline constexpr const char* phone = "phone";
    inline constexpr const char* width = "width";
    inline constexpr const char* delayOn = "delayOn";
    inline constexpr const char* delayNote = "delayNote";
    inline constexpr const char* delayMode = "delayMode";
    inline constexpr const char* delayFeedback = "delayFeedback";
    inline constexpr const char* delayMix = "delayMix";
    inline constexpr const char* reverbOn = "reverbOn";
    inline constexpr const char* reverbType = "reverbType";
    inline constexpr const char* reverbDecay = "reverbDecay";
    inline constexpr const char* reverbPre = "reverbPre";
    inline constexpr const char* reverbMix = "reverbMix";
    inline constexpr const char* output = "output";
    inline constexpr const char* bypass = "bypass";
}

struct DelayNotes
{
    static const juce::StringArray& names();       // 1/16 .. 1/2 (with dotted)
    static double beats (int index);
};

/** Choice lists for "choice" parameters. */
juce::StringArray choicesFor (const juce::String& id);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** Factory presets = vocal styles. Also used by the offline assistant. */
std::vector<lt::presets::ParamPreset> factoryPresets();

} // namespace vocal
