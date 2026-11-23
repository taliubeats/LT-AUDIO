#pragma once

#include "Render.h"
#include "Fx.h"

#include <cmath>

namespace halo
{

/**
    What a voice strip sings. The first entries are harmonies (an Interval); the rest are production layers used in
    reggaeton / trap / R&B (doubles, stacks, octaves, pitched voices, gang, echo, refuerzo, shout).
*/
enum class Part
{
    third, fifth, sixth, fourth, seventh, thirdDown, fifthDown, sixthDown, octaveUp, octaveDown, twoOctavesUp, unison,
    autoHarmony,
    doubleTake, tightDouble, whisperDouble, stack, octaveDownLayer, octaveUpAir, pitchedUp, pitchedDown, gang, echo,
    refuerzo, refuerzoHarmony, shout, shoutRefuerzo, gangShout,
    count
};

struct PartInfo
{
    const char* name;           // shown in the strip menu
    const char* description;    // tooltip / manual (Spanish)
    int interval;               // halo::Interval used for the notes, -1 = AUTO
    int takes = 1;              // independent takes summed (stack / gang)
    double spread = 0.0;        // pan spread of the takes
    double force = 0.0;         // fx::force amount
    bool refuerzo = false;      // only the ending words of each phrase
    bool echo = false;          // phrase endings one beat later
    bool isLayer = false;       // listed under "LAYERS" in the menu
    // defaults applied to the strip when the part is chosen
    Style style = Style::natural;
    VoiceType voice = VoiceType::same;
    double humanize = 0.6, delayMs = 12.0, pan = 0.0, levelDb = -6.0;
    std::vector<VoiceType> takeVoices {};   // per take (gang)
};

const PartInfo& partInfo (Part);

/** Everything needed to (re)generate one strip. */
struct StripSpec
{
    Part part = Part::third;
    bool smart = true;
    Style style = Style::natural;
    VoiceType voice = VoiceType::same;
    double humanize = 0.6;        // 0..1
    double delayMs = 12.0;
    unsigned long long seed = 1;
    int stripIndex = 0;           // for AUTO

    bool operator== (const StripSpec& o) const
    {
        return part == o.part && smart == o.smart && style == o.style && voice == o.voice
            && std::abs (humanize - o.humanize) < 1e-6 && std::abs (delayMs - o.delayMs) < 1e-6 && seed == o.seed
            && stripIndex == o.stripIndex;
    }
    bool operator!= (const StripSpec& o) const { return ! (*this == o); }
};

struct Stereo
{
    std::vector<float> l, r;
    void resize (size_t n) { l.assign (n, 0.0f); r.assign (n, 0.0f); }
    size_t size() const noexcept { return l.size(); }
    bool empty() const noexcept { return l.empty(); }
};

struct GlobalSpec
{
    double tightness = 0.0;       // 0..1: 1 = everything perfectly with the lead
    double bpm = 90.0;            // echo
    int refuerzoWords = 2;
};

/** The notes a strip sings (for the UI moons and the report). */
std::vector<HarmonyNote> stripLine (const Analysis&, const StripSpec&);

/** Renders one strip (FAST/WORLD). Takes are panned by the part's spread around the centre; the strip pan is applied
    at playback. Returns stereo at an.sampleRate. */
Stereo renderStrip (const Analysis&, const StripSpec&, const GlobalSpec&, const std::vector<Region>& refuerzo,
                    const std::function<bool()>& shouldStop = {});

/** Ready-made sets of strips. */
struct PresetStrip { Part part; double pan, levelDb, delayMs, humanize; Style style; VoiceType voice; bool smart = true; };
struct HarmonyPreset { const char* name; const char* group; std::vector<PresetStrip> strips; double tightness = 0.2; };
const std::vector<HarmonyPreset>& harmonyPresets();

// ------------------------------------------------------------------ choir

enum class ChoirType { duo, church, gospel, stadium, angels, count };
enum class ChoirHarmony { unison, octaves, triad, seventh, autoMode, count };
const char* choirTypeName (ChoirType);
const char* choirHarmonyName (ChoirHarmony);

struct ChoirSpec
{
    int voices = 32;
    ChoirType type = ChoirType::stadium;
    ChoirHarmony harmony = ChoirHarmony::triad;
    double sections[4] { 30, 25, 25, 20 };   // soprano, alto, tenor, bass (%)
    double spread = 0.5, loose = 0.5, width = 0.8, room = 0.5, breath = 0.2;   // 0..1
    unsigned long long seed = 1;
};

/** Defaults for each choir type (voices, harmony, knobs). */
ChoirSpec choirDefaults (ChoirType);

/** Voices per section for a total count and percentages (sums to `voices`). */
std::array<int, 4> sectionCounts (const ChoirSpec&);

/** One singer of the choir: every singer is an independent performance (not a detuned copy). */
struct ChoirSinger
{
    int section = 0;              // 0 S, 1 A, 2 T, 3 B
    Interval interval = Interval::unison;
    VoiceType voice = VoiceType::same;
    Style style = Style::natural;
    double humanize = 0.6, delayMs = 10, detuneCents = 0, pan = 0, gain = 1;
    unsigned long long seed = 1;
};

std::vector<ChoirSinger> planChoir (const Analysis&, const ChoirSpec&);

/** Renders one singer (mono, already gain-scaled). */
std::vector<float> renderSinger (const Analysis&, const ChoirSinger&);

} // namespace halo
