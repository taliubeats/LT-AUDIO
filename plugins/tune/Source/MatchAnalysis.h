#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <functional>
#include <vector>

namespace tune
{

/**
    MATCH REFERENCE: analyses a song (or a vocal stem) and estimates the tuning settings used on its voice.

    Vocal isolation (first version): the centre channel band-limited to the voice range - lead vocals are
    almost always panned to the centre. A vocal stem gives the most exact result.
    From the pitch curve it estimates key & scale, retune speed (how fast notes snap), humanize,
    flex tune, vibrato (kept / flattened) and a formant offset.
*/
struct MatchResult
{
    bool ok = false;
    juce::String error, fileName;
    double seconds = 0.0;

    int key = 9, scale = 2;            // A minor
    float keyConfidence = 0.0f;        // 0..1
    float retuneMs = 20.0f;
    bool hard = false;                 // HARD (fast, robotic) or SOFT
    float humanize = 0.0f;             // 0..100
    float flex = 0.0f;                 // 0..100
    float vibrato = 0.0f;              // -100..100 (negative = flattened)
    float vibratoDepthCents = 0.0f;
    float formant = 0.0f;              // semitones
    float voicedFraction = 0.0f;

    static constexpr double hopSeconds = 0.005;
    std::vector<float> pitch;          // MIDI per hop, 0 = no voice
    std::vector<float> envelope;       // 0..1 peak per hop (waveform display)
};

struct MatchAnalysis
{
    /** Analyses audio already in memory (any channel count). shouldExit lets a background thread stop early. */
    static MatchResult analyse (const juce::AudioBuffer<float>&, double sampleRate,
                                std::function<bool()> shouldExit = {});

    /** Reads a file (WAV, AIFF, FLAC, OGG, MP3) - at most the first 6 minutes - and analyses it. */
    static MatchResult analyseFile (const juce::File&, std::function<bool()> shouldExit = {});

    /** Estimate the key from a pitch-class histogram (Krumhansl profiles). Returns confidence 0..1. */
    static float estimateKey (const std::array<double, 12>& histogram, int& key, int& scale);

    static juce::String vibratoText (float vibrato)  { return vibrato < -10.0f ? "Flattened" : vibrato > 10.0f ? "Added" : "Kept"; }
};

} // namespace tune
