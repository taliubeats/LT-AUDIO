#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Kit.h"

namespace eclipse
{

/** Per-pad parameters. Every pad has all of them, all automatable: "p<pad>_<name>". */
enum class PadParam { gain = 0, pan, tune, attack, fade, hp, lp, clean, vibe, choke, out, mute, solo, start, end };
constexpr int numPadParams = 15;
const char* padParamName (PadParam);                    // "gain"...
juce::String padParamId (int pad, PadParam);            // "p12_gain"

namespace Outs { enum { main = 0, mainAndFamily, familyOnly }; }

namespace IDs
{
    inline constexpr const char* output = "output";       // dB
    inline constexpr const char* sensitivity = "sensitivity";
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** Fast access to the raw values of a pad. */
struct PadParamPtrs
{
    std::array<std::atomic<float>*, numPadParams> v {};
    float get (PadParam p) const { return v[(size_t) p]->load(); }
};

} // namespace eclipse
