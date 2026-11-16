#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <lt_dsp/lt_dsp.h>

namespace knock
{

struct DistTypes { enum { soft = 0, hard, fuzz, tube }; static const juce::StringArray& names(); };
struct DelayTimes { static const juce::StringArray& names(); static double beats (int); };

struct KnockSettings
{
    bool melody = false;           // DRUMS | MELODY
    float input = 1.0f, output = 1.0f;   // linear gains
    float knock = 0.75f;           // global dry / wet
    // TONE (0..1)
    float punch = 0, saturation = 0, distortion = 0, bright = 0, low = 0, glue = 0;
    int distType = DistTypes::soft;
    float width = 1.0f;            // 0..2
    // FX
    float crush = 0, lofi = 0, stutter = 0, filter = 0 /* -1..1 */, ring = 0, reverse = 0;
    // SPACE
    float reverb = 0, delay = 0;
    int delayTime = 3;
};

struct Transport { bool playing = false, hasPosition = false; double ppq = 0, bpm = 120; };

/** Simple biquad (RBJ shelves), allocation free. */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};
    void lowShelf (double sr, float hz, float db)  { shelf (sr, hz, db, false); }
    void highShelf (double sr, float hz, float db) { shelf (sr, hz, db, true); }
    void shelf (double sr, float hz, float db, bool high);
    float process (int ch, float x) noexcept
    {
        auto y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
};

/**
    LT KNOCK: drum & melody processor. Chain: INPUT -> (Frequency Focus split) -> TONE -> FX -> SPACE
    -> KNOCK dry/wet -> OUTPUT. No latency.
*/
class KnockEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>&, const KnockSettings&, const Transport&, const lt::dsp::FocusSettings&);

    float getInputLevel (int ch) const noexcept  { return inLevel[(size_t) (ch & 1)].load(); }
    float getOutputLevel (int ch) const noexcept { return outLevel[(size_t) (ch & 1)].load(); }
    float getEnergy() const noexcept             { return energy.load(); }
    int getHitCount() const noexcept             { return hits.load(); }

    lt::dsp::SpectrumAnalyser spectrum;

private:
    void processTarget (juce::AudioBuffer<float>&, const KnockSettings&, const Transport&, int numSamples);

    double sr = 44100.0;
    juce::AudioBuffer<float> target, dry;
    lt::dsp::FocusSplitter splitter;

    // punch
    float envFast[2] {}, envSlow[2] {}, punchGain = 1;
    // tone shelves
    Biquad lowShelf, highShelf;
    float lastLowDb = -100, lastHighDb = -100;
    // glue
    float compEnv = 0, compGain = 1;
    // crush
    int crushCount = 0; float crushHold[2] {};
    // lo-fi
    std::vector<float> wowBuf[2]; int wowPos = 0; float wowPhase = 0;
    juce::dsp::StateVariableTPTFilter<float> lofiLp, sweep;
    juce::Random rnd;
    // stutter
    std::vector<float> stutBuf[2];
    int stutLen = 0, stutWrite = 0, stutRead = 0;
    bool stutActive = false;
    double lastSlice = -1.0;
    double freeBeats = 0;
    // ring
    float ringPhase = 0;
    // space
    juce::Reverb reverb, reverseVerb;
    juce::AudioBuffer<float> verbBuf, revVerbBuf;
    std::vector<float> revBuf[2]; int revPos = 0; float revPhase = 0;
    std::vector<float> delayBuf[2]; int delayPos = 0;
    float delayNow = 0;
    juce::dsp::StateVariableTPTFilter<float> delayLp;

    float inEnvHit = 0;
    std::array<std::atomic<float>, 2> inLevel {}, outLevel {};
    std::atomic<float> energy { 0 };
    std::atomic<int> hits { 0 };
};

} // namespace knock
