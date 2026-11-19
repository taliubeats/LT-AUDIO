#include "Params.h"
#include "ModDefs.h"
#include "FxTypes.h"
#include "Wavetables.h"

namespace quasar
{

namespace
{
    const char* oscLetter (int o) { static const char* l[] = { "A", "B", "C" }; return l[juce::jlimit (0, 2, o)]; }
    const char* busKey (int b)    { static const char* k[] = { "M", "1", "2" }; return k[juce::jlimit (0, 2, b)]; }
}

juce::String IDs::osc (int o, const char* w)      { return juce::String ("osc") + oscLetter (o) + "_" + w; }
juce::String IDs::flt (int f, const char* w)      { return "flt" + juce::String (f + 1) + "_" + w; }
juce::String IDs::env (int e, const char* w)      { return e == 3 ? juce::String ("envP_") + w : "env" + juce::String (e + 1) + "_" + w; }
juce::String IDs::lfo (int l, const char* w)      { return "lfo" + juce::String (l + 1) + "_" + w; }
juce::String IDs::macro (int m)                   { return "macro" + juce::String (m + 1); }
juce::String IDs::mod (int s, const char* w)      { return "mod" + juce::String (s + 1) + "_" + w; }
juce::String IDs::bus (int b, const char* w)      { return juce::String ("bus") + busKey (b) + "_" + w; }
juce::String IDs::fx (int b, int s, const char* w) { return juce::String ("fx") + busKey (b) + juce::String (s + 1) + "_" + w; }
juce::String IDs::fxParam (int b, int s, int p)   { return juce::String ("fx") + busKey (b) + juce::String (s + 1) + "_p" + juce::String (p + 1); }

juce::String IDs::mixer (int ch, const char* w)
{
    switch (ch)
    {
        case chSub:   return juce::String ("sub_") + w;
        case chNoise: return juce::String ("noise_") + w;
        case chFlt1:  return flt (0, w);
        case chFlt2:  return flt (1, w);
        default:      return osc (ch - chOscA, w);
    }
}

const juce::StringArray& Choices::oscTypes()     { static const juce::StringArray n { "WAVETABLE", "ANALOG", "SAMPLE" }; return n; }
const juce::StringArray& Choices::warpModes()    { static const juce::StringArray n { "OFF", "SYNC", "BEND +", "BEND -", "PWM", "MIRROR", "ASYM", "FM", "RING", "FOLD", "QUANTIZE" }; return n; }
const juce::StringArray& Choices::sourceDests()  { static const juce::StringArray n { "FILTER 1", "FILTER 2", "FILTER 1+2", "MAIN", "BUS 1", "BUS 2" }; return n; }
const juce::StringArray& Choices::filterDests()  { static const juce::StringArray n { "FILTER 2", "MAIN", "BUS 1", "BUS 2" }; return n; }
const juce::StringArray& Choices::filterTypes()  { static const juce::StringArray n { "LADDER 24", "LADDER 12", "LP 24", "LP 12", "HP 24", "HP 12", "BP 24", "BP 12", "NOTCH", "PEAK", "COMB +", "COMB -", "FORMANT", "DIODE" }; return n; }
const juce::StringArray& Choices::subShapes()    { static const juce::StringArray n { "SINE", "TRIANGLE", "SAW", "SQUARE", "PULSE" }; return n; }
const juce::StringArray& Choices::noiseTypes()   { static const juce::StringArray n { "WHITE", "PINK", "BROWN", "VINYL", "TAPE HISS", "AIR", "CRACKLE", "DIGITAL" }; return n; }
const juce::StringArray& Choices::lfoModes()     { static const juce::StringArray n { "FREE", "RETRIG", "ENV" }; return n; }
const juce::StringArray& Choices::divisions()    { static const juce::StringArray n { "8/1", "4/1", "2/1", "1/1", "1/2", "1/2T", "1/2D", "1/4", "1/4T", "1/4D", "1/8", "1/8T", "1/8D", "1/16", "1/16T", "1/16D", "1/32", "1/32T", "1/64" }; return n; }
double Choices::divisionBeats (int i)
{
    static constexpr double b[] = { 32, 16, 8, 4, 2, 4.0 / 3, 3, 1, 2.0 / 3, 1.5, 0.5, 1.0 / 3, 0.75, 0.25, 1.0 / 6, 0.375, 0.125, 1.0 / 12, 0.0625 };
    return b[juce::jlimit (0, 18, i)];
}
const juce::StringArray& Choices::voiceModes()   { static const juce::StringArray n { "POLY", "MONO", "LEGATO" }; return n; }
const juce::StringArray& Choices::stealModes()   { static const juce::StringArray n { "SOFT", "SMOOTH", "NONE" }; return n; }
const juce::StringArray& Choices::priorities()   { static const juce::StringArray n { "LAST", "HIGH", "LOW" }; return n; }
const juce::StringArray& Choices::qualities()    { static const juce::StringArray n { "ECO", "HIGH", "ULTRA" }; return n; }
const juce::StringArray& Choices::oversamplings(){ static const juce::StringArray n { "2x", "4x" }; return n; }
const juce::StringArray& Choices::scales()       { static const juce::StringArray n { "12-TET", "JUST (C)", "PYTHAGOREAN" }; return n; }
const juce::StringArray& Choices::arpModes()     { static const juce::StringArray n { "UP", "DOWN", "UP/DOWN", "DOWN/UP", "RANDOM", "ORDER", "CHORD" }; return n; }
const juce::StringArray& Choices::arpPatterns()  { static const juce::StringArray n { "STRAIGHT", "TRAP ROLL", "TRIPLET FLOW", "BOUNCE", "DEMBOW", "SKIP", "STAIRS", "RATCHET", "OCT JUMP", "DRILL", "PLUGG", "LO-FI SWING" }; return n; }
const juce::StringArray& Choices::modCurves()    { static const juce::StringArray n { "LINEAR", "EXP", "LOG", "S-CURVE" }; return n; }
const juce::StringArray& Choices::busNames()     { static const juce::StringArray n { "MAIN", "BUS 1", "BUS 2" }; return n; }
const juce::StringArray& Choices::mixerChannels(){ static const juce::StringArray n { "SUB", "OSC A", "OSC B", "OSC C", "NOISE", "FILTER 1", "FILTER 2" }; return n; }

namespace
{
    juce::NormalisableRange<float> logRange (float lo, float hi, float zeroBelow = -1.0f)
    {
        // log mapping; when zeroBelow >= 0 the very bottom of the range snaps to 0 (for times)
        return { zeroBelow >= 0 ? 0.0f : lo, hi,
                 [lo, hi, zeroBelow] (float, float, float t)
                 {
                     if (zeroBelow >= 0 && t <= 0.0001f) return 0.0f;
                     return lo * std::pow (hi / lo, t);
                 },
                 [lo, hi, zeroBelow] (float, float, float v)
                 {
                     if (zeroBelow >= 0 && v <= lo * 0.5f) return 0.0f;
                     return juce::jlimit (0.0f, 1.0f, std::log (juce::jmax (lo, v) / lo) / std::log (hi / lo));
                 },
                 [] (float, float, float v) { return v; } };
    }

