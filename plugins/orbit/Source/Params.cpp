#include "Params.h"

namespace orbit
{

const juce::StringArray& Notes::names()
{
    static const juce::StringArray n { "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1/1" };
    return n;
}

double Notes::beats (int i, int feel)
{
    static constexpr double b[] = { 1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 1.0, 2.0, 4.0 };
    auto v = b[juce::jlimit (0, 6, i)];
    return feel == 1 ? v * 1.5 : feel == 2 ? v * 2.0 / 3.0 : v;
}

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
    auto P = [] (const char* id) { return ParameterID { id, 1 }; };
    auto pct = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto hz = AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
                  { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });

    l.add (std::make_unique<AudioParameterChoice> (P (IDs::mode), "Mode", Modes::names(), Modes::tape));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::sync), "Sync", true));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::note), "Time (sync)", Notes::names(), 4));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::ms), "Time (ms)", NormalisableRange<float> (1.0f, 2000.0f, 0.1f, 0.4f), 375.0f,
                                                  AudioParameterFloatAttributes().withLabel ("ms")
                                                      .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::feel), "Feel", StringArray { "NORMAL", "DOTTED", "TRIPLET" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::feedback), "Feedback", NormalisableRange<float> (0, 100, 0.1f), 45, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::mix), "Mix", NormalisableRange<float> (0, 100, 0.1f), 30, pct));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::freeze), "Freeze", false));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::lowCut), "Low Cut", logRange (20, 2000), 120, hz));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::highCut), "High Cut", logRange (1000, 20000), 9000, hz));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::width), "Width", NormalisableRange<float> (0, 150, 0.1f), 100, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::duck), "Duck", NormalisableRange<float> (0, 100, 0.1f), 0, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::wobble), "Wobble", NormalisableRange<float> (0, 100, 0.1f), 20, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::drive), "Drive", NormalisableRange<float> (0, 100, 0.1f), 20, pct));
    return l;
}

std::vector<lt::presets::ParamPreset> factoryPresets()
{
    //                       mode sync note  ms  feel  fb  mix lowC  highC width duck wob drive
    struct Row { const char* name; float v[13]; };
    static const Row rows[] = {
        { "Default",              { 1, 1, 4, 375, 0, 45, 30, 120, 9000, 100, 0, 20, 20 } },
        { "Clean Digital 1/8",    { 0, 1, 3, 250, 0, 35, 25, 80, 16000, 100, 0, 0, 0 } },
        { "Slapback",             { 0, 0, 4, 110, 0, 10, 30, 120, 9000, 60, 0, 5, 10 } },
        { "Warm Tape 1/4",        { 1, 1, 4, 375, 0, 50, 30, 150, 7000, 100, 0, 30, 30 } },
        { "Tape Echo Dotted",     { 1, 1, 3, 375, 1, 55, 28, 180, 6000, 110, 20, 35, 35 } },
        { "Analog BBD",           { 2, 1, 4, 375, 0, 50, 30, 120, 5000, 100, 0, 20, 25 } },
        { "Dark Analog 1/8T",     { 2, 1, 3, 250, 2, 60, 30, 200, 3000, 90, 0, 25, 40 } },
        { "Ping-Pong 1/8",        { 3, 1, 3, 250, 0, 50, 30, 150, 10000, 130, 0, 10, 10 } },
        { "Wide Ping-Pong",       { 3, 1, 3, 375, 1, 55, 32, 200, 9000, 150, 25, 10, 10 } },
        { "Reverse Swell 1/2",    { 4, 1, 5, 750, 0, 40, 35, 150, 12000, 120, 0, 10, 0 } },
        { "Reverse Vocal 1/4",    { 4, 1, 4, 375, 0, 30, 30, 250, 11000, 110, 30, 5, 0 } },
        { "Shimmer Pad 1/2",      { 5, 1, 5, 750, 0, 60, 35, 300, 12000, 140, 0, 15, 0 } },
        { "Shimmer Lead 1/4D",    { 5, 1, 4, 375, 1, 45, 25, 400, 13000, 120, 30, 10, 0 } },
        { "Lo-Fi Cassette",       { 6, 1, 4, 375, 0, 45, 30, 300, 3200, 80, 0, 50, 40 } },
        { "Broken Radio",         { 6, 1, 3, 250, 1, 55, 35, 500, 2500, 50, 0, 70, 60 } },
        { "Dub Siren 1/4D",       { 7, 1, 4, 375, 1, 75, 35, 250, 2600, 110, 0, 25, 50 } },
        { "Dub Space 1/2",        { 7, 1, 5, 750, 0, 85, 30, 300, 2200, 130, 0, 30, 60 } },
    };
    static const char* ids[13] = { IDs::mode, IDs::sync, IDs::note, IDs::ms, IDs::feel, IDs::feedback, IDs::mix,
                                   IDs::lowCut, IDs::highCut, IDs::width, IDs::duck, IDs::wobble, IDs::drive };
    std::vector<lt::presets::ParamPreset> out;
    for (auto& r : rows)
    {
        lt::presets::ParamPreset p;
        p.name = r.name;
        p.group = Modes::names()[(int) r.v[0]];
        for (int i = 0; i < 13; ++i) p.values[ids[i]] = r.v[i];
        p.values[IDs::freeze] = 0.0f;
        out.push_back (p);
    }
    return out;
}

} // namespace orbit
