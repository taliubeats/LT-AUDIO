#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_dsp/lt_dsp.h>
#include "Scales.h"

namespace tune
{

namespace IDs
{
    inline constexpr const char* key = "key";
    inline constexpr const char* scale = "scale";
    inline constexpr const char* edited = "edited";      // EDIT NOTES changed the scale's notes
    inline constexpr const char* voice = "voice";
    inline constexpr const char* retune = "retune";
    inline constexpr const char* humanize = "humanize";
    inline constexpr const char* flexTune = "flexTune";
    inline constexpr const char* vibrato = "vibrato";
    inline constexpr const char* formant = "formant";
    inline constexpr const char* preserve = "preserve";
    inline constexpr const char* transpose = "transpose";
    inline constexpr const char* detune = "detune";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* mode = "mode";
    inline constexpr const char* bypass = "bypass";

    /** "note0" (C) ... "note11" (B) */
    juce::String note (int pitchClass);
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
std::vector<lt::presets::ParamPreset> factoryPresets();

/** Notes the engine may tune to: the key & scale, or the EDIT NOTES selection. */
NoteMask effectiveMask (juce::AudioProcessorValueTreeState&);

/** Sets key/scale and clears the EDIT NOTES changes (as one undoable gesture each). */
void setKeyScale (juce::AudioProcessorValueTreeState&, int key, int scale);

/** EDIT NOTES click: toggles one note (starting from the scale's notes the first time). */
void toggleNote (juce::AudioProcessorValueTreeState&, int pitchClass);

/** ↺: back to the notes of the key & scale. */
void resetNotes (juce::AudioProcessorValueTreeState&);

void setParam (juce::AudioProcessorValueTreeState&, const juce::String& id, float realValue);

} // namespace tune
