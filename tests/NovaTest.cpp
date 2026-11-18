#include "NovaEngine.h"
#include <iostream>

using namespace nova;

static int failures = 0;
#define CHECK(c, msg) do { if (! (c)) { ++failures; std::cout << "  FAIL: " << msg << "\n"; } } while (0)

// Harmonic magnitude of a mono signal at k * f0 (single-bin DFT)
static double harmonic (const float* x, int n, double sr, double f)
{
    double re = 0, im = 0;
    for (int i = 0; i < n; ++i)
    {
        auto w = 0.5 - 0.5 * std::cos (2 * juce::MathConstants<double>::pi * i / (n - 1));
        re += x[i] * w * std::cos (2 * juce::MathConstants<double>::pi * f * i / sr);
        im -= x[i] * w * std::sin (2 * juce::MathConstants<double>::pi * f * i / sr);
    }
    return std::sqrt (re * re + im * im);
}

struct Result { double h1, h2, h3, rmsIn, rmsOut; bool finite; };

static Result run (NovaSettings s, float amp = 0.5f, double f0 = 1000.0)
{
    const double sr = 48000.0; const int block = 512, blocks = 120;
    NovaEngine e;
    e.prepare (sr, block, 2);
    juce::AudioBuffer<float> b (2, block);
    std::vector<float> outL, inL;
    double ph = 0;
    for (int k = 0; k < blocks; ++k)
    {
        for (int i = 0; i < block; ++i)
        {
            auto v = amp * (float) std::sin (ph); ph += 2 * juce::MathConstants<double>::pi * f0 / sr;
            b.setSample (0, i, v); b.setSample (1, i, v); inL.push_back (v);
        }
        e.process (b, s);
        for (int i = 0; i < block; ++i) outL.push_back (b.getSample (0, i));
    }
    Result r {};
    r.finite = std::all_of (outL.begin(), outL.end(), [] (float v) { return std::isfinite (v); });
    auto n = 16384; auto* x = outL.data() + outL.size() - n;
    r.h1 = harmonic (x, n, sr, f0); r.h2 = harmonic (x, n, sr, 2 * f0); r.h3 = harmonic (x, n, sr, 3 * f0);
    double a = 0, c = 0;
    for (int i = 0; i < n; ++i) { a += inL[inL.size() - n + i] * inL[inL.size() - n + i]; c += x[i] * x[i]; }
    r.rmsIn = std::sqrt (a / n); r.rmsOut = std::sqrt (c / n);
    return r;
}

int main()
{
    auto names = SatTypes::names();
    for (int t = 0; t < (int) SatType::count; ++t)
    {
        NovaSettings s; s.type = t; s.drive = 0.8f; s.autoGain = true;
        auto r = run (s);
        auto thd = std::sqrt (r.h2 * r.h2 + r.h3 * r.h3) / r.h1;
        auto gainDb = 20 * std::log10 (r.rmsOut / r.rmsIn);
        std::cout << names[t] << ": THD " << juce::String (thd * 100, 2) << " %   auto gain level " << juce::String (gainDb, 2) << " dB\n";
        CHECK (r.finite, names[t] << " output not finite");
        CHECK (thd > 0.003, names[t] << " adds no harmonics");
        CHECK (std::abs (gainDb) < 2.0, names[t] << " auto gain off by " << gainDb << " dB");
    }

    // more drive -> more distortion
    {
        NovaSettings lo; lo.type = (int) SatType::tube; lo.drive = 0.1f;
        NovaSettings hi = lo; hi.drive = 0.9f;
        auto a = run (lo), b = run (hi);
        CHECK (b.h3 / b.h1 + b.h2 / b.h1 > a.h3 / a.h1 + a.h2 / a.h1, "drive does not increase distortion");
    }

    // CHARACTER: 0 -> even harmonics dominate, 1 -> odd harmonics dominate
    {
        NovaSettings even; even.type = (int) SatType::tube; even.drive = 0.6f; even.character = 0.0f;
        NovaSettings odd = even; odd.character = 1.0f;
        auto e = run (even), o = run (odd);
        std::cout << "character even: h2/h3 " << e.h2 / e.h3 << "   odd: h2/h3 " << o.h2 / o.h3 << "\n";
        CHECK (e.h2 / e.h3 > o.h2 / o.h3 * 3.0, "character does not move between even and odd");
    }

    // MIX 0 = untouched (just delayed by the latency)
    {
        NovaSettings s; s.drive = 1.0f; s.mix = 0.0f; s.autoGain = false;
        auto r = run (s);
        CHECK (r.h2 / r.h1 < 1.0e-3 && r.h3 / r.h1 < 1.0e-3, "mix 0 still distorts");
        CHECK (std::abs (20 * std::log10 (r.rmsOut / r.rmsIn)) < 0.1, "mix 0 changes level");
    }

    // silence in -> silence out (no DC, no runaway), loud input stays bounded
    {
        NovaSettings s; s.type = (int) SatType::diode; s.drive = 1.0f; s.autoGain = true;
        auto r = run (s, 0.0f);
        CHECK (r.rmsOut < 1.0e-3, "silence produces output");
        auto loud = run (s, 4.0f);
        CHECK (loud.finite && loud.rmsOut < 8.0, "loud input explodes");
    }

    NovaEngine e; e.prepare (44100.0, 256, 2);
    std::cout << "latency: " << e.getLatencySamples() << " samples\n";
    CHECK (e.getLatencySamples() >= 0 && e.getLatencySamples() < 512, "odd latency");

    std::cout << (failures == 0 ? "ALL NOVA TESTS PASSED\n" : "NOVA TESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
