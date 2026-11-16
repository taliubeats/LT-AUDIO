#include "Params.h"

namespace nebula
{

juce::String IDs::step (int i) { return "step" + juce::String (i + 1); }
juce::String IDs::fxSlot (int i) { return "fxSlot" + juce::String (i + 1); }
juce::String IDs::fxAmt (int i) { return "fxAmt" + juce::String (i + 1); }
juce::String IDs::mod1Step (int i) { return "mod1_" + juce::String (i + 1); }

const juce::StringArray& Choices::stepModes() { static const juce::StringArray n { "OFF", "+5", "+7", "+12", "-12", "CUSTOM" }; return n; }
const juce::StringArray& Choices::stepRates() { static const juce::StringArray n { "1/16", "1/8", "1/4", "1/2", "1 BAR", "2 BAR", "4 BAR" }; return n; }
double Choices::stepRateBeats (int i)         { static constexpr double b[] = { 0.25, 0.5, 1, 2, 4, 8, 16 }; return b[juce::jlimit (0, 6, i)]; }
const juce::StringArray& Choices::tapeEvery() { static const juce::StringArray n { "OFF", "1 BAR", "2 BAR", "4 BAR" }; return n; }
double Choices::tapeEveryBeats (int i)        { static constexpr double b[] = { 0, 4, 8, 16 }; return b[juce::jlimit (0, 3, i)]; }
const juce::StringArray& Choices::scales()    { static const juce::StringArray n { "OFF", "MAJOR", "MINOR", "PENTA" }; return n; }
const juce::StringArray& Choices::keys()      { static const juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n; }

namespace
{
    juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        return { lo, hi,
                 [] (float s, float e, float t) { return s * std::pow (e / s, t); },
                 [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                 [] (float, float, float v) { return v; } };
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto P = [] (const String& id) { return ParameterID { id, 1 }; };
    auto pct = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto amount = [&] (const char* id, const char* name, float def)
    { l.add (std::make_unique<AudioParameterFloat> (P (id), name, NormalisableRange<float> (0, 100, 0.1f), def, pct)); };
    auto unit = [] (const char* u, int dec = 0) { return AudioParameterFloatAttributes().withLabel (u)
                     .withStringFromValueFunction ([u, dec] (float v, int) { return String (v, dec) + " " + u; }); };
    auto hz = AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
                 { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });

    amount (IDs::motion, "Motion", 45);
    amount (IDs::texture, "Texture", 72);
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::pitch), "Pitch", NormalisableRange<float> (-1200, 1200, 1), 0,
                                                  AudioParameterFloatAttributes().withLabel ("ct")
                                                      .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ct"; })));
    amount (IDs::space, "Space", 80);
    amount (IDs::dirt, "Dirt", 15);
    amount (IDs::mix, "Mix", 50);

