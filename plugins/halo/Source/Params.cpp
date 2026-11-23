#include "Params.h"

namespace halo
{

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using API = juce::AudioParameterInt;

juce::String IDs::strip (int i, const char* name) { return "s" + juce::String (i) + "_" + name; }

juce::StringArray partNames()
{
    juce::StringArray a;
    for (int i = 0; i < (int) Part::count; ++i) a.add (partInfo ((Part) i).name);
    return a;
}

juce::StringArray styleNames()
{
    juce::StringArray a;
    for (int i = 0; i < (int) Style::count; ++i) a.add (styleName ((Style) i));
    return a;
}

juce::StringArray voiceNames()
{
    juce::StringArray a;
    for (int i = 0; i < (int) VoiceType::count; ++i) a.add (voiceTypeName ((VoiceType) i));
    return a;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                      juce::AudioParameterFloatAttributes().withLabel ("%"));
    };

    l.add (pct (IDs::dry, "Dry", 100.0f));
    l.add (pct (IDs::wet, "Wet", 100.0f));
    l.add (pct (IDs::width, "Width", 100.0f));
    l.add (pct (IDs::tightness, "Tightness", 20.0f));
    l.add (std::make_unique<APC> (juce::ParameterID { IDs::quality, 1 }, "Quality", juce::StringArray { "FAST", "BEST" }, 0));
    l.add (std::make_unique<APB> (juce::ParameterID { IDs::withLead, 1 }, "With Lead", true));
    l.add (std::make_unique<APB> (juce::ParameterID { IDs::keyAuto, 1 }, "Key Auto", true));
    l.add (std::make_unique<APC> (juce::ParameterID { IDs::key, 1 }, "Key",
                                  juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9));
    l.add (std::make_unique<APC> (juce::ParameterID { IDs::scale, 1 }, "Scale", juce::StringArray { "Major", "Minor" }, 1));
    l.add (std::make_unique<API> (juce::ParameterID { IDs::refuerzoWords, 1 }, "Refuerzo Words", 1, 3, 2));
    l.add (std::make_unique<APB> (juce::ParameterID { IDs::bypass, 1 }, "Bypass", false));

    // strips
    auto parts = partNames(), styles = styleNames(), voices = voiceNames();
    for (int i = 0; i < numStrips; ++i)
    {
        auto n = "Voice " + juce::String (i + 1) + " ";
        l.add (std::make_unique<APB> (juce::ParameterID { IDs::strip (i, IDs::on), 1 }, n + "On", i < 4));
        static const int defaults[] = { (int) Part::third, (int) Part::fifth, (int) Part::thirdDown, (int) Part::octaveUp,
                                        (int) Part::doubleTake, (int) Part::doubleTake, (int) Part::octaveDownLayer, (int) Part::refuerzo };
        l.add (std::make_unique<APC> (juce::ParameterID { IDs::strip (i, IDs::part), 1 }, n + "Part", parts, defaults[i]));
        l.add (std::make_unique<APB> (juce::ParameterID { IDs::strip (i, IDs::smart), 1 }, n + "Smart", true));
        l.add (std::make_unique<APC> (juce::ParameterID { IDs::strip (i, IDs::style), 1 }, n + "Style", styles, 0));
        l.add (std::make_unique<APC> (juce::ParameterID { IDs::strip (i, IDs::voice), 1 }, n + "Voice", voices, 0));
        l.add (std::make_unique<APF> (juce::ParameterID { IDs::strip (i, IDs::level), 1 }, n + "Level",
                                      juce::NormalisableRange<float> (-36.0f, 6.0f, 0.1f, 2.0f), -6.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("dB")));
        static const float pans[] = { -40, 40, 25, -25, -80, 80, 0, 0 };
        l.add (std::make_unique<APF> (juce::ParameterID { IDs::strip (i, IDs::pan), 1 }, n + "Pan",
                                      juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), pans[i]));
        l.add (std::make_unique<APF> (juce::ParameterID { IDs::strip (i, IDs::delay), 1 }, n + "Delay",
                                      juce::NormalisableRange<float> (0.0f, 60.0f, 0.1f), 12.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("ms")));
        l.add (pct (IDs::strip (i, IDs::humanize).toRawUTF8(), (n + "Humanize").toRawUTF8(), 60.0f));
        l.add (std::make_unique<APB> (juce::ParameterID { IDs::strip (i, IDs::mute), 1 }, n + "Mute", false));
        l.add (std::make_unique<APB> (juce::ParameterID { IDs::strip (i, IDs::solo), 1 }, n + "Solo", false));
    }

    // choir
    juce::StringArray types, harmonies;
    for (int i = 0; i < (int) ChoirType::count; ++i) types.add (choirTypeName ((ChoirType) i));
    for (int i = 0; i < (int) ChoirHarmony::count; ++i) harmonies.add (choirHarmonyName ((ChoirHarmony) i));
    l.add (std::make_unique<API> (juce::ParameterID { IDs::cVoices, 1 }, "Choir Voices", 2, 64, 32));
    l.add (std::make_unique<APC> (juce::ParameterID { IDs::cType, 1 }, "Choir Type", types, (int) ChoirType::stadium));
    l.add (std::make_unique<APC> (juce::ParameterID { IDs::cHarmony, 1 }, "Choir Harmony", harmonies, (int) ChoirHarmony::octaves));
    l.add (pct (IDs::cSop, "Sopranos", 30.0f));
    l.add (pct (IDs::cAlto, "Altos", 25.0f));
    l.add (pct (IDs::cTenor, "Tenors", 25.0f));
    l.add (pct (IDs::cBass, "Basses", 20.0f));
    l.add (pct (IDs::cSpread, "Spread", 70.0f));
    l.add (pct (IDs::cLoose, "Loose", 80.0f));
    l.add (pct (IDs::cWidth, "Choir Width", 100.0f));
    l.add (pct (IDs::cRoom, "Room", 60.0f));
    l.add (pct (IDs::cBreath, "Breath", 10.0f));
    l.add (std::make_unique<APF> (juce::ParameterID { IDs::cLevel, 1 }, "Choir Level",
                                  juce::NormalisableRange<float> (-36.0f, 6.0f, 0.1f, 2.0f), -3.0f,
                                  juce::AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<APB> (juce::ParameterID { IDs::cSeparate, 1 }, "Separate Sections", false));
    return l;
}

