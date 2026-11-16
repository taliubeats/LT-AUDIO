// Tests for the LT NEBULA engine: grains sound, mix, freeze, pitch step, tape stop, stability.
#include "NebulaEngine.h"
#include "Modulation.h"
#include "Banks.h"
#include "Params.h"

using namespace nebula;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl;
    if (! ok) ++failures;
}

static float rms (const juce::AudioBuffer<float>& b, int from, int to)
{
    double s = 0;
    for (int c = 0; c < 2; ++c) for (int i = from; i < to; ++i) s += (double) b.getSample (c, i) * b.getSample (c, i);
    return (float) std::sqrt (s / (2.0 * (to - from)));
}

/** Average frequency by counting zero crossings. */
static float freq (const juce::AudioBuffer<float>& b, int from, int to, double sr)
{
    int z = 0;
    for (int i = from + 1; i < to; ++i) if ((b.getSample (0, i - 1) < 0) != (b.getSample (0, i) < 0)) ++z;
    return (float) (z * 0.5 * sr / (to - from));
}

static void run (juce::AudioBuffer<float>& b, const NebulaSettings& s, double sr, int silenceFrom = -1)
{
    NebulaEngine e;
    e.prepare (sr, 512);
    Transport t; t.playing = true; t.hasPosition = true; t.bpm = 120;
    for (int start = 0; start < b.getNumSamples(); start += 512)
    {
        auto n = juce::jmin (512, b.getNumSamples() - start);
        juce::AudioBuffer<float> v (b.getArrayOfWritePointers(), 2, start, n);
        if (silenceFrom >= 0 && start >= silenceFrom) v.clear();
        e.process (v, s, t);
        t.ppq += n / (sr * 0.5);
    }
}

static juce::AudioBuffer<float> tone (double sr, double seconds, double hz)
{
    juce::AudioBuffer<float> b (2, (int) (sr * seconds));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        auto v = (float) (0.4 * std::sin (juce::MathConstants<double>::twoPi * hz * i / sr));
        b.setSample (0, i, v); b.setSample (1, i, v);
    }
    return b;
}

