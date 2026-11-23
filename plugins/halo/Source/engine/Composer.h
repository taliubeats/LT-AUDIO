#pragma once

#include "Analysis.h"

namespace halo
{

/**
    Harmony composer: one MIDI note per lead note.
    SMART follows the implied chord (n-th chord tone above/below), uses diatonic motion on short passing notes,
    follows ornaments (melisma) in parallel and never lands a second against the lead. FIXED = exact semitones.
*/
enum class Interval
{
    third, fifth, sixth, fourth, seventh, thirdDown, fifthDown, sixthDown,
    octaveUp, octaveDown, twoOctavesUp, unison, count
};

const char* intervalName (Interval);           // "+3rd"
int fixedSemitones (Interval);                 // exact size used by FIXED

struct HarmonyNote
{
    const Note* lead = nullptr;
    int midi = 60;
};

std::vector<HarmonyNote> harmonize (const std::vector<Note>&, const Key&, Interval, bool smart);
std::vector<HarmonyNote> harmonizeSemitones (const std::vector<Note>&, int semitones);

/** Diatonic shift of a MIDI note by scale steps (keeps chromatic offset). */
int diatonicShift (const Key&, int midi, int steps);

/** AUTO: interval that sits best for this melody's range. */
Interval autoInterval (const std::vector<Note>&, int index);

} // namespace halo
