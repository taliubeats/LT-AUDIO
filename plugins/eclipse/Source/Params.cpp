#include "Params.h"

namespace eclipse
{

const char* padParamName (PadParam p)
{
    static const char* n[] = { "gain", "pan", "tune", "attack", "fade", "hp", "lp", "clean", "vibe", "choke", "out", "mute", "solo", "start", "end" };
    return n[(int) p];
}

juce::String padParamId (int pad, PadParam p) { return "p" + juce::String (pad) + "_" + padParamName (p); }

namespace
{
juce::String padLabel (int pad)
{
    return juce::String::charToString ((juce::juce_wchar) ('A' + pad / padsPerBank)) + juce::String (pad % padsPerBank + 1);
}

juce::NormalisableRange<float> freqRange (float lo, float hi)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (std::sqrt (lo * hi));
    return r;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-36.0f, 6.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::sensitivity, 1 }, "Sensitivity", NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    auto ms = [] (float v, int) { return String (v, v < 10 ? 1 : 0) + " ms"; };
    auto pct = [] (float v, int) { return String (roundToInt (v * 100)) + "%"; };
    auto hz = [] (float v, int) { return v >= 1000 ? String (v / 1000, 1) + " kHz" : String (roundToInt (v)) + " Hz"; };

    for (int i = 0; i < numPads; ++i)
    {
        auto grp = std::make_unique<AudioProcessorParameterGroup> ("pad" + String (i), "Pad " + padLabel (i), "|");
        auto nm = [&] (const char* s) { return "Pad " + padLabel (i) + " " + s; };
        auto id = [&] (PadParam p) { return ParameterID { padParamId (i, p), 1 }; };
        grp->addChild (std::make_unique<AudioParameterFloat> (id (PadParam::gain), nm ("Gain"), NormalisableRange<float> (-36.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (std::abs (v) < 0.05f ? 0.0f : v, 1) + " dB"; })));
        grp->addChild (std::make_unique<AudioParameterFloat> (id (PadParam::pan), nm ("Pan"), NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return std::abs (v) < 0.005f ? String ("C") : String (roundToInt (std::abs (v) * 100)) + (v < 0 ? "L" : "R"); })));
        grp->addChild (std::make_unique<AudioParameterFloat> (id (PadParam::tune), nm ("Tune"), NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return std::abs (v - std::round (v)) < 0.01f ? String ((int) std::round (v)) + " st" : String (v, 1) + " st"; })));
        { NormalisableRange<float> r (0.0f, 200.0f, 0.1f); r.setSkewForCentre (20.0f);
          grp->addChild (std::make_unique<AudioParameterFloat> (id (PadParam::attack), nm ("Attack"), r, 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (ms))); }
        grp->addChild (std::make_unique<AudioParameterFloat> (id (PadParam::fade), nm ("Fade"), NormalisableRange<float> (0.0f, 1.0f), 0.25f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        grp->addChild (std::make_unique<AudioParameterFloat> (id (PadParam::hp), nm ("HP"), freqRange (20.0f, 2000.0f), 20.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([hz] (float v, int n) { return v <= 20.5f ? String ("OFF") : hz (v, n); })));
        grp->addChild (std::make_unique<AudioParameterFloat> (id (PadParam::lp), nm ("LP"), freqRange (200.0f, 20000.0f), 20000.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([hz] (float v, int n) { return v >= 19900 ? String ("OFF") : hz (v, n); })));
        grp->addChild (std::make_unique<AudioParameterFloat> (id (PadParam::clean), nm ("Clean"), NormalisableRange<float> (0.0f, 1.0f), 0.7f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        grp->addChild (std::make_unique<AudioParameterFloat> (id (PadParam::vibe), nm ("Vibe"), NormalisableRange<float> (0.0f, 10.0f, 0.01f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));
        grp->addChild (std::make_unique<AudioParameterChoice> (id (PadParam::choke), nm ("Choke"), StringArray { "Off", "1", "2", "3", "4" }, 0));
        grp->addChild (std::make_unique<AudioParameterChoice> (id (PadParam::out), nm ("Output"), StringArray { "Main", "Main + Family", "Family only" }, 1));
        grp->addChild (std::make_unique<AudioParameterBool> (id (PadParam::mute), nm ("Mute"), false));
        grp->addChild (std::make_unique<AudioParameterBool> (id (PadParam::solo), nm ("Solo"), false));
        grp->addChild (std::make_unique<AudioParameterFloat> (id (PadParam::start), nm ("Start"), NormalisableRange<float> (0.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        grp->addChild (std::make_unique<AudioParameterFloat> (id (PadParam::end), nm ("End"), NormalisableRange<float> (0.0f, 1.0f), 1.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        layout.add (std::move (grp));
    }
    return layout;
}

} // namespace eclipse
