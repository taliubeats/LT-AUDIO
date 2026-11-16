#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Analysis.h"

namespace typebeats
{

struct MidiExport
{
    /** Block chords (bass note + close voicing) on a tempo-mapped track starting at the first beat. */
    static juce::MidiFile createChordMidi (const AnalysisResult&);

    /** Writes the MIDI file. If `file` is a directory a descriptive name is chosen. */
    static juce::File write (const AnalysisResult&, const juce::String& songName, const juce::File& fileOrFolder);

    static juce::String progressionText (const AnalysisResult&, int maxChords = 4);
};

} // namespace typebeats
