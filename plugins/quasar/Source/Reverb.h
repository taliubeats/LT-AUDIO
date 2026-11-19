#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Dsp.h"

namespace quasar
{

/**
    QUASAR's reverb, written to stay clean at 100 % wet:
    pre-delay -> 4 lossless diffusion steps (16 delays + Hadamard) -> 16-line feedback delay network
    with a Householder matrix, per-line damping and slow delay modulation. Every part of the loop is
    energy-preserving or lossy, so it can never run away, and the dense diffusion avoids metallic ringing.
*/
class QuasarVerb
{
public:
    static constexpr int N = 16;
    enum Type { plate = 0, hall, room, chamber, space, vintage };

    struct Settings
    {
        int type = plate;
        float size = 0.6f;          // 0..1
        float preDelayMs = 12.0f;
        float decay = 2.2f;         // seconds (T60)
        float damp = 8000.0f;       // Hz
        float width = 1.0f;         // 0..1
        float lowCut = 120.0f;      // Hz
        float mod = 0.35f;          // 0..1
    };

    void prepare (double sampleRate);
    void reset();
    /** Writes the 100 % wet signal into wetL / wetR. */
    void process (const float* inL, const float* inR, float* wetL, float* wetR, int n, const Settings&);

private:
    struct Line
    {
        std::vector<float> buf; int mask = 0, pos = 0;
        void prepare (int maxLen) { int s = 1; while (s < maxLen + 4) s <<= 1; buf.assign ((size_t) s, 0.0f); mask = s - 1; pos = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
        inline void push (float v) noexcept { buf[(size_t) pos] = v; pos = (pos + 1) & mask; }
        inline float tap (int d) const noexcept { return buf[(size_t) ((pos - d) & mask)]; }
        inline float read (float d) const noexcept
        {
            auto i = (int) d; auto f = d - (float) i;
            auto a = buf[(size_t) ((pos - i) & mask)], b = buf[(size_t) ((pos - i - 1) & mask)];
            return a + f * (b - a);
        }
    };

    void updateLengths (const Settings&);
    static void hadamard (float* x) noexcept;   // in place, normalised (N = 16)

    double sr = 48000.0;
    Line preL, preR;
    std::array<std::array<Line, N>, 4> diff;
    std::array<std::array<int, N>, 4> diffLen {};
    std::array<std::array<float, N>, 4> diffSign {};
    std::array<Line, N> fdn;
    std::array<float, N> fdnLen {}, fdnTarget {}, fdnGain {}, lfoPhase {}, lfoInc {}, dampZ {}, hpZ {};
    std::array<float, N> outSignL {}, outSignR {};
    dsp::Ramp preRamp;
    float dampCoeff = 0.5f, inLpZL = 0, inLpZR = 0, inCoeff = 1, wetHpL1 = 0, wetHpR1 = 0, wetHpL2 = 0, wetHpR2 = 0;
    float sizeNow = -1, decayNow = -1; int typeNow = -1;
    float modDepth = 0, gainComp = 1;
    juce::SmoothedValue<float> widthS, compS;
};

} // namespace quasar
