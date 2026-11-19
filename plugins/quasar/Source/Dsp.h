#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace quasar::dsp
{

inline constexpr float pi = juce::MathConstants<float>::pi;
inline constexpr float twoPi = juce::MathConstants<float>::twoPi;

/** 75 % level = unity, 100 % = +5 dB, quadratic below. */
inline float levelToGain (float pct) noexcept { auto x = juce::jmax (0.0f, pct) * 0.01f; return x * x * 1.7777778f; }
inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (juce::jmax (1.0e-9f, g)); }

/** Smooth, cheap saturator (Pade tanh, exact limits at +-3). */
inline float softClip (float x) noexcept
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    auto x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/** Equal-power pan, pan -1..1. */
inline void panGains (float pan, float& l, float& r) noexcept
{
    auto a = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * 0.25f * pi;
    l = std::cos (a) * 1.41421356f; r = std::sin (a) * 1.41421356f;
}

/** Fast 2^x for pitch maths (good to ~0.01 cent in the audio range). */
inline float exp2Fast (float x) noexcept { return std::exp2 (x); }

struct OnePole
{
    float z = 0.0f;
    float lp (float x, float a) noexcept { z += a * (x - z); return z; }
    void reset (float v = 0.0f) noexcept { z = v; }
    static float coeff (float hz, float sr) noexcept { return 1.0f - std::exp (-twoPi * juce::jmin (hz, sr * 0.49f) / sr); }
};

struct DcBlocker
{
    float x1 = 0, y1 = 0, r = 0.9995f;
    void prepare (float sr) noexcept { r = 1.0f - (twoPi * 12.0f / sr); }
    float process (float x) noexcept { auto y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() noexcept { x1 = y1 = 0; }
};

/** Linear ramp towards a target over a fixed number of samples (click-free parameter changes). */
struct Ramp
{
    float value = 0, target = 0, step = 0; int left = 0, length = 64;
    void setLength (int n) noexcept { length = juce::jmax (1, n); }
    void snap (float v) noexcept { value = target = v; left = 0; step = 0; }
    void set (float t) noexcept
    {
        if (t == target) return;
        target = t; left = length; step = (target - value) / (float) length;
    }
    float next() noexcept
    {
        if (left > 0) { value += step; if (--left == 0) value = target; }
        return value;
    }
    bool isSmoothing() const noexcept { return left > 0; }
};

/** Pink noise (Paul Kellet's economy filter) and a xorshift white source. */
struct Noise
{
    uint32_t s = 0x9e3779b9u;
    float b0 = 0, b1 = 0, b2 = 0, brown = 0;
    void seed (uint32_t v) noexcept { s = v ? v : 1u; }
    float white() noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
    float uni() noexcept { return white() * 0.5f + 0.5f; }
    float pink() noexcept
    {
        auto w = white();
        b0 = 0.99765f * b0 + w * 0.0990460f;
        b1 = 0.96300f * b1 + w * 0.2965164f;
        b2 = 0.57000f * b2 + w * 1.0526913f;
        return (b0 + b1 + b2 + w * 0.1848f) * 0.22f;
    }
    float brownNoise() noexcept
    {
        brown = juce::jlimit (-1.0f, 1.0f, brown * 0.998f + white() * 0.04f);
        return brown * 2.2f;
    }
};

// ================================================================== envelope

/**
    ADSR with hold and curves. Attack restarts from the current level (no clicks on retrigger),
    release starts from wherever the envelope is.
*/
struct Envelope
{
    enum Stage { idle, attack, hold, decay, sustain, release };
    Stage stage = idle;
    float level = 0, start = 0, t = 0;
    float attackInc = 1, holdInc = 1, decayInc = 1, releaseInc = 1, sus = 1;
    float aCurve = 0, dCurve = 0, rCurve = 0;

    static float shape (float x, float c) noexcept
    {
        // c in -1..1: 0 = linear, > 0 = exponential-like (slow start), < 0 = logarithmic (fast start)
        if (std::abs (c) < 0.01f) return x;
        auto k = std::exp2 (std::abs (c) * 3.5f);
        return c > 0 ? std::pow (x, k) : 1.0f - std::pow (1.0f - x, k);
    }

    void set (float sr, float attMs, float holdMs, float decMs, float susPct, float relMs, float ac, float dc, float rc) noexcept
    {
        auto inc = [sr] (float ms) { return 1.0f / juce::jmax (1.0f, ms * 0.001f * sr); };
        attackInc = inc (juce::jmax (0.4f, attMs));   // 0.4 ms minimum: avoids clicks on sharp waves
        holdInc = holdMs <= 0.0f ? 2.0f : inc (holdMs);
        decayInc = inc (decMs);
        releaseInc = inc (juce::jmax (1.0f, relMs));
        sus = juce::jlimit (0.0f, 1.0f, susPct * 0.01f);
        aCurve = ac * 0.01f; dCurve = dc * 0.01f; rCurve = rc * 0.01f;
    }

    void noteOn (bool fromZero) noexcept
    {
        if (fromZero) level = 0;
        start = level; t = 0; stage = attack;
    }
    void noteOff() noexcept
    {
        if (stage == idle || stage == release) return;
        start = level; t = 0; stage = release;
    }
    bool isActive() const noexcept { return stage != idle; }

