#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "PitchDetector.h"
#include "Scales.h"
#include <atomic>
#include <deque>

namespace tune
{

struct TuneSettings
{
    NoteMask mask {};            // allowed notes (none = chromatic)
    int voice = Voices::tenor;
    bool hq = false;             // MODE: LOW LATENCY | HQ
    float retuneMs = 20.0f;      // 0 = robot
    float humanize = 0.0f;       // 0..1
    float flex = 0.0f;           // 0..1
    float vibrato = 0.0f;        // -1 (flatten) .. +1 (add)
    float formant = 0.0f;        // semitones
    bool preserve = true;        // formant preservation
    float transpose = 0.0f;      // semitones
    float refHz = 440.0f;        // DETUNE
    float mix = 1.0f;            // 0..1
    bool bypass = false;
};

/**
    LT TUNE's real-time engine:
      1. pitch detection (YIN) on the mono sum,
      2. correction: target note from the allowed notes + retune speed (glide), humanize, flex tune, vibrato,
      3. pitch shifting with formant preservation (TD-PSOLA: two-period grains re-spaced at the new period).
    The latency is fixed for a given voice and mode and is reported by getLatency().
*/
class TuneEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize, int voice, bool hq);
    void reset();
    void process (juce::AudioBuffer<float>&, const TuneSettings&);

    /** True when voice/mode differ from the ones the engine was prepared with (latency changes). */
    bool needsReconfigure (const TuneSettings& s) const noexcept { return s.voice != voice || s.hq != hq; }
    int getLatency() const noexcept                 { return latency; }
    static int latencyFor (double sampleRate, int voice, bool hq);

    // ---- read by the UI (any thread)
    float getInputLevel() const noexcept            { return inLevel.load(); }
    float getOutputLevel() const noexcept           { return outLevel.load(); }
    bool isVoiced() const noexcept                  { return uiVoiced.load(); }
    float getInputMidi() const noexcept             { return uiInputMidi.load(); }   // sung pitch (fractional MIDI)
    int getTargetNote() const noexcept              { return uiTarget.load(); }       // MIDI note it is tuned to
    float getOutputMidi() const noexcept            { return uiOutputMidi.load(); }
    float getCorrection() const noexcept            { return uiCorrection.load(); }   // semitones applied (no transpose)
    int getRetuneCount() const noexcept             { return retunes.load(); }        // increments on every new corrected note

private:
    struct Mark { double pos, period; };

    void analyseHop (const TuneSettings&);
    void placeGrain (double s, const Mark& a, double rf, double radius, int numChannels);
    float readInput (int ch, double pos) const noexcept;

    double sr = 44100.0;
    int voice = -1;
    bool hq = false;
    int latency = 0, hop = 128, hopCounter = 0, maxPeriod = 512, margin = 0;
    PitchDetector detector;

    static constexpr int maxCh = 2;
    std::vector<float> in[maxCh], acc[maxCh], wsum;
    int mask = 0;
    juce::int64 now = 0;   // samples written

    // marks
    std::deque<Mark> marks;
    double nextMark = 0.0, nextSynth = 0.0;

    // correction state (updated every hop)
    float periodNow = 256.0f, unvoicedPeriod = 220.0f;
    double ratio = 1.0, formantRatio = 1.0;
    bool voiced = false;
    int unvoicedHops = 0, target = -1;
    float lastHz = 0.0f, trend = 0.0f, corr = 0.0f, noteAge = 0.0f, vibPhase = 0.0f, voicedAge = 0.0f;

    juce::SmoothedValue<float> mixS;
    float inPeak = 0.0f, outPeak = 0.0f;

    std::atomic<float> inLevel { 0 }, outLevel { 0 }, uiInputMidi { 0 }, uiOutputMidi { 0 }, uiCorrection { 0 };
    std::atomic<int> uiTarget { -1 }, retunes { 0 };
    std::atomic<bool> uiVoiced { false };
};

} // namespace tune
