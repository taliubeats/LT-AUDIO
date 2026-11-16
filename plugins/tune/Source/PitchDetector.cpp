#include "PitchDetector.h"
#include <cmath>

namespace tune
{

void PitchDetector::Biquad::lowpass (double rate, double fc, double q)
{
    auto w = juce::MathConstants<double>::twoPi * fc / rate;
    auto cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
    auto a0 = 1.0 + alpha;
    b0 = (float) ((1.0 - cw) * 0.5 / a0);
    b1 = (float) ((1.0 - cw) / a0);
    b2 = b0;
    a1 = (float) (-2.0 * cw / a0);
    a2 = (float) ((1.0 - alpha) / a0);
    z1 = z2 = 0;
}

static int nextPow2 (int n)
{
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

void PitchDetector::prepare (double sampleRate, float minHz, float maxHz, float windowPeriods)
{
    sr = sampleRate;
    dec = juce::jmax (1, (int) std::round (sr / 11025.0));
    srD = sr / dec;
    tauMin = juce::jmax (2, (int) std::floor (srD / maxHz));
    tauMax = juce::jmax (tauMin + 4, (int) std::ceil (srD / minHz));
    W = juce::jmax (8, (int) std::ceil (tauMax * windowPeriods));

    if (dec > 1)
    {
        auto fc = juce::jmin (0.4 * srD, 4500.0);
        aa1.lowpass (sr, fc, 0.5412);   // 4th order Butterworth
        aa2.lowpass (sr, fc, 1.3066);
    }

    decBuf.assign ((size_t) nextPow2 (W + tauMax + 8), 0.0f);
    decMask = (int) decBuf.size() - 1;
    fullBuf.assign ((size_t) nextPow2 ((W + tauMax + 8) * dec + 4 * dec), 0.0f);
    fullMask = (int) fullBuf.size() - 1;
    diff.assign ((size_t) tauMax + 3, 0.0f);
    window.assign ((size_t) (W + tauMax + 2), 0.0f);
    reset();
}

void PitchDetector::reset()
{
    std::fill (decBuf.begin(), decBuf.end(), 0.0f);
    std::fill (fullBuf.begin(), fullBuf.end(), 0.0f);
    fullPos = decPos = decPhase = 0;
    aa1.z1 = aa1.z2 = aa2.z1 = aa2.z2 = 0;
}

void PitchDetector::push (float x) noexcept
{
    fullBuf[(size_t) fullPos] = x;
    fullPos = (fullPos + 1) & fullMask;

    auto f = dec > 1 ? aa2.process (aa1.process (x)) : x;
    if (++decPhase >= dec)
    {
        decPhase = 0;
        decBuf[(size_t) decPos] = f;
        decPos = (decPos + 1) & decMask;
    }
}

PitchDetector::Result PitchDetector::analyse() noexcept
{
    Result r;
    const int n = W + tauMax + 1;
    // oldest -> newest
    double energy = 0.0;
    for (int i = 0; i < n; ++i)
    {
        auto v = deci (n - 1 - i);
        window[(size_t) i] = v;
        energy += (double) v * v;
    }
    r.rms = (float) std::sqrt (energy / n);
    if (r.rms < juce::Decibels::decibelsToGain (gateDb))
        return r;

    // difference function
    const float* x = window.data();
    diff[0] = 0.0f;
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        float s = 0.0f;
        for (int i = 0; i < W; ++i)
        {
            auto d = x[i] - x[i + tau];
            s += d * d;
        }
        diff[(size_t) tau] = s;
    }
    // cumulative mean normalised difference
    float running = 0.0f;
    diff[0] = 1.0f;
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        running += diff[(size_t) tau];
        diff[(size_t) tau] = running > 0.0f ? diff[(size_t) tau] * (float) tau / running : 1.0f;
    }

    int best = -1;
    for (int tau = tauMin; tau <= tauMax; ++tau)
    {
        if (diff[(size_t) tau] < threshold)
        {
            while (tau + 1 <= tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
            best = tau;
            break;
        }
    }
    if (best < 0)
    {
        float m = 1e9f;
        for (int tau = tauMin; tau <= tauMax; ++tau)
            if (diff[(size_t) tau] < m) { m = diff[(size_t) tau]; best = tau; }
        if (m > 0.35f) return r;
    }

    r.clarity = juce::jlimit (0.0f, 1.0f, 1.0f - diff[(size_t) best]);

    // coarse lag (parabolic) in decimated samples
    auto lag = (double) best;
    if (best > 1 && best < tauMax + 1)
    {
        auto a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
        auto den = a - 2.0f * b + c;
        if (std::abs (den) > 1e-9f) lag += juce::jlimit (-0.5, 0.5, 0.5 * (double) (a - c) / den);
    }

    // refine at full rate around lag * dec
    auto period = lag * dec;
    if (dec > 1)
    {
        const int wf = W * dec;
        const int lo = juce::jmax (2, (int) std::floor (period) - dec - 1);
        const int hi = (int) std::ceil (period) + dec + 1;
        if (hi + wf + 2 < (int) fullBuf.size())
        {
            float bestV = 1e30f;
            int bestL = -1;
            float stack[64];
            const int count = juce::jmin (64, hi - lo + 1);
            for (int k = 0; k < count; ++k)
            {
                const int L = lo + k;
                float s = 0.0f;
                for (int i = 0; i < wf; ++i)
                {
                    auto d = full (i) - full (i + L);
                    s += d * d;
                }
                stack[k] = s;
                if (s < bestV) { bestV = s; bestL = k; }
            }
            if (bestL > 0 && bestL < count - 1)
            {
                auto a = stack[bestL - 1], b = stack[bestL], c = stack[bestL + 1];
                auto den = a - 2.0f * b + c;
                auto off = std::abs (den) > 1e-12f ? juce::jlimit (-0.5, 0.5, 0.5 * (double) (a - c) / den) : 0.0;
                period = (double) (lo + bestL) + off;
            }
        }
    }

    r.period = (float) period;
    r.hz = (float) (sr / period);
    r.voiced = true;
    return r;
}

} // namespace tune
