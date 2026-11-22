#include "ChordEngine.h"

namespace lyra
{

static const int naturals[7] = { 0, 2, 4, 5, 7, 9, 11 };
static inline int mod12 (int v) { return ((v % 12) + 12) % 12; }

int naturalPc (int letter) { return naturals[((letter % 7) + 7) % 7]; }
int pcOf (Spelling s)      { return mod12 (naturalPc (s.letter) + s.accidental); }

static Spelling spellOnLetter (int pc, int letter)
{
    letter = ((letter % 7) + 7) % 7;
    auto acc = mod12 (pc - naturalPc (letter));
    if (acc > 6) acc -= 12;
    return { letter, juce::jlimit (-2, 2, acc) };
}

juce::String noteText (Spelling s, Language lang, bool unicode)
{
    static const char* letters = "CDEFGAB";
    juce::String base = juce::String::charToString ((juce::juce_wchar) letters[s.letter]);
    auto acc = s.accidental;
    if (lang == Language::german && s.letter == 6)
    {
        if (acc == 0) return "H";
        if (acc == -1) return "B";
        if (acc == 1) return unicode ? juce::String (juce::CharPointer_UTF8 ("H\xe2\x99\xaf")) : "H#";
    }
    const juce::String sharp = unicode ? juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xaf")) : "#";
    const juce::String flat = unicode ? juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xad")) : "b";
    if (acc == 2) return base + (unicode ? juce::String (juce::CharPointer_UTF8 ("\xf0\x9d\x84\xaa")) : "##");
    if (acc == -2) return base + flat + flat;
    if (acc == 1) return base + sharp;
    if (acc == -1) return base + flat;
    return base;
}

//============================================================================== Key
Spelling Key::tonicSpelling() const
{
    // majors: C Db D Eb E F F# G Ab A Bb B ; minors: C C# D Eb E F F# G G# A Bb B
    static const Spelling major[12] = { { 0, 0 }, { 1, -1 }, { 1, 0 }, { 2, -1 }, { 2, 0 }, { 3, 0 }, { 3, 1 }, { 4, 0 }, { 5, -1 }, { 5, 0 }, { 6, -1 }, { 6, 0 } };
    static const Spelling minorK[12] = { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 2, -1 }, { 2, 0 }, { 3, 0 }, { 3, 1 }, { 4, 0 }, { 4, 1 }, { 5, 0 }, { 6, -1 }, { 6, 0 } };
    return (minor ? minorK : major)[mod12 (tonic)];
}

std::array<bool, 12> Key::scale() const
{
    static const int maj[] = { 0, 2, 4, 5, 7, 9, 11 }, nat[] = { 0, 2, 3, 5, 7, 8, 10 };
    std::array<bool, 12> s {};
    for (auto i : minor ? nat : maj) s[(size_t) mod12 (tonic + i)] = true;
    if (minor) { s[(size_t) mod12 (tonic + 9)] = true; s[(size_t) mod12 (tonic + 11)] = true; }   // melodic / harmonic
    return s;
}

int Key::signature() const
{
    static const int sig[12] = { 0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5 };
    auto s = sig[mod12 (minor ? tonic + 3 : tonic)];
    // minor tonics spelled with sharps (C#, G#) use the sharp side
    if (minor && (mod12 (tonic) == 1)) s = 4;
    if (minor && (mod12 (tonic) == 8)) s = 5;
    return s;
}

juce::String Key::name (Language lang) const
{
    auto t = noteText (tonicSpelling(), lang);
    switch (lang)
    {
        case Language::spanish:  return t + (minor ? " menor" : " mayor");
        case Language::german:   return minor ? t.toLowerCase() + "-Moll" : t + "-Dur";
        case Language::japanese: return t + (minor ? juce::String (juce::CharPointer_UTF8 ("\xe3\x83\x9e\xe3\x82\xa4\xe3\x83\x8a\xe3\x83\xbc"))
                                                  : juce::String (juce::CharPointer_UTF8 ("\xe3\x83\xa1\xe3\x82\xb8\xe3\x83\xa3\xe3\x83\xbc")));
        case Language::english:  break;
    }
    return t + (minor ? " minor" : " major");
}

