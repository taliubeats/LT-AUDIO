#include "Params.h"

namespace meteor
{

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto P = [] (const char* id) { return ParameterID { id, 1 }; };
    auto pct = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto r = NormalisableRange<float> (0, 100, 0.1f);

    l.add (std::make_unique<AudioParameterBool> (P (IDs::revOn), "Reverse On", false));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::revSize), "Reverse Size", Choices::reverseSizes(), 2));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::revMix), "Reverse Mix", r, 100, pct));

    l.add (std::make_unique<AudioParameterBool> (P (IDs::dblOn), "Double On", false));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::dblVoices), "Double Voices", Choices::voices(), 1));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::dblDetune), "Double Detune", r, 40, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::dblWidth), "Double Width", r, 80, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::dblMix), "Double Mix", r, 60, pct));

    l.add (std::make_unique<AudioParameterBool> (P (IDs::chopOn), "Chop On", false));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::chopRate), "Chop Rate", Choices::rates(), 2));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::chopPattern), "Chop Pattern", Choices::patterns(), 0));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::chopDepth), "Chop Depth", r, 100, pct));

    l.add (std::make_unique<AudioParameterBool> (P (IDs::glitchOn), "Glitch On", false));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::glitchRate), "Glitch Rate", Choices::rates(), 2));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::glitchChance), "Glitch Chance", r, 30, pct));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::glitchRepeats), "Glitch Repeats", Choices::repeats(), 1));

    l.add (std::make_unique<AudioParameterFloat> (P (IDs::mix), "Mix", r, 100, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::output), "Output", NormalisableRange<float> (-24, 12, 0.1f), 0,
                                                  AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
                                                  { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));

    l.add (std::make_unique<AudioParameterFloat> (P (IDs::strokeLength), "Stroke Length", r, 50, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::strokeIntensity), "Stroke Intensity", r, 50, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::strokeTone), "Stroke Tone", NormalisableRange<float> (-100, 100, 0.1f), 0,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  {
                                                      auto i = roundToInt (v);
                                                      return i == 0 ? String ("0") : i < 0 ? "DARK " + String (-i) : "BRIGHT " + String (i);
                                                  })));
    return l;
}

std::vector<lt::presets::ParamPreset> factoryPresets()
{
    //                         revOn size mix  dblOn vox det wid mix  chopOn rate pat depth  glOn rate chance rep  mix out
    struct Row { const char* name; const char* group; float v[18]; };
    static const Row rows[] = {
        { "Default",            "DEFAULT", { 0, 2, 100,  0, 1, 40, 80, 60,  0, 2, 0, 100,  0, 2, 30, 1,  100, 0 } },
        { "Rewind Vocal 1/4",   "REVERSE", { 1, 2, 100,  0, 1, 40, 80, 60,  0, 2, 0, 100,  0, 2, 30, 1,  100, 0 } },
        { "Backwards Bar",      "REVERSE", { 1, 4, 100,  1, 0, 30, 70, 40,  0, 2, 0, 100,  0, 2, 30, 1,  100, 0 } },
        { "Half Reverse Blend", "REVERSE", { 1, 3, 50,   0, 1, 40, 80, 60,  0, 2, 0, 100,  0, 2, 30, 1,  100, 0 } },
        { "Tight Double",       "DOUBLE",  { 0, 2, 100,  1, 0, 25, 90, 55,  0, 2, 0, 100,  0, 2, 30, 1,  100, 0 } },
        { "Wide Stack x4",      "DOUBLE",  { 0, 2, 100,  1, 2, 55, 100, 70, 0, 2, 0, 100,  0, 2, 30, 1,  100, 0 } },
        { "Chorus Doubles",     "DOUBLE",  { 0, 2, 100,  1, 1, 90, 80, 60,  0, 2, 0, 100,  0, 2, 30, 1,  100, 0 } },
        { "Chop 1/16",          "CHOP",    { 0, 2, 100,  0, 1, 40, 80, 60,  1, 2, 0, 100,  0, 2, 30, 1,  100, 0 } },
        { "Offbeat Chops",      "CHOP",    { 0, 2, 100,  0, 1, 40, 80, 60,  1, 1, 1, 100,  0, 2, 30, 1,  100, 0 } },
        { "Tresillo Gate",      "CHOP",    { 0, 2, 100,  1, 0, 30, 80, 40,  1, 2, 2, 90,   0, 2, 30, 1,  100, 0 } },
        { "Random Cuts",        "CHOP",    { 0, 2, 100,  0, 1, 40, 80, 60,  1, 3, 3, 100,  0, 2, 30, 1,  100, 0 } },
        { "Glitch Light",       "GLITCH",  { 0, 2, 100,  0, 1, 40, 80, 60,  0, 2, 0, 100,  1, 1, 20, 1,  100, 0 } },
        { "Glitch Storm",       "GLITCH",  { 0, 2, 100,  0, 1, 40, 80, 60,  1, 3, 3, 70,   1, 2, 55, 3,  100, 0 } },
        { "Stutter Rolls 1/8",  "GLITCH",  { 0, 2, 100,  0, 1, 40, 80, 60,  0, 2, 0, 100,  1, 1, 40, 4,  100, 0 } },
        { "Meteor Shower",      "COMBO",   { 1, 1, 60,   1, 2, 50, 100, 55, 1, 2, 3, 60,   1, 2, 35, 2,  100, 0 } },
        { "Broken Doubles",     "COMBO",   { 0, 2, 100,  1, 1, 60, 90, 60,  0, 2, 0, 100,  1, 2, 30, 1,  100, 0 } },
    };
    std::vector<lt::presets::ParamPreset> out;
    for (auto& row : rows)
    {
        lt::presets::ParamPreset p;
        p.name = row.name;
        p.group = row.group;
        for (size_t i = 0; i < IDs::live.size(); ++i) p.values[IDs::live[i]] = row.v[i];
        out.push_back (p);
    }
    return out;
}

} // namespace meteor