void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float v)
{
    if (auto* p = s.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
}

float getParam (juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    auto* raw = s.getRawParameterValue (id);
    return raw != nullptr ? raw->load() : 0.0f;
}

void applyPartDefaults (juce::AudioProcessorValueTreeState& s, int i, Part p)
{
    auto& info = partInfo (p);
    setParam (s, IDs::strip (i, IDs::style), (float) info.style);
    setParam (s, IDs::strip (i, IDs::voice), (float) info.voice);
    setParam (s, IDs::strip (i, IDs::humanize), (float) info.humanize * 100.0f);
    setParam (s, IDs::strip (i, IDs::delay), (float) info.delayMs);
    if (info.isLayer)
    {
        setParam (s, IDs::strip (i, IDs::level), (float) info.levelDb);
        // doubles alternate sides so two strips of the same layer open up L / R
        auto pan = (float) info.pan * 100.0f;
        if (std::abs (pan) > 1.0f && (i % 2) == 1) pan = -pan;
        setParam (s, IDs::strip (i, IDs::pan), pan);
    }
}

void applyChoirDefaults (juce::AudioProcessorValueTreeState& s, ChoirType t)
{
    auto c = choirDefaults (t);
    setParam (s, IDs::cVoices, (float) c.voices);
    setParam (s, IDs::cHarmony, (float) c.harmony);
    setParam (s, IDs::cSop, (float) c.sections[0]);
    setParam (s, IDs::cAlto, (float) c.sections[1]);
    setParam (s, IDs::cTenor, (float) c.sections[2]);
    setParam (s, IDs::cBass, (float) c.sections[3]);
    setParam (s, IDs::cSpread, (float) c.spread * 100.0f);
    setParam (s, IDs::cLoose, (float) c.loose * 100.0f);
    setParam (s, IDs::cWidth, (float) c.width * 100.0f);
    setParam (s, IDs::cRoom, (float) c.room * 100.0f);
    setParam (s, IDs::cBreath, (float) c.breath * 100.0f);
}

std::vector<lt::presets::ParamPreset> factoryPresets()
{
    std::vector<lt::presets::ParamPreset> out;
    for (auto& hp : harmonyPresets())
    {
        lt::presets::ParamPreset p;
        p.name = hp.name;
        p.group = hp.group;
        p.values[IDs::tightness] = (float) hp.tightness * 100.0f;
        for (int i = 0; i < numStrips; ++i)
        {
            bool used = i < (int) hp.strips.size();
            p.values[IDs::strip (i, IDs::on)] = used ? 1.0f : 0.0f;
            p.values[IDs::strip (i, IDs::mute)] = 0.0f;
            p.values[IDs::strip (i, IDs::solo)] = 0.0f;
            if (! used) continue;
            auto& st = hp.strips[(size_t) i];
            p.values[IDs::strip (i, IDs::part)] = (float) st.part;
            p.values[IDs::strip (i, IDs::smart)] = st.smart ? 1.0f : 0.0f;
            p.values[IDs::strip (i, IDs::style)] = (float) st.style;
            p.values[IDs::strip (i, IDs::voice)] = (float) st.voice;
            p.values[IDs::strip (i, IDs::level)] = (float) st.levelDb;
            p.values[IDs::strip (i, IDs::pan)] = (float) st.pan * 100.0f;
            p.values[IDs::strip (i, IDs::delay)] = (float) st.delayMs;
            p.values[IDs::strip (i, IDs::humanize)] = (float) st.humanize * 100.0f;
        }
        out.push_back (p);
    }
    const char* choirNames[] = { "Duo", "Church Choir", "Gospel Choir", "Stadium Anthem", "Angels" };
    for (int t = 0; t < (int) ChoirType::count; ++t)
    {
        auto c = choirDefaults ((ChoirType) t);
        lt::presets::ParamPreset p;
        p.name = choirNames[t];
        p.group = "CHOIR";
        p.values[IDs::cType] = (float) t;
        p.values[IDs::cVoices] = (float) c.voices;
        p.values[IDs::cHarmony] = (float) c.harmony;
        p.values[IDs::cSop] = (float) c.sections[0];
        p.values[IDs::cAlto] = (float) c.sections[1];
        p.values[IDs::cTenor] = (float) c.sections[2];
        p.values[IDs::cBass] = (float) c.sections[3];
        p.values[IDs::cSpread] = (float) c.spread * 100.0f;
        p.values[IDs::cLoose] = (float) c.loose * 100.0f;
        p.values[IDs::cWidth] = (float) c.width * 100.0f;
        p.values[IDs::cRoom] = (float) c.room * 100.0f;
        p.values[IDs::cBreath] = (float) c.breath * 100.0f;
        out.push_back (p);
    }
    return out;
}

} // namespace halo
