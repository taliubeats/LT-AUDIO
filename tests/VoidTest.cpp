// Tests for the LT VOID reverb engine: decay time, freeze, mix, every type stays stable.
#include "VoidEngine.h"

using namespace lt_void;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl;
    if (! ok) ++failures;
}

static float rmsDb (const juce::AudioBuffer<float>& b, int start, int n)
{
    double sum = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = start; i < start + n; ++i)
            sum += (double) b.getSample (c, i) * b.getSample (c, i);
    return juce::Decibels::gainToDecibels ((float) std::sqrt (sum / (double) (n * b.getNumChannels())), -200.0f);
}

static juce::AudioBuffer<float> run (VoidSettings s, int seconds, bool impulse = true, double sr = 48000.0)
{
    VoidEngine e;
    e.prepare (sr, 512);
    juce::AudioBuffer<float> out (2, (int) sr * seconds);
    out.clear();
    if (impulse) { out.setSample (0, 0, 1.0f); out.setSample (1, 0, 1.0f); }
    else
    {
        juce::Random r (2);
        for (int i = 0; i < (int) (sr * 0.5); ++i)
            for (int c = 0; c < 2; ++c) out.setSample (c, i, (r.nextFloat() - 0.5f) * 0.5f);
    }
    for (int start = 0; start < out.getNumSamples(); start += 512)
    {
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, start, juce::jmin (512, out.getNumSamples() - start));
        e.process (view, s);
    }
    return out;
}

static bool finite (const juce::AudioBuffer<float>& b, float limit = 4.0f)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            auto v = b.getSample (c, i);
            if (! std::isfinite (v) || std::abs (v) > limit) return false;
        }
    return true;
}

int main()
{
    constexpr double sr = 48000.0;
    VoidSettings s;
    s.mix = 1.0f; s.preDelayMs = 0; s.attack = 0; s.lowCut = 20; s.highCut = 20000; s.highFreq = 20000; s.lowFreq = 20;
    s.type = (int) ReverbType::hall;

    std::cout << "Decay" << std::endl;
    for (auto decay : { 1.0f, 4.0f })
    {
        s.decay = decay;
        auto b = run (s, 7);
        auto win = (int) (0.1 * sr);
        auto a = rmsDb (b, (int) (0.3 * sr), win);
        auto later = rmsDb (b, (int) ((0.3 + decay * 0.5) * sr), win);
        auto drop = a - later;   // should be ~30 dB over half the T60
        std::cout << "     decay " << decay << " s: dropped " << drop << " dB in T60/2" << std::endl;
        check (drop > 20.0f && drop < 42.0f, "DECAY " + juce::String (decay) + " s is about right");
    }

    std::cout << "Freeze" << std::endl;
    {
        auto f = s; f.decay = 1.0f; f.freeze = true;
        auto b = run (f, 4, false);
        auto a = rmsDb (b, (int) (1.0 * sr), (int) (0.2 * sr));
        auto later = rmsDb (b, (int) (3.5 * sr), (int) (0.2 * sr));
        std::cout << "     frozen level 1 s: " << a << " dB, 3.5 s: " << later << " dB" << std::endl;
        check (a > -60.0f && std::abs (a - later) < 6.0f, "FREEZE holds the tail");
    }

    std::cout << "Mix" << std::endl;
    {
        auto m = s; m.mix = 0.0f;
        auto b = run (m, 1);
        check (std::abs (b.getSample (0, 0) - 1.0f) < 1e-4f && rmsDb (b, 1000, 20000) < -100.0f, "MIX 0% = dry only");
    }

    std::cout << "Every type" << std::endl;
    for (int t = 0; t < TypeInfo::count; ++t)
    {
        auto x = s; x.type = t; x.decay = 6.0f; x.size = 1.0f; x.modDepth = 1.0f; x.diffusion = 1.0f; x.attack = 0.5f;
        auto b = run (x, 4, false);
        auto level = rmsDb (b, (int) (0.6 * sr), (int) (0.3 * sr));
        std::cout << "     " << TypeInfo::names()[t] << ": tail " << level << " dB" << std::endl;
        check (finite (b) && level > -70.0f, TypeInfo::names()[t] + " is stable and makes a tail");
    }

    std::cout << "Gated stops" << std::endl;
    {
        auto g = s; g.type = (int) ReverbType::gated; g.decay = 4.0f;
        auto b = run (g, 3, false);
        check (rmsDb (b, (int) (1.6 * sr), (int) (0.3 * sr)) < -70.0f, "GATED cuts the tail");
    }

    std::cout << (failures == 0 ? "All VOID tests passed." : juce::String (failures) + " test(s) failed.") << std::endl;
    return failures == 0 ? 0 : 1;
}
