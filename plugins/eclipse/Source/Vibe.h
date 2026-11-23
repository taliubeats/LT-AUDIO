#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Skins.h"

namespace eclipse
{

/**
    VIBE: one saturation knob whose character comes from the skin.
    amount 0..10. At 0 the signal passes untouched (bit exact).
    Stereo, stateful (filters, envelope); one instance per pad bus.
*/
class VibeSaturator
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float* const* ch, int numChannels, int numSamples, Vibe type, float amount);

    /** Static curve used by the UI and the tests (no filters). */
    static float shape (Vibe type, float x, float drive);

private:
    struct OnePole
    {
        float a = 0, z = 0;
        void set (double sr, double hz) { a = (float) std::exp (-juce::MathConstants<double>::twoPi * hz / sr); }
        float lp (float x) { z = x + a * (z - x); return z; }
    };
    double sr = 44100;
    OnePole tone[2], band[2], low[2], dc[2];
    float env = 0, smoothedAmount = 0;
    float dcX[2] {}, dcY[2] {};
};

} // namespace eclipse
