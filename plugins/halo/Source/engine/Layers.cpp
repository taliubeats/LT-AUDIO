#include "Layers.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace halo
{

namespace
{
    using S = Style;
    using V = VoiceType;

    std::vector<PartInfo> makeParts()
    {
        std::vector<PartInfo> t ((size_t) Part::count);
        auto harm = [&] (Part p, Interval iv, const char* desc, double pan)
        {
            PartInfo i { intervalName (iv), desc, (int) iv };
            i.pan = pan;
            i.delayMs = 12;
            t[(size_t) p] = i;
        };
        harm (Part::third,        Interval::third,        "3a arriba (sigue el acorde)", -0.4);
        harm (Part::fifth,        Interval::fifth,        "5a arriba (sigue el acorde)", 0.4);
        harm (Part::sixth,        Interval::sixth,        "6a arriba", -0.25);
        harm (Part::fourth,       Interval::fourth,       "4a arriba", 0.25);
        harm (Part::seventh,      Interval::seventh,      "7a arriba (color R&B)", 0.3);
        harm (Part::thirdDown,    Interval::thirdDown,    "3a abajo", 0.25);
        harm (Part::fifthDown,    Interval::fifthDown,    "5a abajo", -0.25);
        harm (Part::sixthDown,    Interval::sixthDown,    "6a abajo", 0.0);
        harm (Part::octaveUp,     Interval::octaveUp,     "octava arriba", 0.0);
        harm (Part::octaveDown,   Interval::octaveDown,   "octava abajo", 0.0);
        harm (Part::twoOctavesUp, Interval::twoOctavesUp, "dos octavas arriba", 0.0);
        harm (Part::unison,       Interval::unison,       "unisono (doble)", 0.0);
        t[(size_t) Part::autoHarmony] = { "AUTO", "la IA elige la mejor armonia para la melodia", -1 };

        auto layer = [&] (Part p, const char* name, const char* desc, Interval iv, PartInfo extra)
        {
            extra.name = name; extra.description = desc; extra.interval = (int) iv; extra.isLayer = true;
            t[(size_t) p] = extra;
        };
        PartInfo e;
        e = {}; e.humanize = 0.85; e.delayMs = 20; e.pan = -0.8; e.levelDb = -4;
        layer (Part::doubleTake, "Double", "la misma voz cantada otra vez (otra toma)", Interval::unison, e);
        e = {}; e.humanize = 0.35; e.delayMs = 7; e.pan = -0.6; e.levelDb = -5;
        layer (Part::tightDouble, "Tight Double", "doble pegado a la voz (trap)", Interval::unison, e);
        e = {}; e.style = S::whisper; e.humanize = 0.6; e.delayMs = 10; e.levelDb = -9;
        layer (Part::whisperDouble, "Whisper Double", "susurro debajo de la voz", Interval::unison, e);
        e = {}; e.takes = 4; e.spread = 0.9; e.humanize = 0.9; e.delayMs = 25; e.levelDb = -6;
        layer (Part::stack, "Stack", "4 dobles abiertos (hook ancho)", Interval::unison, e);
        e = {}; e.style = S::chest; e.voice = V::lower; e.humanize = 0.6; e.delayMs = 10; e.levelDb = -7;
        layer (Part::octaveDownLayer, "Octave Down", "octava abajo grave (cuerpo)", Interval::octaveDown, e);
        e = {}; e.style = S::breathy; e.humanize = 0.6; e.delayMs = 14; e.levelDb = -11;
        layer (Part::octaveUpAir, "Octave Up Air", "octava arriba con aire (brillo)", Interval::octaveUp, e);
        e = {}; e.voice = V::chipmunk; e.humanize = 0.4; e.delayMs = 3; e.pan = 0.3; e.levelDb = -9;
        layer (Part::pitchedUp, "Pitched Up", "voz aguda pitcheada (ad-lib)", Interval::octaveUp, e);
        e = {}; e.voice = V::demon; e.humanize = 0.4; e.delayMs = 3; e.pan = -0.3; e.levelDb = -8;
        layer (Part::pitchedDown, "Pitched Down", "voz grave 'demon' (ad-lib)", Interval::octaveDown, e);
        e = {}; e.style = S::chest; e.takes = 6; e.spread = 1.0; e.humanize = 1.0; e.delayMs = 35; e.levelDb = -6;
        e.takeVoices = { V::same, V::lower, V::higher, V::same, V::lower, V::same };
        layer (Part::gang, "Gang", "6 voces cantando juntas (gang vocals)", Interval::unison, e);
        e = {}; e.echo = true; e.humanize = 0.7; e.delayMs = 0; e.pan = 0.5; e.levelDb = -8;
        layer (Part::echo, "Echo", "repite el final de cada frase un tiempo despues", Interval::unison, e);
        e = {}; e.style = S::chest; e.takes = 3; e.spread = 0.5; e.force = 1.0; e.refuerzo = true; e.humanize = 0.35;
        e.delayMs = 7; e.levelDb = -2;
        layer (Part::refuerzo, "Refuerzo", "remarca los finales de frase con fuerza", Interval::unison, e);
        e = {}; e.style = S::chest; e.takes = 2; e.spread = 0.7; e.force = 0.8; e.refuerzo = true; e.humanize = 0.35;
        e.delayMs = 7; e.levelDb = -5;
        layer (Part::refuerzoHarmony, "Refuerzo Harmony", "los mismos finales, en 3a", Interval::third, e);
        e = {}; e.style = S::shout; e.takes = 2; e.spread = 0.6; e.force = 1.0; e.humanize = 0.5; e.delayMs = 12; e.levelDb = -6;
        layer (Part::shout, "Shout", "voz gritada", Interval::unison, e);
        e = {}; e.style = S::shout; e.takes = 3; e.spread = 0.7; e.force = 1.0; e.refuerzo = true; e.humanize = 0.45;
        e.delayMs = 9; e.levelDb = -4;
        layer (Part::shoutRefuerzo, "Shout Refuerzo", "finales de frase gritados", Interval::unison, e);
        e = {}; e.style = S::shout; e.takes = 8; e.spread = 1.0; e.force = 0.7; e.humanize = 1.0; e.delayMs = 35; e.levelDb = -6;
        e.takeVoices = { V::same, V::lower, V::higher, V::same, V::lower, V::same, V::higher, V::lower };
        layer (Part::gangShout, "Gang Shout", "8 voces gritando (estadio)", Interval::unison, e);
        return t;
    }

    std::mt19937_64 rngFor (unsigned long long seed) { return std::mt19937_64 (seed * 0x9E3779B97F4A7C15ull + 12345); }
    double uni (std::mt19937_64& g, double a, double b) { return std::uniform_real_distribution<double> (a, b) (g); }
}

const PartInfo& partInfo (Part p)
{
    static const auto table = makeParts();
    return table[(size_t) p];
}

std::vector<HarmonyNote> stripLine (const Analysis& an, const StripSpec& s)
{
    auto& info = partInfo (s.part);
    auto iv = info.interval < 0 ? autoInterval (an.notes, s.stripIndex) : (Interval) info.interval;
    return harmonize (an.notes, an.key, iv, s.smart);
}

Stereo renderStrip (const Analysis& an, const StripSpec& s, const GlobalSpec& g, const std::vector<Region>& refuerzo,
                    const std::function<bool()>& shouldStop)
{
    auto& info = partInfo (s.part);
    auto line = stripLine (an, s);
    const size_t n = an.audio.size();
    Stereo out;
    out.resize (n);
    if (line.empty()) return out;

    double meanShift = 0;
    for (auto& h : line) meanShift += h.midi - h.lead->midi;
    meanShift /= (double) line.size();

    const double tight = std::clamp (g.tightness, 0.0, 1.0);
    auto rng = rngFor (s.seed);
    const int takes = std::max (1, info.takes);
    const double norm = 1.0 / std::sqrt ((double) takes);

    for (int k = 0; k < takes; ++k)
    {
        if (shouldStop && shouldStop()) return {};
        PerformOptions po;
        po.humanize = s.humanize * (1.0 - 0.7 * tight);
        double extra = takes > 1 ? uni (rng, 0.0, 25.0) : uni (rng, 0.0, 6.0);
        po.delayMs = (s.delayMs + extra) * (1.0 - 0.8 * tight);
        po.style = s.style;
        po.seed = s.seed * 1000 + (unsigned long long) k * 37 + 1;
        auto voice = info.takeVoices.empty() ? s.voice : info.takeVoices[(size_t) k % info.takeVoices.size()];
        auto perf = perform (an, line, po);
        auto y = renderWorld (an, perf, meanShift, s.style, voice, po.seed);

        if (info.force > 0) fx::force (y, an.sampleRate, info.force);
        if (info.refuerzo) fx::gate (y, an.sampleRate, refuerzo);
        if (info.echo) y = fx::phraseEndEcho (y, an, 60.0 / std::max (40.0, g.bpm));

        double pan = takes > 1 ? -info.spread + 2.0 * info.spread * k / (takes - 1) : 0.0;
        double a = (pan + 1) * 3.14159265358979 / 4;
        float gl = (float) (std::cos (a) * std::sqrt (2.0) * norm), gr = (float) (std::sin (a) * std::sqrt (2.0) * norm);
        for (size_t i = 0; i < n && i < y.size(); ++i)
        {
            out.l[i] += y[i] * gl;
            out.r[i] += y[i] * gr;
        }
    }
    return out;
}

const std::vector<HarmonyPreset>& harmonyPresets()
{
    using P = Part;
    static const std::vector<HarmonyPreset> list = {
        { "Pop Thirds", "POP", {
            { P::third, -0.4, -6, 12, 0.6, S::natural, V::same }, { P::thirdDown, 0.4, -7, 16, 0.6, S::natural, V::same },
            { P::doubleTake, -0.8, -6, 20, 0.85, S::natural, V::same }, { P::doubleTake, 0.8, -6, 26, 0.85, S::natural, V::same } } },
        { "Reggaeton", "REGGAETON", {
            { P::doubleTake, -0.8, -4, 18, 0.85, S::natural, V::same }, { P::doubleTake, 0.8, -4, 26, 0.85, S::natural, V::same },
            { P::third, -0.85, -7, 14, 0.7, S::natural, V::same }, { P::third, 0.85, -7, 22, 0.7, S::natural, V::same },
            { P::octaveDownLayer, 0.0, -7, 10, 0.6, S::chest, V::lower }, { P::refuerzo, 0.0, -2, 7, 0.35, S::chest, V::same },
            { P::echo, 0.5, -8, 0, 0.7, S::natural, V::same } } },
        { "Reggaeton Full", "REGGAETON", {
            { P::doubleTake, -0.8, -4, 18, 0.85, S::natural, V::same }, { P::doubleTake, 0.8, -4, 26, 0.85, S::natural, V::same },
            { P::third, -0.85, -7, 14, 0.7, S::natural, V::same }, { P::third, 0.85, -7, 22, 0.7, S::natural, V::same },
            { P::thirdDown, -0.7, -8, 14, 0.7, S::natural, V::same }, { P::thirdDown, 0.7, -8, 22, 0.7, S::natural, V::same },
            { P::refuerzo, 0.0, -2, 7, 0.35, S::chest, V::same }, { P::refuerzoHarmony, 0.0, -5, 7, 0.35, S::chest, V::same } } },
        { "Trap", "TRAP", {
            { P::tightDouble, -0.6, -5, 7, 0.35, S::natural, V::same }, { P::tightDouble, 0.6, -5, 9, 0.35, S::natural, V::same },
            { P::octaveDownLayer, 0.0, -7, 10, 0.6, S::chest, V::lower }, { P::pitchedUp, 0.3, -9, 3, 0.4, S::natural, V::chipmunk },
            { P::pitchedDown, -0.3, -8, 3, 0.4, S::natural, V::demon }, { P::shoutRefuerzo, 0.0, -4, 9, 0.45, S::shout, V::same },
            { P::gang, 0.0, -6, 35, 1.0, S::chest, V::same }, { P::echo, 0.5, -8, 0, 0.7, S::natural, V::same } } },
        { "R&B Sevenths", "R&B", {
            { P::third, -0.85, -7, 14, 0.7, S::natural, V::same }, { P::third, 0.85, -7, 22, 0.7, S::natural, V::same },
            { P::fifth, -0.6, -9, 14, 0.7, S::natural, V::same }, { P::fifth, 0.6, -9, 22, 0.7, S::natural, V::same },
            { P::seventh, 0.3, -10, 16, 0.6, S::breathy, V::same }, { P::thirdDown, 0.25, -8, 16, 0.6, S::natural, V::same },
            { P::octaveUpAir, 0.0, -11, 14, 0.6, S::breathy, V::same } } },
        { "Hook Stack", "HOOK", {
            { P::stack, 0.0, -6, 25, 0.9, S::natural, V::same }, { P::third, -0.85, -7, 14, 0.7, S::natural, V::same },
            { P::third, 0.85, -7, 22, 0.7, S::natural, V::same }, { P::octaveDownLayer, 0.0, -7, 10, 0.6, S::chest, V::lower },
            { P::octaveUpAir, 0.0, -11, 14, 0.6, S::breathy, V::same }, { P::refuerzo, 0.0, -2, 7, 0.35, S::chest, V::same },
            { P::gangShout, 0.0, -6, 35, 1.0, S::shout, V::same } } },
        { "Gospel Stack", "GOSPEL", {
            { P::third, -0.5, -6, 12, 0.6, S::chest, V::same }, { P::fifth, 0.5, -7, 14, 0.6, S::chest, V::same },
            { P::thirdDown, -0.3, -7, 16, 0.6, S::chest, V::lower }, { P::octaveDown, 0.3, -8, 12, 0.6, S::chest, V::lower },
            { P::seventh, 0.0, -10, 16, 0.6, S::natural, V::higher } } },
        { "Octave Doubles", "POP", {
            { P::octaveUp, -0.3, -9, 12, 0.6, S::falsetto, V::same }, { P::octaveDown, 0.3, -7, 12, 0.6, S::chest, V::lower },
            { P::doubleTake, -0.8, -6, 20, 0.85, S::natural, V::same }, { P::doubleTake, 0.8, -6, 26, 0.85, S::natural, V::same } } },
        { "Falsetto Air", "R&B", {
            { P::octaveUp, 0.0, -9, 12, 0.6, S::falsetto, V::same }, { P::third, -0.5, -9, 14, 0.6, S::falsetto, V::higher },
            { P::fifth, 0.5, -10, 18, 0.6, S::falsetto, V::higher }, { P::whisperDouble, 0.0, -10, 8, 0.6, S::whisper, V::same } } },
        { "Wide Doubles", "POP", {
            { P::doubleTake, -1.0, -5, 18, 0.9, S::natural, V::same }, { P::doubleTake, 1.0, -5, 28, 0.9, S::natural, V::same },
            { P::tightDouble, -0.5, -8, 6, 0.35, S::natural, V::same }, { P::tightDouble, 0.5, -8, 9, 0.35, S::natural, V::same },
            { P::whisperDouble, 0.0, -10, 8, 0.6, S::whisper, V::same } } },
        { "Shout Hook", "TRAP", {
            { P::shout, -0.5, -6, 12, 0.5, S::shout, V::same }, { P::shout, 0.5, -6, 18, 0.5, S::shout, V::same },
            { P::gangShout, 0.0, -6, 35, 1.0, S::shout, V::same }, { P::shoutRefuerzo, 0.0, -4, 9, 0.45, S::shout, V::same },
            { P::octaveDownLayer, 0.0, -7, 10, 0.6, S::chest, V::lower } } },
    };
    return list;
}

// ------------------------------------------------------------------ choir

const char* choirTypeName (ChoirType t)
{
    static const char* n[] = { "DUO", "CHURCH", "GOSPEL", "STADIUM", "ANGELS" };
    return n[(int) t];
}

const char* choirHarmonyName (ChoirHarmony h)
{
    static const char* n[] = { "Unison", "Octaves", "Triad", "Seventh", "Auto" };
    return n[(int) h];
}

ChoirSpec choirDefaults (ChoirType t)
{
    ChoirSpec c;
    c.type = t;
    auto set = [&] (int v, ChoirHarmony h, std::array<double, 4> sec, double sp, double lo, double wi, double ro, double br)
    {
        c.voices = v; c.harmony = h;
        for (int i = 0; i < 4; ++i) c.sections[i] = sec[(size_t) i];
        c.spread = sp; c.loose = lo; c.width = wi; c.room = ro; c.breath = br;
    };
    switch (t)
    {
        case ChoirType::duo:     set (2,  ChoirHarmony::triad,   { 50, 50, 0, 0 },   0.15, 0.30, 0.6, 0.20, 0.10); break;
        case ChoirType::church:  set (12, ChoirHarmony::triad,   { 30, 25, 25, 20 }, 0.20, 0.20, 0.7, 0.85, 0.20); break;
        case ChoirType::gospel:  set (18, ChoirHarmony::seventh, { 30, 30, 25, 15 }, 0.35, 0.35, 0.8, 0.50, 0.10); break;
        case ChoirType::stadium: set (32, ChoirHarmony::octaves, { 30, 25, 25, 20 }, 0.70, 0.80, 1.0, 0.60, 0.10); break;
        case ChoirType::angels:  set (16, ChoirHarmony::octaves, { 50, 30, 20, 0 },  0.25, 0.30, 0.9, 0.90, 0.70); break;
        default: break;
    }
    return c;
}

std::array<int, 4> sectionCounts (const ChoirSpec& c)
{
    std::array<int, 4> out {};
    double total = 0;
    for (auto s : c.sections) total += std::max (0.0, s);
    if (total <= 0) { out[1] = c.voices; return out; }
    double rem[4];
    int used = 0;
    for (int i = 0; i < 4; ++i)
    {
        double exact = c.voices * std::max (0.0, c.sections[i]) / total;
        out[(size_t) i] = (int) std::floor (exact);
        rem[i] = exact - out[(size_t) i];
        used += out[(size_t) i];
    }
    while (used < c.voices)
    {
        int best = (int) (std::max_element (rem, rem + 4) - rem);
        out[(size_t) best]++; rem[best] = -1; ++used;
    }
    return out;
}

std::vector<ChoirSinger> planChoir (const Analysis& an, const ChoirSpec& c)
{
    std::vector<ChoirSinger> out;
    auto counts = sectionCounts (c);
    auto rng = rngFor (c.seed);

    auto sectionInterval = [&] (int sec)
    {
        auto h = c.harmony == ChoirHarmony::autoMode ? ChoirHarmony::triad : c.harmony;
        switch (h)
        {
            case ChoirHarmony::unison:  return sec == 3 ? Interval::octaveDown : Interval::unison;
            case ChoirHarmony::octaves: return sec == 0 ? Interval::octaveUp : sec == 3 ? Interval::octaveDown : Interval::unison;
            case ChoirHarmony::triad:   return sec == 0 ? Interval::fifth : sec == 1 ? Interval::third : sec == 2 ? Interval::unison : Interval::octaveDown;
            case ChoirHarmony::seventh: return sec == 0 ? Interval::seventh : sec == 1 ? Interval::third : sec == 2 ? Interval::fifthDown : Interval::octaveDown;
            default: return Interval::unison;
        }
    };
    const VoiceType sectionVoice[] = { VoiceType::higher, VoiceType::same, VoiceType::same, VoiceType::lower };

    int total = std::max (1, c.voices);
    for (int sec = 0; sec < 4; ++sec)
        for (int i = 0; i < counts[(size_t) sec]; ++i)
        {
            ChoirSinger s;
            s.section = sec;
            s.interval = sectionInterval (sec);
            s.voice = sectionVoice[sec];
            if (sec == 1 && uni (rng, 0, 1) < 0.4) s.voice = VoiceType::higher;
            bool breathy = uni (rng, 0, 1) < c.breath;
            s.style = c.type == ChoirType::angels ? (breathy ? Style::breathy : Style::falsetto)
                    : c.type == ChoirType::stadium ? (uni (rng, 0, 1) < 0.35 ? Style::shout : Style::chest)
                    : breathy ? Style::breathy : Style::natural;
            s.humanize = 0.5 + 0.5 * c.loose;
            s.delayMs = uni (rng, 0, 15 + 60 * c.loose);
            s.detuneCents = uni (rng, -1, 1) * (4 + 26 * c.spread);
            s.pan = uni (rng, -1, 1) * c.width;
            s.gain = 1.0 / std::sqrt ((double) total) * 1.6;
            s.seed = c.seed * 7919 + (unsigned long long) out.size() * 104729 + 3;
            out.push_back (s);
        }
    (void) an;
    return out;
}

std::vector<float> renderSinger (const Analysis& an, const ChoirSinger& s)
{
    auto line = harmonize (an.notes, an.key, s.interval, true);
    if (line.empty()) return std::vector<float> (an.audio.size(), 0.0f);
    double meanShift = 0;
    for (auto& h : line) meanShift += h.midi - h.lead->midi;
    meanShift /= (double) line.size();
    PerformOptions po;
    po.humanize = s.humanize;
    po.delayMs = s.delayMs;
    po.detuneCents = s.detuneCents;
    po.style = s.style;
    po.seed = s.seed;
    auto y = renderWorld (an, perform (an, line, po), meanShift, s.style, s.voice, s.seed);
    if (s.style == Style::shout) fx::force (y, an.sampleRate, 0.6);
    for (auto& v : y) v *= (float) s.gain;
    return y;
}

} // namespace halo
