#include "FxTypes.h"

namespace quasar
{

juce::String FxParamSpec::format (float norm) const
{
    auto v = toReal (norm);
    switch (kind)
    {
        case choice: return choices[(int) v];
        case toggle: return v > 0.5f ? "ON" : "OFF";
        default: break;
    }
    juce::String u (unit);
    if (u == "Hz")
        return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (v, v < 10.0f ? 2 : 0) + " Hz";
    if (u == "ms")
        return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10.0f ? 1 : 0) + " ms";
    if (u == "s")  return juce::String (v, v < 10.0f ? 2 : 1) + " s";
    if (u == "dB") return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
    if (u == ":1") return juce::String (v, 1) + ":1";
    if (u == "%")  return juce::String (juce::roundToInt (v)) + "%";
    if (u == "bit") return juce::String (v, 1) + " bit";
    return juce::String (v, 2) + (u.isNotEmpty() ? " " + u : juce::String());
}

namespace
{
    using S = FxParamSpec;
    S lin (const char* n, float lo, float hi, float def, const char* u = "%") { S s; s.name = n; s.min = lo; s.max = hi; s.def = (def - lo) / (hi - lo); s.unit = u; return s; }
    S lg (const char* n, float lo, float hi, float def, const char* u)
    {
        S s; s.name = n; s.min = lo; s.max = hi; s.kind = S::log; s.unit = u;
        s.def = std::log (def / lo) / std::log (hi / lo);
        return s;
    }
    S ch (const char* n, juce::StringArray c, int def) { S s; s.name = n; s.kind = S::choice; s.choices = c; s.def = ((float) def + 0.5f) / (float) c.size(); return s; }
    S tg (const char* n, bool def) { S s; s.name = n; s.kind = S::toggle; s.def = def ? 1.0f : 0.0f; return s; }

