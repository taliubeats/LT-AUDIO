#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace cosmos
{

/**
    ITU-R BS.1770-4 / EBU R128 loudness meter for mono or stereo.

    Runs on the audio thread (no allocation after prepare). Readings are published
    through atomics so the editor can read them at any time.

    - momentary  : 400 ms window, updated every 100 ms
    - short-term : 3 s window, updated every 100 ms
    - integrated : gated (absolute -70 LUFS, relative -10 LU), since the last reset
    - LRA        : EBU Tech 3342 (short-term values, -20 LU relative gate, 10th..95th percentile)
    - true peak  : 4x oversampled below 96 kHz, 2x below 192 kHz (BS.1770-4 Annex 2)
*/
class LoudnessMeter
{
public:
    static constexpr float silence = -200.0f;   // "no reading yet"

    void prepare (double sampleRate, int numChannels);
    void reset();                                // integrated, LRA and maxima start again
    void process (const float* const* channels, int numChannels, int numSamples);

    float getMomentary() const noexcept   { return momentary.load(); }
    float getShortTerm() const noexcept   { return shortTerm.load(); }
    float getIntegrated() const noexcept  { return integrated.load(); }
    float getRange() const noexcept       { return range.load(); }
    float getMaxMomentary() const noexcept { return maxMomentary.load(); }
    float getMaxShortTerm() const noexcept { return maxShortTerm.load(); }
    /** Highest true peak since reset, dBTP. */
    float getTruePeakMax (int ch) const noexcept { return truePeakMax[(size_t) (ch & 1)].load(); }
    /** True peak of the most recent 100 ms, dBTP (for live bars). */
    float getTruePeakNow (int ch) const noexcept { return truePeakNow[(size_t) (ch & 1)].load(); }

    /** Thread-safe reset request (from the editor); applied on the next process call. */
    void requestReset() noexcept { resetPending.store (true); }

    static float powerToLufs (double power) noexcept
    {
        return power <= 1.0e-20 ? silence : (float) (-0.691 + 10.0 * std::log10 (power));
    }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        inline double run (double x) noexcept
        {
            auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    /** Histogram of gating-block powers in 0.01 LU steps (-70 .. +5 LUFS), so memory stays fixed. */
    struct Histogram
    {
        static constexpr int bins = 7500;
        std::vector<unsigned> counts = std::vector<unsigned> (bins, 0);
        std::vector<double> binPower;
        void init();
        void clear() { std::fill (counts.begin(), counts.end(), 0u); }
        void add (float lufs);
        static int binFor (float lufs) noexcept;
    };

    struct TruePeak
    {
        int factor = 4, taps = 0;
        std::vector<float> coeffs;                // [phase][tap]
        std::array<std::vector<float>, 2> hist;   // circular input history per channel
        int pos = 0;
        void prepare (double sr);
        float push (int ch, float x) noexcept;    // returns max |y| over the oversampled phases
        void advance() noexcept;
    };

    void finishSubBlock();
    void updateIntegrated();

    double sr = 48000.0;
    int channels = 2, subBlockLength = 4800, subBlockFill = 0;
    std::array<std::array<Biquad, 2>, 2> filters;   // [channel][stage]
    std::array<double, 2> subSum {};
    static constexpr int ring = 30;                  // 30 x 100 ms = 3 s
    std::array<double, ring> subPower {};            // channel-summed mean square per 100 ms
    int subIndex = 0, subCount = 0;
    Histogram gated, shortHist;
    int blocksSinceIntegrated = 0;
    TruePeak tp;
    std::array<float, 2> tpBlock {};

    std::atomic<float> momentary { silence }, shortTerm { silence }, integrated { silence }, range { 0.0f },
                       maxMomentary { silence }, maxShortTerm { silence };
    std::array<std::atomic<float>, 2> truePeakMax { silence, silence }, truePeakNow { silence, silence };
    std::atomic<bool> resetPending { false };
};

} // namespace cosmos
