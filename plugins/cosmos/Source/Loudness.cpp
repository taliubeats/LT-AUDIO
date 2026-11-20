#include "Loudness.h"

#include <algorithm>

namespace cosmos
{

static constexpr double pi = 3.14159265358979323846;

//==============================================================================
void LoudnessMeter::Histogram::init()
{
    binPower.resize (bins);
    for (int i = 0; i < bins; ++i)
    {
        auto lufs = -70.0 + (i + 0.5) * 0.01;
        binPower[(size_t) i] = std::pow (10.0, (lufs + 0.691) / 10.0);
    }
}

int LoudnessMeter::Histogram::binFor (float lufs) noexcept
{
    return std::clamp ((int) std::floor ((lufs + 70.0f) * 100.0f), 0, bins - 1);
}

void LoudnessMeter::Histogram::add (float lufs)
{
    if (lufs <= -70.0f) return;   // absolute gate
    ++counts[(size_t) binFor (lufs)];
}

//==============================================================================
void LoudnessMeter::TruePeak::prepare (double rate)
{
    factor = rate < 96000.0 ? 4 : rate < 192000.0 ? 2 : 1;
    taps = 24;
    auto n = factor * taps;
    coeffs.assign ((size_t) n, 0.0f);
    if (factor > 1)
    {
        // windowed-sinc interpolator (Kaiser, beta 8), cut-off just under the original Nyquist
        auto besselI0 = [] (double x)
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 30; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
            return sum;
        };
        const double beta = 8.0, cut = 0.92, centre = (n - 1) * 0.5;
        std::vector<double> h ((size_t) n);
        for (int k = 0; k < n; ++k)
        {
            auto t = (k - centre) / factor;
            auto x = pi * cut * t;
            auto s = std::abs (x) < 1e-12 ? 1.0 : std::sin (x) / x;
            auto r = (k - centre) / centre;
            auto w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
            h[(size_t) k] = s * w;
        }
        // each polyphase branch gets unity DC gain
        for (int p = 0; p < factor; ++p)
        {
            double sum = 0;
            for (int j = 0; j < taps; ++j) sum += h[(size_t) (p + factor * j)];
            for (int j = 0; j < taps; ++j) coeffs[(size_t) (p * taps + j)] = (float) (h[(size_t) (p + factor * j)] / sum);
        }
    }
    for (auto& v : hist) v.assign ((size_t) taps * 2, 0.0f);
    pos = 0;
}

float LoudnessMeter::TruePeak::push (int ch, float x) noexcept
{
    auto& hst = hist[(size_t) ch];
    if (factor == 1) return std::abs (x);
    // history is stored twice so the dot product never wraps
    hst[(size_t) pos] = x;
    hst[(size_t) (pos + taps)] = x;
    float peak = 0.0f;
    const float* xs = hst.data() + pos + 1;   // xs[taps-1] is the newest sample
    for (int p = 0; p < factor; ++p)
    {
        const float* c = coeffs.data() + p * taps;
        float y = 0.0f;
        for (int j = 0; j < taps; ++j) y += c[j] * xs[taps - 1 - j];
        peak = std::max (peak, std::abs (y));
    }
    return std::max (peak, std::abs (x));
}

void LoudnessMeter::TruePeak::advance() noexcept
{
    if (++pos >= taps) pos = 0;
}

//==============================================================================
void LoudnessMeter::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    channels = std::clamp (numChannels, 1, 2);
    subBlockLength = std::max (1, (int) std::lround (sr * 0.1));

    // K-weighting for any sample rate (same analogue prototype as the 48 kHz table in BS.1770-4)
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        auto K = std::tan (pi * f0 / sr);
        auto Vh = std::pow (10.0, G / 20.0);
        auto Vb = std::pow (Vh, 0.4996667741545416);
        auto a0 = 1.0 + K / Q + K * K;
        for (auto& f : filters)
        {
            auto& b = f[0];
            b.b0 = (Vh + Vb * K / Q + K * K) / a0;
            b.b1 = 2.0 * (K * K - Vh) / a0;
            b.b2 = (Vh - Vb * K / Q + K * K) / a0;
            b.a1 = 2.0 * (K * K - 1.0) / a0;
            b.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        auto K = std::tan (pi * f0 / sr);
        auto d = 1.0 + K / Q + K * K;
        for (auto& f : filters)
        {
            auto& b = f[1];
            b.b0 = 1.0; b.b1 = -2.0; b.b2 = 1.0;
            b.a1 = 2.0 * (K * K - 1.0) / d;
            b.a2 = (1.0 - K / Q + K * K) / d;
        }
    }
    gated.init();
    shortHist.init();
    tp.prepare (sr);
    reset();
}

