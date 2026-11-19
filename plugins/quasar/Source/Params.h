#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace quasar
{

// ------------------------------------------------------------------ sizes
inline constexpr int numOscs = 3;          // OSC A, B, C
inline constexpr int numFilters = 2;
inline constexpr int numEnvs = 4;          // ENV 1 (amp), ENV 2, ENV 3, PITCH ENV
inline constexpr int numLfos = 4;
inline constexpr int numMacros = 8;
inline constexpr int numModSlots = 32;
inline constexpr int numBuses = 3;         // MAIN, BUS 1, BUS 2
inline constexpr int numFxSlots = 8;       // per bus
inline constexpr int numFxParams = 8;      // generic parameters of an effect slot
inline constexpr int maxVoices = 32;
inline constexpr int maxUnison = 16;

/** Parameter IDs. Every ID is a plain string so presets stay readable JSON. */
namespace IDs
{
    juce::String osc (int o, const char* what);      // "oscA_wtpos"
    juce::String flt (int f, const char* what);      // "flt1_cutoff"
    juce::String env (int e, const char* what);      // "env1_att", "envP_att"
    juce::String lfo (int l, const char* what);      // "lfo1_rate"
    juce::String macro (int m);                      // "macro1"
    juce::String mod (int s, const char* what);      // "mod1_amt"
    juce::String bus (int b, const char* what);      // "busM_level", "bus1_level"
    juce::String fx (int b, int slot, const char* what);  // "fxM1_type", "fx12_p3"
    juce::String fxParam (int b, int slot, int p);   // "fxM1_p1".."p8"
    juce::String mixer (int channel, const char* what);   // channel 0..6 = SUB A B C NOISE F1 F2

    inline constexpr const char* subOn = "sub_on", *subShape = "sub_shape", *subOct = "sub_oct", *subLevel = "sub_level", *subPan = "sub_pan";
    inline constexpr const char* noiseOn = "noise_on", *noiseType = "noise_type", *noiseColor = "noise_color", *noiseLevel = "noise_level", *noisePan = "noise_pan";
    inline constexpr const char* envPAmount = "envP_amt";
    inline constexpr const char* ampVel = "amp_vel";
    inline constexpr const char* voiceMode = "voice_mode", *glide = "glide", *glideLegato = "glide_legato";
    inline constexpr const char* poly = "poly", *stack = "stack", *steal = "steal", *priority = "priority", *mpe = "mpe";
    inline constexpr const char* quality = "quality", *oversampling = "oversampling";
    inline constexpr const char* masterTune = "master_tune", *transpose = "transpose", *pbUp = "pb_up", *pbDown = "pb_down", *scale = "scale";
    inline constexpr const char* drift = "drift", *warmth = "warmth";
    inline constexpr const char* limiter = "limiter", *master = "master";
    inline constexpr const char* arpOn = "arp_on", *arpMode = "arp_mode", *arpRate = "arp_rate", *arpOct = "arp_oct", *arpGate = "arp_gate",
                                 *arpSwing = "arp_swing", *arpPattern = "arp_pattern", *arpLatch = "arp_latch";
}

/** Labels of every choice parameter. */
namespace Choices
{
    const juce::StringArray& oscTypes();       // WAVETABLE ANALOG SAMPLE
    const juce::StringArray& warpModes();
    const juce::StringArray& sourceDests();    // FILTER 1, FILTER 2, FILTER 1+2, MAIN, BUS 1, BUS 2
    const juce::StringArray& filterDests();    // FILTER 2 / MAIN / BUS 1 / BUS 2  (filter 2 has no FILTER 2)
    const juce::StringArray& filterTypes();
    const juce::StringArray& subShapes();
    const juce::StringArray& noiseTypes();
    const juce::StringArray& lfoModes();       // FREE RETRIG ENV
    const juce::StringArray& divisions();      // 8/1 .. 1/64
    double divisionBeats (int index);
    const juce::StringArray& voiceModes();     // POLY MONO LEGATO
    const juce::StringArray& stealModes();
    const juce::StringArray& priorities();
    const juce::StringArray& qualities();      // ECO HIGH ULTRA
    const juce::StringArray& oversamplings();  // 2x 4x
    const juce::StringArray& scales();
    const juce::StringArray& arpModes();
    const juce::StringArray& arpPatterns();
    const juce::StringArray& modCurves();
    const juce::StringArray& busNames();       // MAIN BUS 1 BUS 2
    const juce::StringArray& mixerChannels();  // SUB OSC A OSC B OSC C NOISE FILTER 1 FILTER 2
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** Mixer channel indices (also used by the engine streams). */
enum MixerChannel { chSub = 0, chOscA, chOscB, chOscC, chNoise, chFlt1, chFlt2, numMixerChannels };

} // namespace quasar
