#pragma once

#include "Kit.h"

namespace eclipse
{

struct CloneSettings
{
    float sensitivity = 0.5f;     // 0..1: more = more (and softer) hits
    bool ai = false;              // analyse a separated drum stem instead of the full mix
    bool splitKits = true;        // a song with several kits: one bank per kit
    bool useClassifier = true;    // v2: the trained network decides the families (false = the v1 rules)
    std::array<std::shared_ptr<juce::AudioBuffer<float>>, 4> parts {};   // v3: kick, snare, cymbals, toms of the drum stem
    std::shared_ptr<juce::AudioBuffer<float>> drumStem;   // v2: the AI (neural) drum stem; with ai and no stem, the DSP stem is used
};

/** Result of cloning a source: the analysis, the 64 pads and the patterns. */
struct CloneResult
{
    std::shared_ptr<Analysis> analysis;
    std::array<PadSound, numPads> pads;
    std::vector<int> padOfGroup;  // group -> pad (-1 = not on a pad)
    std::shared_ptr<juce::AudioBuffer<float>> extractFrom;   // the audio the one-shots come from (mix or drum stem)
    std::array<std::shared_ptr<juce::AudioBuffer<float>>, 3> parts {};   // v3: kick / snare / cymbals parts (groups with a source use them)
};

/**
    Turns a song, loop or one-shot into a drum kit. Offline; runs on a background thread.

    1. Detect: spectral flux of the percussive part (harmonic/percussive split), in three regions
       (lows, mids, highs) so a kick and a hat at the same time are two hits.
    2. Classify: new energy per region, noisiness, decay and clap "flams".
    3. Group: hits with the same spectral fingerprint are one sound.
    4. Isolate: CLEAN = median of the aligned hits of a group (what repeats is the drum; voice,
       bass and chords change from hit to hit and fall out).
    Then tempo, downbeat and the repeating patterns as MIDI.
*/
class CloneEngine
{
public:
    using Progress = std::function<void (float, const juce::String&)>;

    static CloneResult clone (const juce::AudioBuffer<float>& source, double sampleRate, const CloneSettings&,
                              const std::atomic<bool>* cancel = nullptr, Progress progress = {});

    /** Makes the raw and clean one-shot of hit #hitIndex of a group. */
    static PadSound makePad (const juce::AudioBuffer<float>& from, const Analysis&, int group, int hitIndex);

    /** Harmonic/percussive split: returns the percussive part (the "drum stem"). */
    static std::shared_ptr<juce::AudioBuffer<float>> percussiveStem (const juce::AudioBuffer<float>&, double sampleRate,
                                                                     const std::atomic<bool>* cancel = nullptr);

    /** Loads any audio file as a float buffer (stereo kept). */
    static std::shared_ptr<juce::AudioBuffer<float>> load (const juce::File&, double& sampleRate, juce::String& error);
};

} // namespace eclipse
