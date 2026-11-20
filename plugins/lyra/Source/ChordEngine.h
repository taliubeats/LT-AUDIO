#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace lyra
{

/** Letters 0..6 = C D E F G A B. */
struct Spelling
{
    int letter = 0, accidental = 0;   // accidental: -2 .. +2
    bool operator== (const Spelling& o) const { return letter == o.letter && accidental == o.accidental; }
};

int naturalPc (int letter);
int pcOf (Spelling);

enum class Language { english, spanish, japanese, german };
enum class SpellMode { key, sharps, flats };

/** Note name: letters, with the German H / B convention when asked. unicode = use the ♯ ♭ signs. */
juce::String noteText (Spelling, Language = Language::english, bool unicode = true);

struct Key
{
    int tonic = 0;        // pitch class
    bool minor = false;
    Spelling tonicSpelling() const;
    /** Pitch classes treated as "in key" (minor also accepts raised 6 and 7). */
    std::array<bool, 12> scale() const;
    /** Sharps (> 0) or flats (< 0) in the key signature. */
    int signature() const;
    juce::String name (Language) const;
};

/** One way of reading the notes as a chord. */
struct ChordReading
{
    int root = -1, bass = -1;                 // pitch classes
    juce::String suffix;                      // "maj9", "7#9", "m7b5", ...
    float score = 0;
    int third = 0;                            // 0 none, 3 minor, 4 major
    int fifth = 0;                            // 0 none, 6 b5, 7 P5, 8 #5
    int seventh = 0;                          // 0 none, 9 dim7, 10 b7, 11 maj7
    bool sus = false;
    int inversion = 0;                        // 0 root, 1 first, 2 second, 3 third, -1 other bass
    std::array<int, 12> letterOffset {};      // per interval from the root: letters above the root letter, -1 = absent
    int numNotes = 0;                         // distinct pitch classes
};

struct Roman
{
    juce::String numeral;                     // "V", "ii", "bVII", "V/V"
    juce::String quality;                     // "°", "ø", "+", ""
    juce::String figureTop, figureBottom;     // "7" / "", "6" / "5", ...
    enum Function { none, tonic, subdominant, dominant, secondaryDominant, chromatic } function = none;
};

class ChordEngine
{
public:
    /** All readings, best first. Empty when no notes. */
    static std::vector<ChordReading> analyse (const std::vector<int>& midiNotes);

    /** Readings worth showing as "also" next to the best one (close score, at most two). */
    static std::vector<ChordReading> alternatives (const std::vector<ChordReading>&);

    /** Spelling of a chord root in a key / mode. */
    static Spelling spellRoot (int pc, const Key&, SpellMode);
    /** Spelling of every pitch class in a reading (index by pc; letter -1 for absent). */
    static std::array<Spelling, 12> spellChord (const ChordReading&, const Key&, SpellMode);
    /** "Cmaj9", "D7♯9", "G/B". */
    static juce::String symbol (const ChordReading&, const Key&, SpellMode, Language);
    static Roman roman (const ChordReading&, const Key&);

    /** Most likely key for a weighted pitch-class histogram (Krumhansl-Kessler). */
    static Key detectKey (const std::array<float, 12>& histogram, float* confidence = nullptr);

    /** Single-note and two-note names ("E", "C + G · P5"). */
    static juce::String intervalName (int semitones, Language);
};

/** Translated UI strings. */
juce::String tr (const char* id, Language);

} // namespace lyra
