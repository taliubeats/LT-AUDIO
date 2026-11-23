#include "Render.h"

#include <algorithm>
#include <cmath>
#include <random>

#include <world/synthesis.h>

namespace halo
{

namespace
{
    constexpr double pi = 3.14159265358979323846;

    struct Rng
    {
        explicit Rng (unsigned long long seed) : gen (seed) {}
        double uniform (double a, double b) { return std::uniform_real_distribution<double> (a, b) (gen); }
        double normal() { return std::normal_distribution<double> (0.0, 1.0) (gen); }
        std::mt19937_64 gen;
    };

    /** Low-frequency random curve of length n, standard deviation `amp`. */
    std::vector<double> smoothNoise (Rng& rng, int n, int corr, double amp)
    {
        std::vector<double> out ((size_t) std::max (0, n), 0.0);
        if (n <= 0) return out;
        int k = std::max (3, corr);
        std::vector<double> raw ((size_t) (n + 2 * k));
        for (auto& r : raw) r = rng.normal();
        std::vector<double> win ((size_t) (2 * k + 1));
        double ws = 0;
        for (int i = 0; i <= 2 * k; ++i) { win[(size_t) i] = 0.5 - 0.5 * std::cos (2 * pi * i / (2 * k)); ws += win[(size_t) i]; }
        double mean = 0;
        for (int i = 0; i < n; ++i)
        {
            double s = 0;
            int c = i + k;
            for (int j = -k; j <= k; ++j) s += raw[(size_t) (c + j)] * win[(size_t) (j + k)];
            out[(size_t) i] = s / ws;
            mean += out[(size_t) i];
        }
        mean /= n;
        double var = 0;
        for (auto v : out) var += (v - mean) * (v - mean);
        double sd = std::sqrt (var / n);
        if (sd <= 1e-12) sd = 1.0;
        for (auto& v : out) v = v / sd * amp;
        return out;
    }

    double voiceFormant (VoiceType v)
    {
        switch (v)
        {
            case VoiceType::higher:   return 1.10;
            case VoiceType::lower:    return 0.91;
            case VoiceType::chipmunk: return 1.45;
            case VoiceType::demon:    return 0.70;
            default:                  return 1.0;
        }
    }