//============================================================================== naming
static bool readAs (int root, int mask, int bassPc, ChordReading& out)
{
    auto has = [&] (int i) { return (mask >> mod12 (root + i) & 1) != 0; };
    ChordReading c;
    c.root = root;
    c.bass = bassPc;
    c.letterOffset.fill (-1);

    const bool M3 = has (4), m3 = has (3) && ! M3, sharp9 = has (3) && M3;
    const bool P5 = has (7);
    const bool b5 = has (6) && ! P5;
    const bool sharp11 = has (6) && P5;
    const bool sharp5 = has (8) && ! P5 && M3 && ! b5;
    const bool flat13 = has (8) && ! sharp5;
    int seventh = has (10) ? 10 : has (11) ? 11 : (has (9) && m3 && b5) ? 9 : 0;
    const bool dim7 = seventh == 9;
    const bool six = has (9) && ! dim7;               // 6 without a 7th, 13 with one
    const bool flat9 = has (1);
    const bool hasNine = has (2), hasEleven = has (5);
    const int third = M3 ? 4 : m3 ? 3 : 0;
    const bool sus4 = third == 0 && hasEleven, sus2 = third == 0 && ! hasEleven && hasNine;

    c.third = third;
    c.fifth = P5 ? 7 : b5 ? 6 : sharp5 ? 8 : 0;
    c.seventh = seventh;
    c.sus = sus4 || sus2;

    // letters above the root for every interval present
    for (int i = 0; i < 12; ++i)
    {
        if (! has (i)) continue;
        int off = 0;
        switch (i)
        {
            case 0: off = 0; break;
            case 1: case 2: off = 1; break;
            case 3: off = sharp9 ? 1 : 2; break;
            case 4: off = 2; break;
            case 5: off = 3; break;
            case 6: off = b5 ? 4 : 3; break;
            case 7: off = 4; break;
            case 8: off = sharp5 ? 4 : 5; break;
            case 9: off = dim7 ? 6 : 5; break;
            default: off = 6; break;
        }
        c.letterOffset[(size_t) i] = off;
    }

    // natural extensions (they need a 7th to stack) and the rest as alterations / adds
    juce::StringArray alts, adds;
    int ext = 0;
    if (seventh == 10 || seventh == 11)
    {
        if (hasNine && ! sus2) ext = 9;
        if (hasEleven && third != 0) ext = 11;
        if (six) ext = 13;
        // a 13 or 11 chord without the 9 still reads as 13 / 11 (the 9 is implied); keep it simple
    }
    if (flat9) alts.add ("b9");
    if (sharp9) alts.add ("#9");
    if (sharp11) alts.add ("#11");
    if (flat13) alts.add (seventh != 0 || P5 ? "b13" : "b6");

    juce::String s;
    float score = 0;
    if (dim7)
    {
        s = "dim7";
        if (hasNine) adds.add ("9");
        if (hasEleven) adds.add ("11");
        score -= 0.5f;
    }
    else if (third == 3 && b5)
    {
        if (seventh == 10) s = "m7b5";
        else if (seventh == 11) s = "dim(maj7)";
        else s = "dim";
        if (hasNine) adds.add ("9");
        if (hasEleven) adds.add ("11");
        score -= 1.0f;
    }
    else if (sharp5)
    {
        s = seventh == 10 ? "7#5" : seventh == 11 ? "maj7#5" : "aug";
        if (ext == 9) s = seventh == 10 ? "9#5" : "maj9#5";
        score -= 0.8f;
    }
    else
    {
        juce::String q = third == 3 ? "m" : "";
        if (seventh == 10)      s = q + juce::String (ext ? ext : 7);
        else if (seventh == 11) s = q + (third == 3 ? "Maj" : "maj") + juce::String (ext ? ext : 7);
        else
        {
            s = q;
            if (six) { s << (hasNine && ! sus2 ? "6/9" : "6"); score -= 0.4f; }
            else if (hasNine && ! sus2) adds.add ("9");
            if (hasEleven && third != 0) adds.add ("11");
        }
        if (b5 && third == 4) alts.insert (0, "b5");
        if (sus4) s << (hasNine && seventh == 0 && ! six ? "sus4" : "sus4");
        else if (sus2) s << "sus2";
        if (third == 0 && ! c.sus && P5 && seventh == 0 && ! six && alts.isEmpty() && adds.isEmpty()) s = "5";
        if (third == 0 && ! c.sus && ! P5 && seventh == 0) s << "(no3)";
        if (sus4 && hasNine && seventh == 0 && ! six) adds.add ("9");
    }

    if (! alts.isEmpty())
        s << (seventh != 0 ? alts.joinIntoString ("") : "(" + alts.joinIntoString (",") + ")");
    if (! adds.isEmpty())
        s << (s.isEmpty() || s == "m" ? "add" + adds.joinIntoString ("add") : "(add" + adds.joinIntoString (",") + ")");

    // how natural is this reading?
    if (root == bassPc) score += 3.0f;
    if (third != 0) score += 2.0f;
    if (P5) score += 1.0f;
    if (c.sus) score -= 1.0f;
    score -= 2.5f * (float) alts.size();
    score -= 0.6f * (float) adds.size();
    if (ext >= 11) score -= 0.3f;
    if (ext == 13) score -= 0.3f;
    c.suffix = s;
    c.score = score;

    // inversion from the bass interval
    auto bi = mod12 (bassPc - root);
    if (bi == 0) c.inversion = 0;
    else if (bi == third || (third == 0 && (bi == 5 || bi == 2))) c.inversion = 1;
    else if (bi == c.fifth) c.inversion = 2;
    else if (seventh != 0 && bi == seventh) c.inversion = 3;
    else c.inversion = -1;

    out = c;
    return true;
}

