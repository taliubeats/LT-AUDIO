#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include "Skins.h"

namespace eclipse
{

constexpr int numPads = 64, padsPerBank = 16, numBanks = 4;
constexpr int firstNote = 36;                         // C1 = pad A1
inline int noteForPad (int pad) { return firstNote + pad; }
juce::String noteName (int midiNote);                 // "C1", "F#1"

using AudioPtr = std::shared_ptr<const juce::AudioBuffer<float>>;

/** One detected drum hit in the source. */
struct Hit
{
    juce::int64 pos = 0;          // onset, in source samples
    Family family = Family::perc;
    float strength = 0;           // 0..1 relative to the loudest hit of its detector
    int group = -1;               // index into Analysis::groups
};

/** One sound: all the hits that are the same drum. */
struct Group
{
    Family family = Family::perc;
    std::vector<int> hits;        // indices into Analysis::hits, best first
    int length = 0;               // one-shot length in samples
    int source = -1;              // v3: the part it is cut from (0 kick, 1 snare, 2 cymbals), -1 = the drum stem
};

/** A drum pattern: notes on a 1/64 grid. */
struct Note
{
    int pad = 0;
    int tick = 0;                 // 1/64 note steps from the pattern start (16 per beat)
    int length = 4;               // in ticks
    float velocity = 0.8f;        // 0..1
};

struct Pattern
{
    int bars = 1;
    std::vector<Note> notes;
    juce::String name;            // "Original", "V1 Hats"...
    int kit = 0;                  // which kit of the song (bank A, B...)
    int ticks() const { return bars * 64; }
};

/** Everything the clone engine found in a source. */
struct Analysis
{
    double sampleRate = 44100;
    juce::int64 length = 0;
    std::vector<Hit> hits;
    std::vector<Group> groups;
    std::vector<juce::Range<juce::int64>> drumSections;   // the parts with the most drums (highlighted)
    double bpm = 120;
    juce::int64 beatOffset = 0;                           // sample of the first downbeat
    std::vector<Pattern> patterns;                        // in pad numbers (after assignment)
    std::vector<std::pair<juce::Range<juce::int64>, int>> kitSections;   // where each kit plays (only when there are 2+)
    int numKits = 1;
    bool neural = false;                                  // v2: analysed on the AI (neural) drum stem
    bool parts = false;                                   // v3: and on its kick / snare / cymbals parts
    int count (Family f) const;
};

/** What a pad holds. Audio is immutable and shared; swapping is done by the Kit with a lock. */
struct PadSound
{
    juce::String name;            // custom names survive type changes
    Family type = Family::sample;
    juce::String tag;             // custom tag (New tag...)
    AudioPtr raw, clean;          // one-shot as found, and the cleaned version
    double sampleRate = 44100;
    int group = -1;               // into the analysis, -1 = user sample
    int hitIndex = 0;             // which hit of the group (HIT 1 of N)
    bool empty() const { return raw == nullptr; }
};

/** The 64 pads + the source they were cloned from. Thread-safe for the audio thread via snapshot(). */
class Kit
{
public:
    struct Snapshot { std::array<AudioPtr, numPads> raw, clean; std::array<double, numPads> rate {}; };

    PadSound pad (int i) const;
    void setPad (int i, PadSound);
    void clearAll();
    std::shared_ptr<const Snapshot> snapshot() const;        // audio thread: lock-free read of the current audio

    // source
    juce::String sourceName;
    juce::File sourceFile;
    std::shared_ptr<const juce::AudioBuffer<float>> sourceAudio;   // mono, analysis rate
    std::shared_ptr<const Analysis> analysis;
    bool usedAI = false;

    // patterns (edited by the user; the analysis keeps the originals)
    std::vector<Pattern> patterns;
    int currentPattern = 0;

    juce::ValueTree toValueTree() const;                      // includes the pad audio (FLAC)
    void fromValueTree (const juce::ValueTree&);

    std::function<void()> onChange;                           // message thread
    void changed() { if (onChange) onChange(); }

private:
    mutable juce::CriticalSection lock;
    std::array<PadSound, numPads> pads;
    std::shared_ptr<const Snapshot> snap = std::make_shared<Snapshot>();
    void rebuildSnapshot();
};

/** Writes / reads audio as FLAC in memory. */
juce::MemoryBlock encodeFlac (const juce::AudioBuffer<float>&, double sampleRate);
AudioPtr decodeAudio (const juce::MemoryBlock&, double& sampleRate);
bool writeWav (const juce::File&, const juce::AudioBuffer<float>&, double sampleRate);

} // namespace eclipse