    juce::String fmtMs (float v)
    {
        if (v < 1.0f)    return juce::String (v, 2) + " ms";
        if (v < 1000.0f) return juce::String (v, v < 10.0f ? 1 : 0) + " ms";
        return juce::String (v / 1000.0f, 2) + " s";
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto P = [] (const String& id) { return ParameterID { id, 1 }; };

    auto pctA = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto pct = [&] (const String& id, const String& name, float def, float lo = 0.0f)
    { l.add (std::make_unique<AudioParameterFloat> (P (id), name, NormalisableRange<float> (lo, 100, 0.01f), def, pctA)); };
    auto panA = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                { auto r = roundToInt (v); return r == 0 ? String ("C") : r < 0 ? String (-r) + "L" : String (r) + "R"; });
    auto pan = [&] (const String& id, const String& name) { l.add (std::make_unique<AudioParameterFloat> (P (id), name, NormalisableRange<float> (-100, 100, 0.01f), 0.0f, panA)); };
    auto boolean = [&] (const String& id, const String& name, bool def) { l.add (std::make_unique<AudioParameterBool> (P (id), name, def)); };
    auto choice = [&] (const String& id, const String& name, const StringArray& c, int def) { l.add (std::make_unique<AudioParameterChoice> (P (id), name, c, def)); };
    auto integer = [&] (const String& id, const String& name, int lo, int hi, int def) { l.add (std::make_unique<AudioParameterInt> (P (id), name, lo, hi, def)); };
    auto msA = AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return fmtMs (v); });
    auto ms = [&] (const String& id, const String& name, float lo, float hi, float def, bool canBeZero)
    { l.add (std::make_unique<AudioParameterFloat> (P (id), name, logRange (lo, hi, canBeZero ? 0.0f : -1.0f), def, msA)); };
    auto hzA = AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
               { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (v, v < 10.0f ? 2 : 0) + " Hz"; });
    auto stA = [] (int dec) { return AudioParameterFloatAttributes().withLabel ("st").withStringFromValueFunction ([dec] (float v, int)
               { return (v > 0 ? "+" : "") + String (v, dec) + " st"; }); };

    // ---------------------------------------------------------------- oscillators
    const char* oscNames[] = { "Osc A", "Osc B", "Osc C" };
    for (int o = 0; o < numOscs; ++o)
    {
        String n (oscNames[o]);
        boolean (IDs::osc (o, "on"), n + " On", o == 0);
        choice (IDs::osc (o, "type"), n + " Type", Choices::oscTypes(), 0);
        choice (IDs::osc (o, "table"), n + " Table", WavetableLibrary::names(), o == 0 ? 1 : 0);
        integer (IDs::osc (o, "oct"), n + " Octave", -4, 4, 0);
        l.add (std::make_unique<AudioParameterFloat> (P (IDs::osc (o, "semi")), n + " Semi", NormalisableRange<float> (-24, 24, 1.0f), 0.0f, stA (0)));
        l.add (std::make_unique<AudioParameterFloat> (P (IDs::osc (o, "fine")), n + " Fine", NormalisableRange<float> (-100, 100, 0.1f), 0.0f,
                                                      AudioParameterFloatAttributes().withLabel ("ct").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ct"; })));
        l.add (std::make_unique<AudioParameterFloat> (P (IDs::osc (o, "phase")), n + " Phase", NormalisableRange<float> (0, 360, 0.1f), 0.0f,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")); })));
        pct (IDs::osc (o, "rand"), n + " Phase Random", 100.0f);
        pct (IDs::osc (o, "wtpos"), n + " WT Pos", 0.0f);
        choice (IDs::osc (o, "warpmode"), n + " Warp Mode", Choices::warpModes(), 0);
        pct (IDs::osc (o, "warp"), n + " Warp", 0.0f);
        integer (IDs::osc (o, "unison"), n + " Unison", 1, maxUnison, 1);
        pct (IDs::osc (o, "detune"), n + " Detune", 25.0f);
        pct (IDs::osc (o, "blend"), n + " Blend", 75.0f);
        pct (IDs::osc (o, "spread"), n + " Spread", 100.0f);
        pan (IDs::osc (o, "pan"), n + " Pan");
        pct (IDs::osc (o, "level"), n + " Level", 75.0f);
    }
    boolean (IDs::subOn, "Sub On", false);
    choice (IDs::subShape, "Sub Shape", Choices::subShapes(), 0);
    integer (IDs::subOct, "Sub Octave", -3, 1, -1);
    pct (IDs::subLevel, "Sub Level", 75.0f);
    pan (IDs::subPan, "Sub Pan");
    boolean (IDs::noiseOn, "Noise On", false);
    choice (IDs::noiseType, "Noise Type", Choices::noiseTypes(), 1);
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::noiseColor), "Noise Color", NormalisableRange<float> (-100, 100, 0.01f), 0.0f, pctA));
    pct (IDs::noiseLevel, "Noise Level", 50.0f);
    pan (IDs::noisePan, "Noise Pan");

    // ---------------------------------------------------------------- mixer routing (sources and filters)
    for (int ch = 0; ch < numMixerChannels; ++ch)
    {
        auto n = Choices::mixerChannels()[ch];
        n = n.substring (0, 1) + n.substring (1).toLowerCase();
        if (ch == chFlt1)      choice (IDs::mixer (ch, "dest"), n + " Dest", Choices::filterDests(), 1);
        else if (ch == chFlt2) choice (IDs::mixer (ch, "dest"), n + " Dest", StringArray { "MAIN", "BUS 1", "BUS 2" }, 0);
        else                   choice (IDs::mixer (ch, "dest"), n + " Dest", Choices::sourceDests(), 0);
        pct (IDs::mixer (ch, "send1"), n + " Send 1", 0.0f);
        pct (IDs::mixer (ch, "send2"), n + " Send 2", 0.0f);
        boolean (IDs::mixer (ch, "mute"), n + " Mute", false);
        boolean (IDs::mixer (ch, "solo"), n + " Solo", false);
    }

    // ---------------------------------------------------------------- filters
    for (int f = 0; f < numFilters; ++f)
    {
        auto n = "Filter " + String (f + 1);
        boolean (IDs::flt (f, "on"), n + " On", false);
        choice (IDs::flt (f, "type"), n + " Type", Choices::filterTypes(), 0);
        l.add (std::make_unique<AudioParameterFloat> (P (IDs::flt (f, "cutoff")), n + " Cutoff", logRange (20.0f, 20000.0f), f == 0 ? 2500.0f : 8000.0f, hzA));
        pct (IDs::flt (f, "reso"), n + " Resonance", 15.0f);
        pct (IDs::flt (f, "drive"), n + " Drive", 0.0f);
        pct (IDs::flt (f, "mix"), n + " Mix", 100.0f);
        pct (IDs::flt (f, "key"), n + " Keytrack", 0.0f);
        pan (IDs::flt (f, "pan"), n + " Pan");
        pct (IDs::flt (f, "level"), n + " Level", 75.0f);
    }

    // ---------------------------------------------------------------- envelopes
    const char* envNames[] = { "Env 1", "Env 2", "Env 3", "Pitch Env" };
    for (int e = 0; e < numEnvs; ++e)
    {
        String n (envNames[e]);
        auto amp = e == 0, pitch = e == 3;
        ms (IDs::env (e, "att"), n + " Attack", 0.1f, 20000.0f, amp ? 1.0f : 0.0f, true);
        ms (IDs::env (e, "hold"), n + " Hold", 0.1f, 10000.0f, 0.0f, true);
        ms (IDs::env (e, "dec"), n + " Decay", 1.0f, 30000.0f, pitch ? 150.0f : amp ? 1000.0f : 600.0f, false);
        pct (IDs::env (e, "sus"), n + " Sustain", amp ? 100.0f : 0.0f);
        ms (IDs::env (e, "rel"), n + " Release", 1.0f, 30000.0f, amp ? 80.0f : 300.0f, false);
        l.add (std::make_unique<AudioParameterFloat> (P (IDs::env (e, "acurve")), n + " Attack Curve", NormalisableRange<float> (-100, 100, 0.1f), -30.0f, pctA));
        l.add (std::make_unique<AudioParameterFloat> (P (IDs::env (e, "dcurve")), n + " Decay Curve", NormalisableRange<float> (-100, 100, 0.1f), 60.0f, pctA));
        l.add (std::make_unique<AudioParameterFloat> (P (IDs::env (e, "rcurve")), n + " Release Curve", NormalisableRange<float> (-100, 100, 0.1f), 60.0f, pctA));
    }
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::envPAmount), "Pitch Env Amount", NormalisableRange<float> (-48, 48, 0.01f), 0.0f, stA (1)));
    pct (IDs::ampVel, "Velocity To Amp", 40.0f);

    // ---------------------------------------------------------------- LFOs
    for (int i = 0; i < numLfos; ++i)
    {
        auto n = "LFO " + String (i + 1);
        l.add (std::make_unique<AudioParameterFloat> (P (IDs::lfo (i, "rate")), n + " Rate", logRange (0.01f, 40.0f), 2.0f, hzA));
        boolean (IDs::lfo (i, "sync"), n + " Sync", true);
        choice (IDs::lfo (i, "div"), n + " Division", Choices::divisions(), 7);
        choice (IDs::lfo (i, "mode"), n + " Mode", Choices::lfoModes(), 1);
        ms (IDs::lfo (i, "rise"), n + " Rise", 1.0f, 10000.0f, 0.0f, true);
        ms (IDs::lfo (i, "delay"), n + " Delay", 1.0f, 10000.0f, 0.0f, true);
        pct (IDs::lfo (i, "smooth"), n + " Smooth", 0.0f);
        l.add (std::make_unique<AudioParameterFloat> (P (IDs::lfo (i, "phase")), n + " Phase", NormalisableRange<float> (0, 360, 0.1f), 0.0f,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")); })));
    }

    for (int m = 0; m < numMacros; ++m) pct (IDs::macro (m), "Macro " + String (m + 1), 0.0f);

    // ---------------------------------------------------------------- modulation matrix
    auto notAuto = AudioParameterIntAttributes().withAutomatable (false);
    for (int s = 0; s < numModSlots; ++s)
    {
        auto n = "Mod " + String (s + 1);
        l.add (std::make_unique<AudioParameterInt> (P (IDs::mod (s, "src")), n + " Source", 0, numModSources - 1, 0, notAuto));
        l.add (std::make_unique<AudioParameterInt> (P (IDs::mod (s, "dst")), n + " Destination", 0, (int) ModDefs::destinations().size() - 1, 0, notAuto));
        l.add (std::make_unique<AudioParameterFloat> (P (IDs::mod (s, "amt")), n + " Amount", NormalisableRange<float> (-100, 100, 0.01f), 0.0f, pctA));
        l.add (std::make_unique<AudioParameterInt> (P (IDs::mod (s, "curve")), n + " Curve", 0, Choices::modCurves().size() - 1, 0, notAuto));
        l.add (std::make_unique<AudioParameterBool> (P (IDs::mod (s, "bip")), n + " Bipolar", false, AudioParameterBoolAttributes().withAutomatable (false)));
        l.add (std::make_unique<AudioParameterInt> (P (IDs::mod (s, "aux")), n + " Aux Source", 0, numModSources - 1, 0, notAuto));
        pct (IDs::mod (s, "out"), n + " Output", 100.0f);
        l.add (std::make_unique<AudioParameterBool> (P (IDs::mod (s, "on")), n + " On", true, AudioParameterBoolAttributes().withAutomatable (false)));
    }

    // ---------------------------------------------------------------- buses and effects
    for (int b = 0; b < numBuses; ++b)
    {
        auto n = Choices::busNames()[b];
        n = n.substring (0, 1) + n.substring (1).toLowerCase();
        pct (IDs::bus (b, "level"), n + " Level", 75.0f);
        pan (IDs::bus (b, "pan"), n + " Pan");
        boolean (IDs::bus (b, "mute"), n + " Mute", false);
        boolean (IDs::bus (b, "solo"), n + " Solo", false);
        for (int s = 0; s < numFxSlots; ++s)
        {
            auto fn = n + " FX" + String (s + 1);
            choice (IDs::fx (b, s, "type"), fn + " Type", fxNames(), 0);
            boolean (IDs::fx (b, s, "on"), fn + " On", true);
            pct (IDs::fx (b, s, "mix"), fn + " Mix", 100.0f);
            for (int p = 0; p < numFxParams; ++p)
                l.add (std::make_unique<AudioParameterFloat> (P (IDs::fxParam (b, s, p)), fn + " P" + String (p + 1), NormalisableRange<float> (0, 1, 0.0001f), 0.5f));
        }
    }

    // ---------------------------------------------------------------- voicing and global
    choice (IDs::voiceMode, "Voice Mode", Choices::voiceModes(), 0);
    ms (IDs::glide, "Glide", 1.0f, 5000.0f, 0.0f, true);
    boolean (IDs::glideLegato, "Glide Legato Only", false);
    integer (IDs::poly, "Polyphony", 1, maxVoices, 16);
    integer (IDs::stack, "Unison Voices", 1, 8, 1);
    choice (IDs::steal, "Voice Steal", Choices::stealModes(), 0);
    choice (IDs::priority, "Note Priority", Choices::priorities(), 0);
    boolean (IDs::mpe, "MPE", false);
    choice (IDs::quality, "Quality", Choices::qualities(), 1);
    choice (IDs::oversampling, "Oversampling", Choices::oversamplings(), 0);
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::masterTune), "Master Tune", NormalisableRange<float> (-100, 100, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " ct"; })));
    integer (IDs::transpose, "Transpose", -24, 24, 0);
    integer (IDs::pbUp, "Pitch Bend Up", 0, 48, 2);
    integer (IDs::pbDown, "Pitch Bend Down", 0, 48, 2);
    choice (IDs::scale, "Scale", Choices::scales(), 0);
    pct (IDs::drift, "Drift", 20.0f);
    pct (IDs::warmth, "Warmth", 20.0f);
    boolean (IDs::limiter, "Safety Limiter", true);
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::master), "Master", NormalisableRange<float> (-48, 6, 0.01f, 2.0f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
                                                  { return v <= -47.9f ? String ("-inf dB") : (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));

    boolean (IDs::arpOn, "Arp On", false);
    choice (IDs::arpMode, "Arp Mode", Choices::arpModes(), 0);
    choice (IDs::arpRate, "Arp Rate", Choices::divisions(), 13);
    integer (IDs::arpOct, "Arp Octaves", 1, 4, 1);
    pct (IDs::arpGate, "Arp Gate", 70.0f, 5.0f);
    pct (IDs::arpSwing, "Arp Swing", 0.0f);
    choice (IDs::arpPattern, "Arp Pattern", Choices::arpPatterns(), 0);
    boolean (IDs::arpLatch, "Arp Latch", false);
    return l;
}

} // namespace quasar
