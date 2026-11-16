#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace nebula
{

/** The effects of the FX CHAIN (index 0 = empty slot). */
struct FxTypes
{
    enum
    {
        none = 0, particles, grainCloud, reverb, shimmer, delay, pingPong, reverseDelay, chorus, flanger, phaser,
        tremolo, autoPan, widener, pitchShifter, freqShifter, harmonizer, formant, saturation, distortion, tape,
        filterLP, filterHP, filterBP, comb, eq, bitcrusher, tapeWobble, stutter, spectralFreeze, ringMod, vinyl, count
    };
    static const juce::StringArray& names();
    /** Effects that switch on a part of the grain engine instead of processing the sound afterwards. */
    static bool isEngineFx (int t) noexcept { return t == particles || t == grainCloud; }
};

/** One slot of the FX CHAIN. Processes a stereo block in place; `amount` 0..1 is the slot's knob. */
class FxUnit
{
public:
    void prepare (double sampleRate, int maxBlock);
    void setType (int);
    int getType() const noexcept { return type; }
    void process (float* L, float* R, int n, float amount, double bpm, double ppq, bool playing);

private:
    struct Line
    {
        std::vector<float> b; int pos = 0;
        void prepare (int n) { b.assign ((size_t) juce::jmax (8, n), 0.0f); pos = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); }
        void push (float v) noexcept { b[(size_t) pos] = v; pos = (pos + 1) % (int) b.size(); }
        float read (float d) const noexcept
        {
            auto size = (int) b.size();
            auto rp = (float) pos - 1.0f - juce::jlimit (0.0f, (float) size - 3.0f, d);
            while (rp < 0) rp += (float) size;
            auto i0 = (int) rp, i1 = (i0 + 1) % size;
            auto f = rp - (float) i0;
            return b[(size_t) i0] * (1.0f - f) + b[(size_t) i1] * f;
        }
    };

    float pitchShift (Line&, float& phase, float ratio, float window) const noexcept;
    void reset();

    double sr = 44100.0;
    int type = 0;
    Line a[2], c[2];
    juce::Reverb reverb;
    juce::dsp::StateVariableTPTFilter<float> f1, f2, f3;
    float lfo = 0, lfo2 = 0, ph[2] {}, ph2[2] {}, apZ[2][6] {}, fbState[2] {}, hold[2] {};
    float hilbZ[2][2][4][4] {}, hilbDelay[2] {}, oscPhase = 0, env = 0;
    int count = 0, loopLen = 0, loopPos = 0;
    double lastSlice = -1;
    bool repeating = false, captured = false;
    juce::Random rnd;
};

} // namespace nebula