    void formantShift (const float* in, double* out, int bins, double ratio)
    {
        if (std::abs (ratio - 1.0) < 1e-3)
        {
            for (int b = 0; b < bins; ++b) out[b] = in[b];
            return;
        }
        for (int b = 0; b < bins; ++b)
        {
            double s = b / ratio;
            int i0 = (int) s;
            if (i0 >= bins - 1) { out[b] = in[bins - 1]; continue; }
            double w = s - i0;
            out[b] = in[i0] * (1 - w) + in[i0 + 1] * w;
        }
    }
}

const char* styleName (Style s)
{
    static const char* n[] = { "Natural", "Falsetto", "Chest", "Breathy", "Whisper", "Shout" };
    return n[(int) s];
}

const char* voiceTypeName (VoiceType v)
{
    static const char* n[] = { "Same", "Higher", "Lower", "Chipmunk", "Demon" };
    return n[(int) v];
}

// ------------------------------------------------------------------ performance

Performance perform (const Analysis& an, const std::vector<HarmonyNote>& line, const PerformOptions& o)
{
    Rng rng (o.seed);
    const int n = an.numFrames();
    const double fs = 1000.0 / frameMs;
    std::vector<double> target ((size_t) n, 0.0), leadDev ((size_t) n, 0.0), own ((size_t) n, 0.0);

    double vibDepth = an.vibratoDepth * o.vibratoScale * (o.style == Style::falsetto ? 0.7 : 1.0);
    double vibRate = an.vibratoRate * rng.uniform (0.9, 1.1);
    double vibPhase = rng.uniform (0, 2 * pi);

    int prevEnd = -100000, prevMidi = -1;
    for (auto& hn : line)
    {
        auto& note = *hn.lead;
        int s = note.start, e = note.end, L = e - s;
        if (L <= 0) continue;
        for (int i = s; i < e; ++i)
        {
            bool voiced = an.midi[(size_t) i] > 0;
            target[(size_t) i] = voiced ? hn.midi : 0.0;
            leadDev[(size_t) i] = voiced ? an.midi[(size_t) i] - (note.midi + an.tuning) : 0.0;
        }

        std::vector<double> expr ((size_t) L, 0.0);
        bool connected = prevMidi >= 0 && (s - prevEnd) * frameMs < 40;
        if (connected)
        {
            int g = std::clamp ((int) (rng.uniform (35, 80) / frameMs), 2, std::max (2, L / 2));
            double d = prevMidi - hn.midi;
            for (int t = 0; t < L; ++t)
                expr[(size_t) t] += d * 0.5 * (1 + std::cos (pi * std::min (1.0, (double) t / g)));
        }
        else
        {
            int g = std::clamp ((int) (rng.uniform (45, 95) / frameMs), 2, std::max (2, L / 2));
            double depth = -rng.uniform (0.25, 0.7) * std::min (1.0, L * frameMs / 300.0);
            for (int t = 0; t < L; ++t)
                expr[(size_t) t] += depth * 0.5 * (1 + std::cos (pi * std::min (1.0, (double) t / g)));
        }
        if (L * frameMs > 300 && vibDepth > 0.02)
        {
            int onset = (int) (180 / frameMs);
            auto rateMod = smoothNoise (rng, L, 60, 0.03);
            double amt = vibDepth * rng.uniform (0.8, 1.15);
            double ph = vibPhase;
            for (int t = 0; t < L; ++t)
            {
                ph += 2 * pi * vibRate * (1 + rateMod[(size_t) t]) / fs;
                double fade = std::clamp ((t - onset) / (200.0 / frameMs), 0.0, 1.0);
                expr[(size_t) t] += fade * amt * std::sin (ph);
            }
            vibPhase = ph;
        }
        auto drift = smoothNoise (rng, L, 40, 0.06);
        for (int t = 0; t < L; ++t) own[(size_t) (s + t)] = expr[(size_t) t] + drift[(size_t) t];
        prevEnd = e;
        prevMidi = hn.midi;
    }

    double h = std::clamp (o.humanize, 0.0, 1.0);
    std::vector<double> f0 ((size_t) n, 0.0);
    for (int i = 0; i < n; ++i)
        if (target[(size_t) i] > 0)
            f0[(size_t) i] = midiToHz (target[(size_t) i] + an.tuning + (1 - h) * leadDev[(size_t) i] + h * own[(size_t) i]
                                       + o.detuneCents / 100.0);

    auto wander = smoothNoise (rng, n, (int) (400 / frameMs), 1.0);
    Performance p;
    p.srcFrame.resize ((size_t) n);
    double run = 0;
    for (int i = 0; i < n; ++i)
    {
        double off = o.delayMs + wander[(size_t) i] * (6 + 22 * h);
        double src = std::clamp (i - off / frameMs, 0.0, (double) (n - 1));
        run = i == 0 ? src : std::max (run, src);
        p.srcFrame[(size_t) i] = run;
    }
    p.f0.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        double s = p.srcFrame[(size_t) i];
        int i0 = (int) s;
        int i1 = std::min (i0 + 1, n - 1);
        double w = s - i0;
        double vm = (f0[(size_t) i0] > 0 ? 1.0 : 0.0) * (1 - w) + (f0[(size_t) i1] > 0 ? 1.0 : 0.0) * w;
        p.f0[(size_t) i] = vm > 0.5 ? f0[(size_t) i0] * (1 - w) + f0[(size_t) i1] * w : 0.0;
        if (p.f0[(size_t) i] > 0 && (f0[(size_t) i0] <= 0 || f0[(size_t) i1] <= 0))
            p.f0[(size_t) i] = std::max (f0[(size_t) i0], f0[(size_t) i1]);
    }
    return p;
}

// ------------------------------------------------------------------ WORLD synthesis

