#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace typebeats
{

struct Chord
{
    int root = -1;          // 0 = C ... 11 = B, -1 = no chord
    int quality = 0;        // index into qualities()

    bool isValid() const noexcept { return root >= 0; }
    bool operator== (const Chord& o) const noexcept { return root == o.root && quality == o.quality; }
    bool operator!= (const Chord& o) const noexcept { return ! (*this == o); }

    std::vector<int> pitchClasses() const;
    juce::String name (bool preferFlats) const;

    struct Quality { const char* suffix; std::vector<int> intervals; float bias; };
    static const std::vector<Quality>& qualities();
};

struct ChordSegment
{
    double start = 0, end = 0;      // seconds
    Chord chord;
    std::vector<Chord> alternatives;
};

struct KeyCandidate
{
    int tonic = 0;           // 0..11
    bool minor = false;
    int percent = 0;
    juce::String name (bool shortForm = false) const;
    bool prefersFlats() const;
};

struct AnalysisResult
{
    double duration = 0;
    double bpm = 0;
    double firstBeat = 0;               // seconds
    std::vector<KeyCandidate> keys;      // best first
    std::vector<ChordSegment> chords;
    std::vector<float> peaks;            // waveform overview, 0..1, fixed number of points

    bool isValid() const noexcept { return duration > 0; }
    bool prefersFlats() const { return ! keys.empty() && keys.front().prefersFlats(); }

    juce::var toVar() const;
    static AnalysisResult fromVar (const juce::var&);
};

/** Offline analysis of a whole song: tempo, key and chord progression. */
class Analyzer
{
public:
    /** `shouldExit` lets the background thread abort. Progress 0..1 is reported through `progress`. */
    static AnalysisResult analyse (const juce::AudioBuffer<float>& audio, double sampleRate,
                                   std::function<bool()> shouldExit = {},
                                   std::function<void (float)> progress = {});

    static juce::String pitchName (int pc, bool flats);
};

} // namespace typebeats
