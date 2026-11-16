// Tests for the LT ORBIT delay engine: echo timing, feedback, ping-pong, freeze, every mode stable.
#include "OrbitEngine.h"

using namespace orbit;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl;
    if (! ok) ++failures;
}

static juce::AudioBuffer<float> run (OrbitSettings s, double seconds, bool impulse = true, double sr = 48000.0)
{
    OrbitEngine e;
    e.prepare (sr, 512);
    juce::AudioBuffer<float> b (2, (int) (sr * seconds));
    b.clear();
    if (impulse) { b.setSample (0, 0, 1.0f); b.setSample (1, 0, 1.0f); }
    else { juce::Random r (4); for (int i = 0; i < (int) (sr * 0.3); ++i) for (int c = 0; c < 2; ++c) b.setSample (c, i, (r.nextFloat() - 0.5f) * 0.5f); }
    for (int start = 0; start < b.getNumSamples(); start += 512)
    {
        juce::AudioBuffer<float> v (b.getArrayOfWritePointers(), 2, start, juce::jmin (512, b.getNumSamples() - start));
        e.process (v, s);
    }
    return b;
}

static int peakIndex (const juce::AudioBuffer<float>& b, int ch, int from, int to)
{
    int best = from; float v = 0;
    for (int i = from; i < to; ++i) if (std::abs (b.getSample (ch, i)) > v) { v = std::abs (b.getSample (ch, i)); best = i; }
    return best;
}

static float peak (const juce::AudioBuffer<float>& b, int ch, int from, int to)
{
    float v = 0;
    for (int i = from; i < to; ++i) v = juce::jmax (v, std::abs (b.getSample (ch, i)));
    return v;
}

int main()
{
    constexpr double sr = 48000.0;
    OrbitSettings s;
    s.mode = Modes::digital; s.delaySeconds = 0.25; s.feedback = 0.5f; s.mix = 1.0f; s.wobble = 0; s.drive = 0;
    s.lowCut = 20; s.highCut = 20000;

    std::cout << "Timing" << std::endl;
    {
        auto b = run (s, 1.2);
        auto d = (int) (0.25 * sr);
        auto p1 = peakIndex (b, 0, d - 200, d + 200), p2 = peakIndex (b, 0, 2 * d - 200, 2 * d + 200);
        check (std::abs (p1 - d) < 8, "first echo at TIME");
        check (std::abs (p2 - 2 * d) < 16, "second echo at 2 x TIME");
        auto energy = [&] (int from, int to) { double e = 0; for (int i = from; i < to; ++i) e += (double) b.getSample (0, i) * b.getSample (0, i); return (float) std::sqrt (e); };
        auto a1 = energy (d - 200, d + 200), a2 = energy (2 * d - 200, 2 * d + 200);
        std::cout << "     echo 1 " << a1 << ", echo 2 " << a2 << std::endl;
        check (std::abs (a2 / a1 - 0.49f) < 0.08f, "FEEDBACK 50% halves each echo");
    }

    std::cout << "Ping-pong" << std::endl;
    {
        auto p = s; p.mode = Modes::pingPong; p.width = 1.0f;
        auto b = run (p, 1.0);
        auto d = (int) (0.25 * sr);
        auto l1 = peak (b, 0, d - 200, d + 200), r1 = peak (b, 1, d - 200, d + 200);
        auto l2 = peak (b, 0, 2 * d - 200, 2 * d + 200), r2 = peak (b, 1, 2 * d - 200, 2 * d + 200);
        check (l1 > r1 * 4 && r2 > l2 * 4, "echoes bounce left, right");
    }

    std::cout << "Freeze" << std::endl;
    {
        auto f = s; f.feedback = 0.3f; f.freeze = true; f.delaySeconds = 0.2;
        auto b = run (f, 3.0, false);
        auto a = peak (b, 0, (int) (1.0 * sr), (int) (1.4 * sr)), c = peak (b, 0, (int) (2.5 * sr), (int) (2.9 * sr));
        check (a > 0.05f && std::abs (a - c) / a < 0.25f, "FREEZE loops forever");
    }

    std::cout << "Every mode" << std::endl;
    for (int m = 0; m < Modes::count; ++m)
    {
        auto x = s; x.mode = m; x.feedback = 1.0f; x.drive = 1.0f; x.wobble = 1.0f; x.delaySeconds = 0.3;
        auto b = run (x, 4.0, false);
        bool ok = true;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < b.getNumSamples(); ++i) { auto v = b.getSample (c, i); if (! std::isfinite (v) || std::abs (v) > 4.0f) ok = false; }
        auto tail = peak (b, 0, (int) (0.4 * sr), (int) (1.0 * sr));
        check (ok && tail > 0.01f, Modes::names()[m] + " is stable at full feedback and echoes");
    }

    std::cout << (failures == 0 ? "All ORBIT tests passed." : juce::String (failures) + " test(s) failed.") << std::endl;
    return failures == 0 ? 0 : 1;
}
