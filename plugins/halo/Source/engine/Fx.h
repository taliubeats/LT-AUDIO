#pragma once

#include "Analysis.h"

namespace halo
{

/** A sung word with its time (from the lyrics transcription). */
struct Word
{
    std::string text;
    double start = 0, end = 0;   // seconds
    bool reinforce = false;      // user can click words in the UI to add/remove refuerzo
};

struct Region
{
    double start = 0, end = 0;   // seconds
    std::string text;
};

namespace fx
{
    /** Low cut + presence + saturation + compression: makes a voice hit harder (refuerzo, shout). amount 0..1. */
    void force (std::vector<float>&, int sampleRate, double amount);

    /** Keeps the signal only inside the regions (with short fades). */
    void gate (std::vector<float>&, int sampleRate, const std::vector<Region>&, double fadeIn = 0.015, double fadeOut = 0.08);

    /** Last note of every phrase moved one beat later (ad-lib answer). */
    std::vector<float> phraseEndEcho (const std::vector<float>&, const Analysis&, double beatSeconds);

    /** Phrases from words: a pause > gapSec or a capitalised first word starts a new phrase. */
    std::vector<std::vector<Word>> splitPhrases (const std::vector<Word>&, double gapSec = 0.35);

    /**
        Refuerzo regions: the last `nWords` of each phrase (or the words the user marked). The end is extended to where
        the singer really stops (sustained vowel). Without words it falls back to the last ~0.8 s of each phrase.
    */
    std::vector<Region> refuerzoRegions (const std::vector<Word>&, const Analysis&, int nWords);
}

/** Tempo guess from a file name ("beat 92 bpm.wav" / "verse_140.wav"); 0 if none. */
double bpmFromName (const std::string&);

} // namespace halo
