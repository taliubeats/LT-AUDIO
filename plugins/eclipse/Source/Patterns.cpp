#include "Patterns.h"

namespace eclipse
{

juce::String varyName (VaryTarget t, int number)
{
    static const char* n[] = { "All", "Kick", "Snare", "Hats" };
    return "V" + juce::String (number) + " " + n[(int) t];
}

void tidy (Pattern& p)
{
    std::sort (p.notes.begin(), p.notes.end(), [] (const Note& a, const Note& b) { return a.tick != b.tick ? a.tick < b.tick : a.pad < b.pad; });
    p.notes.erase (std::unique (p.notes.begin(), p.notes.end(), [] (const Note& a, const Note& b) { return a.tick == b.tick && a.pad == b.pad; }), p.notes.end());
    const int len = p.ticks();
    p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [len] (const Note& n) { return n.tick < 0 || n.tick >= len; }), p.notes.end());
}

namespace
{
bool isHat (Family f)   { return f == Family::hat; }
bool isKick (Family f)  { return f == Family::kick; }
bool isSnare (Family f) { return f == Family::snare || f == Family::clap; }

float rollVelocity (RollShape s, float base, int i, int n)
{
    const float t = n > 1 ? (float) i / (float) (n - 1) : 1.0f;
    switch (s)
    {
        case RollShape::up:   return base * (0.35f + 0.65f * t);
        case RollShape::down: return base * (1.0f - 0.65f * t);
        case RollShape::vee:  return base * (0.4f + 0.6f * std::abs (2 * t - 1));
        case RollShape::flat: return base * 0.8f;
    }
    return base;
}

int firstPadOf (const std::array<Family, numPads>& fam, bool (*pred) (Family), const Pattern& p)
{
    for (auto& n : p.notes) if (pred (fam[(size_t) n.pad])) return n.pad;   // the pad the pattern already uses
    for (int i = 0; i < numPads; ++i) if (pred (fam[(size_t) i])) return i;
    return -1;
}
}

Pattern vary (const Pattern& base, const std::array<Family, numPads>& fam, const VaryOptions& o, juce::Random& rnd)
{
    Pattern p = base;
    const float a = juce::jlimit (0.0f, 1.0f, o.amount);
    const bool doKick = o.target == VaryTarget::all || o.target == VaryTarget::kick;
    const bool doSnare = o.target == VaryTarget::all || o.target == VaryTarget::snare;
    const bool doHats = o.target == VaryTarget::all || o.target == VaryTarget::hats;
    auto chance = [&] (float x) { return rnd.nextFloat() < x; };
    std::vector<Note> add;
    const int len = p.ticks();

    // ---------------- hats: rolls, open/close the groove, shuffle 16ths
    if (doHats)
    {
        const int hat = firstPadOf (fam, isHat, p);
        std::vector<int> rates;
        for (int r = 0; r < 3; ++r) if (o.rates[(size_t) r]) rates.push_back (r);
        if (hat >= 0)
        {
            std::vector<Note> keep;
            for (auto& n : p.notes)
            {
                if (! isHat (fam[(size_t) n.pad])) { keep.push_back (n); continue; }
                if (o.rolls && ! rates.empty() && n.tick % 8 == 0 && chance (0.12f + 0.45f * a))
                {   // a roll over the next 1/8 (or 1/4 when bold)
                    const auto rate = (RollRate) rates[(size_t) rnd.nextInt ((int) rates.size())];
                    const int span = chance (a * 0.5f) ? 16 : 8;
                    const int count = rate == RollRate::r64 ? span : rate == RollRate::r32 ? span / 2 : span * 3 / 8;   // 1/32T = 3 per 1/16
                    for (int i = 0; i < count; ++i)
                    {
                        Note r = n;
                        r.tick = n.tick + (int) std::round ((double) i * span / count);
                        r.length = juce::jmax (1, span / count);
                        r.velocity = juce::jlimit (0.15f, 1.0f, rollVelocity (o.shape, n.velocity, i, count));
                        add.push_back (r);
                    }
                    continue;
                }
                if (chance (0.08f * a)) continue;                     // leave a gap
                keep.push_back (n);
            }
            p.notes = keep;
            // extra 16th hats between the 8ths
            for (int t = 4; t < len; t += 8)
                if (chance (0.25f * a)) add.push_back ({ hat, t, 2, juce::jlimit (0.2f, 1.0f, 0.35f + 0.3f * rnd.nextFloat()) });
        }
    }

    // ---------------- kick: syncopation, doubles, drops
    if (doKick)
    {
        const int kick = firstPadOf (fam, isKick, p);
        for (auto& n : p.notes)
        {
            if (! isKick (fam[(size_t) n.pad]) || n.tick % 64 == 0) continue;   // the downbeat stays
            if (chance (0.18f * a)) n.tick += (chance (0.5f) ? 4 : -4);          // push or pull a 16th
            else if (chance (0.12f * a)) n.velocity = 0;                         // drop it
        }
        if (kick >= 0)
            for (int t = 0; t < len; t += 16)
                if (chance (0.22f * a)) add.push_back ({ kick, t + 12 + (chance (0.5f) ? 0 : -4), 4, juce::jlimit (0.3f, 1.0f, 0.55f + 0.3f * rnd.nextFloat()) });   // doubles before the beat
    }

    // ---------------- snare: ghosts, flams, fill
    if (doSnare)
    {
        const int snare = firstPadOf (fam, isSnare, p);
        for (auto& n : p.notes)
        {
            if (! isSnare (fam[(size_t) n.pad]) || n.velocity <= 0) continue;
            if (chance (0.35f * a)) add.push_back ({ n.pad, n.tick - 4, 4, juce::jlimit (0.12f, 1.0f, n.velocity * 0.32f) });   // ghost before
            if (chance (0.2f * a))  add.push_back ({ n.pad, n.tick - 1, 1, juce::jlimit (0.12f, 1.0f, n.velocity * 0.5f) });    // flam
        }
        if (snare >= 0 && a > 0.45f && chance (0.35f + 0.5f * (a - 0.45f)))
        {   // fill on the last beat: 16ths, louder and louder
            const int from = len - 16;
            p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [&] (const Note& n) { return n.tick >= from && isSnare (fam[(size_t) n.pad]); }), p.notes.end());
            const int step = a > 0.8f ? 2 : 4;
            for (int t = from, i = 0; t < len; t += step, ++i)
                add.push_back ({ snare, t, step, juce::jlimit (0.3f, 1.0f, 0.45f + 0.55f * (float) (t - from) / 16.0f) });
        }
    }

    p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [] (const Note& n) { return n.velocity <= 0; }), p.notes.end());
    for (auto& n : add) p.notes.push_back (n);
    tidy (p);
    return p;
}

juce::MidiFile toMidiFile (const Pattern& p, double bpm)
{
    constexpr int ppq = 960, perTick = ppq / 16;
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / juce::jmax (20.0, bpm))), 0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    for (auto& n : p.notes)
    {
        const int note = noteForPad (n.pad);
        const auto vel = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (n.velocity * 127));
        seq.addEvent (juce::MidiMessage::noteOn (10, note, vel), n.tick * perTick);
        seq.addEvent (juce::MidiMessage::noteOff (10, note), (n.tick + juce::jmax (1, n.length)) * perTick - 1);
    }
    seq.addEvent (juce::MidiMessage::endOfTrack(), p.ticks() * perTick);
    seq.updateMatchedPairs();
    juce::MidiFile f;
    f.setTicksPerQuarterNote (ppq);
    f.addTrack (seq);
    return f;
}

} // namespace eclipse