std::vector<ChordReading> ChordEngine::analyse (const std::vector<int>& notes)
{
    std::vector<ChordReading> res;
    if (notes.empty()) return res;
    int mask = 0, low = 999;
    for (auto n : notes) { mask |= 1 << mod12 (n); low = juce::jmin (low, n); }
    auto bassPc = mod12 (low);
    int count = 0;
    for (int i = 0; i < 12; ++i) count += (mask >> i) & 1;

    for (int r = 0; r < 12; ++r)
    {
        if (! ((mask >> r) & 1)) continue;
        ChordReading c;
        if (readAs (r, mask, bassPc, c))
        {
            c.numNotes = count;
            res.push_back (c);
        }
    }
    std::stable_sort (res.begin(), res.end(), [] (const ChordReading& a, const ChordReading& b) { return a.score > b.score; });
    if (count <= 2)
    {
        // a single note or an interval: one reading, rooted on the bass
        auto it = std::find_if (res.begin(), res.end(), [bassPc] (const ChordReading& c) { return c.root == bassPc; });
        if (it != res.end()) { auto c = *it; res.clear(); res.push_back (c); }
    }
    return res;
}

std::vector<ChordReading> ChordEngine::alternatives (const std::vector<ChordReading>& all)
{
    std::vector<ChordReading> out;
    if (all.size() < 2 || all[0].numNotes < 3) return out;
    for (size_t i = 1; i < all.size() && out.size() < 2; ++i)
    {
        auto& c = all[i];
        // only plain, sensible rereadings: no alterations, adds or missing thirds unless the best one has them too
        auto odd = [] (const juce::String& s) { return s.contains ("(") || s.contains ("b5") || s.contains ("#") || s.contains ("b9") || s.contains ("b13"); };
        if (c.score < all[0].score - 2.6f || c.fifth == 8 || c.seventh == 9) continue;
        if (odd (c.suffix) && ! odd (all[0].suffix)) continue;
        if (c.third == 0 && all[0].third != 0) continue;
        out.push_back (c);
    }
    return out;
}