std::vector<float> renderWorld (const Analysis& an, const Performance& perf, double meanShift, Style style,
                                VoiceType voice, unsigned long long)
{
    const auto& S = an.spectra;
    const int n = an.numFrames(), bins = S.bins, sr = an.sampleRate;
    const double binHz = (double) sr / S.fftSize;

    bool character = voice == VoiceType::chipmunk || voice == VoiceType::demon;
    double ratio = voiceFormant (voice) * std::pow (2.0, std::clamp (meanShift, -12.0, 12.0) / 12.0 * (character ? 0.0 : 0.12));
    if (style == Style::falsetto) ratio *= 1.04;
    if (style == Style::shout) ratio *= 1.06;

    // per-bin style curves (power domain, like WORLD's spectrogram)
    std::vector<double> spGain ((size_t) bins, 1.0);
    double apMix = 0.0, apScale = 1.0;
    for (int b = 0; b < bins; ++b)
    {
        double f = b * binHz;
        switch (style)
        {
            case Style::falsetto: { double t = f > 900 ? std::pow (std::max (f, 1.0) / 900.0, -1.2) : 1.0; spGain[(size_t) b] = t * t; break; }
            case Style::chest:    { double g = 1 + 0.5 * std::exp (-std::pow ((f - 450) / 300, 2)); spGain[(size_t) b] = g * g; break; }
            case Style::shout:    { double g = (1 + 1.4 * std::exp (-std::pow ((f - 2500) / 1300, 2))) * std::clamp (f / 200, 0.3, 1.0);
                                    spGain[(size_t) b] = g * g; break; }
            default: break;
        }
    }
    switch (style)
    {
        case Style::falsetto: apMix = 0.35; break;
        case Style::breathy:  apMix = 0.5;  break;
        case Style::whisper:  apMix = 0.85; break;
        case Style::shout:    apMix = 0.18; break;
        case Style::chest:    apScale = 0.85; break;
        default: break;
    }

    // synthesis in chunks of frames to bound memory; chunks overlap and are cross-faded
    const int chunk = 1600, pad = 40;   // 8 s, 0.2 s
    const double spf = sr * frameSec;
    std::vector<float> out (an.audio.size(), 0.0f);
    std::vector<float> mixed ((size_t) bins);

    for (int c0 = 0; c0 < n; c0 += chunk)
    {
        int a = std::max (0, c0 - pad), b = std::min (n, c0 + chunk + pad);
        int cn = b - a;
        std::vector<double> spBuf ((size_t) cn * (size_t) bins), apBuf ((size_t) cn * (size_t) bins);
        std::vector<double*> spP ((size_t) cn), apP ((size_t) cn);
        std::vector<double> f0 ((size_t) cn);
        for (int i = 0; i < cn; ++i)
        {
            int fi = a + i;
            double src = perf.srcFrame[(size_t) fi];
            int i0 = (int) src, i1 = std::min (i0 + 1, n - 1);
            double w = src - i0;
            const float* s0 = S.spRow (i0); const float* s1 = S.spRow (i1);
            const float* p0 = S.apRow (i0); const float* p1 = S.apRow (i1);
            for (int k = 0; k < bins; ++k) mixed[(size_t) k] = (float) (s0[k] * (1 - w) + s1[k] * w);
            spP[(size_t) i] = spBuf.data() + (size_t) i * (size_t) bins;
            apP[(size_t) i] = apBuf.data() + (size_t) i * (size_t) bins;
            formantShift (mixed.data(), spP[(size_t) i], bins, ratio);
            double fv = perf.f0[(size_t) fi];
            f0[(size_t) i] = fv;
            double unvoicedGain = fv > 0 ? 1.0 : 0.2;   // backing voices sing consonants softer
            for (int k = 0; k < bins; ++k)
            {
                spP[(size_t) i][k] = std::max (1e-16, spP[(size_t) i][k] * spGain[(size_t) k] * unvoicedGain);
                double apv = p0[k] * (1 - w) + p1[k] * w;
                apv = std::clamp (apv * apScale, 0.0, 1.0);
                apP[(size_t) i][k] = std::clamp (apv + (1 - apv) * apMix, 0.001, 0.999);
            }
        }
        int ylen = (int) ((cn - 1) * spf) + 1;
        std::vector<double> y ((size_t) ylen, 0.0);
        Synthesis (f0.data(), cn, spP.data(), apP.data(), S.fftSize, frameMs, sr, ylen, y.data());

        // place into output with linear cross-fades over the padded regions
        auto startSample = (long) std::llround (a * spf);
        auto fadeIn = c0 > 0 ? (long) (pad * spf) : 0;
        auto fadeOut = b < n ? (long) (pad * spf) : 0;
        for (long i = 0; i < ylen; ++i)
        {
            long o = startSample + i;
            if (o < 0 || o >= (long) out.size()) continue;
            double g = 1.0;
            // overlapping halves: first `2*pad` frames fade in, last `2*pad` fade out
            if (c0 > 0 && i < 2 * fadeIn) g *= (double) i / (2.0 * fadeIn);
            if (b < n && i > ylen - 2 * fadeOut) g *= (double) (ylen - i) / (2.0 * fadeOut);
            out[(size_t) o] += (float) (y[(size_t) i] * g);
        }
    }
    return out;
}

} // namespace halo
