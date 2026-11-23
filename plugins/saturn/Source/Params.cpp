#include "Params.h"

namespace saturn
{

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto P = [] (const char* id) { return ParameterID { id, 1 }; };
    auto pct = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto pctRange = NormalisableRange<float> (0, 100, 0.1f);

    l.add (std::make_unique<AudioParameterChoice> (P (IDs::tape), "Tape", Tapes::names(), Tapes::strings));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::age), "Age", pctRange, 40, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::wow), "Wow", pctRange, 30, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::flutter), "Flutter", pctRange, 20, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::drift), "Drift", pctRange, 20, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::saturation), "Saturation", pctRange, 35, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::tone), "Tone", NormalisableRange<float> (-100, 100, 0.1f), 0,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  {
                                                      auto r = roundToInt (v);
                                                      return r == 0 ? String ("0") : r < 0 ? "DARK " + String (-r) : "BRIGHT " + String (r);
                                                  })));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::bandwidth), "Bandwidth", pctRange, 30, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::hiss), "Hiss", pctRange, 15, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::ensemble), "Ensemble", pctRange, 30, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::mix), "Mix", pctRange, 100, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::output), "Output", NormalisableRange<float> (-24, 12, 0.1f), 0,
                                                  AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
                                                  { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
    return l;
}

std::vector<lt::presets::ParamPreset> factoryPresets()
{
    //                           tape  age wow flut drift sat tone band hiss ens mix out
    struct Row { const char* name; float v[12]; };
    static const Row rows[] = {
        { "Default",              { 0, 40, 30, 20, 20, 35,   0, 30, 15, 30, 100, 0 } },
        { "Mello Strings Classic",{ 0, 50, 35, 25, 25, 35, -10, 35, 20, 55, 100, 0 } },
        { "Strings Lush Ensemble",{ 0, 35, 25, 15, 20, 25,   5, 25, 10, 85, 100, 0 } },
        { "Mello Flute Breath",   { 1, 45, 45, 20, 25, 25,  -5, 40, 25, 35, 100, 0 } },
        { "Flute Seasick",        { 1, 70, 80, 30, 50, 30, -15, 50, 25, 30, 100, 0 } },
        { "Mello Choir Aahs",     { 2, 45, 30, 20, 25, 30,  -5, 35, 15, 75, 100, 0 } },
        { "Choir Haunted",        { 2, 75, 55, 25, 55, 40, -25, 55, 30, 90, 100, 0 } },
        { "Cassette Demo",        { 3, 50, 35, 40, 20, 45, -10, 45, 40, 15, 100, 0 } },
        { "Cassette Lo-Fi Beat",  { 3, 65, 45, 55, 30, 55, -20, 60, 55, 10, 100, 0 } },
        { "Reel Warmth",          { 4, 20, 10,  8, 10, 45,   0, 10,  8,  0, 100, 0 } },
        { "Reel Glue (subtle)",   { 4, 15,  8,  5,  5, 30,   5,  5,  5,  0,  60, 0 } },
        { "Broken Tape",          { 5, 70, 60, 50, 50, 55, -20, 55, 45, 20, 100, 0 } },
        { "Broken Memory",        { 5, 95, 85, 60, 80, 60, -35, 75, 60, 40, 100, 0 } },
    };
    std::vector<lt::presets::ParamPreset> out;
    for (auto& r : rows)
    {
        lt::presets::ParamPreset p;
        p.name = r.name;
        p.group = Tapes::names()[(int) r.v[0]];
        for (size_t i = 0; i < IDs::all.size(); ++i) p.values[IDs::all[i]] = r.v[i];
        out.push_back (p);
    }
    return out;
}

} // namespace saturn
