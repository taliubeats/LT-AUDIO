#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace tune
{

/**
    Monophonic pitch detector (YIN) for voices.

    Samples are pushed one by one; analyse() looks at the most recent window. To keep it cheap the
    search runs on a decimated copy (~11 kHz) and the best lag is then refined at full rate, so the
    result is accurate to a fraction of a cent on steady tones.
    Used in real time by TuneEngine and offline by MatchAnalysis.
*/
class PitchDetector
{
public:
    struct Result
    {
        float hz = 0.0f;          // 0 when unvoiced
        float period = 0.0f;      // in input samples
        float clarity = 0.0f;     // 0..1 (1 = perfectly periodic)
        float rms = 0.0f;
        bool voiced = false;
    };

    /** windowPeriods: analysis window length in periods of the lowest note (1 = fast, 2 = HQ). */
    void prepare (double sampleRate, float minHz, float maxHz, float windowPeriods);
    void reset();

    void push (float x) noexcept;
    Result analyse() noexcept;

    int getDecimation() const noexcept             { return dec; }
    /** Samples (at the input rate) the analysis window spans; its centre is half of this behind "now". */
    int getWindowSamples() const noexcept          { return (W + tauMax + 2) * dec; }
    float gateDb = -52.0f;
    float threshold = 0.16f;

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void lowpass (double sr, double fc, double q);
        float process (float x) noexcept
        {
            auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    float full (int back) const noexcept   { return fullBuf[(size_t) ((fullPos - 1 - back) & fullMask)]; }
    float deci (int back) const noexcept   { return decBuf[(size_t) ((decPos - 1 - back) & decMask)]; }

    double sr = 44100.0, srD = 11025.0;
    int dec = 4, decPhase = 0;
    int tauMin = 2, tauMax = 100, W = 100;
    Biquad aa1, aa2;

    std::vector<float> fullBuf, decBuf, diff, window;
    int fullMask = 0, fullPos = 0, decMask = 0, decPos = 0;
};

} // namespace tune
