#include "MidiExport.h"

namespace typebeats
{

juce::MidiFile MidiExport::createChordMidi (const AnalysisResult& r)
{
    constexpr int ppq = 960;
    auto bpm = r.bpm > 0 ? r.bpm : 120.0;

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, "LT TYPE BEATS chords"), 0);
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);

    auto toTicks = [&] (double seconds)
    {
        auto beats = (seconds - r.firstBeat) * bpm / 60.0;
        return juce::jmax (0.0, std::round (beats * 4.0) / 4.0) * ppq;   // quantise to 1/16
    };

    for (auto& seg : r.chords)
    {
        if (! seg.chord.isValid()) continue;

        auto start = toTicks (seg.start), end = toTicks (seg.end);
        if (end <= start) continue;

        std::vector<int> notes;
        notes.push_back (36 + seg.chord.root);                 // bass, octave 2
        for (auto pc : seg.chord.pitchClasses())                // close voicing around middle C
        {
            auto n = 60 + pc;
            if (n > 67) n -= 12;
            notes.push_back (n);
        }

        for (auto n : notes)
        {
            seq.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) (n < 48 ? 100 : 88)), start);
            seq.addEvent (juce::MidiMessage::noteOff (1, n), end - 1);
        }
    }

    seq.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote (ppq);
    file.addTrack (seq);
    return file;
}

juce::String MidiExport::progressionText (const AnalysisResult& r, int maxChords)
{
    juce::StringArray names;
    for (auto& seg : r.chords)
    {
        auto n = seg.chord.name (r.prefersFlats());
        if (names.isEmpty() || names[names.size() - 1] != n)
            names.add (n);
        if (names.size() >= maxChords) break;
    }
    return names.joinIntoString (" ");
}

juce::File MidiExport::write (const AnalysisResult& r, const juce::String& songName, const juce::File& target)
{
    auto file = target;
    if (target.isDirectory() || ! target.hasFileExtension ("mid"))
    {
        auto key = r.keys.empty() ? juce::String() : " " + r.keys.front().name (true);
        auto name = juce::File::createLegalFileName (songName + " - " + juce::String ((int) std::lround (r.bpm)) + "bpm" + key
                                                     + " - " + progressionText (r));
        file = (target.isDirectory() ? target : target.getParentDirectory()).getChildFile (name + ".mid");
    }

    auto midi = createChordMidi (r);
    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk() || ! midi.writeTo (out))
        return {};
    return file;
}

} // namespace typebeats
