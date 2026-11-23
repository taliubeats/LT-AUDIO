#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Kit.h"

namespace eclipse
{

/** What VARY changes. */
enum class VaryTarget { all = 0, kick, snare, hats };
enum class RollRate { r32 = 0, r32t, r64 };
enum class RollShape { up = 0, down, vee, flat };

struct VaryOptions
{
    VaryTarget target = VaryTarget::all;
    float amount = 0.5f;              // 0 = subtle, 1 = bold
    bool rolls = true;
    std::array<bool, 3> rates { true, true, false };
    RollShape shape = RollShape::up;
};

/**
    Pattern variations: hat rolls, ghost notes, flams, fills, syncopation and doubles.
    The pad families tell which notes are kicks, snares and hats.
*/
Pattern vary (const Pattern& base, const std::array<Family, numPads>& families, const VaryOptions&, juce::Random&);

/** The name the history shows, e.g. "V3 Hats". */
juce::String varyName (VaryTarget, int number);

/** A MIDI file of the pattern (notes on the pads' notes), at 960 PPQ, with the tempo. */
juce::MidiFile toMidiFile (const Pattern&, double bpm);

/** Sorts notes and removes duplicates at the same tick on the same pad. */
void tidy (Pattern&);

} // namespace eclipse