//============================================================================== spelling
Spelling ChordEngine::spellRoot (int pc, const Key& key, SpellMode mode)
{
    pc = mod12 (pc);
    if (mode == SpellMode::sharps)
    {
        static const Spelling s[12] = { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 2, 0 }, { 3, 0 }, { 3, 1 }, { 4, 0 }, { 4, 1 }, { 5, 0 }, { 5, 1 }, { 6, 0 } };
        return s[pc];
    }
    if (mode == SpellMode::flats)
    {
        static const Spelling s[12] = { { 0, 0 }, { 1, -1 }, { 1, 0 }, { 2, -1 }, { 2, 0 }, { 3, 0 }, { 4, -1 }, { 4, 0 }, { 5, -1 }, { 5, 0 }, { 6, -1 }, { 6, 0 } };
        return s[pc];
    }
    // by scale degree from the tonic: 1 b2 2 b3 3 4 #4 5 b6 6 b7 7
    static const int letterOff[12] = { 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6 };
    auto t = key.tonicSpelling();
    auto iv = mod12 (pc - key.tonic);
    auto s = spellOnLetter (pc, t.letter + letterOff[iv]);
    if (std::abs (s.accidental) > 1) s = spellRoot (pc, key, key.signature() < 0 ? SpellMode::flats : SpellMode::sharps);
    return s;
}

std::array<Spelling, 12> ChordEngine::spellChord (const ChordReading& c, const Key& key, SpellMode mode)
{
    std::array<Spelling, 12> out;
    for (auto& s : out) s = { -1, 0 };
    if (c.root < 0) return out;
    auto r = spellRoot (c.root, key, mode);
    for (int i = 0; i < 12; ++i)
    {
        auto off = c.letterOffset[(size_t) i];
        if (off < 0) continue;
        out[(size_t) mod12 (c.root + i)] = spellOnLetter (mod12 (c.root + i), r.letter + off);
    }
    return out;
}

static juce::String prettySuffix (juce::String s)
{
    juce::String out;
    for (int i = 0; i < s.length(); ++i)
    {
        auto ch = s[i];
        auto nextDigit = i + 1 < s.length() && juce::CharacterFunctions::isDigit (s[i + 1]);
        if (ch == 'b' && nextDigit) out << juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xad"));
        else if (ch == '#') out << juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xaf"));
        else out << juce::String::charToString (ch);
    }
    return out;
}

juce::String ChordEngine::intervalName (int semis, Language)
{
    static const char* n[12] = { "P1", "m2", "M2", "m3", "M3", "P4", "TT", "P5", "m6", "M6", "m7", "M7" };
    return n[mod12 (semis)];
}

juce::String ChordEngine::symbol (const ChordReading& c, const Key& key, SpellMode mode, Language lang)
{
    if (c.root < 0) return {};
    auto sp = spellChord (c, key, mode);
    auto rootName = noteText (sp[(size_t) c.root], lang);
    if (c.numNotes == 1) return rootName;
    if (c.numNotes == 2)
    {
        int other = -1;
        for (int i = 1; i < 12; ++i) if (c.letterOffset[(size_t) i] >= 0) other = i;
        if (other == 7) return rootName + "5";
        return rootName + " + " + noteText (sp[(size_t) mod12 (c.root + other)], lang) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + intervalName (other, lang);
    }
    auto s = rootName + prettySuffix (c.suffix);
    if (c.bass != c.root && c.bass >= 0)
    {
        // the bass is read on its own: no Fb, B# or double accidentals under a slash
        auto b = sp[(size_t) c.bass];
        auto awkward = std::abs (b.accidental) > 1 || (b.accidental == 1 && (b.letter == 2 || b.letter == 6))
                    || (b.accidental == -1 && (b.letter == 0 || b.letter == 3));
        if (awkward) b = spellRoot (c.bass, key, mode);
        s << "/" << noteText (b, lang);
    }
    return s;
}

