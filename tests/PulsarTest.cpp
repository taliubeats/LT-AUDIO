// Tests for LT PULSAR: the curve, the ducking engine (tempo sync, MIDI trigger) and Frequency Focus.
#include "DuckEngine.h"

using namespace pulsar;

static int failures = 0;

static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl;
    if (! ok) ++failures;
}

static bool near (float a, float b, float tol) { return std::abs (a - b) <= tol; }

static float rms (const juce::AudioBuffer<float>& b, int start, int n)
{
    double sum = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = start; i < start + n; ++i)
            sum += (double) b.getSample (c, i) * b.getSample (c, i);
    return (float) std::sqrt (sum / (double) (n * b.getNumChannels()));
}

static juce::AudioBuffer<float> constant (int n, float v)
{
    juce::AudioBuffer<float> b (2, n);
    for (int c = 0; c < 2; ++c)
        juce::FloatVectorOperations::fill (b.getWritePointer (c), v, n);
    return b;
}

static juce::AudioBuffer<float> sine (int n, double sr, double hz)
{
    juce::AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i)
    {
        auto v = (float) (0.5 * std::sin (juce::MathConstants<double>::twoPi * hz * i / sr));
        b.setSample (0, i, v);
        b.setSample (1, i, v);
    }
    return b;
}

/** Runs the engine block by block like a host would. */
static void run (DuckEngine& e, juce::AudioBuffer<float>& audio, DuckEngine::Settings s, DuckEngine::Transport t,
                 lt::dsp::FocusSettings f, double sr, const juce::MidiBuffer& midi = {})
{
    constexpr int block = 512;
    for (int start = 0; start < audio.getNumSamples(); start += block)
    {
        auto n = juce::jmin (block, audio.getNumSamples() - start);
        juce::AudioBuffer<float> view (audio.getArrayOfWritePointers(), 2, start, n);
        juce::MidiBuffer m;
        for (auto meta : midi)
            if (meta.samplePosition >= start && meta.samplePosition < start + n)
                m.addEvent (meta.getMessage(), meta.samplePosition - start);
        e.process (view, m, s, t, f);
        if (t.hasPosition)
            t.ppq += n / (sr * 60.0 / t.bpm);
    }
}

