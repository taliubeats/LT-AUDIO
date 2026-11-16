#include "Params.h"

namespace knock
{

const std::vector<ChaosRange>& chaosTone()
{
    static const std::vector<ChaosRange> r { { IDs::punch, 0, 70 }, { IDs::saturation, 0, 60 }, { IDs::distortion, 0, 35 },
                                             { IDs::distType, 0, 3 }, { IDs::bright, 0, 55 }, { IDs::low, 0, 55 },
                                             { IDs::glue, 0, 60 }, { IDs::width, 80, 150 } };
    return r;
}

const std::vector<ChaosRange>& chaosFx()
{
    static const std::vector<ChaosRange> r { { IDs::crush, 0, 45 }, { IDs::lofi, 0, 55 }, { IDs::stutter, 0, 40 },
                                             { IDs::filter, -45, 45 }, { IDs::ring, 0, 25 }, { IDs::reverse, 0, 35 } };
    return r;
}

const std::vector<ChaosRange>& chaosSpace()
{
    static const std::vector<ChaosRange> r { { IDs::reverb, 0, 45 }, { IDs::delay, 0, 40 }, { IDs::delayTime, 0, 5 } };
    return r;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto P = [] (const char* id) { return ParameterID { id, 1 }; };
    auto pct = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto db = AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
                 { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    auto amount = [&] (const char* id, const char* name, float def = 0.0f)
    { l.add (std::make_unique<AudioParameterFloat> (P (id), name, NormalisableRange<float> (0, 100, 0.1f), def, pct)); };

    l.add (std::make_unique<AudioParameterFloat> (P (IDs::knock), "Knock", NormalisableRange<float> (0, 100, 0.1f), 75, pct));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::mode), "Mode", StringArray { "DRUMS", "MELODY" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::input), "Input", NormalisableRange<float> (-24, 24, 0.1f), 0, db));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::output), "Output", NormalisableRange<float> (-24, 24, 0.1f), 0, db));
    amount (IDs::punch, "Punch", 30); amount (IDs::saturation, "Saturation", 20); amount (IDs::distortion, "Distortion");
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::distType), "Distortion Type", DistTypes::names(), 0));
    amount (IDs::bright, "Bright", 15); amount (IDs::low, "Low", 15); amount (IDs::glue, "Glue", 25);
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::width), "Width", NormalisableRange<float> (0, 200, 0.1f), 100, pct));
    amount (IDs::crush, "Crush"); amount (IDs::lofi, "Lo-Fi"); amount (IDs::stutter, "Stutter");
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::filter), "Filter", NormalisableRange<float> (-100, 100, 0.1f), 0,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                      { return std::abs (v) < 2 ? String ("OFF") : (v < 0 ? "LP " : "HP ") + String (roundToInt (std::abs (v))) + "%"; })));
    amount (IDs::ring, "Ring"); amount (IDs::reverse, "Reverse");
    amount (IDs::reverb, "Reverb", 10); amount (IDs::delay, "Delay");
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::delayTime), "Delay Time", DelayTimes::names(), 3));
    lt::dsp::FocusParams::addTo (l);
    return l;
}

std::vector<lt::presets::ParamPreset> factoryPresets()
{
    //                           mode knock punch sat dist type bright low glue width crush lofi stut filt ring rev verb dly dtime
    struct Row { const char* name; float v[19]; };
    static const Row rows[] = {
        { "Default Preset",    { 0, 75, 30, 20, 0, 0, 15, 15, 25, 100, 0, 0, 0, 0, 0, 0, 10, 0, 3 } },
        { "Punchy Kick",       { 0, 100, 75, 25, 0, 0, 10, 45, 30, 100, 0, 0, 0, 0, 0, 0, 0, 0, 3 } },
        { "Fat Snare",         { 0, 90, 55, 40, 10, 3, 35, 20, 40, 110, 0, 0, 0, 0, 0, 0, 20, 0, 3 } },
        { "Trap 808 Boom",     { 0, 85, 20, 55, 25, 0, 0, 60, 20, 90, 0, 0, 0, 0, 0, 0, 0, 0, 3 } },
        { "Lo-Fi Drums",       { 0, 80, 25, 35, 0, 0, 0, 25, 35, 90, 25, 60, 0, -30, 0, 0, 15, 0, 3 } },
        { "Crushed Hats",      { 0, 70, 30, 10, 0, 0, 40, 0, 20, 130, 55, 0, 0, 25, 0, 0, 10, 0, 3 } },
        { "Glitch Loop",       { 0, 85, 35, 20, 15, 1, 20, 10, 25, 120, 20, 0, 55, 0, 15, 0, 10, 20, 1 } },
        { "Space Drums",       { 0, 70, 40, 15, 0, 0, 25, 15, 30, 140, 0, 0, 0, 0, 0, 30, 45, 25, 4 } },
        { "Dusty Keys",        { 1, 80, 0, 30, 0, 3, 0, 20, 25, 100, 15, 65, 0, -25, 0, 0, 25, 0, 3 } },
        { "Wide Pad",          { 1, 75, 0, 15, 0, 0, 25, 10, 20, 180, 0, 15, 0, 0, 0, 25, 50, 20, 5 } },
        { "Dark Melody",       { 1, 80, 0, 25, 10, 3, 0, 35, 25, 110, 0, 20, 0, -55, 0, 15, 35, 15, 3 } },
        { "Stutter Vox",       { 1, 70, 10, 15, 0, 0, 30, 0, 25, 110, 0, 0, 60, 15, 0, 0, 20, 20, 1 } },
        { "Radio Lead",        { 1, 85, 15, 30, 30, 2, 20, 0, 30, 70, 25, 40, 0, 35, 20, 0, 15, 15, 2 } },
    };
    static const char* ids[19] = { IDs::mode, IDs::knock, IDs::punch, IDs::saturation, IDs::distortion, IDs::distType, IDs::bright,
                                   IDs::low, IDs::glue, IDs::width, IDs::crush, IDs::lofi, IDs::stutter, IDs::filter, IDs::ring,
                                   IDs::reverse, IDs::reverb, IDs::delay, IDs::delayTime };
    std::vector<lt::presets::ParamPreset> out;
    for (auto& r : rows)
    {
        lt::presets::ParamPreset p;
        p.name = r.name;
        p.group = r.v[0] > 0.5f ? "MELODY" : "DRUMS";
        for (int i = 0; i < 19; ++i) p.values[ids[i]] = r.v[i];
        p.values[IDs::input] = 0; p.values[IDs::output] = 0;
        out.push_back (p);
    }
    return out;
}

} // namespace knock
