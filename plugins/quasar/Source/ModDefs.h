#pragma once

#include "Params.h"
#include <juce_graphics/juce_graphics.h>

namespace quasar
{

/** Modulation sources. 0 = none. */
enum ModSource
{
    srcNone = 0,
    srcEnv1, srcEnv2, srcEnv3, srcEnvP,
    srcLfo1, srcLfo2, srcLfo3, srcLfo4,
    srcMacro1, srcMacro2, srcMacro3, srcMacro4, srcMacro5, srcMacro6, srcMacro7, srcMacro8,
    srcVelocity, srcNote, srcModWheel, srcAftertouch, srcPitchBend, srcRandom, srcSlide,
    numModSources
};

struct ModDest
{
    juce::String id;        // parameter ID
    juce::String name;      // "OSC A WT POS"
    bool perVoice;          // evaluated per voice (else once per block, from the newest voice)
};

namespace ModDefs
{
    const juce::StringArray& sourceNames();             // "NONE", "ENV 1", ...
    juce::Colour sourceColour (int source);
    bool isBipolarByNature (int source);                // pitch bend, note
    bool isVoiceSource (int source);                    // envelopes, velocity, note, random...

    /** Every modulatable parameter. Index 0 is "NONE"; the order never changes (presets store indices). */
    const std::vector<ModDest>& destinations();
    int destIndex (const juce::String& paramId);        // 0 when not modulatable
    juce::StringArray destNames();
}

} // namespace quasar
