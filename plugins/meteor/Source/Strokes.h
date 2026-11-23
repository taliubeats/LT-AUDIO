#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace meteor
{

struct Strokes
{
    enum { reverbTail = 0, delayOnly, tapeStop, pitchDown, distortion, reverse, reverseReverb, stutter, count };
    static const juce::StringArray& names();          // "REVERB TAIL" ...
    static const juce::StringArray& fileNames();      // "Reverb Tail" ... (for exported WAVs)
};

struct StrokeSettings
{
    double sampleRate = 48000.0;
    double bpm = 120.0;
    float length = 0.5f;       // 0..1: tail length, tape-stop time, glide time, delay time...
    float intensity = 0.5f;    // 0..1: feedback, drive, depth...
    float tone = 0.0f;         // -1 dark .. +1 bright
};

/**
    Offline renders of a source sound ("strokes" ready to drag into the DAW).
    Pure functions: safe to call from any background thread. Output is stereo,
    peak-normalised to -1 dBFS and faded out at the end.
*/
struct StrokeRenderer
{
    static juce::AudioBuffer<float> render (int stroke, const juce::AudioBuffer<float>& source, const StrokeSettings&);

    static constexpr double maxSeconds = 90.0;    // hard limit of any render

    // individual effects (exposed for the tests)
    static juce::AudioBuffer<float> reverbWet (const juce::AudioBuffer<float>&, const StrokeSettings&);
    static juce::AudioBuffer<float> delayWet (const juce::AudioBuffer<float>&, const StrokeSettings&);
    static juce::AudioBuffer<float> tapeStop (const juce::AudioBuffer<float>&, const StrokeSettings&);
    static juce::AudioBuffer<float> pitchDown (const juce::AudioBuffer<float>&, const StrokeSettings&);
    static juce::AudioBuffer<float> distort (const juce::AudioBuffer<float>&, const StrokeSettings&);
    static juce::AudioBuffer<float> reversed (const juce::AudioBuffer<float>&);
    static juce::AudioBuffer<float> stutterRoll (const juce::AudioBuffer<float>&, const StrokeSettings&);

    static void normalise (juce::AudioBuffer<float>&, float peakDb = -1.0f);
    static void trimSilentEnd (juce::AudioBuffer<float>&, float thresholdDb, double sampleRate);
    static void fadeEdges (juce::AudioBuffer<float>&, double sampleRate, double fadeInMs, double fadeOutMs);
    static double delayBeats (float length);      // the musical delay time chosen by LENGTH
};

} // namespace meteor
