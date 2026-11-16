#include "Params.h"

namespace lt_void
{

const juce::StringArray& Notes::names()
{
    static const juce::StringArray n { "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/8D", "1/4" };
    return n;
}

double Notes::beats (int i)
{
    static constexpr double b[] = { 1.0 / 16, 1.0 / 12, 1.0 / 8, 1.0 / 6, 1.0 / 4, 1.0 / 3, 1.0 / 2, 3.0 / 4, 1.0 };
    return b[juce::jlimit (0, 8, i)];
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

    juce::String hz (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }
    juce::String pct (float v, int) { return juce::String (juce::roundToInt (v)) + "%"; }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto P = [] (const char* id) { return ParameterID { id, 1 }; };
    auto pctAttr = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pct);
    auto hzAttr = AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (hz);

    l.add (std::make_unique<AudioParameterChoice> (P (IDs::type), "Reverb Type", TypeInfo::names(), 0));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::mix), "Mix", NormalisableRange<float> (0, 100, 0.1f), 35, pctAttr));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::preDelay), "Pre-Delay", NormalisableRange<float> (0, 500, 0.1f, 0.5f), 20,
                                                  AudioParameterFloatAttributes().withLabel ("ms")
                                                      .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::preSync), "Pre-Delay Sync", false));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::preNote), "Pre-Delay Note", Notes::names(), 4));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::size), "Size", NormalisableRange<float> (0, 100, 0.1f), 90, pctAttr));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::decay), "Decay", NormalisableRange<float> (0.2f, 30.0f, 0.01f, 0.4f), 8.0f,
                                                  AudioParameterFloatAttributes().withLabel ("s")
                                                      .withStringFromValueFunction ([] (float v, int) { return String (v, v < 10 ? 1 : 0) + " s"; })));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::highFreq), "Damping High Freq", logRange (1000, 20000), 8000, hzAttr));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::lowFreq), "Damping Low Freq", logRange (20, 1000), 120, hzAttr));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::attack), "Attack", NormalisableRange<float> (0, 100, 0.1f), 20, pctAttr));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::diffusion), "Diffusion", NormalisableRange<float> (0, 100, 0.1f), 70, pctAttr));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::modRate), "Mod Rate", logRange (0.05f, 5.0f), 0.5f,
                                                  AudioParameterFloatAttributes().withLabel ("Hz")
                                                      .withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; })));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::modDepth), "Mod Depth", NormalisableRange<float> (0, 100, 0.1f), 50, pctAttr));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::lowCut), "Low Cut", logRange (20, 1000), 80, hzAttr));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::highCut), "High Cut", logRange (1000, 20000), 14000, hzAttr));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::width), "Width", NormalisableRange<float> (0, 150, 0.1f), 100, pctAttr));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::duck), "Duck", NormalisableRange<float> (0, 100, 0.1f), 0, pctAttr));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::freeze), "Freeze", false));
    return l;
}

