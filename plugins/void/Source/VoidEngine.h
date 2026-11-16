#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "ReverbTypes.h"

namespace lt_void
{

/** Everything the reverb needs for one block (real units). */
struct VoidSettings
{
    int type = 0;
    float mix = 0.35f;            // 0..1
    float preDelayMs = 20.0f;
    float size = 0.9f;            // 0..1
    float decay = 8.0f;           // seconds (T60)
    float highFreq = 8000.0f;     // damping (Hz)
    float lowFreq = 120.0f;       // damping (Hz)
    float attack = 0.2f;          // 0..1 bloom
    float diffusion = 0.7f;       // 0..1
    float modRate = 0.5f;         // Hz
    float modDepth = 0.5f;        // 0..1
    float lowCut = 80.0f;         // wet EQ (Hz)
    float highCut = 14000.0f;
    float width = 1.0f;           // 0..1.5
    float duck = 0.0f;            // 0..1
    bool freeze = false;
};

/**
    LT VOID's reverb: 8-line feedback delay network with modulated, interpolated delays,
    input diffusion, bloom, early reflections, damping, plus per-type stages (shimmer, spring
    dispersion, reverse, gate, vintage / lo-fi crush). No latency.
*/
class VoidEngine
{
public:
    static constexpr int numLines = 8;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>&, const VoidSettings&);

    // meters / visuals (any thread)
    float getInputLevel() const noexcept  { return inputLevel.load(); }
    float getOutputLevel() const noexcept { return outputLevel.load(); }
    float getWetLevel() const noexcept    { return wetLevel.load(); }
    int getHitCount() const noexcept      { return hits.load(); }

private:
    struct Delay
    {
        std::vector<float> buf;
        int mask = 0, pos = 0;
        void prepare (int maxSamples);
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        void push (float v) noexcept { buf[(size_t) pos] = v; pos = (pos + 1) & mask; }
        float tap (int d) const noexcept { return buf[(size_t) ((pos - 1 - d) & mask)]; }
        float read (float d) const noexcept;  // fractional (cubic)
    };

    struct Allpass   // Schroeder allpass
    {
        Delay d; int len = 1; float g = 0.5f;
        float process (float x) noexcept
        {
            auto delayed = d.tap (len - 1);
            auto v = x + g * delayed;
            d.push (v);
            return delayed - g * v;
        }
    };

    struct OnePole { float z = 0; };

    float onePoleLP (OnePole& s, float x, float coeff) noexcept { s.z += coeff * (x - s.z); return s.z; }
    float coeffFor (float hz) const noexcept;

    void updateBlock (const VoidSettings&);

    double sr = 44100.0;
    TypeCharacter ch;
    int lastType = -1;

    // pre-delay + bloom
    Delay preL, preR;
    // input diffusion
    std::array<Allpass, 4> diffL, diffR;
    // tank
    std::array<Delay, numLines> lines;
    std::array<float, numLines> baseLen {}, gains {}, lfoPhase {};
    std::array<OnePole, numLines> damp {}, lowDamp {}, loopHp {};
    std::array<std::array<float, 4>, numLines> dispState {};
    float sizeNow = 1.0f, sizeTarget = 1.0f;
    // early reflections
    std::array<float, 12> erTimeL {}, erTimeR {}, erGain {};
    // shimmer
    Delay shimBuf;
    float shimPhase = 0.0f;
    OnePole shimLp;
    // reverse
    Delay revL, revR;
    float revPhase = 0.0f;
    int revEnd = 0;
    // gate
    float gateEnv = 0.0f, gateGain = 0.0f;
    int gateHold = 0;
    // crush
    int crushCount = 0;
    float crushL = 0, crushR = 0;
    juce::Random noise;
    // wet EQ
    juce::dsp::StateVariableTPTFilter<float> lowCutF, highCutF;
    // duck
    float duckEnv = 0.0f;
    float freezeAmt = 0.0f;
    float inEnv = 0.0f;

    juce::SmoothedValue<float> mixS, preS, widthS;
    juce::AudioBuffer<float> wet;

    std::atomic<float> inputLevel { 0 }, outputLevel { 0 }, wetLevel { 0 };
    std::atomic<int> hits { 0 };

    // per-block derived
    float dampCoeff = 0.5f, lowDampCoeff = 0.01f, loopHpCoeff = 0.0f, modDepthSamples = 0, modInc = 0;
    float diffG = 0.5f, bloomSamples = 0;
    VoidSettings cur;
};

} // namespace lt_void