//============================================================================== roman numerals
Roman ChordEngine::roman (const ChordReading& c, const Key& key)
{
    Roman r;
    if (c.root < 0 || c.numNotes < 3) return r;
    static const char* numerals[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    auto tonic = key.tonicSpelling();
    auto rootSp = spellRoot (c.root, key, SpellMode::key);
    auto degree = ((rootSp.letter - tonic.letter) % 7 + 7) % 7;

    // accidental against the key's own scale degree (minor: natural minor, leading tone written plain)
    static const int majorDeg[7] = { 0, 2, 4, 5, 7, 9, 11 }, minorDeg[7] = { 0, 2, 3, 5, 7, 8, 10 };
    auto expected = mod12 (key.tonic + (key.minor ? minorDeg : majorDeg)[degree]);
    auto acc = mod12 (c.root - expected);
    if (acc > 6) acc -= 12;
    if (key.minor && (degree == 6 || degree == 5) && acc == 1) acc = 0;   // raised 6 / 7 in minor

    auto quality = [] (const ChordReading& ch)
    {
        if (ch.seventh == 9) return juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
        if (ch.third == 3 && ch.fifth == 6) return ch.seventh == 10 ? juce::String (juce::CharPointer_UTF8 ("\xc3\xb8")) : juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
        if (ch.fifth == 8) return juce::String ("+");
        return juce::String();
    };
    auto caseFor = [] (const ChordReading& ch, juce::String n) { return ch.third == 3 ? n.toLowerCase() : n; };

    juce::String num = (acc < 0 ? "b" : acc > 0 ? "#" : "") + juce::String (numerals[degree]);
    r.numeral = caseFor (c, num);
    r.quality = quality (c);

    // figures
    bool seventhChord = c.seventh != 0;
    switch (c.inversion)
    {
        case 0: if (seventhChord) r.figureTop = c.seventh == 11 ? "M7" : "7"; break;
        case 1: if (seventhChord) { r.figureTop = "6"; r.figureBottom = "5"; } else r.figureTop = "6"; break;
        case 2: if (seventhChord) { r.figureTop = "4"; r.figureBottom = "3"; } else { r.figureTop = "6"; r.figureBottom = "4"; } break;
        case 3: r.figureTop = "4"; r.figureBottom = "2"; break;
        default: if (seventhChord) r.figureTop = c.seventh == 11 ? "M7" : "7"; break;
    }

    // function
    auto scale = key.scale();
    bool diatonic = true;
    for (int i = 0; i < 12; ++i)
        if (c.letterOffset[(size_t) i] >= 0 && ! scale[(size_t) mod12 (c.root + i)]) diatonic = false;

    const bool dominantShape = c.third == 4 && (c.fifth == 7 || c.fifth == 0 || c.fifth == 6) && (c.seventh == 10 || c.seventh == 0);
    if (diatonic && acc == 0)
    {
        switch (degree)
        {
            case 0: case 2: case 5: r.function = Roman::tonic; break;
            case 1: case 3:         r.function = Roman::subdominant; break;
            default:                r.function = Roman::dominant; break;
        }
        return r;
    }
    // secondary dominant: a major / dominant 7th chord a fifth above a diatonic degree other than the tonic
    if (dominantShape)
    {
        auto target = mod12 (c.root + 5);
        if (scale[(size_t) target] && target != key.tonic)
        {
            auto tSp = spellRoot (target, key, SpellMode::key);
            auto tDeg = ((tSp.letter - tonic.letter) % 7 + 7) % 7;
            // case of the target's diatonic triad
            auto third = mod12 (key.tonic + (key.minor ? minorDeg : majorDeg)[(tDeg + 2) % 7] - target);
            juce::String tn = numerals[tDeg];
            if (third == 3) tn = tn.toLowerCase();
            r.numeral = "V";
            r.quality = {};
            r.numeral << (c.seventh == 10 && c.inversion == 0 ? "7" : "") << "/" << tn;
            if (c.seventh == 10 && c.inversion == 0) r.figureTop = {};
            r.function = Roman::secondaryDominant;
            return r;
        }
    }
    r.function = Roman::chromatic;
    return r;
}

//============================================================================== key detection
Key ChordEngine::detectKey (const std::array<float, 12>& h, float* confidence)
{
    static const float majorP[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float minorP[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
    float mx = 0, my = 0;
    for (int i = 0; i < 12; ++i) mx += h[(size_t) i];
    mx /= 12.0f;
    float best = -2, second = -2;
    Key bestKey;
    for (int k = 0; k < 24; ++k)
    {
        auto* p = k < 12 ? majorP : minorP;
        my = 0;
        for (int i = 0; i < 12; ++i) my += p[i];
        my /= 12.0f;
        float sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i)
        {
            auto x = h[(size_t) mod12 (i + k)] - mx, y = p[i] - my;
            sxy += x * y; sxx += x * x; syy += y * y;
        }
        auto c = sxy / std::sqrt (sxx * syy + 1.0e-9f);
        if (c > best) { second = best; best = c; bestKey = { k % 12, k >= 12 }; }
        else if (c > second) second = c;
    }
    if (confidence != nullptr) *confidence = juce::jlimit (0.0f, 1.0f, best - second + (best - 0.5f));
    return bestKey;
}

//============================================================================== strings
juce::String tr (const char* id, Language lang)
{
    struct Row { const char* id; const char* en; const char* es; const char* ja; const char* de; };
    static const Row rows[] = {
        { "key", "KEY", "TONALIDAD", "\xe3\x82\xad\xe3\x83\xbc", "TONART" },
        { "auto", "AUTO", "AUTO", "\xe8\x87\xaa\xe5\x8b\x95", "AUTO" },
        { "spelling", "SPELLING", "ESCRITURA", "\xe8\xa1\xa8\xe8\xa8\x98", "SCHREIBWEISE" },
        { "bykey", "BY KEY", "POR TONALIDAD", "\xe3\x82\xad\xe3\x83\xbc\xe3\x81\xab\xe5\x90\x88\xe3\x82\x8f\xe3\x81\x9b\xe3\x82\x8b", "NACH TONART" },
        { "sharps", "SHARPS", "SOSTENIDOS", "\xe3\x82\xb7\xe3\x83\xa3\xe3\x83\xbc\xe3\x83\x97", "KREUZE" },
        { "flats", "FLATS", "BEMOLES", "\xe3\x83\x95\xe3\x83\xa9\xe3\x83\x83\xe3\x83\x88", "BS" },
        { "also", "ALSO", "TAMBI\xc3\x89N", "\xe5\x88\xa5\xe8\xa7\xa3\xe9\x87\x88", "AUCH" },
        { "tonic", "TONIC", "T\xc3\x93NICA", "\xe3\x83\x88\xe3\x83\x8b\xe3\x83\x83\xe3\x82\xaf", "TONIKA" },
        { "subdominant", "SUBDOMINANT", "SUBDOMINANTE", "\xe3\x82\xb5\xe3\x83\x96\xe3\x83\x89\xe3\x83\x9f\xe3\x83\x8a\xe3\x83\xb3\xe3\x83\x88", "SUBDOMINANTE" },
        { "dominant", "DOMINANT", "DOMINANTE", "\xe3\x83\x89\xe3\x83\x9f\xe3\x83\x8a\xe3\x83\xb3\xe3\x83\x88", "DOMINANTE" },
        { "secondary", "SECONDARY DOMINANT", "DOMINANTE SECUNDARIA", "\xe3\x82\xbb\xe3\x82\xab\xe3\x83\xb3\xe3\x83\x80\xe3\x83\xaa\xe3\x83\xbc\xe3\x83\xbb\xe3\x83\x89\xe3\x83\x9f\xe3\x83\x8a\xe3\x83\xb3\xe3\x83\x88", "ZWISCHENDOMINANTE" },
        { "chromatic", "CHROMATIC", "CROM\xc3\x81TICO", "\xe3\x82\xaf\xe3\x83\xad\xe3\x83\x9e\xe3\x83\x86\xe3\x82\xa3\xe3\x83\x83\xe3\x82\xaf", "CHROMATISCH" },
        { "theme", "THEME", "TEMA", "\xe3\x83\x86\xe3\x83\xbc\xe3\x83\x9e", "THEMA" },
        { "light", "LIGHT", "CLARO", "\xe3\x83\xa9\xe3\x82\xa4\xe3\x83\x88", "HELL" },
        { "dark", "DARK", "OSCURO", "\xe3\x83\x80\xe3\x83\xbc\xe3\x82\xaf", "DUNKEL" },
        { "colour", "COLOUR", "COLOR", "\xe8\x89\xb2", "FARBE" },
        { "language", "LANGUAGE", "IDIOMA", "\xe8\xa8\x80\xe8\xaa\x9e", "SPRACHE" },
        { "play", "Play a chord", "Toca un acorde", "\xe5\x92\x8c\xe9\x9f\xb3\xe3\x82\x92\xe5\xbc\xbe\xe3\x81\x84\xe3\x81\xa6\xe3\x81\x8f\xe3\x81\xa0\xe3\x81\x95\xe3\x81\x84", "Spiel einen Akkord" },
        { "inv0", "ROOT POSITION", "ESTADO FUNDAMENTAL", "\xe5\x9f\xba\xe6\x9c\xac\xe5\xbd\xa2", "GRUNDSTELLUNG" },
        { "inv1", "1ST INVERSION", "1.\xc2\xaa INVERSI\xc3\x93N", "\xe7\xac\xac""1\xe8\xbb\xa2\xe5\x9b\x9e\xe5\xbd\xa2", "1. UMKEHRUNG" },
        { "inv2", "2ND INVERSION", "2.\xc2\xaa INVERSI\xc3\x93N", "\xe7\xac\xac""2\xe8\xbb\xa2\xe5\x9b\x9e\xe5\xbd\xa2", "2. UMKEHRUNG" },
        { "inv3", "3RD INVERSION", "3.\xc2\xaa INVERSI\xc3\x93N", "\xe7\xac\xac""3\xe8\xbb\xa2\xe5\x9b\x9e\xe5\xbd\xa2", "3. UMKEHRUNG" },
        { "slash", "OTHER BASS", "OTRO BAJO", "\xe5\x88\xa5\xe3\x81\xae\xe3\x83\x99\xe3\x83\xbc\xe3\x82\xb9", "ANDERER BASS" },
        { "notes", "NOTES", "NOTAS", "\xe9\x9f\xb3", "NOTEN" },
        { "held", "LAST CHORD", "\xc3\x9aLTIMO ACORDE", "\xe7\x9b\xb4\xe5\x89\x8d\xe3\x81\xae\xe5\x92\x8c\xe9\x9f\xb3", "LETZTER AKKORD" },
        { "hint", "click keys to try chords, right-click to clear", "clic en las teclas para probar acordes, clic derecho para borrar",
                  "\xe9\x8d\xb5\xe7\x9b\xa4\xe3\x82\x92\xe3\x82\xaf\xe3\x83\xaa\xe3\x83\x83\xe3\x82\xaf\xe3\x81\x97\xe3\x81\xa6\xe5\x92\x8c\xe9\x9f\xb3\xe3\x82\x92\xe8\xa9\xa6\xe3\x81\x99\xe3\x80\x81\xe5\x8f\xb3\xe3\x82\xaf\xe3\x83\xaa\xe3\x83\x83\xe3\x82\xaf\xe3\x81\xa7\xe6\xb6\x88\xe5\x8e\xbb",
                  "Tasten anklicken zum Ausprobieren, Rechtsklick l\xc3\xb6scht" },
        { "input", "INPUT", "ENTRADA", "\xe5\x85\xa5\xe5\x8a\x9b", "EINGANG" },
        { "pedal", "PEDAL", "PEDAL", "\xe3\x83\x9a\xe3\x83\x80\xe3\x83\xab", "PEDAL" },
        { "readonly", "READS MIDI, NEVER PLAYS", "S\xc3\x93LO LEE MIDI, NUNCA SUENA", "MIDI\xe3\x82\x92\xe8\xaa\xad\xe3\x82\x80\xe3\x81\xa0\xe3\x81\x91", "LIEST NUR MIDI" },
    };
    for (auto& r : rows)
        if (juce::String (r.id) == id)
        {
            const char* s = lang == Language::spanish ? r.es : lang == Language::japanese ? r.ja : lang == Language::german ? r.de : r.en;
            return juce::String (juce::CharPointer_UTF8 (s));
        }
    return id;
}

} // namespace lyra
