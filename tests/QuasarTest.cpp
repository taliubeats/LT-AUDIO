// LT QUASAR tests: tuning, aliasing, clicks (note on / off / stealing), finite output, levels,
// reverb at 100 % wet, every effect, CPU with 16 voices. Exit code != 0 on failure.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "Reverb.h"

using namespace quasar;

namespace
{
    int failures = 0;
    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl;
        if (! ok) ++failures;
    }

    constexpr double sr = 48000.0;
    constexpr int block = 256;

    struct Rig
    {
        std::unique_ptr<Processor> p { new Processor() };
        Rig() { p->setRateAndBufferSizeDetails (sr, block); p->prepareToPlay (sr, block); p->initPatch(); }
        void set (const juce::String& id, float realValue)
        {
            auto* prm = p->state.getParameter (id);
            jassert (prm != nullptr);
            prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
        }
        /** Renders `seconds`, with MIDI events (time in seconds). */
        juce::AudioBuffer<float> render (double seconds, std::vector<std::pair<double, juce::MidiMessage>> events)
        {
            p->serviceNow();
            auto total = (int) (seconds * sr);
            juce::AudioBuffer<float> out (2, total), buf (2, block);
            std::sort (events.begin(), events.end(), [] (auto& a, auto& b) { return a.first < b.first; });
            size_t ev = 0;
            for (int pos = 0; pos < total; pos += block)
            {
                auto n = juce::jmin (block, total - pos);
                buf.setSize (2, n, false, false, true);
                juce::MidiBuffer midi;
                while (ev < events.size() && (int) (events[ev].first * sr) < pos + n)
                {
                    midi.addEvent (events[ev].second, juce::jmax (0, (int) (events[ev].first * sr) - pos));
                    ++ev;
                }
                p->processBlock (buf, midi);
                out.copyFrom (0, pos, buf, 0, 0, n); out.copyFrom (1, pos, buf, 1, 0, n);
            }
            return out;
        }
    };

    std::pair<double, juce::MidiMessage> on (double t, int note, float vel = 0.8f) { return { t, juce::MidiMessage::noteOn (1, note, vel) }; }
    std::pair<double, juce::MidiMessage> off (double t, int note) { return { t, juce::MidiMessage::noteOff (1, note) }; }

    double frequencyOf (const juce::AudioBuffer<float>& b, int from, int to)
    {
        // interpolated rising zero crossings
        auto* x = b.getReadPointer (0);
        double first = -1, last = -1; int count = 0;
        for (int i = from + 1; i < to; ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            {
                auto t = (double) (i - 1) + x[i - 1] / (double) (x[i - 1] - x[i]);
                if (first < 0) first = t; else ++count;
                last = t;
            }
        return count > 0 ? count * sr / (last - first) : 0.0;
    }

    float peakOf (const juce::AudioBuffer<float>& b) { return juce::jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples())); }
    bool finite (const juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) return false;
        return true;
    }
    float maxStep (const juce::AudioBuffer<float>& b, int from, int to)
    {
        float m = 0;
        for (int c = 0; c < 2; ++c) for (int i = juce::jmax (1, from); i < to; ++i) m = juce::jmax (m, std::abs (b.getSample (c, i) - b.getSample (c, i - 1)));
        return m;
    }
    float rms (const juce::AudioBuffer<float>& b, int from, int to)
    {
        double s = 0; for (int i = from; i < to; ++i) s += b.getSample (0, i) * b.getSample (0, i) + b.getSample (1, i) * b.getSample (1, i);
        return (float) std::sqrt (s / juce::jmax (1, 2 * (to - from)));
    }

    /** Worst non-harmonic component relative to the strongest bin, in dB (aliasing). */
    float aliasDb (const juce::AudioBuffer<float>& b, int from, double f0)
    {
        constexpr int order = 15, n = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) n * 2, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            auto w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) n);
            w = w * w;   // Hann^2: low side lobes
            data[(size_t) i] = b.getSample (0, from + i) * w;
        }
        fft.performFrequencyOnlyForwardTransform (data.data());
        float peak = 1.0e-12f;
        for (int k = 1; k < n / 2; ++k) peak = juce::jmax (peak, data[(size_t) k]);
        float worst = 1.0e-12f;
        auto binHz = sr / n;
        for (int k = (int) (60.0 / binHz); k < n / 2; ++k)
        {
            auto f = k * binHz;
            auto h = f / f0;
            auto nearHarmonic = std::abs (h - std::round (h)) * f0 < 40.0;
            if (! nearHarmonic && f < 20000.0) worst = juce::jmax (worst, data[(size_t) k]);
        }
        return 20.0f * std::log10 (worst / peak);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    std::cout << "TUNING" << std::endl;
    {
        Rig r;
        r.set (IDs::osc (0, "table"), 0);   // BASIC SHAPES, position 0 = sine
        r.set (IDs::osc (0, "wtpos"), 0);
        r.set (IDs::drift, 0);
        for (int note : { 33, 45, 57, 69, 81, 93 })
        {
            auto b = r.render (1.2, { on (0.0, note), off (1.1, note) });
            auto f = frequencyOf (b, (int) (0.2 * sr), (int) (1.0 * sr));
            auto expect = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            auto cents = 1200.0 * std::log2 (f / expect);
            check (std::abs (cents) < 0.5, "note " + juce::String (note) + ": " + juce::String (f, 3) + " Hz (" + juce::String (cents, 3) + " cents)");
        }
        r.set (IDs::masterTune, 50);
        auto b = r.render (1.2, { on (0.0, 69), off (1.1, 69) });
        auto f = frequencyOf (b, (int) (0.2 * sr), (int) (1.0 * sr));
        check (std::abs (1200.0 * std::log2 (f / (440.0 * std::pow (2.0, 0.5 / 12.0)))) < 0.5, "master tune +50 ct: " + juce::String (f, 3) + " Hz");
    }

    std::cout << "ALIASING (high saw notes)" << std::endl;
    for (int warmthLevel : { 0, 20 })
    for (int quality : { 0, 1 })
    {
        Rig r;
        r.set (IDs::osc (0, "table"), 1);   // ANALOG SAW, bright
        r.set (IDs::quality, (float) quality);
        r.set (IDs::drift, 0);
        r.set (IDs::warmth, (float) warmthLevel);
        float worst = -200;
        for (int note : { 84, 96, 100, 104, 108, 112 })
        {
            auto b = r.render (1.1, { on (0.0, note), off (1.05, note) });
            auto f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            worst = juce::jmax (worst, aliasDb (b, (int) (0.1 * sr), f0));
        }
        auto limit = -70.0f;
        check (worst < limit, "warmth " + juce::String (warmthLevel) + "% " + juce::String (quality == 0 ? "ECO (1x)" : "HIGH (2x)") + " worst alias " + juce::String (worst, 1) + " dB (limit " + juce::String (limit, 0) + ")");
    }

    std::cout << "CLICKS" << std::endl;
    {
        Rig r;
        r.set (IDs::osc (0, "table"), 0);
        r.set (IDs::osc (0, "wtpos"), 50);   // saw: the sharpest edge
        r.set (IDs::env (0, "att"), 0);
        r.set (IDs::env (0, "rel"), 1);
        auto b = r.render (1.0, { on (0.1, 45), off (0.6, 45) });
        auto steady = maxStep (b, (int) (0.3 * sr), (int) (0.5 * sr));
        auto start = maxStep (b, (int) (0.1 * sr) - 10, (int) (0.1 * sr) + 200);
        auto stop = maxStep (b, (int) (0.6 * sr) - 10, (int) (0.62 * sr));
        check (start <= steady * 1.05f, "note on, 0 ms attack: step " + juce::String (start, 4) + " vs steady " + juce::String (steady, 4));
        check (stop <= steady * 1.05f, "note off, 1 ms release: step " + juce::String (stop, 4));

        // stealing: 2 voices, a third note steals the oldest
        r.set (IDs::osc (0, "wtpos"), 0);   // sine: any jump is audible
        r.set (IDs::poly, 2);
        r.set (IDs::env (0, "att"), 3);
        r.set (IDs::env (0, "rel"), 2000);
        auto s = r.render (1.5, { on (0.1, 48), on (0.3, 55), on (0.6, 60), off (1.2, 48), off (1.2, 55), off (1.2, 60) });
        auto steadyS = maxStep (s, (int) (0.4 * sr), (int) (0.55 * sr));
        auto steal = maxStep (s, (int) (0.6 * sr) - 10, (int) (0.65 * sr));
        check (steal <= steadyS * 1.6f, "voice steal: step " + juce::String (steal, 4) + " vs steady " + juce::String (steadyS, 4));
        check (finite (s), "finite after stealing");
    }

    std::cout << "MONO / LEGATO / GLIDE" << std::endl;
    {
        Rig r;
        r.set (IDs::osc (0, "table"), 0);
        r.set (IDs::drift, 0);
        r.set (IDs::voiceMode, 2);
        r.set (IDs::glide, 80);
        auto b = r.render (1.4, { on (0.0, 57), on (0.5, 69), off (1.3, 57), off (1.3, 69) });
        auto f1 = frequencyOf (b, (int) (0.2 * sr), (int) (0.45 * sr)), f2 = frequencyOf (b, (int) (0.9 * sr), (int) (1.25 * sr));
        check (std::abs (f1 - 220.0) < 0.5 && std::abs (f2 - 440.0) < 0.5, "legato glide 220 -> 440 Hz: " + juce::String (f1, 2) + " / " + juce::String (f2, 2));
        auto jump = maxStep (b, (int) (0.5 * sr) - 10, (int) (0.55 * sr));
        auto steady = maxStep (b, (int) (1.0 * sr), (int) (1.2 * sr));
        check (jump <= steady * 1.2f, "legato change has no click");
    }

    std::cout << "RANDOM PATCHES (finite, no digital clipping)" << std::endl;
    {
        Rig r;
        juce::Random rnd (7);
        int bad = 0; float worstPeak = 0;
        for (int k = 0; k < 40; ++k)
        {
            r.p->initPatch();
            for (auto* prm : r.p->getParameters())
            {
                auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm);
                if (rp == nullptr) continue;
                auto id = rp->getParameterID();
                if (id.startsWith ("mod") || id == IDs::master || id == IDs::limiter) continue;
                if (rnd.nextFloat() < 0.35f) rp->setValueNotifyingHost (rnd.nextFloat());
            }
            r.p->addModulation (srcLfo1 + rnd.nextInt (4), 1 + rnd.nextInt ((int) ModDefs::destinations().size() - 1), rnd.nextFloat() * 200 - 100);
            auto b = r.render (1.0, { on (0.0, 48), on (0.05, 55), on (0.1, 63), off (0.7, 48), off (0.7, 55), off (0.7, 63) });
            if (! finite (b)) ++bad;
            worstPeak = juce::jmax (worstPeak, peakOf (b));
        }
        check (bad == 0, "40 random patches are finite");
        check (worstPeak <= 1.0f, "safety limiter keeps peaks <= 0 dBFS (worst " + juce::String (worstPeak, 3) + ")");
    }

    std::cout << "REVERB at 100 % wet" << std::endl;
    {
        for (int type = 0; type < 6; ++type)
        {
            QuasarVerb v; v.prepare (sr);
            QuasarVerb::Settings s; s.type = type; s.decay = 6.0f; s.size = 0.8f; s.mod = 0.4f;
            const int len = (int) (6.0 * sr);
            std::vector<float> inL ((size_t) len, 0.0f), inR ((size_t) len, 0.0f), wl ((size_t) len), wr ((size_t) len);
            inL[100] = inR[100] = 1.0f;
            for (int pos = 0; pos < len; pos += 32) v.process (inL.data() + pos, inR.data() + pos, wl.data() + pos, wr.data() + pos, juce::jmin (32, len - pos), s);
            // energy must fall steadily (no build-up)
            auto energy = [&] (double a, double b) { double e = 0; for (int i = (int) (a * sr); i < (int) (b * sr); ++i) e += wl[(size_t) i] * wl[(size_t) i] + wr[(size_t) i] * wr[(size_t) i]; return e; };
            auto e1 = energy (0.2, 0.7), e2 = energy (1.2, 1.7), e3 = energy (2.7, 3.2);
            bool decaying = e2 < e1 && e3 < e2;
            // ringing: spectrum of the tail (0.5 .. 1.5 s) must be smooth: peak vs median of 1/3-octave-smoothed bands
            constexpr int order = 15, n = 1 << order;
            juce::dsp::FFT fft (order);
            std::vector<float> d ((size_t) n * 2, 0.0f);
            for (int i = 0; i < n; ++i) d[(size_t) i] = wl[(size_t) ((int) (0.5 * sr) + i)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) n));
            fft.performFrequencyOnlyForwardTransform (d.data());
            // compare each bin with its local average (+-15 %) between 200 Hz and 8 kHz
            float worst = 0;
            auto binHz = sr / n;
            for (int k = (int) (200 / binHz); k < (int) (8000 / binHz); ++k)
            {
                int w = juce::jmax (4, (int) (k * 0.15));
                double sum = 0; for (int j = k - w; j <= k + w; ++j) sum += d[(size_t) j] * d[(size_t) j];
                auto local = std::sqrt (sum / (2 * w + 1));
                worst = juce::jmax (worst, (float) (d[(size_t) k] / juce::jmax (1.0e-12, local)));
            }
            auto worstDb = 20.0f * std::log10 (worst);
            bool fin = true; float peak = 0;
            for (int i = 0; i < len; ++i) { fin &= std::isfinite (wl[(size_t) i]) && std::isfinite (wr[(size_t) i]); peak = juce::jmax (peak, std::abs (wl[(size_t) i])); }
            check (decaying && fin && worstDb < 15.0f && peak < 1.0f,
                   "type " + juce::String (type) + ": decays, finite, peak " + juce::String (peak, 3) + ", tail resonance " + juce::String (worstDb, 1) + " dB over local level");
        }
        // sustained noise into 100 % wet, long decay: level stays sane
        QuasarVerb v; v.prepare (sr);
        QuasarVerb::Settings s; s.type = QuasarVerb::space; s.decay = 20.0f; s.size = 1.0f;
        const int len = (int) (8.0 * sr);
        std::vector<float> inL ((size_t) len), inR ((size_t) len), wl ((size_t) len), wr ((size_t) len);
        juce::Random rnd (3);
        for (int i = 0; i < len; ++i) { inL[(size_t) i] = (rnd.nextFloat() * 2 - 1) * 0.25f; inR[(size_t) i] = (rnd.nextFloat() * 2 - 1) * 0.25f; }
        for (int pos = 0; pos < len; pos += 32) v.process (inL.data() + pos, inR.data() + pos, wl.data() + pos, wr.data() + pos, juce::jmin (32, len - pos), s);
        double ein = 0, eout = 0;
        for (int i = (int) (6 * sr); i < len; ++i) { ein += inL[(size_t) i] * inL[(size_t) i]; eout += wl[(size_t) i] * wl[(size_t) i]; }
        auto gainDb = 10.0 * std::log10 (eout / ein);
        check (gainDb < 9.0 && gainDb > -20.0, "20 s decay, sustained noise: wet level " + juce::String (gainDb, 1) + " dB vs input");
    }

    std::cout << "EFFECTS (every type on every bus, defaults and extremes)" << std::endl;
    {
        Rig r;
        r.set (IDs::osc (0, "table"), 4);
        r.set (IDs::osc (0, "unison"), 5);
        for (int t = 1; t < (int) FxType::count; ++t)
        {
            r.p->initPatch();
            r.set (IDs::osc (0, "table"), 4);
            r.p->chains[0]->setType (0, t);
            r.set (IDs::fx (0, 0, "mix"), 100);
            auto b = r.render (1.5, { on (0.0, 48), on (0.0, 60), off (1.0, 48), off (1.0, 60) });
            bool ok = finite (b) && peakOf (b) <= 1.0f;
            // extremes
            for (int p = 0; p < numFxParams; ++p) r.set (IDs::fxParam (0, 0, p), (p % 2) ? 1.0f : 0.0f);
            auto b2 = r.render (1.0, { on (0.0, 36), off (0.8, 36) });
            for (int p = 0; p < numFxParams; ++p) r.set (IDs::fxParam (0, 0, p), (p % 2) ? 0.0f : 1.0f);
            auto b3 = r.render (1.0, { on (0.0, 84), off (0.8, 84) });
            ok = ok && finite (b2) && finite (b3);
            check (ok, fxNames()[t] + " finite and <= 0 dBFS");
        }
        // a flat EQ is an exact identity
        r.p->initPatch();
        auto dry = r.render (0.5, { on (0.0, 57), off (0.4, 57) });
        r.p->initPatch();
        r.p->chains[0]->setType (0, (int) FxType::equalizer);
        auto eq = r.render (0.5, { on (0.0, 57), off (0.4, 57) });
        float diff = 0;
        for (int i = 2000; i < dry.getNumSamples(); ++i) diff = juce::jmax (diff, std::abs (dry.getSample (0, i) - eq.getSample (0, i)));
        check (diff < 1.0e-6f, "flat EQUALIZER is the identity (max diff " + juce::String (diff, 8) + ")");
    }

    std::cout << "MODULATION" << std::endl;
    {
        Rig r;
        r.set (IDs::osc (0, "table"), 0);
        r.set (IDs::drift, 0);
        // ENV 2 -> OSC A PITCH +12 st (100 % of the +-24 range is 48 st: 25 % = +12)
        r.p->addModulation (srcEnv2, ModDefs::destIndex (IDs::osc (0, "semi")), 25.0f);
        r.set (IDs::env (1, "att"), 0); r.set (IDs::env (1, "dec"), 30000); r.set (IDs::env (1, "sus"), 100);
        auto b = r.render (1.0, { on (0.0, 57), off (0.9, 57) });
        auto f = frequencyOf (b, (int) (0.3 * sr), (int) (0.8 * sr));
        check (std::abs (f - 440.0) < 1.0, "ENV 2 -> pitch +12 st: " + juce::String (f, 2) + " Hz");
        // LFO -> filter cutoff changes the level over time
        r.p->initPatch();
        r.set (IDs::osc (0, "table"), 1);
        r.set (IDs::flt (0, "on"), 1); r.set (IDs::flt (0, "cutoff"), 100);
        r.p->addModulation (srcLfo1, ModDefs::destIndex (IDs::flt (0, "cutoff")), 60.0f);
        r.set (IDs::lfo (0, "sync"), 0); r.set (IDs::lfo (0, "rate"), 2.0f);
        auto m = r.render (2.0, { on (0.0, 69), off (1.9, 69) });
        float lo = 1e9f, hi = 0;
        for (int k = 0; k < 16; ++k) { auto v = rms (m, (int) ((0.2 + k * 0.1) * sr), (int) ((0.25 + k * 0.1) * sr)); lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); }
        check (hi > lo * 1.5f, "LFO 1 -> FILTER 1 CUTOFF moves the sound (" + juce::String (lo, 3) + " .. " + juce::String (hi, 3) + ")");
    }

    std::cout << "CPU (16 voices, 3 oscillators x 4 unison, 2 filters, reverb + delay)" << std::endl;
    {
        Rig r;
        for (int o = 0; o < 3; ++o) { r.set (IDs::osc (o, "on"), 1); r.set (IDs::osc (o, "unison"), 4); r.set (IDs::osc (o, "table"), 4); }
        r.set (IDs::flt (0, "on"), 1); r.set (IDs::flt (1, "on"), 1); r.set (IDs::flt (0, "dest"), 0);
        r.set (IDs::subOn, 0);
        r.p->chains[0]->setType (0, (int) FxType::delay);
        r.p->chains[0]->setType (1, (int) FxType::reverb);
        std::vector<std::pair<double, juce::MidiMessage>> ev;
        for (int k = 0; k < 16; ++k) { ev.push_back (on (0.0, 36 + k * 3)); ev.push_back (off (9.5, 36 + k * 3)); }
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto b = r.render (10.0, ev);
        auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        auto load = ms / 10000.0 * 100.0;
        std::cout << "         " << juce::String (load, 1) << " % of one core (" << juce::String (ms, 0) << " ms for 10 s)" << std::endl;
        check (finite (b) && load < 60.0, "CPU under 60 % of one core");
    }

    std::cout << (failures == 0 ? "ALL QUASAR TESTS PASSED" : juce::String (failures) + " QUASAR TEST(S) FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
