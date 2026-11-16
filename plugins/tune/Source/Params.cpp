#include "Params.h"

namespace tune
{

juce::String IDs::note (int pc)
{
    return "note" + juce::String (pc);
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto P = [] (const String& id) { return ParameterID { id, 1 }; };
    auto pct = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto plain = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)); });
    auto st = AudioParameterFloatAttributes().withLabel ("st").withStringFromValueFunction ([] (float v, int)
                  { auto r = std::round (v * 2.0f) / 2.0f; return (r > 0 ? "+" : "") + (std::abs (r - std::round (r)) > 0.01f ? String (r, 1) : String (roundToInt (r))); });

    l.add (std::make_unique<AudioParameterChoice> (P (IDs::key), "Key", Keys::names(), 9));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::scale), "Scale", Scales::names(), Scales::minor));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::edited), "Notes Edited", false));
    for (int i = 0; i < 12; ++i)
        l.add (std::make_unique<AudioParameterBool> (P (IDs::note (i)), "Note " + Keys::noteName (i), true));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::voice), "Voice", Voices::names(), Voices::tenor));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::retune), "Retune Speed", NormalisableRange<float> (0.0f, 400.0f, 1.0f, 0.4f), 12.0f,
                                                  AudioParameterFloatAttributes().withLabel ("ms")
                                                      .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::humanize), "Humanize", NormalisableRange<float> (0, 100, 1), 20, plain));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::flexTune), "Flex Tune", NormalisableRange<float> (0, 100, 1), 15, plain));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::vibrato), "Vibrato", NormalisableRange<float> (-100, 100, 1), 0,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                      { auto r = roundToInt (v); return r == 0 ? String ("0") : (r > 0 ? "+" : "") + String (r); })));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::formant), "Formant", NormalisableRange<float> (-12.0f, 12.0f, 0.5f), 0.0f, st));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::preserve), "Formant Preserve", true));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::transpose), "Transpose", NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f, st));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::detune), "Detune", NormalisableRange<float> (430.0f, 450.0f, 0.1f), 440.0f,
                                                  AudioParameterFloatAttributes().withLabel ("Hz")
                                                      .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " Hz"; })));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::mix), "Mix", NormalisableRange<float> (0, 100, 1), 100, pct));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::mode), "Mode", StringArray { "LOW LATENCY", "HQ" }, 0));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::bypass), "Bypass", false));
    return l;
}

std::vector<lt::presets::ParamPreset> factoryPresets()
{
    //                         key scale voice retune hum flex vib  form  tr  group
    struct Row { const char* name; const char* group; float v[8]; };
    static const Row rows[] = {
        { "Default",            "DEFAULT", { 9, Scales::minor, 12, 20, 15, 0, 0, 0 } },
        { "Pop Vocal",          "POP",     { 9, Scales::minor, 12, 20, 15, 0, 0, 0 } },
        { "Pop Polish",         "POP",     { 0, Scales::major, 35, 35, 25, 0, 0, 0 } },
        { "Ballad Natural",     "NATURAL", { 0, Scales::major, 90, 60, 45, 0, 0, 0 } },
        { "Gentle Live",        "NATURAL", { 0, Scales::major, 140, 70, 60, 0, 0, 0 } },
        { "Transparent Fix",    "NATURAL", { 0, Scales::chromatic, 60, 50, 70, 0, 0, 0 } },
        { "Hard Tune",          "HARD",    { 9, Scales::minor, 0, 0, 0, -60, 0, 0 } },
        { "Trap Robot",         "HARD",    { 9, Scales::minor, 0, 0, 0, -100, 0, 0 } },
        { "R&B Smooth",         "R&B",     { 2, Scales::minor, 25, 40, 20, 0, 0, 0 } },
        { "R&B Runs",           "R&B",     { 2, Scales::pentatonicMinor, 18, 30, 30, 0, 0, 0 } },
        { "Afro Melodic",       "MELODIC", { 7, Scales::major, 8, 15, 10, -30, 0, 0 } },
        { "Blues Bend",         "MELODIC", { 4, Scales::blues, 45, 40, 50, 0, 0, 0 } },
        { "Opera Vibrato",      "FX",      { 0, Scales::major, 70, 50, 30, 45, 0, 0 } },
        { "Deep Voice",         "FX",      { 9, Scales::minor, 15, 20, 15, 0, -3, -5 } },
        { "Chipmunk",           "FX",      { 9, Scales::minor, 10, 10, 10, 0, 4, 12 } },
        { "Octave Down",        "FX",      { 0, Scales::chromatic, 20, 20, 20, 0, 0, -12 } },
    };
    static const char* ids[8] = { IDs::key, IDs::scale, IDs::retune, IDs::humanize, IDs::flexTune, IDs::vibrato, IDs::formant, IDs::transpose };
    std::vector<lt::presets::ParamPreset> out;
    for (auto& r : rows)
    {
        lt::presets::ParamPreset p;
        p.name = r.name;
        p.group = r.group;
        for (int i = 0; i < 8; ++i) p.values[ids[i]] = r.v[i];
        p.values[IDs::edited] = 0.0f;
        p.values[IDs::preserve] = 1.0f;
        p.values[IDs::mix] = 100.0f;
        p.values[IDs::detune] = 440.0f;
        out.push_back (p);
    }
    return out;
}

NoteMask effectiveMask (juce::AudioProcessorValueTreeState& s)
{
    auto key = juce::roundToInt (s.getRawParameterValue (IDs::key)->load());
    auto scale = juce::roundToInt (s.getRawParameterValue (IDs::scale)->load());
    if (scale != Scales::custom && s.getRawParameterValue (IDs::edited)->load() < 0.5f)
        return Scales::mask (key, scale);
    NoteMask m {};
    for (int i = 0; i < 12; ++i) m[(size_t) i] = s.getRawParameterValue (IDs::note (i))->load() > 0.5f;
    return m;
}

void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float v)
{
    if (auto* p = s.getParameter (id))
    {
        auto n = p->convertTo0to1 (v);
        if (std::abs (p->getValue() - n) < 1.0e-6f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (n);
        p->endChangeGesture();
    }
}

void setKeyScale (juce::AudioProcessorValueTreeState& s, int key, int scale)
{
    setParam (s, IDs::key, (float) key);
    setParam (s, IDs::scale, (float) scale);
    setParam (s, IDs::edited, 0.0f);
}

void toggleNote (juce::AudioProcessorValueTreeState& s, int pc)
{
    auto m = effectiveMask (s);
    m[(size_t) pc] = ! m[(size_t) pc];
    for (int i = 0; i < 12; ++i) setParam (s, IDs::note (i), m[(size_t) i] ? 1.0f : 0.0f);
    if (juce::roundToInt (s.getRawParameterValue (IDs::scale)->load()) != Scales::custom)
        setParam (s, IDs::edited, 1.0f);
}

void resetNotes (juce::AudioProcessorValueTreeState& s)
{
    auto key = juce::roundToInt (s.getRawParameterValue (IDs::key)->load());
    auto scale = juce::roundToInt (s.getRawParameterValue (IDs::scale)->load());
    auto m = Scales::mask (key, scale == Scales::custom ? Scales::chromatic : scale);
    for (int i = 0; i < 12; ++i) setParam (s, IDs::note (i), m[(size_t) i] ? 1.0f : 0.0f);
    setParam (s, IDs::edited, 0.0f);
}

} // namespace tune
