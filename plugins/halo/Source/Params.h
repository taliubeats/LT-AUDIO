#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_dsp/lt_dsp.h>
#include "engine/Layers.h"

namespace halo
{

constexpr int numStrips = 8;

namespace IDs
{
    // global
    inline constexpr const char* dry = "dry";
    inline constexpr const char* wet = "wet";
    inline constexpr const char* width = "width";
    inline constexpr const char* tightness = "tightness";
    inline constexpr const char* quality = "quality";         // 0 FAST, 1 BEST
    inline constexpr const char* withLead = "withLead";       // preview: harmonies solo / with lead
    inline constexpr const char* keyAuto = "keyAuto";         // use the detected key
    inline constexpr const char* key = "key";                 // 0..11
    inline constexpr const char* scale = "scale";             // 0 Major, 1 Minor
    inline constexpr const char* refuerzoWords = "refWords";  // 1..3
    inline constexpr const char* bypass = "bypass";

    // choir
    inline constexpr const char* cVoices = "cVoices";
    inline constexpr const char* cType = "cType";
    inline constexpr const char* cHarmony = "cHarmony";
    inline constexpr const char* cSop = "cSop";
    inline constexpr const char* cAlto = "cAlto";
    inline constexpr const char* cTenor = "cTenor";
    inline constexpr const char* cBass = "cBass";
    inline constexpr const char* cSpread = "cSpread";
    inline constexpr const char* cLoose = "cLoose";
    inline constexpr const char* cWidth = "cWidth";
    inline constexpr const char* cRoom = "cRoom";
    inline constexpr const char* cBreath = "cBreath";
    inline constexpr const char* cLevel = "cLevel";
    inline constexpr const char* cSeparate = "cSeparate";

    // per strip: "s3_part" etc.
    juce::String strip (int index, const char* name);
    inline constexpr const char* on = "on";
    inline constexpr const char* part = "part";
    inline constexpr const char* smart = "smart";
    inline constexpr const char* style = "style";
    inline constexpr const char* voice = "voice";
    inline constexpr const char* level = "level";
    inline constexpr const char* pan = "pan";
    inline constexpr const char* delay = "delay";
    inline constexpr const char* humanize = "hum";
    inline constexpr const char* mute = "mute";
    inline constexpr const char* solo = "solo";
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** All factory presets (HARMONY sets + CHOIR types). */
std::vector<lt::presets::ParamPreset> factoryPresets();

/** Sets a parameter to a real value as one undoable gesture. */
void setParam (juce::AudioProcessorValueTreeState&, const juce::String& id, float realValue);
float getParam (juce::AudioProcessorValueTreeState&, const juce::String& id);

/** When a strip's part changes: apply that part's default style / voice / humanize / delay / pan / level. */
void applyPartDefaults (juce::AudioProcessorValueTreeState&, int strip, Part);

/** When the choir type changes: apply its defaults (voices, harmony, sections, knobs). */
void applyChoirDefaults (juce::AudioProcessorValueTreeState&, ChoirType);

juce::StringArray partNames();
juce::StringArray styleNames();
juce::StringArray voiceNames();

} // namespace halo
