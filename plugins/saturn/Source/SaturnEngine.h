#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace saturn
{

struct Tapes
{
    enum { strings = 0, flute, choir, cassette, reel, broken, count };
    static const juce::StringArray& names();
};

struct SaturnSettings
{
    int tape = Tapes::strings;
    float age = 0.4f;          // 0..1 (macro)
    float wow = 0.3f;          // 0..1
    float flutter = 0.2f;      // 0..1
    float drift = 0.2f;        // 0..1
    float saturation = 0.35f;  // 0..1
    float tone = 0.0f;         // -1 (dark) .. +1 (bright)
    float bandwidth = 0.3f;    // 0 (full) .. 1 (old radio)
    float hiss = 0.15f;        // 0..1
    float ensemble = 0.3f;     // 0..1
    float mix = 1.0f;          // 0..1
    float outputDb = 0.0f;     // -24..+12
};

/** LT SATURN: tape / Mellotron ageing. Latency = getLatencySamples() (base tape delay, dry is aligned). */
class SaturnEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>&, const SaturnSettings&);

    int getLatencySamples() const noexcept       { return latency; }
    float getInputLevel() const noexcept         { return inLevel.load(); }
    float getOutputLevel() const noexcept        { return outLevel.load(); }
    float getSpeed() const noexcept              { return speedNow.load(); }   // 1 = nominal (visual)
    float getDropout() const noexcept            { return dropoutNow.load(); } // 0..1 (visual)

    /** Character of each tape type (also used by the visuals). */
    struct Character
    {
        float wowHz, flutterHz, wowDepth, flutterDepth;   // depths in ms at full amount
        float lowHz, highHz;                              // band at BANDWIDTH 0 -> narrowed by BANDWIDTH/AGE
        float bumpHz, bumpDb;                             // head bump / formant
        float drive, asym;                                // saturation flavour
        float ensembleVoices, ensembleDepthMs;
        float hissDb, rumble;                             // noise floor at full HISS
        float dropouts;                                   // BROKEN only
    };
    static const Character& character (int tape);

private:
    struct Line
    {
        std::vector<float> buf;
        int mask = 0, pos = 0;
        void prepare (int n);
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
        void push (float v) noexcept { buf[(size_t) pos] = v; pos = (pos + 1) & mask; }
        float read (float delaySamples) const noexcept;    // cubic
    };

    double sr = 44100.0;
    int latency = 0;
    float baseDelay = 0;                // samples
    Line tape[2], dry[2];
    juce::dsp::IIR::Filter<float> hp[2], lp[2], lp2[2], bump[2], tiltLo[2], tiltHi[2], hissHp[2], hissLp[2], rumbleLp[2];
    float wowPh = 0, flutPh = 0, ensPh[3] { 0.0f, 2.1f, 4.2f };
    float driftNow = 0, driftTarget = 0, driftTimer = 0;
    float wowRand = 0, wowRandTarget = 0;
    float dropGain = 1, dropTarget = 1, dropTimer = 0;
    float jerk = 0;
    int clickCountdown = 0;
    juce::Random rng { 0x5a7 };
    juce::SmoothedValue<float> mixS, outS;
    bool primed = false;
    int lastTape = -1;
    float lastBand = -1, lastTone = -2, lastAge = -1;

    void updateFilters (const Character&, float bandwidth, float tone, float age);

    std::atomic<float> inLevel { 0 }, outLevel { 0 }, speedNow { 1 }, dropoutNow { 0 };
};

} // namespace saturn
