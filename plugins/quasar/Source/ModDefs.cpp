#include "ModDefs.h"
#include "FxTypes.h"

namespace quasar
{

const juce::StringArray& ModDefs::sourceNames()
{
    static const juce::StringArray n { "NONE", "ENV 1", "ENV 2", "ENV 3", "PITCH ENV", "LFO 1", "LFO 2", "LFO 3", "LFO 4",
                                       "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4", "MACRO 5", "MACRO 6", "MACRO 7", "MACRO 8",
                                       "VELOCITY", "NOTE", "MOD WHEEL", "AFTERTOUCH", "PITCH BEND", "RANDOM", "SLIDE" };
    return n;
}

juce::Colour ModDefs::sourceColour (int s)
{
    if (s >= srcEnv1 && s <= srcEnvP)     return juce::Colour (0xff5ee6ff);   // envelopes: cyan
    if (s >= srcLfo1 && s <= srcLfo4)     return juce::Colour (0xffb48cff);   // LFOs: violet
    if (s >= srcMacro1 && s <= srcMacro8) return juce::Colour (0xffffd35a);   // macros: gold
    return juce::Colour (0xffff7aa8);                                          // MIDI: pink
}

bool ModDefs::isBipolarByNature (int s) { return s == srcPitchBend; }

bool ModDefs::isVoiceSource (int s)
{
    return (s >= srcEnv1 && s <= srcEnvP) || (s >= srcLfo1 && s <= srcLfo4) || s == srcVelocity || s == srcNote
        || s == srcAftertouch || s == srcRandom || s == srcSlide || s == srcPitchBend;
}

const std::vector<ModDest>& ModDefs::destinations()
{
    static const std::vector<ModDest> d = []
    {
        std::vector<ModDest> v;
        v.push_back ({ "", "NONE", false });
        auto add = [&] (const juce::String& id, const juce::String& name, bool voice) { v.push_back ({ id, name, voice }); };
        const char* oscNames[] = { "OSC A", "OSC B", "OSC C" };
        for (int o = 0; o < numOscs; ++o)
        {
            juce::String n (oscNames[o]);
            add (IDs::osc (o, "semi"), n + " PITCH", true);
            add (IDs::osc (o, "fine"), n + " FINE", true);
            add (IDs::osc (o, "wtpos"), n + " WT POS", true);
            add (IDs::osc (o, "warp"), n + " WARP", true);
            add (IDs::osc (o, "detune"), n + " DETUNE", true);
            add (IDs::osc (o, "blend"), n + " BLEND", true);
            add (IDs::osc (o, "spread"), n + " SPREAD", true);
            add (IDs::osc (o, "pan"), n + " PAN", true);
            add (IDs::osc (o, "level"), n + " LEVEL", true);
        }
        add (IDs::subLevel, "SUB LEVEL", true);
        add (IDs::subPan, "SUB PAN", true);
        add (IDs::noiseColor, "NOISE COLOR", true);
        add (IDs::noiseLevel, "NOISE LEVEL", true);
        add (IDs::noisePan, "NOISE PAN", true);
        for (int f = 0; f < numFilters; ++f)
        {
            auto n = "FILTER " + juce::String (f + 1);
            add (IDs::flt (f, "cutoff"), n + " CUTOFF", true);
            add (IDs::flt (f, "reso"), n + " RES", true);
            add (IDs::flt (f, "drive"), n + " DRIVE", true);
            add (IDs::flt (f, "mix"), n + " MIX", true);
            add (IDs::flt (f, "pan"), n + " PAN", true);
            add (IDs::flt (f, "level"), n + " LEVEL", true);
        }
        const char* envNames[] = { "ENV 1", "ENV 2", "ENV 3", "PITCH ENV" };
        for (int e = 0; e < numEnvs; ++e)
        {
            juce::String n (envNames[e]);
            add (IDs::env (e, "att"), n + " ATTACK", true);
            add (IDs::env (e, "hold"), n + " HOLD", true);
            add (IDs::env (e, "dec"), n + " DECAY", true);
            add (IDs::env (e, "sus"), n + " SUSTAIN", true);
            add (IDs::env (e, "rel"), n + " RELEASE", true);
        }
        add (IDs::envPAmount, "PITCH ENV AMOUNT", true);
        for (int l = 0; l < numLfos; ++l)
        {
            auto n = "LFO " + juce::String (l + 1);
            add (IDs::lfo (l, "rate"), n + " RATE", true);
            add (IDs::lfo (l, "rise"), n + " RISE", true);
            add (IDs::lfo (l, "delay"), n + " DELAY", true);
            add (IDs::lfo (l, "smooth"), n + " SMOOTH", true);
        }
        add (IDs::glide, "GLIDE", true);
        add (IDs::drift, "DRIFT", true);
        add (IDs::warmth, "WARMTH", true);
        for (int b = 0; b < numBuses; ++b)
        {
            auto n = Choices::busNames()[b];
            add (IDs::bus (b, "level"), n + " LEVEL", false);
            add (IDs::bus (b, "pan"), n + " PAN", false);
            for (int s = 0; s < numFxSlots; ++s)
            {
                auto slot = n + " FX" + juce::String (s + 1);
                add (IDs::fx (b, s, "mix"), slot + " MIX", false);
                for (int p = 0; p < numFxParams; ++p)
                    add (IDs::fxParam (b, s, p), slot + " P" + juce::String (p + 1), false);
            }
        }
        add (IDs::master, "MASTER", false);
        add (IDs::arpGate, "ARP GATE", false);
        return v;
    }();
    return d;
}

int ModDefs::destIndex (const juce::String& id)
{
    static const auto map = []
    {
        std::map<juce::String, int> m;
        auto& d = destinations();
        for (int i = 1; i < (int) d.size(); ++i) m[d[(size_t) i].id] = i;
        return m;
    }();
    auto it = map.find (id);
    return it == map.end() ? 0 : it->second;
}

juce::StringArray ModDefs::destNames()
{
    juce::StringArray a;
    for (auto& d : destinations()) a.add (d.name);
    return a;
}

} // namespace quasar
