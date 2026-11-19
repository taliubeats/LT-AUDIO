// Fits LT QUASAR to reference one-shots: for each reference it renders the same note and searches the
// synth parameters (oscillators, wavetable, filter, envelopes, unison, noise, effects) until the
// time-frequency picture (40 log bands x 20 ms frames, attack weighted) and the stereo width match.
// Usage: quasar_match <list.txt> <out.json> [evaluations per sound]
//   list.txt: one line per reference "path|CATEGORY"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

using namespace quasar;

namespace
{
    constexpr int numBands = 40;

    struct Features
    {
        std::vector<std::array<float, numBands>> frames;   // dB re. the loudest band of the sound, floor -60
        float width = 0;
    };

    /** Band energies every 20 ms (Hann 2048-ish window), relative to the global peak. */
    Features analyse (const juce::AudioBuffer<float>& b, double sr, double maxSeconds)
    {
        Features f;
        auto hop = (int) (sr * 0.02);
        const int order = 11, n = 1 << order;
        juce::dsp::FFT fft (order);
        std::array<float, numBands + 1> edges {};
        for (int i = 0; i <= numBands; ++i) edges[(size_t) i] = 40.0f * std::pow (16000.0f / 40.0f, (float) i / numBands);
        std::vector<float> d ((size_t) n * 2);
        auto total = juce::jmin (b.getNumSamples(), (int) (maxSeconds * sr));
        double mid = 0, side = 0;
        for (int i = 0; i < total; ++i)
        {
            auto l = b.getSample (0, i), r = b.getSample (b.getNumChannels() > 1 ? 1 : 0, i);
            mid += (l + r) * (l + r) * 0.25; side += (l - r) * (l - r) * 0.25;
        }
        f.width = (float) std::sqrt (side / juce::jmax (1.0e-12, mid));
        float peak = -200;
        for (int pos = 0; pos + n <= total || (pos == 0 && total > 0); pos += hop)
        {
            std::fill (d.begin(), d.end(), 0.0f);
            for (int i = 0; i < n && pos + i < total; ++i)
            {
                auto w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) n);
                d[(size_t) i] = 0.5f * (b.getSample (0, pos + i) + b.getSample (b.getNumChannels() > 1 ? 1 : 0, pos + i)) * w;
            }
            fft.performFrequencyOnlyForwardTransform (d.data());
            std::array<float, numBands> fr {};
            for (int k = 1; k < n / 2; ++k)
            {
                auto hz = (float) (k * sr / n);
                if (hz < edges[0] || hz >= edges[numBands]) continue;
                auto band = (int) (std::log (hz / 40.0f) / std::log (16000.0f / 40.0f) * numBands);
                fr[(size_t) juce::jlimit (0, numBands - 1, band)] += d[(size_t) k] * d[(size_t) k];
            }
            for (auto& v : fr) { v = 10.0f * std::log10 (v + 1.0e-12f); peak = juce::jmax (peak, v); }
            f.frames.push_back (fr);
            if (pos + n > total) break;
        }
        for (auto& fr : f.frames) for (auto& v : fr) v = juce::jmax (-60.0f, v - peak);
        return f;
    }

    float distance (const Features& ref, const Features& c)
    {
        double sum = 0, wsum = 0;
        auto frames = juce::jmin (ref.frames.size(), c.frames.size());
        for (size_t t = 0; t < frames; ++t)
        {
            auto w = t < 10 ? 3.0 : t < 30 ? 1.6 : 1.0;   // attack and early decay matter most
            double e = 0;
            for (int k = 0; k < numBands; ++k) e += std::abs (ref.frames[t][(size_t) k] - c.frames[t][(size_t) k]);
            sum += w * e / numBands; wsum += w;
        }
        // missing frames (candidate too short) count as silence
        for (size_t t = frames; t < ref.frames.size(); ++t)
        {
            double e = 0;
            for (int k = 0; k < numBands; ++k) e += std::abs (ref.frames[t][(size_t) k] + 60.0f);
            sum += e / numBands; wsum += 1;
        }
        return (float) (sum / juce::jmax (1.0, wsum)) + 12.0f * std::abs (ref.width - c.width);
    }

    // ------------------------------------------------------------------ the search space
    const int tables[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35 };
    const int unisons[] = { 1, 1, 2, 3, 4, 5, 7 };
    const int warps[] = { 0, 0, 0, 2, 4, 5, 7, 9 };
    const int filters[] = { 0, 1, 3, 3, 7, 5, 12, 2 };
    const int noises[] = { 3, 5, 1, 4 };

    struct Cand
    {
        // continuous values are normalised 0..1 (mapped in apply())
        int typeA = 0, tableA = 1, uniA = 1, warpA = 0, tableB = 0, octB = 0, semiB = 0, ftype = 0, noiseType = 0, subShape = 0;
        bool onB = false, sub = false, noise = false, filt = true;
        float posA = 0, detA = 0.3f, warpAmt = 0, posB = 0, levelB = 0.5f, subLevel = 0.5f, noiseLevel = 0.3f,
              cutoff = 0.6f, reso = 0.15f, drive = 0.1f, key = 0.3f,
              att = 0.1f, dec = 0.5f, sus = 0.0f, rel = 0.4f, dec2 = 0.4f, envCut = 0.3f, envPos = 0.5f, pitch = 0.0f,
              lfoRate = 0.3f, lfoPos = 0.0f, sat = 0.2f, chorus = 0.2f, delay = 0.0f, verb = 0.2f, verbDecay = 0.4f, spread = 0.8f, blend = 0.7f;
    };

    float lg (float t, float lo, float hi) { return lo * std::pow (hi / lo, juce::jlimit (0.0f, 1.0f, t)); }

    struct Rig
    {
        std::unique_ptr<Processor> p { new Processor() };
        double sr = 48000; int block = 512;
        Rig() { p->setRateAndBufferSizeDetails (sr, block); p->prepareToPlay (sr, block); }

        void set (const juce::String& id, float v)
        {
            if (auto* prm = p->param (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        }
        void setupChain()
        {
            p->initPatch();
            p->chains[0]->setType (0, (int) FxType::saturation);
            p->chains[0]->setType (1, (int) FxType::chorus);
            p->chains[0]->setType (2, (int) FxType::delay);
            p->chains[0]->setType (3, (int) FxType::reverb);
            p->serviceNow();
            juce::AudioBuffer<float> b (2, block); juce::MidiBuffer m;
            for (int i = 0; i < 4; ++i) p->processBlock (b, m);
            // fixed routes, their amounts are part of the search
            auto route = [this] (int slot, int src, const juce::String& dst, bool bip)
            {
                set (IDs::mod (slot, "src"), (float) src); set (IDs::mod (slot, "dst"), (float) ModDefs::destIndex (dst));
                set (IDs::mod (slot, "bip"), bip ? 1.0f : 0.0f); set (IDs::mod (slot, "on"), 1); set (IDs::mod (slot, "out"), 100);
            };
            route (0, srcEnv2, IDs::flt (0, "cutoff"), false);
            route (1, srcEnv2, IDs::osc (0, "wtpos"), false);
            route (2, srcLfo1, IDs::osc (0, "wtpos"), true);
            route (3, srcVelocity, IDs::flt (0, "cutoff"), false);
            set (IDs::mod (3, "amt"), 10);
            set (IDs::lfo (0, "sync"), 0); set (IDs::lfo (0, "mode"), 0);
        }

        void apply (const Cand& c, bool bass)
        {
            set (IDs::osc (0, "on"), 1); set (IDs::osc (0, "type"), (float) c.typeA); set (IDs::osc (0, "table"), (float) c.tableA);
            set (IDs::osc (0, "wtpos"), c.posA * 100); set (IDs::osc (0, "unison"), (float) c.uniA); set (IDs::osc (0, "detune"), c.detA * 60);
            set (IDs::osc (0, "spread"), c.spread * 100); set (IDs::osc (0, "blend"), c.blend * 100);
            set (IDs::osc (0, "warpmode"), (float) c.warpA); set (IDs::osc (0, "warp"), c.warpAmt * 80);
            set (IDs::osc (1, "on"), c.onB ? 1.0f : 0.0f); set (IDs::osc (1, "table"), (float) c.tableB); set (IDs::osc (1, "wtpos"), c.posB * 100);
            set (IDs::osc (1, "oct"), (float) c.octB); set (IDs::osc (1, "semi"), (float) c.semiB); set (IDs::osc (1, "level"), 30 + c.levelB * 50);
            set (IDs::subOn, c.sub ? 1.0f : 0.0f); set (IDs::subLevel, 30 + c.subLevel * 50); set (IDs::subShape, (float) c.subShape);
            set (IDs::noiseOn, c.noise ? 1.0f : 0.0f); set (IDs::noiseType, (float) c.noiseType); set (IDs::noiseLevel, 10 + c.noiseLevel * 45);
            set (IDs::flt (0, "on"), c.filt ? 1.0f : 0.0f); set (IDs::flt (0, "type"), (float) c.ftype);
            set (IDs::flt (0, "cutoff"), lg (c.cutoff, 80, 16000)); set (IDs::flt (0, "reso"), c.reso * 70); set (IDs::flt (0, "drive"), c.drive * 60);
            set (IDs::flt (0, "key"), c.key * 60);
            set (IDs::env (0, "att"), lg (c.att, 0.5f, 1500)); set (IDs::env (0, "dec"), lg (c.dec, 30, 6000)); set (IDs::env (0, "sus"), c.sus * 100);
            set (IDs::env (0, "rel"), lg (c.rel, 20, 4000)); set (IDs::env (0, "acurve"), 0);
            set (IDs::env (1, "att"), 0.5f); set (IDs::env (1, "dec"), lg (c.dec2, 20, 3000)); set (IDs::env (1, "sus"), 0);
            set (IDs::mod (0, "amt"), c.envCut * 60); set (IDs::mod (1, "amt"), (c.envPos - 0.5f) * 120);
            set (IDs::lfo (0, "rate"), lg (c.lfoRate, 0.1f, 8)); set (IDs::mod (2, "amt"), c.lfoPos * 40);
            set (IDs::envPAmount, bass ? c.pitch * 24 : 0); set (IDs::env (3, "att"), 0); set (IDs::env (3, "dec"), 80); set (IDs::env (3, "sus"), 0);
            set (IDs::voiceMode, bass ? 2.0f : 0.0f);
            auto fx = [this] (int slot, int param, float real)
            {
                auto& spec = fxInfo (p->chains[0]->getType (slot)).params[(size_t) param];
                if (auto* prm = p->param (IDs::fxParam (0, slot, param))) prm->setValueNotifyingHost (spec.toNorm (real));
            };
            set (IDs::fx (0, 0, "mix"), c.sat > 0.05f ? 100.0f : 0.0f); fx (0, 1, c.sat * 60);
            set (IDs::fx (0, 1, "mix"), c.chorus * 70);
            set (IDs::fx (0, 2, "mix"), c.delay * 40);
            set (IDs::fx (0, 3, "mix"), c.verb * 50); fx (3, 3, lg (c.verbDecay, 0.5f, 6.0f));
        }

        juce::AudioBuffer<float> render (int note, double hold, double length)
        {
            p->reset();
            auto total = (int) (length * sr);
            juce::AudioBuffer<float> all (2, total), b (2, block);
            for (int pos = 0; pos < total; pos += block)
            {
                auto n = juce::jmin (block, total - pos);
                b.setSize (2, n, false, false, true);
                juce::MidiBuffer m;
                if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
                auto off = (int) (hold * sr);
                if (off >= pos && off < pos + n) m.addEvent (juce::MidiMessage::noteOff (1, note), off - pos);
                p->processBlock (b, m);
                all.copyFrom (0, pos, b, 0, 0, n); all.copyFrom (1, pos, b, 1, 0, n);
            }
            return all;
        }
    };

    Cand randomCand (juce::Random& r, const juce::String& cat)
    {
        Cand c;
        auto pick = [&] (auto& arr) { return arr[(size_t) r.nextInt ((int) (sizeof (arr) / sizeof (arr[0])))]; };
        c.typeA = r.nextFloat() < 0.25f ? 1 : 0;
        c.tableA = pick (tables); c.posA = r.nextFloat(); c.uniA = pick (unisons); c.detA = r.nextFloat() * 0.6f;
        c.warpA = pick (warps); c.warpAmt = r.nextFloat() * 0.6f; c.spread = 0.4f + 0.6f * r.nextFloat(); c.blend = r.nextFloat();
        c.onB = r.nextBool(); c.tableB = pick (tables); c.posB = r.nextFloat(); c.octB = r.nextInt (3) - 1; c.semiB = r.nextFloat() < 0.2f ? 7 : 0;
        c.levelB = r.nextFloat(); c.sub = r.nextFloat() < 0.3f; c.subLevel = r.nextFloat(); c.subShape = r.nextInt (3);
        c.noise = r.nextFloat() < 0.25f; c.noiseType = pick (noises); c.noiseLevel = r.nextFloat();
        c.filt = r.nextFloat() < 0.85f; c.ftype = pick (filters); c.cutoff = r.nextFloat(); c.reso = r.nextFloat() * 0.6f; c.drive = r.nextFloat() * 0.5f; c.key = r.nextFloat();
        c.att = r.nextFloat() * 0.4f; c.dec = r.nextFloat(); c.sus = r.nextFloat() < 0.5f ? 0.0f : r.nextFloat(); c.rel = r.nextFloat();
        c.dec2 = r.nextFloat(); c.envCut = r.nextFloat(); c.envPos = r.nextFloat(); c.pitch = r.nextFloat() * 0.6f;
        c.lfoRate = r.nextFloat(); c.lfoPos = r.nextFloat() < 0.6f ? 0.0f : r.nextFloat();
        c.sat = r.nextFloat() * 0.6f; c.chorus = r.nextFloat() < 0.5f ? 0.0f : r.nextFloat(); c.delay = r.nextFloat() < 0.5f ? 0.0f : r.nextFloat();
        c.verb = r.nextFloat() < 0.3f ? 0.0f : r.nextFloat(); c.verbDecay = r.nextFloat();
        if (cat == "PAD") { c.att = 0.55f + 0.4f * r.nextFloat(); c.sus = 0.5f + 0.5f * r.nextFloat(); c.uniA = juce::jmax (3, c.uniA); c.rel = 0.6f + 0.4f * r.nextFloat(); }
        if (cat == "PLUCK" || cat == "BELL" || cat == "KEYS") { c.sus = r.nextFloat() * 0.3f; c.att = r.nextFloat() * 0.15f; }
        if (cat == "BASS" || cat == "808" || cat == "SUB" || cat == "REESE")
        {
            c.uniA = cat == "REESE" ? 3 + r.nextInt (3) : 1; c.chorus = 0; c.delay = 0; c.verb = 0; c.spread = cat == "REESE" ? 0.6f : 0.0f;
            if (cat == "808" || cat == "SUB") { c.tableA = r.nextBool() ? 19 : (r.nextBool() ? 0 : 20); c.posA = r.nextFloat() * 0.4f; c.filt = r.nextFloat() < 0.3f; }
        }
        return c;
    }

    Cand mutate (Cand c, juce::Random& r, float sigma, const juce::String& cat)
    {
        auto g = [&] (float& v) { v = juce::jlimit (0.0f, 1.0f, v + (r.nextFloat() * 2 - 1) * sigma); };
        auto count = 1 + r.nextInt (3);
        for (int k = 0; k < count; ++k)
            switch (r.nextInt (24))
            {
                case 0: c.tableA = tables[(size_t) r.nextInt (36)]; break;
                case 1: g (c.posA); break;
                case 2: c.uniA = unisons[(size_t) r.nextInt (7)]; g (c.detA); break;
                case 3: c.warpA = warps[(size_t) r.nextInt (8)]; g (c.warpAmt); break;
                case 4: c.onB = ! c.onB; break;
                case 5: c.tableB = tables[(size_t) r.nextInt (36)]; g (c.posB); g (c.levelB); break;
                case 6: c.octB = r.nextInt (3) - 1; break;
                case 7: c.sub = ! c.sub; g (c.subLevel); break;
                case 8: c.noise = ! c.noise; g (c.noiseLevel); break;
                case 9: c.ftype = filters[(size_t) r.nextInt (8)]; break;
                case 10: case 11: g (c.cutoff); break;
                case 12: g (c.reso); g (c.drive); break;
                case 13: g (c.att); break;
                case 14: case 15: g (c.dec); break;
                case 16: g (c.sus); break;
                case 17: g (c.rel); break;
                case 18: g (c.dec2); g (c.envCut); break;
                case 19: g (c.envPos); break;
                case 20: g (c.sat); g (c.chorus); break;
                case 21: g (c.delay); break;
                case 22: g (c.verb); g (c.verbDecay); break;
                default: g (c.spread); g (c.blend); c.typeA = r.nextFloat() < 0.2f ? 1 - c.typeA : c.typeA; break;
            }
        if (cat == "BASS" || cat == "808" || cat == "SUB") { c.chorus = 0; c.delay = 0; c.verb = juce::jmin (c.verb, 0.05f); }
        return c;
    }

    juce::var toVar (const Cand& c, bool bass)
    {
        auto o = std::make_unique<juce::DynamicObject>();
        auto put = [&] (const char* k, float v) { o->setProperty (k, v); };
        put ("typeA", (float) c.typeA); put ("tableA", (float) c.tableA); put ("posA", c.posA); put ("uniA", (float) c.uniA); put ("detA", c.detA);
        put ("warpA", (float) c.warpA); put ("warpAmt", c.warpAmt); put ("spread", c.spread); put ("blend", c.blend);
        put ("onB", c.onB ? 1.0f : 0.0f); put ("tableB", (float) c.tableB); put ("posB", c.posB); put ("octB", (float) c.octB); put ("semiB", (float) c.semiB); put ("levelB", c.levelB);
        put ("sub", c.sub ? 1.0f : 0.0f); put ("subLevel", c.subLevel); put ("subShape", (float) c.subShape);
        put ("noise", c.noise ? 1.0f : 0.0f); put ("noiseType", (float) c.noiseType); put ("noiseLevel", c.noiseLevel);
        put ("filt", c.filt ? 1.0f : 0.0f); put ("ftype", (float) c.ftype); put ("cutoff", c.cutoff); put ("reso", c.reso); put ("drive", c.drive); put ("key", c.key);
        put ("att", c.att); put ("dec", c.dec); put ("sus", c.sus); put ("rel", c.rel); put ("dec2", c.dec2); put ("envCut", c.envCut); put ("envPos", c.envPos);
        put ("pitch", bass ? c.pitch : 0.0f); put ("lfoRate", c.lfoRate); put ("lfoPos", c.lfoPos);
        put ("sat", c.sat); put ("chorus", c.chorus); put ("delay", c.delay); put ("verb", c.verb); put ("verbDecay", c.verbDecay);
        return juce::var (o.release());
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3) { std::cout << "usage: quasar_match list.txt out.json [evals]" << std::endl; return 1; }
    auto lines = juce::StringArray::fromLines (juce::File (juce::String (argv[1])).loadFileAsString());
    auto evals = argc > 3 ? juce::String (argv[3]).getIntValue() : 140;
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    Rig rig;
    juce::Array<juce::var> results;
    juce::Random rnd (12345);
    int done = 0;
    for (auto& line : lines)
    {
        if (line.trim().isEmpty()) continue;
        juce::File file (line.upToFirstOccurrenceOf ("|", false, false));
        auto cat = line.fromFirstOccurrenceOf ("|", false, false).upToFirstOccurrenceOf ("@", false, false).trim();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
        if (r == nullptr) continue;
        auto len = (int) juce::jmin<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 3.0));
        juce::AudioBuffer<float> ref ((int) juce::jmax (1u, juce::jmin (2u, r->numChannels)), len);
        r->read (&ref, 0, len, 0, true, true);
        if (ref.getNumChannels() == 1) { ref.setSize (2, len, true); ref.copyFrom (1, 0, ref, 0, 0, len); }
        // trim the silence before the sound
        int start = 0;
        auto pk = ref.getMagnitude (0, len);
        while (start < len - 1 && std::abs (ref.getSample (0, start)) < pk * 0.01f) ++start;
        juce::AudioBuffer<float> trimmed (2, len - start);
        for (int c = 0; c < 2; ++c) trimmed.copyFrom (c, 0, ref, c, start, len - start);
        auto refF = analyse (trimmed, r->sampleRate, 3.0);
        auto seconds = juce::jmin (3.0, (double) trimmed.getNumSamples() / r->sampleRate);
        if (seconds < 0.15) continue;

        // note: from the analysis (YIN in the Python step) or C3/C1 by category
        auto noteText = line.fromLastOccurrenceOf ("@", false, false);
        auto bass = cat == "BASS" || cat == "808" || cat == "SUB" || cat == "REESE";
        int note = noteText.containsOnly ("0123456789") && noteText.isNotEmpty() ? noteText.getIntValue() : (bass ? 36 : 60);
        // hold: until the reference falls 25 dB under its peak (one-shots are a single note)
        size_t holdFrames = 0; float maxSum = -1e9f;
        std::vector<float> loud;
        for (auto& fr : refF.frames) { float s = -1e9f; for (auto v : fr) s = juce::jmax (s, v); loud.push_back (s); maxSum = juce::jmax (maxSum, s); }
        for (size_t t = 0; t < loud.size(); ++t) if (loud[t] > maxSum - 25.0f) holdFrames = t;
        auto hold = juce::jlimit (0.08, seconds, (double) holdFrames * 0.02);

        rig.setupChain();
        struct Scored { Cand c; float d; };
        std::vector<Scored> pool;
        auto evaluate = [&] (const Cand& c)
        {
            rig.apply (c, bass);
            auto out = rig.render (note, hold, seconds);
            for (int i = 0; i < out.getNumSamples(); ++i)
                if (! std::isfinite (out.getSample (0, i))) return 1.0e9f;
            return distance (refF, analyse (out, rig.sr, 3.0));
        };
        for (int i = 0; i < evals / 3; ++i) { auto c = randomCand (rnd, cat); pool.push_back ({ c, evaluate (c) }); }
        std::sort (pool.begin(), pool.end(), [] (auto& a, auto& b) { return a.d < b.d; });
        pool.resize (std::min<size_t> (5, pool.size()));
        for (int i = 0; i < evals - evals / 3; ++i)
        {
            auto sigma = 0.3f * (1.0f - (float) i / (float) evals) + 0.04f;
            auto& parent = pool[(size_t) rnd.nextInt ((int) pool.size())];
            auto c = mutate (parent.c, rnd, sigma, cat);
            auto d = evaluate (c);
            if (d < pool.back().d) { pool.back() = { c, d }; std::sort (pool.begin(), pool.end(), [] (auto& a, auto& b) { return a.d < b.d; }); }
        }
        auto o = std::make_unique<juce::DynamicObject>();
        o->setProperty ("file", file.getFullPathName());
        o->setProperty ("category", cat);
        o->setProperty ("note", note);
        o->setProperty ("hold", hold);
        o->setProperty ("distance", pool.front().d);
        o->setProperty ("params", toVar (pool.front().c, bass));
        results.add (juce::var (o.release()));
        ++done;
        std::cout << done << " " << file.getFileName() << " " << cat << " d=" << pool.front().d << std::endl;
        juce::File (juce::String (argv[2])).replaceWithText (juce::JSON::toString (juce::var (results)));   // keep partial results
    }
    return 0;
}