    /** Advances `n` samples at once (control rate) and returns the new level. */
    float advance (int n) noexcept
    {
        auto steps = (float) n;
        for (int guard = 0; guard < 6 && steps > 0.0f; ++guard)
        {
            switch (stage)
            {
                case attack:
                {
                    auto need = (1.0f - t) / attackInc;
                    if (steps >= need) { steps -= need; level = 1.0f; t = 0; stage = hold; continue; }
                    t += attackInc * steps; steps = 0;
                    level = start + (1.0f - start) * shape (t, aCurve);
                    break;
                }
                case hold:
                {
                    auto need = (1.0f - t) / holdInc;
                    if (steps >= need) { steps -= need; t = 0; stage = decay; continue; }
                    t += holdInc * steps; steps = 0;
                    break;
                }
                case decay:
                {
                    auto need = (1.0f - t) / decayInc;
                    if (steps >= need) { steps -= need; level = sus; stage = sustain; continue; }
                    t += decayInc * steps; steps = 0;
                    level = sus + (1.0f - sus) * (1.0f - shape (t, -dCurve));
                    break;
                }
                case sustain:
                    level += (1.0f - std::pow (0.998f, steps)) * (sus - level);
                    steps = 0;
                    break;
                case release:
                {
                    auto need = (1.0f - t) / releaseInc;
                    if (steps >= need) { level = 0; stage = idle; steps = 0; break; }
                    t += releaseInc * steps; steps = 0;
                    level = start * (1.0f - shape (t, -rCurve));
                    break;
                }
                case idle: default: level = 0; steps = 0; break;
            }
        }
        return level;
    }

    float next() noexcept
    {
        switch (stage)
        {
            case attack:
                t += attackInc;
                if (t >= 1.0f) { level = 1.0f; t = 0; stage = hold; }
                else level = start + (1.0f - start) * shape (t, aCurve);
                break;
            case hold:
                t += holdInc;
                if (t >= 1.0f) { t = 0; stage = decay; }
                break;
            case decay:
                t += decayInc;
                if (t >= 1.0f) { level = sus; stage = sustain; }
                else level = sus + (1.0f - sus) * (1.0f - shape (t, -dCurve));
                break;
            case sustain:
                level += 0.002f * (sus - level);   // follow sustain changes smoothly
                break;
            case release:
                t += releaseInc;
                if (t >= 1.0f) { level = 0; stage = idle; }
                else level = start * (1.0f - shape (t, -rCurve));
                break;
            case idle: default: level = 0; break;
        }
        return level;
    }
};

// ================================================================== filters

/** Cytomic / Simper state variable filter (TPT, zero delay feedback). */
struct Svf
{
    float ic1 = 0, ic2 = 0;
    float g = 0.1f, k = 1.4f, a1 = 0, a2 = 0, a3 = 0;
    void setCoefs (float gIn, float res01) noexcept
    {
        g = gIn;
        k = 2.0f - 1.98f * juce::jlimit (0.0f, 1.0f, res01);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() noexcept { ic1 = ic2 = 0; }
    /** Returns low, band, high through references. */
    inline void tick (float v0, float& low, float& band, float& high) noexcept
    {
        auto v3 = v0 - ic2;
        auto v1 = a1 * ic1 + a2 * v3;
        auto v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        low = v2; band = v1; high = v0 - k * v1 - v2;
        // a little analogue softness on the integrator keeps high resonance tame
        ic1 = softClip (ic1 * 0.5f) * 2.0f;
    }
};

/**
    4-pole transistor ladder in TPT form. The feedback is solved without unit delay and
    the input stage saturates (tanh), so resonance and drive behave like the analog circuit.
*/
struct Ladder
{
    std::array<float, 4> s {};
    float G = 0, g = 0, k = 0;
    void setCoefs (float gIn, float res01) noexcept
    {
        g = gIn / (1.0f + gIn);
        G = g * g * g * g;
        k = 3.95f * juce::jlimit (0.0f, 1.0f, res01);
    }
    void reset() noexcept { s.fill (0.0f); }
    /** drive >= 1: gain into the saturating input stage. */
    inline float tick (float x, bool twoPole, float drive) noexcept
    {
        const auto a = g, b = 1.0f - g;
        float u;
        if (twoPole)
        {
            // feedback from the 2nd stage: y2 = a^2 u + a S1 + S2
            auto k2 = k * 0.55f;
            auto sigma = a * b * s[0] + b * s[1];
            u = (x * (1.0f + 0.3f * k2) - k2 * sigma) / (1.0f + k2 * a * a);
        }
        else
        {
            // y4 = a^4 u + a^3 S1 + a^2 S2 + a S3 + S4, with S = (1 - a) s
            auto sigma = b * (a * a * a * s[0] + a * a * s[1] + a * s[2] + s[3]);
            u = (x * (1.0f + 0.6f * k) - k * sigma) / (1.0f + k * G);
        }
        u = softClip (u * drive) / drive;
        float y = u, y2 = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            auto v = (y - s[(size_t) i]) * a;
            y = v + s[(size_t) i];
            s[(size_t) i] = y + v;
            if (i == 1) { y2 = y; if (twoPole) break; }
        }
        return twoPole ? y2 : y;
    }
};

} // namespace quasar::dsp