    l.add (std::make_unique<AudioParameterChoice> (P (IDs::stepMode), "Pitch Step", Choices::stepModes(), 0));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::stepRate), "Pitch Step Every", Choices::stepRates(), 3));
    static const int defaults[] = { 0, 7, 12, 5, 0, -5, 12, 7 };
    for (int i = 0; i < 8; ++i)
        l.add (std::make_unique<AudioParameterInt> (P (IDs::step (i)), "Step " + String (i + 1), -12, 12, defaults[i]));
    amount (IDs::glide, "Glide", 0);
    amount (IDs::chance, "Chance", 100);
    amount (IDs::stepAmount, "Step Amount", 100);
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::scaleLock), "Scale Lock", Choices::scales(), 0));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::scaleKey), "Scale Key", Choices::keys(), 9));

    l.add (std::make_unique<AudioParameterChoice> (P (IDs::tapeEvery), "Tape Stop", Choices::tapeEvery(), 0));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::tapeLength), "Tape Stop Length", NormalisableRange<float> (0.125f, 4.0f, 0.001f, 0.5f), 1.0f, unit ("beats", 2)));
    amount (IDs::tapeCurve, "Tape Stop Curve", 50);
    amount (IDs::tapeStart, "Tape Start", 0);
    amount (IDs::tapeChance, "Tape Stop Chance", 100);
    amount (IDs::tapePosition, "Tape Stop Position", 75);
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::tapeTrigger), "Tape Stop Trigger", StringArray { "AUTO", "MANUAL", "MIDI" }, 0));

    l.add (std::make_unique<AudioParameterFloat> (P (IDs::size), "Grain Size", logRange (10, 500), 120, unit ("ms")));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::density), "Density", logRange (1, 80), 20, unit ("/s", 1)));
    amount (IDs::position, "Position", 10);
    amount (IDs::spray, "Spray", 30);
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::grainPitch), "Grain Pitch", NormalisableRange<float> (-24, 24, 0.01f), 0, unit ("st", 1)));
    amount (IDs::stereo, "Stereo", 60);
    amount (IDs::shape, "Shape", 50);
    amount (IDs::reverse, "Reverse", 0);
    l.add (std::make_unique<AudioParameterBool> (P (IDs::freeze), "Freeze", false));

    l.add (std::make_unique<AudioParameterFloat> (P (IDs::gdTime), "Grain Delay Time", logRange (10, 2000), 350, unit ("ms")));
    amount (IDs::gdFeedback, "Grain Delay Feedback", 35);
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::gdHighPass), "Grain Delay HP", logRange (20, 2000), 120, hz));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::gdLowPass), "Grain Delay LP", logRange (1000, 20000), 8000, hz));
    amount (IDs::gdMix, "Grain Delay Dry/Wet", 25);

    l.add (std::make_unique<AudioParameterBool> (P (IDs::particles), "Particles", false));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::cloud), "Grain Cloud", false));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::reverseFx), "Reverse FX", false));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::shimmer), "Shimmer", false));
    for (int i = 0; i < 8; ++i)
    {
        l.add (std::make_unique<AudioParameterChoice> (P (IDs::fxSlot (i)), "FX Slot " + String (i + 1), FxTypes::names(), 0));
        l.add (std::make_unique<AudioParameterFloat> (P (IDs::fxAmt (i)), "FX Amount " + String (i + 1), NormalisableRange<float> (0, 100, 0.1f), 50, pct));
    }
    static const float modDefaults[] = { 10, 70, 100, 55, 10, 30, 100, 70 };
    for (int i = 0; i < 8; ++i)
        l.add (std::make_unique<AudioParameterFloat> (P (IDs::mod1Step (i)), "MOD 1 Step " + String (i + 1), NormalisableRange<float> (0, 100, 0.1f), modDefaults[i], pct));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::mod1Rate), "MOD 1 Rate", Choices::stepRates(), 1));
    amount (IDs::mod1Swing, "MOD 1 Swing", 0);
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::mod2Shape), "MOD 2 Shape", Modulation::mod2Shapes(), 0));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::mod2Rate), "MOD 2 Rate", Choices::stepRates(), 4));
    amount (IDs::mod2Swing, "MOD 2 Swing", 0);
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::rndRate), "RND Rate", Choices::stepRates(), 2));

    auto db = AudioParameterFloatAttributes().withLabel ("dB");
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::input), "Input", NormalisableRange<float> (-24, 24, 0.1f), 0, db));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::output), "Output", NormalisableRange<float> (-24, 24, 0.1f), 0, db));
    return l;
}