    std::vector<FxInfo> build()
    {
        std::vector<FxInfo> v;
        auto add = [&] (const char* name, std::initializer_list<S> ps)
        {
            FxInfo i { name, (int) ps.size(), {} };
            int k = 0;
            for (auto& p : ps) i.params[(size_t) k++] = p;
            v.push_back (i);
        };
        add ("EMPTY", {});
        add ("SATURATION", { ch ("MODE", { "TAPE", "TUBE", "CONSOLE", "TRANSFORMER", "TRANSISTOR", "DIODE", "VINYL", "EXCITER" }, 1),
                             lin ("DRIVE", 0, 100, 35), lin ("TONE", -100, 100, 0), lin ("WARMTH", 0, 100, 30),
                             lin ("CHARACTER", 0, 100, 40), lin ("OUTPUT", -24, 12, 0, "dB") });
        add ("COMPRESSOR", { lin ("THRESHOLD", -60, 0, -18, "dB"), lg ("RATIO", 1, 20, 4, ":1"), lg ("ATTACK", 0.1f, 100, 10, "ms"),
                             lg ("RELEASE", 10, 1000, 120, "ms"), lin ("KNEE", 0, 24, 6, "dB"), lin ("MAKEUP", 0, 24, 0, "dB"),
                             ch ("MODE", { "CLEAN", "GLUE", "PUNCH" }, 1) });
        add ("CHORUS", { lg ("RATE", 0.05f, 5, 0.6f, "Hz"), lin ("DEPTH", 0, 100, 45), lg ("DELAY", 2, 30, 8, "ms"),
                         lin ("FEEDBACK", 0, 90, 10), lin ("WIDTH", 0, 100, 100), ch ("VOICES", { "2", "3", "4" }, 0),
                         lg ("TONE", 1000, 20000, 9000, "Hz") });
        add ("FLANGER", { lg ("RATE", 0.02f, 5, 0.25f, "Hz"), lin ("DEPTH", 0, 100, 60), lg ("DELAY", 0.2f, 10, 1.5f, "ms"),
                          lin ("FEEDBACK", -95, 95, 50), lin ("WIDTH", 0, 100, 60) });
        add ("PHASER", { lg ("RATE", 0.02f, 5, 0.3f, "Hz"), lin ("DEPTH", 0, 100, 60), lg ("FREQ", 100, 8000, 900, "Hz"),
                         lin ("FEEDBACK", -95, 95, 40), ch ("STAGES", { "2", "4", "6", "8", "12" }, 2), lin ("WIDTH", 0, 100, 50) });
        add ("DELAY", { ch ("MODE", { "DIGITAL", "TAPE", "ANALOG", "PING-PONG", "REVERSE", "SHIMMER", "LO-FI", "DUB" }, 3),
                        tg ("SYNC", true),
                        ch ("TIME", { "1/64", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1" }, 7),
                        lg ("TIME MS", 5, 2000, 350, "ms"), lin ("FEEDBACK", 0, 100, 35), lg ("LO CUT", 20, 2000, 150, "Hz"),
                        lg ("HI CUT", 1000, 20000, 7000, "Hz"), lin ("WIDTH", 0, 150, 100) });
        add ("REVERB", { ch ("TYPE", { "PLATE", "HALL", "ROOM", "CHAMBER", "SPACE", "VINTAGE" }, 0), lin ("SIZE", 0, 100, 60),
                         lg ("PRE-DLY", 1, 250, 12, "ms"), lg ("DECAY", 0.2f, 20, 2.2f, "s"), lg ("DAMP", 1000, 20000, 8000, "Hz"),
                         lin ("WIDTH", 0, 100, 100), lg ("LO CUT", 20, 1000, 120, "Hz"), lin ("MOD", 0, 100, 35) });
        add ("EQUALIZER", { lg ("LOW FREQ", 20, 500, 100, "Hz"), lin ("LOW GAIN", -18, 18, 0, "dB"), lg ("MID FREQ", 150, 8000, 1000, "Hz"),
                            lin ("MID GAIN", -18, 18, 0, "dB"), lg ("MID Q", 0.2f, 8, 0.8f, ""), lg ("HIGH FREQ", 1000, 18000, 8000, "Hz"),
                            lin ("HIGH GAIN", -18, 18, 0, "dB"), lg ("LOW CUT", 20, 1000, 20, "Hz") });
        add ("FILTER", { ch ("TYPE", { "LP 12", "LP 24", "HP 12", "HP 24", "BP", "NOTCH", "PEAK" }, 1), lg ("CUTOFF", 20, 20000, 2000, "Hz"),
                         lin ("RESO", 0, 100, 20), lin ("DRIVE", 0, 100, 0), lin ("GAIN", -18, 18, 6, "dB") });
        add ("DISTORTION", { ch ("MODE", { "SOFT", "HARD", "FOLD", "FUZZ", "TUBE", "RECTIFY", "SINE FOLD" }, 0), lin ("DRIVE", 0, 100, 30),
                             lg ("TONE", 500, 20000, 12000, "Hz"), lin ("BIAS", 0, 100, 0), lin ("OUTPUT", -24, 12, 0, "dB") });
        add ("BITCRUSH", { lg ("RATE", 500, 44100, 16000, "Hz"), lin ("BITS", 1, 16, 10, "bit"), lin ("JITTER", 0, 100, 0),
                           lg ("TONE", 500, 20000, 20000, "Hz") });
        add ("TAPE", { lin ("WOW", 0, 100, 25), lin ("FLUTTER", 0, 100, 20), lin ("SATURATION", 0, 100, 35), lin ("HISS", 0, 100, 10),
                       lin ("AGE", 0, 100, 30) });
        add ("STEREO WIDTH", { lin ("WIDTH", 0, 200, 130), lg ("BASS MONO", 20, 500, 120, "Hz"), lin ("HAAS", 0, 30, 0, "ms") });
        add ("UTILITY", { lin ("GAIN", -36, 12, 0, "dB"), lin ("PAN", -100, 100, 0), tg ("MONO", false), tg ("INVERT L", false),
                          tg ("INVERT R", false) });
        jassert ((int) v.size() == (int) FxType::count);
        return v;
    }
}

const FxInfo& fxInfo (int type)
{
    static const auto all = build();
    return all[(size_t) juce::jlimit (0, (int) all.size() - 1, type)];
}

const juce::StringArray& fxNames()
{
    static const juce::StringArray n = []
    {
        juce::StringArray a;
        for (int i = 0; i < (int) FxType::count; ++i) a.add (fxInfo (i).name);
        return a;
    }();
    return n;
}

} // namespace quasar
