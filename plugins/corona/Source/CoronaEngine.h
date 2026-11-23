#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>

namespace corona
{

// LT CORONA: variable-mu tube compressor/limiter.
// Topology follows the classic broadcast limiter of the RCA BA-6A family:
//   input iron -> triode driver -> push-pull remote-cutoff gain cell -> output iron,
//   with the detector fed back from the OUTPUT (feedback limiter, the ratio emerges from the loop).
// All curves and constants here are LT's own model, tuned by measurement in tests/CoronaTest.cpp.
struct Params
{
    float inputDb   = 0.0f;   // -24 .. +24
    float threshold = 12.0f;  // 0 .. 40  (more = more compression; see limitDb())
    int   attack    = 1;      // 3 steps: 0 = SLOW, 1 = middle, 2 = FAST
    int   release   = 2;      // 5 steps: 0 = SLOW .. 4 = FAST
    float debump    = 0.0f;   // 0 .. 10
    int   filter    = 0;      // 0 = OFF, 1..5 = 50..250 Hz sidechain high-pass
    float makeupDb  = 0.0f;   // 0 .. 24
    float mix       = 10.0f;  // 0 = DRY .. 10 = WET
    bool  power     = true;
    bool  autoGain  = false;
    int   mode      = 0;      // 0 stereo (linked), 1 mid/side, 2 mono
    int   oversampling = 1;   // 0 = 1x, 1 = 2x, 2 = 4x
};

// Time constants behind the knobs (also used by the tests and the UI tooltips).
// Detector time constants behind the stepped knobs. They are set so the HEARD (measured, 63 %) times are
// about 59 / 17 / 4.8 ms for attack and 590 / 207 / 73 / 26 / 9 ms for release (see tests/CoronaTest.cpp).
float attackSeconds  (int step) noexcept;
float releaseSeconds (int step) noexcept;
float limitDb (float threshold) noexcept;       // limiting level: +6 dBFS at 0 .. -24 dBFS at 40

class Engine
{
public:
    void prepare (double sampleRate, int blockSize, int channels);
    void reset();
    void setParams (const Params& p) noexcept { params = p; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    int   getLatencySamples() const noexcept { return latency; }   // constant for every OVS setting
    float getGainReductionDb() const noexcept { return grDb.load (std::memory_order_relaxed); }

    // static helpers exposed for tests
    static float cellGain (float vc) noexcept;

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float process (float x) noexcept { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void clear() noexcept { z1 = z2 = 0; }
        void setHighPass (double fs, double f, double q) noexcept;
        void setLowPass  (double fs, double f, double q) noexcept;
        void setPeak     (double fs, double f, double q, double gainDb) noexcept;
    };
    struct OnePole
    {
        float a = 0, z = 0;
        void set (double fs, double f) noexcept { a = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * f / fs)); }
        float lp (float x) noexcept { z += a * (x - z); return z; }
        float hp (float x) noexcept { return x - lp (x); }
    };
    struct Channel
    {
        Biquad inHp, bumpPeak, inLp, outLp, scHp1, scHp2;
        OnePole inIronLf, outIronLf, outDc, driverDc;
        float lastOut = 0;
        void clear() noexcept;
    };
    struct Detector
    {
        float v1 = 0, v2 = 0, vc = 0;
        void clear() noexcept { v1 = v2 = vc = 0; }
    };

    void updateFilters (double fsOs);
    float inputStage  (Channel&, float x, float bump) noexcept;
    float gainStage   (float u, float g, float vc) noexcept;
    float outputStage (Channel&, float y) noexcept;
    void  runDetector (Detector&, float level, float limit, float aA, float aR) noexcept;
    void  processOs (float* const* ch, int numCh, int n, double fsOs) noexcept;

    Params params;
    double fs = 48000.0;
    int maxBlock = 512, numChannels = 2, latency = 0, currentOs = -1;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> os;
    std::array<int, 3> osLatency { 0, 0, 0 };
    // pad delays so total latency never changes with the OVS switch, plus the dry path delay
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None>, 2> padDelay, dryDelay;
    juce::AudioBuffer<float> dry, work;
    std::array<Channel, 2> chans;
    std::array<Detector, 2> dets;
    double filtersFs = 0; int filtersKey = -1; float filtersBump = -1;
    juce::SmoothedValue<float> inGain, outGain, wetMix, powerMix;
    float autoSlowGr = 0;
    std::atomic<float> grDb { 0.0f };
};

} // namespace corona