std::vector<lt::presets::ParamPreset> factoryPresets()
{
    // name, tags, archetype. Each archetype is a sound family; the seed varies the details so no two are alike.
    struct Row { const char* name; const char* tags; int arch; };
    static const Row rows[] = {
        { "Stardust Keys", "PAD,KEYS", 1 }, { "Solaris Lead", "LEAD,SOLO", 2 }, { "Nebula Swell", "PAD,AMBIENT", 0 },
        { "Event Horizon", "FX,DRONE", 3 }, { "Retro Future", "SYNTH,PLUCK", 4 }, { "Gravity Bass", "BASS", 5 },
        { "Cosmo Arp", "ARP,RHYTHMIC", 6 }, { "Quantum Pad", "PAD", 0 }, { "Pulsar Keys", "KEYS", 1 },
        { "Deep Void", "FX,DARK", 3 }, { "Starry Night", "AMBIENT", 0 }, { "Orbit Pluck", "PLUCK", 4 },
        { "Comet Tail", "FX,AMBIENT", 3 }, { "Moon Dust", "LO-FI,KEYS", 8 }, { "Supernova", "FX,GLITCH", 7 },
        { "Aurora Choir", "VOCAL,PAD", 9 }, { "Dark Matter", "DARK,DRONE", 10 }, { "Lunar Tape", "LO-FI,RHYTHMIC", 8 },
        { "Galactic Arp", "ARP", 6 }, { "Celestial Bells", "KEYS,AMBIENT", 1 }, { "Black Hole Sun", "DARK,PAD", 10 },
        { "Ion Storm", "GLITCH", 7 }, { "Meteor Shower", "GLITCH,RHYTHMIC", 7 }, { "Space Station", "FX", 3 },
        { "Zero Gravity", "AMBIENT,PAD", 0 }, { "Andromeda", "PAD", 0 }, { "Red Giant", "BASS,DARK", 5 },
        { "White Dwarf", "KEYS", 1 }, { "Cosmic Ray", "LEAD", 2 }, { "Light Years", "AMBIENT", 0 },
        { "Plasma Lead", "LEAD,SOLO", 2 }, { "Asteroid Belt", "RHYTHMIC", 6 }, { "Ghost Planet", "VOCAL,DARK", 9 },
        { "Star Gate", "FX,RHYTHMIC", 11 }, { "Wormhole", "FX,GLITCH", 11 }, { "Saturn Rings", "PAD,AMBIENT", 0 },
        { "Crystal Moon", "KEYS,AMBIENT", 1 }, { "Solar Wind", "PAD", 0 }, { "Neutron Pulse", "RHYTHMIC,ARP", 6 },
        { "Halo Voices", "VOCAL", 9 }, { "Mars Rover", "LO-FI", 8 }, { "Eclipse", "DARK", 10 },
        { "Hyperdrive", "GLITCH,RHYTHMIC", 11 }, { "Milky Way", "AMBIENT", 0 }, { "Cassini", "SYNTH", 4 },
        { "Vega Pluck", "PLUCK,SYNTH", 4 }, { "Kuiper Drift", "DRONE,AMBIENT", 3 }, { "Big Bang", "FX,GLITCH", 7 },
        { "Lo-Fi Galaxy", "LO-FI,KEYS", 8 }, { "Retro Rewind", "RHYTHMIC,FX", 11 },
    };

    struct Arch
    {
        float motion, texture, space, dirt, mix, size, density, position, spray, stereo, shape, reverse;
        int stepMode, stepRate, tapeEvery; float gdMix, gdFeedback, gdTime; bool particles, cloud, revFx, shimmer;
    };
    static const Arch arch[] = {
        /* 0 pad/ambient */ { 0.4f, 0.75f, 0.85f, 0.05f, 0.6f, 260, 18, 0.15f, 0.35f, 0.8f, 0.2f, 0.0f, 0, 3, 0, 0.2f, 0.4f, 500, true, true, false, true },
        /* 1 keys        */ { 0.35f, 0.6f, 0.7f, 0.1f, 0.5f, 140, 22, 0.08f, 0.25f, 0.6f, 0.5f, 0.0f, 3, 3, 0, 0.25f, 0.35f, 375, true, false, false, false },
        /* 2 lead        */ { 0.3f, 0.5f, 0.55f, 0.15f, 0.45f, 90, 26, 0.05f, 0.15f, 0.5f, 0.6f, 0.0f, 2, 2, 0, 0.35f, 0.45f, 250, false, false, false, true },
        /* 3 fx/drone    */ { 0.7f, 0.8f, 0.9f, 0.1f, 0.7f, 380, 14, 0.4f, 0.6f, 0.9f, 0.2f, 0.4f, 0, 4, 0, 0.3f, 0.6f, 800, true, true, true, true },
        /* 4 pluck/synth */ { 0.3f, 0.45f, 0.5f, 0.15f, 0.45f, 60, 30, 0.03f, 0.1f, 0.6f, 0.75f, 0.0f, 1, 2, 0, 0.3f, 0.4f, 187, true, false, false, false },
        /* 5 bass        */ { 0.2f, 0.4f, 0.25f, 0.35f, 0.35f, 110, 18, 0.02f, 0.05f, 0.2f, 0.7f, 0.0f, 4, 3, 0, 0.1f, 0.2f, 250, false, false, false, false },
        /* 6 arp         */ { 0.35f, 0.55f, 0.55f, 0.1f, 0.55f, 70, 28, 0.03f, 0.1f, 0.7f, 0.8f, 0.0f, 5, 1, 0, 0.3f, 0.35f, 187, true, false, false, false },
        /* 7 glitch      */ { 0.85f, 0.65f, 0.45f, 0.45f, 0.6f, 35, 45, 0.2f, 0.8f, 0.9f, 0.9f, 0.4f, 5, 0, 2, 0.4f, 0.5f, 125, true, true, true, false },
        /* 8 lo-fi       */ { 0.4f, 0.55f, 0.5f, 0.55f, 0.5f, 150, 16, 0.1f, 0.3f, 0.5f, 0.5f, 0.0f, 0, 3, 2, 0.2f, 0.3f, 375, false, false, false, false },
        /* 9 vocal       */ { 0.45f, 0.7f, 0.75f, 0.05f, 0.55f, 200, 20, 0.12f, 0.3f, 0.75f, 0.3f, 0.1f, 2, 4, 0, 0.25f, 0.35f, 500, false, true, false, true },
        /* 10 dark       */ { 0.5f, 0.7f, 0.8f, 0.3f, 0.6f, 300, 14, 0.3f, 0.45f, 0.7f, 0.2f, 0.3f, 4, 4, 0, 0.3f, 0.55f, 666, false, true, true, false },
        /* 11 rhythmic fx*/ { 0.6f, 0.6f, 0.5f, 0.25f, 0.6f, 80, 30, 0.1f, 0.4f, 0.8f, 0.8f, 0.3f, 5, 1, 1, 0.35f, 0.45f, 250, true, false, true, false },
    };

    std::vector<lt::presets::ParamPreset> out;
    juce::Random rnd (2026);
    for (auto& r : rows)
    {
        auto& a = arch[r.arch];
        auto vary = [&] (float v, float amt) { return v * (1.0f + (rnd.nextFloat() - 0.5f) * amt); };
        lt::presets::ParamPreset p;
        p.name = r.name;
        p.group = r.tags;
        auto& v = p.values;
        v[IDs::motion] = juce::jlimit (0.0f, 100.0f, vary (a.motion, 0.3f) * 100.0f);
        v[IDs::texture] = juce::jlimit (0.0f, 100.0f, vary (a.texture, 0.2f) * 100.0f);
        v[IDs::pitch] = 0;
        v[IDs::space] = juce::jlimit (0.0f, 100.0f, vary (a.space, 0.2f) * 100.0f);
        v[IDs::dirt] = juce::jlimit (0.0f, 100.0f, vary (a.dirt, 0.4f) * 100.0f);
        v[IDs::mix] = juce::jlimit (0.0f, 100.0f, vary (a.mix, 0.15f) * 100.0f);
        v[IDs::size] = juce::jlimit (10.0f, 500.0f, vary (a.size, 0.3f));
        v[IDs::density] = juce::jlimit (1.0f, 80.0f, vary (a.density, 0.3f));
        v[IDs::position] = juce::jlimit (0.0f, 100.0f, vary (a.position, 0.4f) * 100.0f);
        v[IDs::spray] = juce::jlimit (0.0f, 100.0f, vary (a.spray, 0.4f) * 100.0f);
        v[IDs::stereo] = juce::jlimit (0.0f, 100.0f, vary (a.stereo, 0.2f) * 100.0f);
        v[IDs::shape] = juce::jlimit (0.0f, 100.0f, vary (a.shape, 0.2f) * 100.0f);
        v[IDs::reverse] = juce::jlimit (0.0f, 100.0f, a.reverse * 100.0f);
        v[IDs::stepMode] = (float) a.stepMode;
        v[IDs::stepRate] = (float) a.stepRate;
        v[IDs::tapeEvery] = (float) a.tapeEvery;
        v[IDs::gdMix] = a.gdMix * 100.0f;
        v[IDs::gdFeedback] = a.gdFeedback * 100.0f;
        v[IDs::gdTime] = a.gdTime;
        v[IDs::particles] = a.particles ? 1.0f : 0.0f;
        v[IDs::cloud] = a.cloud ? 1.0f : 0.0f;
        v[IDs::reverseFx] = a.revFx ? 1.0f : 0.0f;
        v[IDs::shimmer] = a.shimmer ? 1.0f : 0.0f;
        v[IDs::freeze] = 0.0f;

        // a small FX CHAIN per sound family
        using F = FxTypes;
        static const std::vector<std::pair<int, float>> chains[] = {
            { { F::chorus, 35 }, { F::tape, 30 } },                       // pad
            { { F::chorus, 25 }, { F::eq, 30 } },                         // keys
            { { F::delay, 30 }, { F::saturation, 30 } },                  // lead
            { { F::spectralFreeze, 35 }, { F::phaser, 30 }, { F::filterLP, 25 } }, // fx
            { { F::pingPong, 30 }, { F::chorus, 20 } },                   // pluck
            { { F::saturation, 45 }, { F::filterLP, 30 } },               // bass
            { { F::pingPong, 35 }, { F::tremolo, 25 } },                  // arp
            { { F::stutter, 45 }, { F::bitcrusher, 35 }, { F::freqShifter, 25 } }, // glitch
            { { F::vinyl, 55 }, { F::tapeWobble, 40 }, { F::bitcrusher, 20 } },    // lo-fi
            { { F::formant, 35 }, { F::harmonizer, 25 } },                // vocal
            { { F::filterLP, 40 }, { F::comb, 25 }, { F::distortion, 15 } },       // dark
            { { F::stutter, 35 }, { F::reverseDelay, 35 }, { F::autoPan, 40 } },   // rhythmic fx
        };
        auto& chain = chains[r.arch];
        for (int i = 0; i < 8; ++i)
        {
            v[IDs::fxSlot (i)] = i < (int) chain.size() ? (float) chain[(size_t) i].first : 0.0f;
            v[IDs::fxAmt (i)] = i < (int) chain.size() ? chain[(size_t) i].second : 50.0f;
        }
        out.push_back (p);
    }
    return out;
}

} // namespace nebula
