#include "Params.h"

namespace corona
{

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto P = [] (const char* id) { return ParameterID { id, 1 }; };
    auto dB = [] (bool sign) { return AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([sign] (float v, int)
                  { return (sign && v > 0.0f ? "+" : "") + String (v, 1) + " dB"; }); };
    auto plain = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); });

    l.add (std::make_unique<AudioParameterFloat> (P (IDs::input), "Input", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, dB (true)));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::threshold), "Threshold", NormalisableRange<float> (0.0f, 40.0f, 0.1f), 12.0f, plain));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::attack), "Attack", StringArray { "SLOW (59 ms)", "MEDIUM (17 ms)", "FAST (4.8 ms)" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::release), "Release",
                                                   StringArray { "SLOW (590 ms)", "2 (207 ms)", "3 (73 ms)", "4 (26 ms)", "FAST (9 ms)" }, 2));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::debump), "De-Bump", NormalisableRange<float> (0.0f, 10.0f, 0.1f), 0.0f, plain));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::filter), "Filter", StringArray { "OFF", "50 Hz", "100 Hz", "150 Hz", "200 Hz", "250 Hz" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::makeup), "Make Up", NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f, dB (true)));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::mix), "Dry/Wet", NormalisableRange<float> (0.0f, 10.0f, 0.1f), 10.0f, plain));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::power), "Power", true));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::autoGain), "Auto Gain", false));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::mode), "Mode", StringArray { "STEREO", "M/S", "MONO" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::ovs), "Oversampling", StringArray { "1x", "2x", "4x" }, 1));
    return l;
}

std::vector<lt::presets::ParamPreset> factoryPresets()
{
    //                                     input thr  atk rel debump filt makeup mix
    struct Row { const char* group; const char* name; float v[8]; };
    static const Row rows[] = {
        { "INIT",     "Default",          { 0, 12, 1, 2, 0, 0, 0, 10 } },
        { "VOCAL",    "Velvet Lead",      { 3, 14, 1, 2, 3, 2, 5, 10 } },
        { "VOCAL",    "Rap Upfront",      { 6, 20, 2, 3, 5, 3, 7, 10 } },
        { "VOCAL",    "Smooth 12 dB",     { 4, 24, 1, 1, 4, 2, 8, 10 } },
        { "DRUM BUS", "Glue the Kit",     { 2, 12, 0, 2, 0, 2, 4, 7 } },
        { "DRUM BUS", "Thick Drums",      { 4, 18, 1, 4, 0, 2, 6, 10 } },
        { "DRUM BUS", "Parallel Thunder", { 10, 34, 2, 3, 0, 1, 12, 4 } },
        { "BASS",     "808 Tamer",        { 0, 16, 1, 1, 8, 0, 5, 10 } },
        { "BASS",     "Bass Body",        { 3, 18, 0, 2, 0, 0, 5, 10 } },
        { "MIX BUS",  "Crown Glue",       { 0, 8, 0, 2, 6, 2, 2, 10 } },
        { "MIX BUS",  "Master Touch",     { 0, 5, 0, 1, 10, 3, 1, 10 } },
        { "LO-FI",    "Broadcast Squash", { 14, 30, 1, 4, 6, 3, 9, 10 } },
    };
    static const char* ids[8] = { IDs::input, IDs::threshold, IDs::attack, IDs::release, IDs::debump, IDs::filter, IDs::makeup, IDs::mix };
    std::vector<lt::presets::ParamPreset> out;
    for (auto& r : rows)
    {
        lt::presets::ParamPreset p;
        p.name = r.name;
        p.group = r.group;
        for (int i = 0; i < 8; ++i) p.values[ids[i]] = r.v[i];
        p.values[IDs::power] = 1.0f;
        p.values[IDs::autoGain] = 0.0f;
        out.push_back (p);
    }
    return out;
}

} // namespace corona