int main()
{
    constexpr double sr = 48000.0;

    std::cout << "Curve" << std::endl;
    {
        auto lin = Curve::shape (2);
        check (near (lin.evaluate (0.5f), 0.5f, 1e-4f), "linear curve is 0.5 in the middle");
        auto shifted = lin;
        shifted.setOffset (0.25f);
        check (near (shifted.evaluate (0.25f), 0.0f, 1e-4f), "SHIFT moves the curve in time");
        check (near (shifted.evaluate (0.75f), 0.5f, 1e-4f), "SHIFT keeps the shape");
        check (Curve::fromVar (shifted.toVar()) == shifted, "curve survives save / load");

        auto c = lin;
        auto i = c.addPoint (0.5f, 1.0f);
        check (c.getPoints().size() == 3 && near (c.evaluate (0.5f), 1.0f, 1e-4f), "add a point");
        c.movePoint (i, 2.0f, 0.2f);
        check (c.getPoints()[(size_t) i].x <= 1.0f && near (c.getPoints()[(size_t) i].y, 0.2f, 1e-4f), "moving a point stays inside the cycle");
        c.removePoint (i);
        check (c == lin, "remove a point");

        auto bent = lin;
        bent.setTension (0, -0.6f);
        check (bent.evaluate (0.25f) > 0.5f, "negative tension = fast recovery");

        for (int s = 0; s < Curve::numShapes; ++s)
        {
            auto sh = Curve::shape (s);
            check (sh.getPoints().front().x <= 0.0f && sh.getPoints().back().x >= 1.0f, "shape " + Curve::shapeName (s) + " covers the whole cycle");
        }
    }

    std::cout << "Presets" << std::endl;
    {
        auto f = PresetLibrary::factory();
        check (f.size() == 12, "11 factory presets + Default");
        bool allOk = true;
        for (auto& p : f)
        {
            auto back = Preset::fromVar (p.toVar());
            allOk = allOk && back.name == p.name && back.rate == p.rate && near (back.mix, p.mix, 1e-3f) && back.curve == p.curve;
        }
        check (allOk, "every factory preset survives save / load");
    }

    DuckEngine::Transport playing;
    playing.playing = true;
    playing.hasPosition = true;
    playing.bpm = 120.0;
    lt::dsp::FocusSettings full;

    std::cout << "Tempo sync" << std::endl;
    {
        DuckEngine e;
        e.prepare (sr, 2);
        e.setCurve (Curve::shape (2));   // linear 0 -> 1 over each 1/4
        DuckEngine::Settings s;
        s.mix = 1.0f;
        s.rate = Rates::quarter;

        auto beat = (int) (sr * 0.5);   // 120 BPM
        auto a = constant (beat * 4, 1.0f);
        run (e, a, s, playing, full, sr);

        check (near (a.getSample (0, beat + beat / 2), 0.5f, 0.02f), "half way through a beat the volume is 50%");
        check (near (a.getSample (0, 2 * beat + beat / 10), 0.1f, 0.02f), "10% into a beat the volume is 10%");
        check (a.getSample (0, 3 * beat + 200) < 0.1f, "the volume drops at every beat");

        // 1/8: twice as fast
        DuckEngine e2;
        e2.prepare (sr, 2);
        e2.setCurve (Curve::shape (2));
        s.rate = Rates::eighth;
        auto b = constant (beat * 2, 1.0f);
        run (e2, b, s, playing, full, sr);
        check (near (b.getSample (0, beat / 4), 0.5f, 0.02f), "1/8 rate cycles twice per beat");

        // mix 0 = untouched
        DuckEngine e3;
        e3.prepare (sr, 2);
        e3.setCurve (Curve::shape (2));
        s.mix = 0.0f;
        auto c = constant (beat, 1.0f);
        run (e3, c, s, playing, full, sr);
        check (near (c.getSample (0, beat / 3), 1.0f, 1e-4f), "MIX 0% leaves the audio untouched");

        // mix 50% at the bottom of the dip = half volume
        DuckEngine e4;
        e4.prepare (sr, 2);
        e4.setCurve (Curve ({ { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }));
        s.mix = 0.5f;
        s.rate = Rates::quarter;
        auto d = constant (beat, 1.0f);
        run (e4, d, s, playing, full, sr);
        check (near (d.getSample (0, beat / 2), 0.5f, 0.01f), "MIX 50% with full duck = half volume");
    }

    std::cout << "MIDI trigger" << std::endl;
    {
        DuckEngine e;
        e.prepare (sr, 2);
        e.setCurve (Curve::shape (2));
        DuckEngine::Settings s;
        s.mix = 1.0f;
        s.midiTrigger = true;
        DuckEngine::Transport stopped;

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 10000);
        auto a = constant (40000, 1.0f);
        run (e, a, s, stopped, full, sr, midi);
        check (near (a.getSample (0, 5000), 1.0f, 0.01f), "before a note, nothing is ducked");
        check (a.getSample (0, 10200) < 0.1f, "a MIDI note starts the duck");
        check (near (a.getSample (0, 10000 + 12000), 0.5f, 0.02f), "the duck follows the curve after the note");
        check (near (a.getSample (0, 39000), 1.0f, 0.01f), "after one cycle the volume stays up");
    }

    std::cout << "Frequency Focus" << std::endl;
    {
        DuckEngine::Settings s;
        s.mix = 1.0f;
        auto silent = Curve ({ { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } });
        lt::dsp::FocusSettings low;
        low.band = lt::dsp::FocusSettings::low;   // 30 - 150 Hz
        auto n = (int) sr;

        auto test = [&] (double hz, lt::dsp::FocusSettings f)
        {
            DuckEngine e;
            e.prepare (sr, 2);
            e.setCurve (silent);
            auto a = sine (n, sr, hz);
            auto before = rms (a, n / 2, n / 2);
            run (e, a, s, playing, f, sr);
            return rms (a, n / 2, n / 2) / before;
        };

        check (test (80.0, low) < 0.1f, "LOW band: an 80 Hz kick is ducked");
        check (test (2000.0, low) > 0.95f, "LOW band: a 2 kHz melody passes clean");
        check (test (1000.0, full) < 0.01f, "FULL: everything is ducked");

        auto inv = low;
        inv.invert = true;
        check (test (80.0, inv) > 0.9f, "INVERT: the band passes, the rest is ducked");
        check (test (2000.0, inv) < 0.1f, "INVERT: 2 kHz is ducked");

        // with no ducking, splitting and adding back keeps the level (flat crossover)
        DuckEngine e;
        e.prepare (sr, 2);
        e.setCurve (Curve ({ { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }));
        juce::AudioBuffer<float> noise (2, n);
        juce::Random rnd (3);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
                noise.setSample (c, i, rnd.nextFloat() * 2.0f - 1.0f);
        auto before = rms (noise, n / 2, n / 2);
        run (e, noise, s, playing, low, sr);
        check (near (rms (noise, n / 2, n / 2) / before, 1.0f, 0.05f), "band split adds back to the same level");

        auto solo = low;
        solo.solo = true;
        DuckEngine e2;
        e2.prepare (sr, 2);
        e2.setCurve (Curve ({ { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }));
        auto hi = sine (n, sr, 3000.0);
        run (e2, hi, s, playing, solo, sr);
        check (rms (hi, n / 2, n / 2) < 0.02f, "SOLO: only the band is heard");
    }

    std::cout << (failures == 0 ? "All PULSAR tests passed." : juce::String (failures) + " test(s) failed.") << std::endl;
    return failures == 0 ? 0 : 1;
}