void LoudnessMeter::reset()
{
    for (auto& f : filters) for (auto& b : f) b.z1 = b.z2 = 0.0;
    subSum = {};
    subPower = {};
    subIndex = subCount = subBlockFill = 0;
    blocksSinceIntegrated = 0;
    gated.clear();
    shortHist.clear();
    tpBlock = {};
    momentary = shortTerm = integrated = maxMomentary = maxShortTerm = silence;
    range = 0.0f;
    for (auto& t : truePeakMax) t = silence;
    for (auto& t : truePeakNow) t = silence;
}

void LoudnessMeter::process (const float* const* in, int numChannels, int numSamples)
{
    if (resetPending.exchange (false)) reset();
    if (numChannels <= 0) return;

    const int used = std::min (channels, numChannels);
    for (int i = 0; i < numSamples; ++i)
    {
        for (int c = 0; c < used; ++c)
        {
            auto x = in[c][i];
            auto y = filters[(size_t) c][1].run (filters[(size_t) c][0].run ((double) x));
            subSum[(size_t) c] += y * y;
            tpBlock[(size_t) c] = std::max (tpBlock[(size_t) c], tp.push (c, x));
        }
        if (used == 1 && channels == 2) tpBlock[1] = tpBlock[0];
        tp.advance();

        if (++subBlockFill >= subBlockLength)
            finishSubBlock();
    }
}

void LoudnessMeter::finishSubBlock()
{
    double p = 0.0;
    for (int c = 0; c < channels; ++c) p += subSum[(size_t) c] / subBlockFill;   // channel weights are 1.0 for L/R/mono
    subSum = {};
    subBlockFill = 0;

    subPower[(size_t) subIndex] = p;
    subIndex = (subIndex + 1) % ring;
    subCount = std::min (subCount + 1, ring);

    auto meanOf = [this] (int blocks)
    {
        blocks = std::min (blocks, subCount);
        double s = 0.0;
        for (int k = 1; k <= blocks; ++k) s += subPower[(size_t) ((subIndex - k + ring) % ring)];
        return blocks > 0 ? s / blocks : 0.0;
    };

    auto m = powerToLufs (meanOf (4));
    auto s = powerToLufs (meanOf (30));
    momentary = m;
    shortTerm = s;
    if (subCount >= 4)
    {
        maxMomentary = std::max (maxMomentary.load(), m);
        gated.add (m);
    }
    if (subCount >= 30)
    {
        maxShortTerm = std::max (maxShortTerm.load(), s);
        shortHist.add (s);
    }

    for (int c = 0; c < 2; ++c)
    {
        auto db = tpBlock[(size_t) c] > 1.0e-10f ? 20.0f * std::log10 (tpBlock[(size_t) c]) : silence;
        truePeakNow[(size_t) c] = db;
        truePeakMax[(size_t) c] = std::max (truePeakMax[(size_t) c].load(), db);
    }
    tpBlock = {};

    // integrated and LRA are cheap (fixed histograms) but there is no need to run them every 100 ms
    if (++blocksSinceIntegrated >= 2)
    {
        blocksSinceIntegrated = 0;
        updateIntegrated();
    }
}

void LoudnessMeter::updateIntegrated()
{
    auto gatedMean = [] (const Histogram& h, double threshold, double& power, unsigned long long& n)
    {
        power = 0.0; n = 0;
        for (int i = 0; i < Histogram::bins; ++i)
        {
            auto c = h.counts[(size_t) i];
            if (c == 0 || h.binPower[(size_t) i] < threshold) continue;
            power += h.binPower[(size_t) i] * c;
            n += c;
        }
        return n > 0 ? power / (double) n : 0.0;
    };

    double pw; unsigned long long n;
    auto ungated = gatedMean (gated, 0.0, pw, n);
    if (n > 0)
    {
        auto rel = ungated * std::pow (10.0, -10.0 / 10.0);   // -10 LU
        auto mean = gatedMean (gated, rel, pw, n);
        integrated = n > 0 ? powerToLufs (mean) : silence;
    }

    // loudness range
    auto stUngated = gatedMean (shortHist, 0.0, pw, n);
    if (n > 0)
    {
        auto rel = stUngated * std::pow (10.0, -20.0 / 10.0);   // -20 LU
        auto relBin = Histogram::binFor (powerToLufs (rel));
        unsigned long long total = 0;
        for (int i = relBin; i < Histogram::bins; ++i) total += shortHist.counts[(size_t) i];
        if (total > 0)
        {
            auto lowRank = (unsigned long long) std::floor (0.10 * (double) (total - 1));
            auto highRank = (unsigned long long) std::floor (0.95 * (double) (total - 1));
            unsigned long long seen = 0;
            int lowBin = -1, highBin = -1;
            for (int i = relBin; i < Histogram::bins; ++i)
            {
                seen += shortHist.counts[(size_t) i];
                if (lowBin < 0 && seen > lowRank) lowBin = i;
                if (highBin < 0 && seen > highRank) { highBin = i; break; }
            }
            if (lowBin >= 0 && highBin >= 0) range = (float) (highBin - lowBin) * 0.01f;
        }
    }
}

} // namespace cosmos
