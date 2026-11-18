#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "TuneEngine.h"
#include "NovaEngine.h"
#include "OrbitEngine.h"
#include "VoidEngine.h"

namespace vocal
{

/** All the chain settings for one block, in real units. */
struct ChainSettings
{
    // CLEANUP
    float lowCutHz = 80.0f;
    float gateDb = -80.0f;          // -80 = off
    float deEss = 0.3f;             // 0..1
    float deEssHz = 6500.0f;
    // TUNE
    bool tuneOn = true;
    tune::TuneSettings tune;
    // TONE (dB)
    float body = 0.0f, mudCut = 2.0f, presence = 2.0f, air = 3.0f;
    // DYNAMICS
    float comp = 0.4f;              // 0..1 amount
    float compAttackMs = 8.0f, compReleaseMs = 120.0f;
    nova::NovaSettings sat;         // drive / type / mix
    // FX
    float doubler = 0.0f, phone = 0.0f, width = 1.0f;
    // SPACE
    orbit::OrbitSettings delay;
    lt_void::VoidSettings reverb;
    bool delayOn = true, reverbOn = true;
    // OUTPUT
    float outputDb = 0.0f;
    bool bypass = false;
};

/**
    LT VOCAL SUITE chain:
      CLEANUP (low cut, gate, de-esser) -> TUNE -> TONE (body, mud, presence, air)
      -> DYNAMICS (compressor + NOVA saturation) -> FX (doubler, phone, width)
      -> SPACE (ORBIT delay -> VOID reverb) -> OUTPUT.
    Latency = TUNE + NOVA latency; the dry signal for BYPASS is delayed to match.
*/
class VocalChain
{
public:
    void prepare (double sampleRate, int maxBlockSize, int voice, bool hq);
    void reset();
    void process (juce::AudioBuffer<float>&, const ChainSettings&);

    int getLatency() const noexcept                     { return latency; }
    bool needsReconfigure (const ChainSettings& s) const { return tuneEngine.needsReconfigure (s.tune); }

    // meters / visuals
    float getInputLevel() const noexcept                { return inLevel.load(); }
    float getOutputLevel() const noexcept               { return outLevel.load(); }
    float getGainReductionDb() const noexcept           { return grDb.load(); }
    float getDeEssDb() const noexcept                   { return deEssDb.load(); }
    bool isGateOpen() const noexcept                    { return gateOpen.load(); }
    const tune::TuneEngine& getTune() const noexcept    { return tuneEngine; }

private:
    void updateFilters (const ChainSettings&);
    void cleanup (juce::AudioBuffer<float>&, const ChainSettings&);
    void compress (juce::AudioBuffer<float>&, const ChainSettings&);
    void effects (juce::AudioBuffer<float>&, const ChainSettings&);

    double sr = 44100.0;
    int maxBlock = 512, latency = 0;

    tune::TuneEngine tuneEngine;
    nova::NovaEngine novaEngine;
    orbit::OrbitEngine delayEngine;
    lt_void::VoidEngine reverbEngine;

    using IIR = juce::dsp::IIR::Filter<float>;
    std::array<IIR, 2> lowCut, lowCut2, body, mud, presence, air, essDetect, essDetect2, phoneHp, phoneLp;
    juce::dsp::LinkwitzRileyFilter<float> essSplit;          // flat-summing split for the de-esser
    ChainSettings lastFilters;
    bool filtersReady = false;

    float gateEnv = 0.0f, gateGain = 1.0f, compEnv = 0.0f, essEnvHi = 0.0f, essEnvAll = 0.0f, essGainSm = 1.0f, progDb = -24.0f, makeupDb = 0.0f, preCompPow = 1.0e-3f, postCompPow = 1.0e-3f;
    juce::AudioBuffer<float> dry, dbl, workBuf;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 15 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> doubleLine { 1 << 13 };
    double lfoPhase = 0.0;
    juce::SmoothedValue<float> outGain, bypassMix;

    // output safety limiter (short look-ahead, -0.5 dBFS ceiling)
    void limit (juce::AudioBuffer<float>&);
    int limLook = 64, limPos = 0;
    std::vector<float> limBuf[2], limReq;
    float limGain = 1.0f, limAtt = 0.1f, limRel = 0.999f;
    std::atomic<float> limitDb { 0.0f };
public:
    float getLimiterDb() const noexcept { return limitDb.load(); }
private:

    std::atomic<float> inLevel { 0 }, outLevel { 0 }, grDb { 0 }, deEssDb { 0 };
    std::atomic<bool> gateOpen { false };
};

} // namespace vocal
