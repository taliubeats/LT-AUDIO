#include "Params.h"
#include "ReverbTypes.h"

namespace vocal
{

const std::vector<ParamInfo>& paramInfos()
{
    static const std::vector<ParamInfo> list = {
        { IDs::tuneOn,  "Tune On",  "TUNE", 0, 1, 1, "bool", "pitch correction on/off" },
        { IDs::key,     "Key",      "TUNE", 0, 11, 9, "choice", "key of the song (C..B)" },
        { IDs::scale,   "Scale",    "TUNE", 0, 7, 2, "choice", "scale; Chromatic corrects to every semitone" },
        { IDs::voice,   "Voice",    "TUNE", 0, 4, 2, "choice", "singer range for pitch detection" },
        { IDs::retune,  "Retune Speed", "TUNE", 0, 400, 25, "ms", "0 = instant robotic hard tune, 10-25 = modern pop/trap, 40-80 = natural" },
        { IDs::humanize,"Humanize", "TUNE", 0, 100, 15, "%", "keeps long notes more natural" },
        { IDs::flex,    "Flex Tune","TUNE", 0, 100, 0, "%", "only corrects notes that are already close; keeps expressive slides" },
        { IDs::tuneMix, "Tune Mix", "TUNE", 0, 100, 100, "%", "amount of corrected voice" },
        { IDs::lowCut,  "Low Cut",  "CLEANUP", 20, 400, 90, "Hz", "removes rumble and pops below this frequency" },
        { IDs::gate,    "Gate",     "CLEANUP", -80, -20, -80, "dB", "noise gate threshold; -80 = off" },
        { IDs::deEss,   "De-Ess",   "CLEANUP", 0, 100, 35, "%", "reduces harsh s / sh sounds" },
        { IDs::deEssFreq,"De-Ess Freq","CLEANUP", 3000, 12000, 6500, "Hz", "where the sibilance is" },
        { IDs::body,    "Body",     "TONE", -6, 6, 0, "dB", "low shelf at 180 Hz: warmth and weight" },
        { IDs::mud,     "Mud Cut",  "TONE", 0, 10, 2, "dB", "cut at 350 Hz: removes boxy, muddy sound" },
        { IDs::presence,"Presence", "TONE", -6, 8, 2, "dB", "peak at 3.2 kHz: clarity and upfront voice" },
        { IDs::air,     "Air",      "TONE", -6, 10, 3, "dB", "high shelf at 12 kHz: brightness and air" },
        { IDs::comp,    "Compression", "DYNAMICS", 0, 100, 45, "%", "level-adaptive vocal compression amount" },
        { IDs::attack,  "Attack",   "DYNAMICS", 1, 50, 8, "ms", "compressor attack; low = tighter, high = punchier consonants" },
        { IDs::release, "Release",  "DYNAMICS", 30, 500, 120, "ms", "compressor release" },
        { IDs::satType, "Saturation Type", "DYNAMICS", 0, 7, 1, "choice", "TAPE, TUBE, CONSOLE, TRANSFORMER, TRANSISTOR, DIODE, VINYL, EXCITER" },
        { IDs::satDrive,"Saturation", "DYNAMICS", 0, 100, 20, "%", "saturation drive (colour, grit)" },
        { IDs::satMix,  "Saturation Mix", "DYNAMICS", 0, 100, 60, "%", "parallel saturation amount" },
        { IDs::doubler, "Doubler",  "FX", 0, 100, 0, "%", "stereo double: wider, thicker vocal" },
        { IDs::phone,   "Phone",    "FX", 0, 100, 0, "%", "telephone / radio band-pass effect" },
        { IDs::width,   "Width",    "FX", 0, 150, 100, "%", "stereo width (100 = unchanged)" },
        { IDs::delayOn, "Delay On", "SPACE", 0, 1, 1, "bool", "delay on/off" },
        { IDs::delayNote,"Delay Time","SPACE", 0, 5, 3, "choice", "delay time synced to the tempo" },
        { IDs::delayMode,"Delay Mode","SPACE", 0, 7, 1, "choice", "DIGITAL, TAPE, ANALOG, PING-PONG, REVERSE, SHIMMER, LO-FI, DUB" },
        { IDs::delayFeedback,"Delay Feedback","SPACE", 0, 90, 30, "%", "number of repeats" },
        { IDs::delayMix,"Delay Mix","SPACE", 0, 100, 12, "%", "delay level" },
        { IDs::reverbOn,"Reverb On","SPACE", 0, 1, 1, "bool", "reverb on/off" },
        { IDs::reverbType,"Reverb Type","SPACE", 0, 11, 1, "choice", "CLOUD, PLATE, HALL, ROOM, CHAMBER, VINTAGE, SPRING, SHIMMER, REVERSE, GATED, LO-FI, VOID" },
        { IDs::reverbDecay,"Reverb Decay","SPACE", 0.2f, 12, 1.8f, "s", "reverb length" },
        { IDs::reverbPre,"Pre-Delay","SPACE", 0, 200, 25, "ms", "gap before the reverb; keeps words clear" },
        { IDs::reverbMix,"Reverb Mix","SPACE", 0, 100, 16, "%", "reverb level" },
        { IDs::output,  "Output",   "OUTPUT", -18, 18, 0, "dB", "output level" },
        { IDs::bypass,  "Bypass",   "OUTPUT", 0, 1, 0, "bool", "bypass the whole chain" },
    };
    return list;
}

const ParamInfo* findParam (const juce::String& id)
{
    for (auto& p : paramInfos())
        if (id == p.id) return &p;
    return nullptr;
}

const juce::StringArray& DelayNotes::names()
{
    static const juce::StringArray n { "1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2" };
    return n;
}

double DelayNotes::beats (int i)
{
    static const double b[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    return b[juce::jlimit (0, 5, i)];
}

juce::StringArray choicesFor (const juce::String& id)
{
    if (id == IDs::key)        return tune::Keys::names();
    if (id == IDs::scale)      return tune::Scales::names();
    if (id == IDs::voice)      return tune::Voices::names();
    if (id == IDs::satType)    return nova::SatTypes::names();
    if (id == IDs::delayNote)  return DelayNotes::names();
    if (id == IDs::delayMode)  return orbit::Modes::names();
    if (id == IDs::reverbType) return lt_void::TypeInfo::names();
    return {};
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& p : paramInfos())
    {
        ParameterID pid { p.id, 1 };
        String unit (p.unit);
        if (unit == "bool")
            l.add (std::make_unique<AudioParameterBool> (pid, p.name, p.def > 0.5f));
        else if (unit == "choice")
            l.add (std::make_unique<AudioParameterChoice> (pid, p.name, choicesFor (p.id), roundToInt (p.def)));
        else
        {
            NormalisableRange<float> range (p.min, p.max, 0.01f);
            if (unit == "Hz" || unit == "ms" || unit == "s") range.setSkewForCentre (std::sqrt (juce::jmax (0.01f, p.min) * p.max));
            auto attrs = AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction ([unit] (float v, int)
            {
                if (unit == "%")  return String (roundToInt (v)) + "%";
                if (unit == "dB") return (v > 0.05f ? "+" : "") + String (v, 1) + " dB";
                if (unit == "Hz") return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz";
                if (unit == "ms") return String (roundToInt (v)) + " ms";
                if (unit == "s")  return String (v, 1) + " s";
                return String (v, 1);
            });
            l.add (std::make_unique<AudioParameterFloat> (pid, p.name, range, p.def, attrs));
        }
    }
    return l;
}

std::vector<lt::presets::ParamPreset> factoryPresets()
{
    using V = std::map<juce::String, float>;
    auto make = [] (const char* name, const char* group, V overrides)
    {
        lt::presets::ParamPreset pr;
        pr.name = name; pr.group = group; pr.factory = true;
        for (auto& p : paramInfos())
            if (juce::String (p.id) != IDs::bypass && juce::String (p.id) != IDs::key && juce::String (p.id) != IDs::scale
                && juce::String (p.id) != IDs::voice)
                pr.values[p.id] = p.def;
        for (auto& [k, v] : overrides) pr.values[k] = v;
        return pr;
    };
    using namespace IDs;
    return {
        make ("Default", "GENERAL", {}),
        make ("Melodic Trap Lead", "TRAP", { { retune, 8 }, { humanize, 5 }, { comp, 60 }, { air, 5 }, { presence, 3 }, { delayMix, 18 }, { delayNote, 3 }, { reverbType, 0 }, { reverbMix, 18 }, { reverbDecay, 2.6f }, { satDrive, 22 } }),
        make ("Hard Tune Robot", "TRAP", { { retune, 0 }, { humanize, 0 }, { flex, 0 }, { comp, 65 }, { air, 4 }, { delayMix, 14 }, { reverbMix, 12 } }),
        make ("Rage Vocal", "TRAP", { { retune, 0 }, { comp, 75 }, { satType, 4 }, { satDrive, 55 }, { satMix, 70 }, { presence, 4 }, { air, 4 }, { doubler, 35 }, { delayMix, 10 }, { reverbMix, 10 } }),
        make ("Drill Vocal", "RAP", { { retune, 30 }, { comp, 70 }, { mud, 4 }, { presence, 4 }, { satType, 4 }, { satDrive, 30 }, { delayMix, 10 }, { delayNote, 1 }, { reverbMix, 8 }, { reverbType, 3 } }),
        make ("Boom Bap Rap", "RAP", { { tuneOn, 0 }, { comp, 65 }, { body, 2 }, { mud, 3 }, { presence, 2.5f }, { air, 1 }, { satType, 0 }, { satDrive, 30 }, { delayMix, 6 }, { reverbType, 3 }, { reverbMix, 8 }, { reverbDecay, 0.9f } }),
        make ("Modern Rap Upfront", "RAP", { { retune, 35 }, { comp, 72 }, { mud, 4 }, { presence, 4 }, { air, 4 }, { satDrive, 25 }, { delayMix, 8 }, { reverbMix, 9 }, { reverbDecay, 1.2f } }),
        make ("R&B Silk", "R&B", { { retune, 22 }, { humanize, 30 }, { comp, 50 }, { body, 1.5f }, { air, 5 }, { deEss, 50 }, { satType, 1 }, { satDrive, 18 }, { delayMix, 14 }, { delayNote, 4 }, { reverbType, 1 }, { reverbMix, 22 }, { reverbDecay, 2.4f } }),
        make ("Pop Lead Polished", "POP", { { retune, 18 }, { humanize, 20 }, { comp, 58 }, { presence, 3 }, { air, 6 }, { deEss, 50 }, { satType, 2 }, { satDrive, 20 }, { delayMix, 12 }, { reverbType, 1 }, { reverbMix, 18 } }),
        make ("Ballad Natural", "POP", { { retune, 70 }, { humanize, 50 }, { flex, 40 }, { comp, 40 }, { body, 1 }, { air, 4 }, { delayMix, 10 }, { delayNote, 5 }, { reverbType, 2 }, { reverbMix, 26 }, { reverbDecay, 3.2f } }),
        make ("Reggaeton Lead", "LATIN", { { retune, 12 }, { comp, 62 }, { presence, 3.5f }, { air, 5 }, { satDrive, 22 }, { delayMix, 16 }, { delayNote, 1 }, { reverbType, 1 }, { reverbMix, 14 } }),
        make ("Afro Smooth", "LATIN", { { retune, 25 }, { humanize, 25 }, { comp, 50 }, { air, 5 }, { doubler, 15 }, { delayMix, 15 }, { delayNote, 4 }, { reverbType, 4 }, { reverbMix, 18 } }),
        make ("Rock Grit", "ROCK", { { tuneOn, 0 }, { comp, 70 }, { body, 1 }, { presence, 4 }, { satType, 3 }, { satDrive, 45 }, { satMix, 80 }, { delayMix, 8 }, { delayMode, 2 }, { reverbType, 3 }, { reverbMix, 12 } }),
        make ("Lo-Fi Bedroom", "CREATIVE", { { retune, 40 }, { comp, 50 }, { air, -2 }, { satType, 6 }, { satDrive, 45 }, { phone, 20 }, { delayMode, 6 }, { delayMix, 15 }, { reverbType, 10 }, { reverbMix, 18 } }),
        make ("Phone Intro", "CREATIVE", { { phone, 85 }, { comp, 55 }, { reverbMix, 6 }, { delayMix, 8 } }),
        make ("Ad-Lib Wide", "CREATIVE", { { retune, 10 }, { comp, 60 }, { doubler, 55 }, { width, 140 }, { air, 5 }, { delayMix, 20 }, { delayMode, 3 }, { reverbMix, 20 } }),
        make ("Ethereal Shimmer", "CREATIVE", { { retune, 15 }, { comp, 50 }, { air, 7 }, { delayMode, 5 }, { delayMix, 20 }, { delayNote, 5 }, { reverbType, 7 }, { reverbMix, 32 }, { reverbDecay, 5 } }),
        make ("Dry & Clean", "GENERAL", { { comp, 40 }, { delayOn, 0 }, { reverbOn, 0 }, { satMix, 0 } }),
    };
}

} // namespace vocal
