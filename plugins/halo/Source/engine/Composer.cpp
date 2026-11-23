#include "Composer.h"

#include <algorithm>
#include <cmath>

namespace halo
{

namespace
{
    enum class Kind { chordUp, chordDown, diatonic, seventh, semis };
    struct PartDef { const char* name; Kind kind; int value; int fallback; int fixed; };

    const PartDef parts[] = {
        { "+3rd",   Kind::chordUp,   1,   2,   4 },
        { "+5th",   Kind::chordUp,   2,   4,   7 },
        { "+6th",   Kind::diatonic,  5,   5,   9 },
        { "+4th",   Kind::diatonic,  3,   3,   5 },
        { "+7th",   Kind::seventh,   0,   6,  10 },
        { "-3rd",   Kind::chordDown, 1,  -2,  -3 },
        { "-5th",   Kind::chordDown, 2,  -4,  -7 },
        { "-6th",   Kind::chordDown, 2,  -5,  -8 },
        { "+8va",   Kind::semis,    12,   7,  12 },
        { "-8va",   Kind::semis,   -12,  -7, -12 },
        { "+15ma",  Kind::semis,    24,  14,  24 },
        { "Unison", Kind::semis,     0,   0,   0 },
    };

    constexpr double passingMs = 140.0;
    constexpr double ornamentMs = 200.0;

    int pcOf (int m) { return ((m % 12) + 12) % 12; }

    void scaleIndex (const Key& key, int midi, int& idx, int& rem)
    {
        int rel = midi - key.tonic;
        int octave = (int) std::floor (rel / 12.0);
        int pc = rel - octave * 12;
        int deg = 0;
        for (int i = 0; i < 7; ++i) if (key.steps()[i] <= pc) deg = i;
        idx = octave * 7 + deg;
        rem = pc - key.steps()[deg];
    }

    int fromScaleIndex (const Key& key, int idx)
    {
        int octave = (int) std::floor (idx / 7.0);
        int deg = idx - octave * 7;
        return key.tonic + octave * 12 + key.steps()[deg];
    }

    int chordToneAbove (int midi, const Note& n, int nth, int minGap = 3)
    {
        int found = 0;
        for (int m = midi + minGap; m < midi + 25; ++m)
            if (n.chordHas (pcOf (m)) && ++found == nth) return m;
        return -1;
    }

    int chordToneBelow (int midi, const Note& n, int nth, int minGap = 3)
    {
        int found = 0;
        for (int m = midi - minGap; m > midi - 25; --m)
            if (n.chordHas (pcOf (m)) && ++found == nth) return m;
        return -1;
    }

    void followOrnaments (std::vector<HarmonyNote>& line, const Key& key)
    {
        for (size_t i = 0; i < line.size(); ++i)
        {
            auto& n = *line[i].lead;
            if (key.contains (n.midi) || n.ms() > ornamentMs) continue;
            const HarmonyNote* best = nullptr;
            int bestGap = 1 << 30;
            for (int d : { -1, 1 })
            {
                auto j = (long) i + d;
                if (j < 0 || j >= (long) line.size()) continue;
                auto& nb = line[(size_t) j];
                if (! key.contains (nb.lead->midi)) continue;
                int gap = d > 0 ? std::abs (nb.lead->start - n.end) : std::abs (n.start - nb.lead->end);
                if (gap < bestGap) { bestGap = gap; best = &nb; }
            }
            if (best != nullptr) line[i].midi = n.midi + (best->midi - best->lead->midi);
        }
    }

    void smoothLine (std::vector<HarmonyNote>& line, const Key& key)
    {
        auto consonant = [] (int iv) { iv = pcOf (iv); return iv == 0 || iv == 3 || iv == 4 || iv == 5 || iv == 7 || iv == 8 || iv == 9; };
        for (size_t i = 1; i + 1 < line.size(); ++i)
        {
            auto& a = line[i - 1]; auto& b = line[i]; auto& c = line[i + 1];
            if (b.lead->ms() > 200) continue;
            if (a.midi == c.midi && std::abs (b.midi - a.midi) >= 3
                && consonant (a.midi - b.lead->midi) && key.contains (a.midi))
                b.midi = a.midi;
        }
    }
}

const char* intervalName (Interval i) { return parts[(int) i].name; }
int fixedSemitones (Interval i)       { return parts[(int) i].fixed; }

int diatonicShift (const Key& key, int midi, int steps)
{
    int idx, rem;
    scaleIndex (key, midi, idx, rem);
    return fromScaleIndex (key, idx + steps) + rem;
}

std::vector<HarmonyNote> harmonizeSemitones (const std::vector<Note>& notes, int semis)
{
    std::vector<HarmonyNote> out;
    for (auto& n : notes) out.push_back ({ &n, n.midi + semis });
    return out;
}

std::vector<HarmonyNote> harmonize (const std::vector<Note>& notes, const Key& key, Interval iv, bool smart)
{
    const auto& def = parts[(int) iv];
    if (! smart) return harmonizeSemitones (notes, def.fixed);

    std::vector<HarmonyNote> out;
    for (auto& n : notes)
    {
        Note withChord = n;
        if (n.chord[0] < 0)
        {
            auto pcs = key.pitchClasses();
            withChord.chord = { pcs[0], pcs[2], pcs[4] };
        }
        bool chordTone = withChord.chordHas (pcOf (n.midi));
        bool shortNote = n.ms() < passingMs;
        int m = -1;
        if (def.kind == Kind::semis) m = n.midi + def.value;
        else if (def.kind == Kind::diatonic || def.kind == Kind::seventh || (! chordTone && shortNote))
            m = diatonicShift (key, n.midi, def.fallback);
        else if (def.kind == Kind::chordUp)   m = chordToneAbove (n.midi, withChord, def.value);
        else if (def.kind == Kind::chordDown) m = chordToneBelow (n.midi, withChord, def.value);
        if (m < 0) m = diatonicShift (key, n.midi, def.fallback);
        int iv12 = pcOf (m - n.midi);
        if (def.kind != Kind::semis && (iv12 == 1 || iv12 == 2 || iv12 == 11))
            m = diatonicShift (key, n.midi, def.fallback);
        out.push_back ({ &n, m });
    }
    followOrnaments (out, key);
    smoothLine (out, key);
    return out;
}

Interval autoInterval (const std::vector<Note>& notes, int index)
{
    if (notes.empty()) return Interval::third;
    std::vector<int> mids;
    for (auto& n : notes) mids.push_back (n.midi);
    std::sort (mids.begin(), mids.end());
    int top = mids[(size_t) ((double) mids.size() * 0.9)];
    static const Interval high[] = { Interval::thirdDown, Interval::sixthDown, Interval::octaveDown, Interval::third };
    static const Interval low[] = { Interval::third, Interval::fifth, Interval::thirdDown, Interval::octaveUp };
    return (top >= 72 ? high : low)[index % 4];
}

} // namespace halo
