#include "Params.h"

namespace air
{

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto P = [] (const char* id) { return ParameterID { id, 1 }; };
    auto amt = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)); });
    auto db = AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
                                                    { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    auto range = NormalisableRange<float> (0, 100, 0.1f);

    // defaults = "Silky Pop Vocal" (the approved design)
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::bodyAmount), "Body", range, 45, amt));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::bodyModel), "Body Model", Models::names (Models::body), 1));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::midAmount), "Mid Air", range, 20, amt));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::midModel), "Mid Air Model", Models::names (Models::mid), 0));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::highAmount), "High Air", range, 30, amt));
    l.add (std::make_unique<AudioParameterChoice> (P (IDs::highModel), "High Air Model", Models::names (Models::high), 1));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::link), "Link", false));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::trim), "Trim", NormalisableRange<float> (-24, 12, 0.1f), 0, db));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::mix), "Mix", range, 100,
                                                  AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int)
                                                  { return String (roundToInt (v)) + "%"; })));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::output), "Output", NormalisableRange<float> (-24, 12, 0.1f), 0, db));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::bypass), "Bypass", false));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::autoGain), "Auto Gain", false));
    return l;
}

std::vector<lt::presets::ParamPreset> factoryPresets()
{
    // models: BODY 0 GRAVITY 1 CORE 2 PROXIMA 3 TITAN | MID 0 LUMEN 1 NOVA 2 QUASAR 3 PHOTON | HIGH 0 STRATOS 1 ZENITH 2 PULSAR 3 COMET
    //                                   body  bm   mid  mm   high hm  link  trim  mix  out
    struct Row { const char* group; const char* name; float v[10]; };
    static const Row rows[] = {
        { "POP",       "Silky Pop Vocal",      { 45, 1, 20, 0, 30, 1, 0, 0, 100, 0 } },
        { "POP",       "Big Air Hook",         { 30, 1, 45, 0, 65, 0, 1, 0, 100, 0 } },
        { "POP",       "Radio Pop Shine",      { 30, 1, 35, 0, 50, 0, 1, 0, 100, 0 } },
        { "POP",       "Breathy Pop Whisper",  { 25, 2, 15, 0, 60, 0, 0, 0, 100, 0 } },
        { "TRAP",      "Trap Lead Air",        { 35, 0, 40, 3, 55, 0, 1, 0, 100, 0 } },
        { "TRAP",      "Melodic Trap Gloss",   { 40, 3, 25, 0, 45, 2, 0, 0, 100, 0 } },
        { "TRAP",      "Ad-lib Sparkle",       { 15, 1, 40, 3, 55, 3, 1, 0, 85, 0 } },
        { "REGGAETON", "Reggaeton Front",      { 40, 1, 45, 1, 40, 0, 1, 0, 100, 0 } },
        { "REGGAETON", "Perreo Presence",      { 30, 0, 50, 3, 35, 2, 0, 0, 100, 0 } },
        { "R&B",       "R&B Velvet",           { 55, 3, 15, 0, 40, 1, 0, 0, 100, 0 } },
        { "R&B",       "Late Night Silk",      { 50, 2, 15, 1, 45, 1, 0, 0, 100, 0 } },
        { "RAP",       "Rap Punch",            { 50, 1, 45, 3, 30, 0, 0, 0, 100, 0 } },
        { "RAP",       "Boom Bap Grit",        { 45, 3, 35, 1, 20, 2, 0, 0, 100, 0 } },
        { "RAP",       "Drill Edge",           { 35, 0, 45, 2, 40, 3, 1, 0, 100, 0 } },
        { "OTHER",     "Podcast Warm",         { 40, 2, 25, 2, 15, 0, 0, 0, 100, 0 } },
        { "OTHER",     "Backing Vocals Glow",  { 20, 1, 15, 0, 55, 1, 1, 0, 70, 0 } },
        { "OTHER",     "Subtle Polish",        { 20, 1, 10, 0, 20, 1, 0, 0, 100, 0 } },
    };

    std::vector<lt::presets::ParamPreset> out;
    for (auto& r : rows)
    {
        lt::presets::ParamPreset p;
        p.name = r.name;
        p.group = r.group;
        for (size_t i = 0; i < IDs::presetParams.size(); ++i) p.values[IDs::presetParams[i]] = r.v[i];
        out.push_back (p);
    }
    return out;
}

void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float realValue)
{
    if (auto* p = s.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

} // namespace air