int main()
{
    constexpr double sr = 48000.0;
    NebulaSettings base;
    base.space = 0; base.dirt = 0; base.gdMix = 0; base.mix = 1.0f; base.position = 0.05f; base.spray = 0.1f; base.motion = 0.2f;

    {
        auto b = tone (sr, 3, 220); run (b, base, sr);
        auto l = rms (b, (int) (1.0 * sr), (int) (2.5 * sr));
        std::cout << "     grain level " << l << std::endl;
        check (l > 0.08f && l < 1.0f, "grains make sound at a sensible level");
        auto f = freq (b, (int) (1.0 * sr), (int) (2.5 * sr), sr);
        std::cout << "     pitch " << f << " Hz" << std::endl;
        check (std::abs (f - 220.0f) < 25.0f, "grains keep the pitch");
    }
    {
        auto s = base; s.mix = 0.0f;
        auto b = tone (sr, 1, 220), src = b; run (b, s, sr);
        float d = 0; for (int i = 0; i < b.getNumSamples(); ++i) d = juce::jmax (d, std::abs (b.getSample (0, i) - src.getSample (0, i)));
        check (d < 1.0e-3f, "MIX 0% = dry");
    }
    {
        auto s = base; s.grainPitch = 12.0f;
        auto b = tone (sr, 3, 220); run (b, s, sr);
        auto f = freq (b, (int) (1.0 * sr), (int) (2.5 * sr), sr);
        std::cout << "     +12 pitch " << f << " Hz" << std::endl;
        check (std::abs (f - 440.0f) < 50.0f, "PITCH +12 = one octave up");
    }
    {
        // freeze from the start would hold silence, so freeze after the tone is recorded: emulate by recording then freezing
        NebulaEngine e; e.prepare (sr, 512);
        Transport t; t.playing = true; t.hasPosition = true;
        auto b = tone (sr, 4, 220);
        for (int start = 0; start < b.getNumSamples(); start += 512)
        {
            juce::AudioBuffer<float> v (b.getArrayOfWritePointers(), 2, start, 512);
            auto x = base;
            if (start >= (int) (1.5 * sr)) { v.clear(); x.freeze = true; }
            e.process (v, x, t);
            t.ppq += 512 / (sr * 0.5);
        }
        auto l = rms (b, (int) (3.0 * sr), (int) (3.9 * sr));
        check (l > 0.05f, "FREEZE keeps playing after the input stops");
    }
    {
        auto s = base; s.stepMode = 3; s.stepBeats = 1.0;   // +12 every other beat
        auto b = tone (sr, 4, 220); run (b, s, sr);
        auto f0 = freq (b, (int) (1.05 * sr), (int) (1.45 * sr), sr), f1 = freq (b, (int) (1.55 * sr), (int) (1.95 * sr), sr);
        std::cout << "     step a " << f0 << " Hz, step b " << f1 << " Hz" << std::endl;
        check (std::abs (f1 / f0 - 2.0f) < 0.3f || std::abs (f0 / f1 - 2.0f) < 0.3f, "PITCH STEP +12 jumps an octave");
    }
    {
        auto s = base; s.mix = 0.0f; s.tapeEveryBeats = 4.0; s.tapeLengthBeats = 1.0; s.tapePosition = 0.75f;
        auto b = tone (sr, 5, 440); run (b, s, sr);
        // stop starts at beat 3 of each bar (ppq 3 = 1.5 s at 120 BPM) for 1 beat; the end of it is slow and quiet
        auto during = freq (b, (int) (3.5 * sr + 0.25 * sr), (int) (3.5 * sr + 0.45 * sr), sr);
        auto normal = freq (b, (int) (2.6 * sr), (int) (2.9 * sr), sr);
        std::cout << "     tape stop pitch " << during << " Hz vs " << normal << " Hz" << std::endl;
        check (during < normal * 0.7f, "TAPE STOP slows the sound down (pitch drops)");
    }
    {
        auto s = base; s.particles = s.grainCloud = s.reverseFx = s.shimmer = true; s.space = 1; s.dirt = 1; s.gdMix = 1; s.gdFeedback = 0.95f;
        s.texture = 1; s.motion = 1; s.stepMode = 5;
        auto b = tone (sr, 4, 330); run (b, s, sr);
        bool ok = true;
        for (int i = 0; i < b.getNumSamples(); ++i) for (int c = 0; c < 2; ++c) { auto v = b.getSample (c, i); if (! std::isfinite (v) || std::abs (v) > 8) ok = false; }
        check (ok, "everything on at maximum stays stable");
    }

    for (int type = 1; type < FxTypes::count; ++type)
    {
        if (FxTypes::isEngineFx (type)) continue;
        auto s = base; s.fxTypes[0] = type; s.fxAmounts[0] = 1.0f; s.texture = 0.8f;
        auto ref = tone (sr, 2, 330); run (ref, base, sr);
        auto b = tone (sr, 2, 330); run (b, s, sr);
        bool ok = true; float diff = 0;
        for (int i = 0; i < b.getNumSamples(); ++i)
            for (int c = 0; c < 2; ++c)
            {
                auto x = b.getSample (c, i);
                if (! std::isfinite (x) || std::abs (x) > 8) ok = false;
                diff = juce::jmax (diff, std::abs (x - ref.getSample (c, i)));
            }
        check (ok && diff > 1.0e-3f, "FX " + FxTypes::names()[type] + " works and is stable");
    }

    {
        Modulation m;
        auto target = Modulation::targetIndex ("space");
        m.addRoute (Modulation::mod1, target, 0.5f);
        Modulation::Inputs in; in.steps = { 0, 1, 0, 1, 0, 1, 0, 1 }; in.mod1Beats = 1.0; in.playing = true; in.numSamples = 512; in.sampleRate = sr;
        in.ppq = 0.1; m.advance (in); auto a = m.modulated (target, 0.2f);
        in.ppq = 1.1; m.advance (in); auto b = m.modulated (target, 0.2f);
        check (std::abs (a - 0.2f) < 1e-4f && std::abs (b - 0.7f) < 1e-4f, "MOD 1 moves a routed knob step by step");
        m.removeRoute (Modulation::mod1, target);
        m.advance (in);
        check (std::abs (m.modulated (target, 0.2f) - 0.2f) < 1e-4f, "removing the route stops the modulation");
        m.addRoute (Modulation::mod2, target, 1.0f);
        auto saved = m.toVar();
        Modulation m2; m2.fromVar (saved);
        check (std::abs (m2.getDepth (Modulation::mod2, target) - 1.0f) < 1e-4f, "routes are saved with the project");
    }

    {
        juce::ScopedJuceInitialiser_GUI gui;
        BankLibrary lib (factoryPresets());
        check (lib.banks().size() >= 2 && lib.banks()[0].presets.size() == 50, "LT COSMOS has 50 presets, MY BANK exists");
        auto name = "LT Test Bank " + juce::String (juce::Random::getSystemRandom().nextInt (100000));
        auto bi = lib.createBank (name);
        check (bi >= 2, "+ NEW BANK creates a bank");
        auto preset = lib.banks()[0].presets[3];
        preset.name = "My Swell";
        check (lib.savePreset (bi, preset) && lib.banks()[(size_t) lib.indexOfBank (name)].presets.size() == 1, "SAVE puts the preset in that bank");
        lib.toggleFavourite (name, "My Swell");
        check (lib.isFavourite (name, "My Swell"), "presets can be starred");
        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name + ".ltbank");
        bi = lib.indexOfBank (name);
        check (lib.exportBank (bi, file) && file.getSize() > 100, "EXPORT writes a .ltbank file");
        lib.deleteBank (bi);
        check (lib.indexOfBank (name) < 0, "a bank can be deleted");
        auto back = lib.importFile (file);
        check (back >= 2 && lib.banks()[(size_t) back].presets.size() == 1 && lib.banks()[(size_t) back].presets[0].name == "My Swell",
               "IMPORT brings the bank back with its presets");
        lib.toggleFavourite (name, "My Swell");
        lib.deleteBank (lib.indexOfBank (name));
        file.deleteFile();
    }

    std::cout << (failures == 0 ? "All NEBULA tests passed." : juce::String (failures) + " test(s) failed.") << std::endl;
    return failures == 0 ? 0 : 1;
}
