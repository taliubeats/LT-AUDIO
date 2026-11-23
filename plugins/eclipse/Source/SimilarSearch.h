#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Classifier.h"

namespace eclipse
{

/**
    FIND SIMILAR: an index of the one-shots of a folder (your sample library), and a search for the
    sounds closest to a pad. The index keeps, per file, the same features the drum network uses (the
    shape of the sound in 36 bands over 300 ms) and what the network thinks it is.
    Saved in ~/Library/Application Support/LT Audio/LT Eclipse/Library Index.
*/
class SimilarSearch : private juce::Thread
{
public:
    SimilarSearch();
    ~SimilarSearch() override;

    struct Entry { juce::File file; juce::int64 modified = 0; classifier::Features f {}; classifier::Probs p {}; int label = -1; };
    struct Match { juce::File file; float score = 0; Family family = Family::sample; };

    void setFolder (const juce::File&);          // starts (re)scanning in the background
    juce::File folder() const;
    bool isScanning() const { return scanning.load(); }
    float scanProgress() const { return progress.load(); }
    int size() const;

    /** The closest sounds to this one. sameFamily = only the same kind of drum. */
    std::vector<Match> find (const juce::AudioBuffer<float>&, double sampleRate, Family family, bool sameFamily, int count = 30) const;

    static juce::AudioBuffer<float> loadOneShot (const juce::File&, double& sampleRate, double maxSeconds = 0.8);

private:
    void run() override;
    void save() const;
    void load();
    juce::File indexFile() const;

    mutable juce::CriticalSection lock;
    juce::File root;
    std::vector<Entry> entries;
    std::atomic<bool> scanning { false };
    std::atomic<float> progress { 0 };
};

/** The family a sample's folder or file name says (kick, snare, clap, hat...), -1 = no idea. */
int familyFromPath (const juce::String& pathLower);

} // namespace eclipse