std::vector<lt::presets::ParamPreset> factoryPresets()
{
    //                       mix  pre  size decay  hiF    loF  att diff rate depth lowCut highCut width duck
    struct Row { int type; const char* name; float v[14]; };
    static const Row rows[] = {
        { 0,  "Infinite Cloud",   { 35, 20, 90, 8.0f, 8000, 120, 20, 70, 0.5f, 50, 80, 14000, 100, 0 } },
        { 0,  "Soft Bloom",       { 30, 40, 75, 5.0f, 7000, 150, 60, 80, 0.3f, 40, 120, 12000, 110, 10 } },
        { 0,  "Ambient Wash",     { 50, 10, 100, 14.0f, 6000, 100, 40, 85, 0.25f, 70, 60, 11000, 130, 0 } },
        { 1,  "Vocal Plate",      { 25, 30, 55, 2.2f, 12000, 200, 0, 75, 0.8f, 20, 180, 16000, 100, 25 } },
        { 1,  "Bright Plate",     { 30, 10, 65, 3.0f, 16000, 150, 0, 85, 1.0f, 25, 150, 18000, 110, 0 } },
        { 1,  "Snare Plate",      { 30, 0, 45, 1.4f, 14000, 250, 0, 90, 0.9f, 15, 220, 15000, 100, 0 } },
        { 2,  "Concert Hall",     { 30, 25, 80, 3.2f, 8500, 100, 10, 70, 0.4f, 30, 60, 14000, 110, 0 } },
        { 2,  "Cathedral",        { 40, 45, 100, 7.5f, 6500, 80, 25, 80, 0.3f, 35, 40, 12000, 120, 0 } },
        { 2,  "Dark Hall",        { 30, 20, 85, 4.0f, 4500, 120, 15, 70, 0.4f, 30, 80, 8000, 100, 0 } },
        { 3,  "Small Room",       { 20, 5, 40, 0.6f, 9000, 150, 0, 60, 0.6f, 15, 100, 15000, 90, 0 } },
        { 3,  "Drum Room",        { 25, 0, 60, 0.9f, 10000, 120, 0, 70, 0.5f, 10, 80, 16000, 100, 0 } },
        { 3,  "Studio Live",      { 18, 8, 50, 0.7f, 11000, 180, 0, 65, 0.7f, 15, 120, 17000, 100, 0 } },
        { 4,  "Echo Chamber",     { 28, 15, 65, 2.0f, 7000, 150, 5, 75, 0.5f, 25, 100, 11000, 100, 0 } },
        { 4,  "Warm Chamber",     { 30, 20, 70, 2.6f, 5500, 120, 10, 80, 0.4f, 25, 90, 9000, 100, 0 } },
        { 4,  "Stone Chamber",    { 25, 10, 55, 1.8f, 9000, 200, 0, 85, 0.6f, 20, 140, 13000, 100, 0 } },
        { 5,  "80s Digital",      { 30, 25, 70, 3.0f, 8000, 120, 10, 70, 0.9f, 60, 100, 10000, 120, 0 } },
        { 5,  "Retro Rack",       { 35, 30, 80, 4.5f, 7000, 150, 15, 75, 0.6f, 70, 120, 9000, 110, 0 } },
        { 5,  "Gold Vintage",     { 28, 15, 60, 2.2f, 9000, 120, 5, 65, 1.2f, 50, 90, 11000, 100, 0 } },
        { 6,  "Classic Spring",   { 30, 0, 50, 2.0f, 5000, 150, 0, 60, 0.8f, 20, 150, 6000, 80, 0 } },
        { 6,  "Surf Tank",        { 40, 0, 65, 3.0f, 5500, 180, 0, 55, 1.0f, 25, 180, 6500, 90, 0 } },
        { 6,  "Dub Spring",       { 45, 20, 75, 4.0f, 4000, 120, 0, 50, 0.6f, 30, 120, 5000, 110, 20 } },
        { 7,  "Heaven",           { 40, 30, 90, 9.0f, 9000, 150, 40, 85, 0.3f, 50, 150, 14000, 130, 0 } },
        { 7,  "Octave Up Pad",    { 50, 20, 100, 14.0f, 8000, 200, 60, 90, 0.2f, 60, 200, 13000, 140, 0 } },
        { 7,  "Crystal",          { 35, 15, 70, 5.0f, 12000, 250, 20, 80, 0.5f, 40, 250, 17000, 120, 0 } },
        { 8,  "Reverse Swell",    { 40, 0, 85, 4.0f, 9000, 120, 0, 80, 0.4f, 30, 100, 13000, 110, 0 } },
        { 8,  "Backwards Vocal",  { 35, 10, 70, 3.0f, 10000, 200, 0, 75, 0.5f, 25, 180, 14000, 100, 0 } },
        { 8,  "Rewind",           { 50, 0, 100, 7.0f, 7500, 100, 0, 85, 0.3f, 40, 80, 12000, 120, 0 } },
        { 9,  "80s Gated Snare",  { 35, 0, 70, 4.0f, 10000, 150, 0, 85, 0.5f, 15, 150, 14000, 110, 0 } },
        { 9,  "Tight Gate",       { 30, 0, 55, 2.0f, 11000, 200, 0, 80, 0.5f, 10, 180, 15000, 100, 0 } },
        { 9,  "Long Gate",        { 35, 5, 85, 8.0f, 9000, 120, 0, 85, 0.4f, 15, 120, 13000, 120, 0 } },
        { 10, "Dusty Tape",       { 35, 20, 70, 3.0f, 5000, 150, 10, 70, 0.7f, 60, 250, 4000, 90, 0 } },
        { 10, "Bedroom Lo-Fi",    { 30, 10, 55, 2.0f, 4500, 200, 5, 65, 0.9f, 70, 300, 3500, 80, 10 } },
        { 10, "Old Radio",        { 40, 15, 60, 2.5f, 3500, 300, 0, 60, 1.1f, 50, 400, 3000, 50, 0 } },
        { 11, "The Void",         { 45, 30, 100, 20.0f, 5000, 80, 50, 90, 0.15f, 50, 40, 9000, 130, 0 } },
        { 11, "Event Horizon",    { 50, 60, 100, 30.0f, 4000, 60, 70, 95, 0.1f, 60, 30, 7000, 140, 0 } },
        { 11, "Dark Matter",      { 40, 20, 90, 12.0f, 3500, 100, 40, 85, 0.2f, 40, 60, 6000, 120, 0 } },
    };
    static const char* ids[14] = { IDs::mix, IDs::preDelay, IDs::size, IDs::decay, IDs::highFreq, IDs::lowFreq, IDs::attack,
                                   IDs::diffusion, IDs::modRate, IDs::modDepth, IDs::lowCut, IDs::highCut, IDs::width, IDs::duck };

    std::vector<lt::presets::ParamPreset> out;
    for (auto& r : rows)
    {
        lt::presets::ParamPreset p;
        p.name = r.name;
        p.group = TypeInfo::names()[r.type];
        p.values[IDs::type] = (float) r.type;
        for (int i = 0; i < 14; ++i)
            p.values[ids[i]] = r.v[i];
        p.values[IDs::freeze] = 0.0f;
        p.values[IDs::preSync] = 0.0f;
        out.push_back (p);
    }
    return out;
}

} // namespace lt_void
