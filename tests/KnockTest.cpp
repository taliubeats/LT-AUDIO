// Tests for the LT KNOCK engine: transparent when idle, every effect stable, KNOCK mix, stutter repeats.
#include "KnockEngine.h"

using namespace knock;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl;
    if (! ok) ++failures;
}

static juce::AudioBuffer<float> drums (double sr, double seconds)
{
    juce::AudioBuffer<float> b (2, (int) (sr * seconds));
    juce::Random r (9);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        auto t = i / sr, bt = std::fmod (t, 0.5);
        auto v = (float) (0.6 * std::exp (-bt * 14) * std::sin (6.2832 * (50 + 120 * std::exp (-bt * 30)) * bt)
                          + 0.1 * (r.nextFloat() - 0.5f) * std::exp (-std::fmod (t, 0.25) * 40));
        b.setSample (0, i, v); b.setSample (1, i, v * 0.9f);
    }
    return b;
}

static void run (juce::AudioBuffer<float>& b, const KnockSettings& s, double sr, bool playing = true)
{
    KnockEngine e;
    e.prepare (sr, 512);
    Transport t; t.playing = playing; t.hasPosition = playing; t.bpm = 120;
    lt::dsp::FocusSettings f;
    for (int start = 0; start < b.getNumSamples(); start += 512)
    {
        auto n = juce::jmin (512, b.getNumSamples() - start);
        juce::AudioBuffer<float> v (b.getArrayOfWritePointers(), 2, start, n);
        e.process (v, s, t, f);
        t.ppq += n / (sr * 0.5);
    }
}

static float maxDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float d = 0;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < a.getNumSamples(); ++i) d = juce::jmax (d, std::abs (a.getSample (c, i) - b.getSample (c, i)));
    return d;
}

static bool finite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < 2; ++c) for (int i = 0; i < b.getNumSamples(); ++i) { auto v = b.getSample (c, i); if (! std::isfinite (v) || std::abs (v) > 4.1f) return false; }
    return true;
}

int main()
{
    constexpr double sr = 48000.0;
    auto src = drums (sr, 2.0);

    {
        KnockSettings s; s.knock = 1.0f;
        auto b = src; run (b, s, sr);
        check (maxDiff (b, src) < 1.0e-3f, "everything at zero = untouched");
        s.punch = 1; s.distortion = 1;
        s.knock = 0.0f;
        auto c = src; run (c, s, sr);
        check (maxDiff (c, src) < 1.0e-3f, "KNOCK 0% = original sound");
    }

    const char* names[] = { "PUNCH", "SATURATION", "DISTORTION", "BRIGHT", "LOW", "GLUE", "CRUSH", "LO-FI", "STUTTER", "FILTER LP",
                            "FILTER HP", "RING", "REVERSE", "REVERB", "DELAY" };
    for (int k = 0; k < 15; ++k)
        for (auto melody : { false, true })
        {
            KnockSettings s; s.knock = 1.0f; s.melody = melody;
            float* f[] = { &s.punch, &s.saturation, &s.distortion, &s.bright, &s.low, &s.glue, &s.crush, &s.lofi, &s.stutter,
                           &s.filter, &s.filter, &s.ring, &s.reverse, &s.reverb, &s.delay };
            *f[k] = k == 9 ? -1.0f : 1.0f;
            auto b = src; run (b, s, sr);
            check (finite (b) && maxDiff (b, src) > 1.0e-3f, juce::String (names[k]) + (melody ? " (melody)" : " (drums)") + " works and is stable");
        }

    for (int d = 0; d < 4; ++d)
    {
        KnockSettings s; s.knock = 1; s.distortion = 0.8f; s.distType = d;
        auto b = src; run (b, s, sr);
        check (finite (b), DistTypes::names()[d] + " distortion is stable");
    }

    std::cout << (failures == 0 ? "All KNOCK tests passed." : juce::String (failures) + " test(s) failed.") << std::endl;
    return failures == 0 ? 0 : 1;
}
