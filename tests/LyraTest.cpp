// LT LYRA: chord naming, spelling, roman numerals and key detection.
#include "ChordEngine.h"
#include <iostream>

using namespace lyra;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

static const Key C { 0, false };

/** Names with ASCII accidentals so the expectations are easy to type. */
static juce::String ascii (juce::String s)
{
    return s.replace (juce::CharPointer_UTF8 ("\xe2\x99\xad"), "b").replace (juce::CharPointer_UTF8 ("\xe2\x99\xaf"), "#")
            .replace (juce::CharPointer_UTF8 ("\xc2\xb0"), "o").replace (juce::CharPointer_UTF8 ("\xc3\xb8"), "0")
            .replace (juce::CharPointer_UTF8 ("\xc2\xb7"), ".");
}

static void name (std::vector<int> notes, const char* expected, const Key& key = C, SpellMode mode = SpellMode::key, Language lang = Language::english)
{
    auto r = ChordEngine::analyse (notes);
    auto got = r.empty() ? juce::String ("-") : ascii (ChordEngine::symbol (r[0], key, mode, lang));
    juce::StringArray also;
    for (size_t i = 1; i < juce::jmin ((size_t) 3, r.size()); ++i) also.add (ascii (ChordEngine::symbol (r[i], key, mode, lang)));
    check (got == expected, juce::String (expected) + " -> " + got + "   (also " + also.joinIntoString (", ") + ")");
}

static void roman (std::vector<int> notes, const Key& key, const char* expected, Roman::Function fn)
{
    auto r = ChordEngine::analyse (notes);
    auto ro = ChordEngine::roman (r[0], key);
    auto got = ascii (ro.numeral + ro.quality + ro.figureTop + (ro.figureBottom.isNotEmpty() ? "/" + ro.figureBottom : juce::String()));
    check (got == expected && ro.function == fn, juce::String (expected) + " -> " + got + " fn " + juce::String ((int) ro.function));
}

int main()
{
    std::cout << "names" << std::endl;
    name ({ 60, 64, 67 }, "C");
    name ({ 57, 60, 64 }, "Am");
    name ({ 48, 64, 71, 74, 79 }, "Cmaj9");
    name ({ 50, 54, 57, 60, 65 }, "D7#9");
    name ({ 59, 62, 67 }, "G/B");
    name ({ 48, 52, 55, 57 }, "C6");
    name ({ 45, 52, 55, 60 }, "Am7");
    name ({ 52, 55, 58, 62 }, "Em7b5");
    name ({ 48, 51, 54, 57 }, "Cdim7");
    name ({ 55, 59, 62, 65 }, "G7");
    name ({ 60, 64, 68 }, "Caug");
    name ({ 60, 65, 67 }, "Csus4");
    name ({ 60, 62, 67 }, "Csus2");
    name ({ 48, 55 }, "C5");
    name ({ 60, 64 }, "C + E . M3");
    name ({ 64 }, "E");
    name ({ 48, 52, 55, 58, 62, 66 }, "C9#11");
    name ({ 48, 52, 58, 62, 69 }, "C13");
    name ({ 43, 47, 50, 53, 56 }, "G7b9");
    name ({ 48, 51, 55, 58, 62 }, "Cm9");
    name ({ 48, 51, 55, 59 }, "CmMaj7");
    name ({ 48, 52, 55, 62 }, "Cadd9");
    name ({ 48, 52, 55, 57, 62 }, "C6/9");
    name ({ 48, 53, 55, 58 }, "C7sus4");
    name ({ 52, 55, 60 }, "C/E");
    name ({ 55, 60, 64 }, "C/G");
    name ({ 53, 55, 59, 62 }, "G7/F");

    std::cout << "spelling" << std::endl;
    name ({ 61, 65, 68 }, "Db", { 3, false });             // in Eb major
    name ({ 61, 65, 68 }, "C#", { 9, false });             // in A major
    name ({ 61, 64, 67, 70 }, "C#dim7", { 2, true });      // leading-tone dim7 in D minor
    name ({ 66, 70, 73 }, "Gb", C, SpellMode::flats);
    name ({ 66, 70, 73 }, "F#", C, SpellMode::sharps);
    name ({ 59, 63, 66, 69 }, "H7", C, SpellMode::key, Language::german);
    name ({ 58, 62, 65 }, "B", C, SpellMode::key, Language::german);   // German B = Bb
    {
        auto r = ChordEngine::analyse ({ 61, 64, 67, 70 });
        auto sp = ChordEngine::spellChord (r[0], { 2, true }, SpellMode::key);
        check (ascii (noteText (sp[10])) == "Bb" && ascii (noteText (sp[4])) == "E", "C#dim7 tones C# E G Bb: " + ascii (noteText (sp[1])) + " " + ascii (noteText (sp[4])) + " " + ascii (noteText (sp[7])) + " " + ascii (noteText (sp[10])));
    }

    std::cout << "roman numerals" << std::endl;
    roman ({ 60, 64, 67 }, C, "I", Roman::tonic);
    roman ({ 50, 53, 57, 60 }, C, "ii7", Roman::subdominant);
    roman ({ 55, 59, 62, 65 }, C, "V7", Roman::dominant);
    roman ({ 59, 62, 67, 65 }, C, "V6/5", Roman::dominant);
    roman ({ 59, 62, 65 }, C, "viio", Roman::dominant);
    roman ({ 50, 54, 57, 60 }, C, "V7/V", Roman::secondaryDominant);
    roman ({ 52, 56, 59 }, C, "V/vi", Roman::secondaryDominant);
    roman ({ 56, 60, 63 }, C, "bVI", Roman::chromatic);
    roman ({ 57, 60, 64 }, { 9, true }, "i", Roman::tonic);
    roman ({ 52, 56, 59, 62 }, { 9, true }, "V7", Roman::dominant);      // E7 in A minor (harmonic)
    roman ({ 59, 62, 65, 69 }, { 9, true }, "ii07", Roman::subdominant);  // Bm7b5 in A minor
    roman ({ 59, 62, 65, 68 }, { 9, true }, "iio7", Roman::subdominant);  // symmetric dim7: the bass names it
    roman ({ 64, 67, 72 }, C, "I6", Roman::tonic);
    roman ({ 67, 72, 76 }, C, "I6/4", Roman::tonic);

    std::cout << "key detection" << std::endl;
    {
        std::array<float, 12> h {};
        for (int pc : { 0, 2, 4, 5, 7, 9, 11 }) h[(size_t) pc] = 1.0f;
        h[0] = 2.5f; h[7] = 2.0f; h[4] = 1.6f;
        auto k = ChordEngine::detectKey (h);
        check (k.tonic == 0 && ! k.minor, "C major scale -> " + k.name (Language::english));
        std::array<float, 12> m {};
        for (int pc : { 9, 11, 0, 2, 4, 5, 7 }) m[(size_t) pc] = 1.0f;
        m[9] = 2.5f; m[4] = 2.0f; m[0] = 1.6f; m[8] = 0.8f;
        auto km = ChordEngine::detectKey (m);
        check (km.tonic == 9 && km.minor, "A minor -> " + km.name (Language::english));
        check (Key { 3, false }.signature() == -3 && Key { 4, true }.signature() == 1 && Key { 6, false }.signature() == 6, "key signatures Eb (-3), e (+1), F# (+6)");
        check (Key { 9, true }.name (Language::german) == "a-Moll", "German key name: " + Key { 9, true }.name (Language::german));
    }

    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
