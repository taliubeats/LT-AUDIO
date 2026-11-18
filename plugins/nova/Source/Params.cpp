#include "Params.h"

namespace nova
{

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto P = [] (const char* id) { return ParameterID { id, 1 }; };
    auto pct = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto db = AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
                 { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    auto bip = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                 { return std::abs (v) < 0.5f ? String ("0") : (v > 0 ? "+" : "") + String (roundToInt (v)); });

    l.add (std::make_unique<AudioParameterChoice> (P (IDs::type), "Type", SatTypes::names(), (int) SatType::tube));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::drive), "Drive", NormalisableRange<float> (0, 100, 0.1f), 35, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::tone), "Tone", NormalisableRange<float> (-100, 100, 0.1f), 0, bip));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::warmth), "Warmth", NormalisableRange<float> (0, 100, 0.1f), 20, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::air), "Air", NormalisableRange<float> (0, 100, 0.1f), 10, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::character), "Character", NormalisableRange<float> (0, 100, 0.1f), 40,
           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
           { return v < 45 ? "EVEN " + String (roundToInt (100 - v * 2)) + "%" : v > 55 ? "ODD " + String (roundToInt (v * 2 - 100)) + "%" : String ("BALANCED"); })));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::input), "Input", NormalisableRange<float> (-18, 18, 0.1f), 0, db));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::mix), "Mix", NormalisableRange<float> (0, 100, 0.1f), 100, pct));
    l.add (std::make_unique<AudioParameterFloat> (P (IDs::output), "Output", NormalisableRange<float> (-18, 18, 0.1f), 0, db));
    l.add (std::make_unique<AudioParameterBool> (P (IDs::autoGain), "Auto Gain", true));
    lt::dsp::FocusParams::addTo (l);
    return l;
}

std::vector<lt::presets::ParamPreset> factoryPresets()
{
    struct P { const char* name; const char* group; int type; float drive, tone, warmth, air, character, mix; };
    static const P list[] = {
        { "Default",              "GENERAL", (int) SatType::tube,        35,   0, 20, 10, 40, 100 },
        { "Warm Glue",            "MIX",     (int) SatType::tape,        28,  -5, 30,  5, 30, 100 },
        { "Master Sheen",         "MIX",     (int) SatType::console,     30,  10, 10, 25, 45,  80 },
        { "Expensive Console",    "MIX",     (int) SatType::console,     45,   0, 15, 10, 50, 100 },
        { "Tape Bus",             "MIX",     (int) SatType::tape,        50,  -8, 25, 0,  35, 100 },
        { "Vocal Warmth",         "VOCAL",   (int) SatType::tube,        30,   5, 25, 30, 25,  70 },
        { "Vocal Presence",       "VOCAL",   (int) SatType::exciter,     40,  10,  0, 40, 60, 100 },
        { "Rap Vocal Grit",       "VOCAL",   (int) SatType::transistor,  45,  15, 10, 20, 70,  45 },
        { "Drum Bus Smash",       "DRUMS",   (int) SatType::transistor,  60,   5, 20, 15, 65,  65 },
        { "Kick Knock",           "DRUMS",   (int) SatType::transformer, 55,   0, 40,  0, 40, 100 },
        { "Snare Crack",          "DRUMS",   (int) SatType::diode,       50,  20,  0, 25, 80,  60 },
        { "808 Heat",             "BASS",    (int) SatType::transformer, 65, -10, 45,  0, 35, 100 },
        { "808 Distorted",        "BASS",    (int) SatType::diode,       75,   0, 30,  0, 85,  80 },
        { "Bass Tube Fat",        "BASS",    (int) SatType::tube,        55, -10, 50,  0, 20, 100 },
        { "Lo-Fi Record",         "LO-FI",   (int) SatType::vinyl,       55, -30, 30,  0, 40, 100 },
        { "Old Cassette",         "LO-FI",   (int) SatType::tape,        70, -40, 35,  0, 45, 100 },
        { "Crushed Keys",         "LO-FI",   (int) SatType::diode,       65, -20, 10,  0, 90,  55 },
        { "Hi-Hat Shine",         "DRUMS",   (int) SatType::exciter,     50,  15,  0, 45, 70, 100 },
        { "Guitar Amp Crunch",    "CREATIVE",(int) SatType::tube,        85,  10, 20, 10, 70, 100 },
        { "Supernova",            "CREATIVE",(int) SatType::transistor, 100,  10, 30, 30, 100, 100 },
    };

    std::vector<lt::presets::ParamPreset> out;
    for (auto& p : list)
    {
        lt::presets::ParamPreset pr;
        pr.name = p.name; pr.group = p.group; pr.factory = true;
        pr.values = { { IDs::type, (float) p.type }, { IDs::drive, p.drive }, { IDs::tone, p.tone }, { IDs::warmth, p.warmth },
                      { IDs::air, p.air }, { IDs::character, p.character }, { IDs::mix, p.mix }, { IDs::input, 0.0f },
                      { IDs::output, 0.0f }, { IDs::autoGain, 1.0f } };
        out.push_back (pr);
    }
    return out;
}

} // namespace nova
