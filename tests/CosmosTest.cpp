// LT COSMOS: loudness meter against EBU Tech 3341 cases, plus analyzer checks
// (tempo, drum hits, key, chord, tuner) on synthetic material.
#include <juce_audio_basics/juce_audio_basics.h>
#include "Loudness.h"
#include "Analysis.h"
#include <iostream>

using namespace cosmos;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

struct Signal
{
    double sr;
    std::vector<float> l, r;
    void sine (double seconds, double hz, double dbfs, double phase = 0.0, bool rightToo = true)
    {
        auto a = std::pow (10.0, dbfs / 20.0);
        auto n = (size_t) (seconds * sr), start = l.size();
        l.resize (start + n); r.resize (start + n);
        for (size_t i = 0; i < n; ++i)
        {
            auto v = (float) (a * std::sin (6.283185307179586 * hz * (double) (start + i) / sr + phase));
            l[start + i] = v;
            r[start + i] = rightToo ? v : 0.0f;
        }
    }
};

static void run (LoudnessMeter& m, const Signal& s, int block = 512)
{
    for (size_t i = 0; i < s.l.size(); i += (size_t) block)
    {
        auto n = (int) std::min ((size_t) block, s.l.size() - i);
        const float* ch[2] = { s.l.data() + i, s.r.data() + i };
        m.process (ch, 2, n);
    }
}

static void loudnessTests (double sr)
{
    std::cout << "loudness @ " << sr << " Hz" << std::endl;
    // Tech 3341 case 1: stereo 1 kHz sine at -23 dBFS for 20 s -> M, S, I = -23.0 (+-0.1)
    {
        LoudnessMeter m; m.prepare (sr, 2);
        Signal s { sr, {}, {} }; s.sine (20.0, 1000.0, -23.0);
        run (m, s);
        check (std::abs (m.getMomentary() + 23.0f) < 0.1f, "case 1 momentary " + juce::String (m.getMomentary(), 2));
        check (std::abs (m.getShortTerm() + 23.0f) < 0.1f, "case 1 short-term " + juce::String (m.getShortTerm(), 2));
        check (std::abs (m.getIntegrated() + 23.0f) < 0.1f, "case 1 integrated " + juce::String (m.getIntegrated(), 2));
    }
    // case 2: -33 dBFS -> -33.0
    {
        LoudnessMeter m; m.prepare (sr, 2);
        Signal s { sr, {}, {} }; s.sine (20.0, 1000.0, -33.0);
        run (m, s);
        check (std::abs (m.getIntegrated() + 33.0f) < 0.1f, "case 2 integrated " + juce::String (m.getIntegrated(), 2));
    }
    // case 3: -36 / -23 / -36 dBFS for 10 / 60 / 10 s -> I = -23.0 (relative gate)
    {
        LoudnessMeter m; m.prepare (sr, 2);
        Signal s { sr, {}, {} }; s.sine (10, 1000, -36); s.sine (60, 1000, -23); s.sine (10, 1000, -36);
        run (m, s, 441);
        check (std::abs (m.getIntegrated() + 23.0f) < 0.1f, "case 3 gated integrated " + juce::String (m.getIntegrated(), 2));
    }
    // case 4: -72 / -36 / -23 / -36 / -72 dBFS for 10 / 10 / 60 / 10 / 10 s -> I = -23.0 (absolute gate too)
    {
        LoudnessMeter m; m.prepare (sr, 2);
        Signal s { sr, {}, {} }; s.sine (10, 1000, -72); s.sine (10, 1000, -36); s.sine (60, 1000, -23); s.sine (10, 1000, -36); s.sine (10, 1000, -72);
        run (m, s, 1024);
        check (std::abs (m.getIntegrated() + 23.0f) < 0.1f, "case 4 gated integrated " + juce::String (m.getIntegrated(), 2));
    }
    // LRA (Tech 3342 case 1): 20 s at -20 dBFS then 20 s at -30 dBFS -> LRA 10 LU (+-1)
    {
        LoudnessMeter m; m.prepare (sr, 2);
        Signal s { sr, {}, {} }; s.sine (20, 1000, -20); s.sine (20, 1000, -30);
        run (m, s);
        check (std::abs (m.getRange() - 10.0f) < 1.0f, "LRA case 1 " + juce::String (m.getRange(), 2));
    }
    // true peak: fs/4 sine at 0 dBFS with 45 deg phase -> samples at -3 dB, true peak 0 dBTP (+0.2 / -0.4)
    if (sr < 96000)
    {
        LoudnessMeter m; m.prepare (sr, 2);
        Signal s { sr, {}, {} }; s.sine (3, sr / 4.0, 0.0, 3.14159265 / 4.0);
        run (m, s);
        auto tp = m.getTruePeakMax (0);
        check (tp > -0.4f && tp < 0.2f, "true peak fs/4 sine " + juce::String (tp, 2) + " dBTP");
    }
    // 997 Hz full scale -> ~0 dBTP
    {
        LoudnessMeter m; m.prepare (sr, 2);
        Signal s { sr, {}, {} }; s.sine (3, 997.0, -6.0);
        run (m, s);
        auto tp = m.getTruePeakMax (0);
        check (std::abs (tp + 6.0f) < 0.2f, "true peak -6 dBFS 997 Hz " + juce::String (tp, 2) + " dBTP");
    }
    // reset
    {
        LoudnessMeter m; m.prepare (sr, 2);
        Signal s { sr, {}, {} }; s.sine (5, 1000, -10);
        run (m, s);
        m.requestReset();
        Signal q { sr, {}, {} }; q.sine (10, 1000, -30);
        run (m, q);
        check (std::abs (m.getIntegrated() + 30.0f) < 0.2f, "reset clears integrated " + juce::String (m.getIntegrated(), 2));
    }
}

//==============================================================================
static void groove (Analyzer& a, double sr, double bpm, double seconds, bool chords)
{
    juce::Random rnd (5);
    const double beat = 60.0 / bpm;
    // A minor progression: Am F C G, one bar each
    static const int prog[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 48, 52, 55 }, { 55, 59, 62 } };
    std::vector<float> l (512), r (512);
    double prevNoise = 0;
    for (juce::int64 t = 0; t < (juce::int64) (seconds * sr);)
    {
        for (int i = 0; i < 512; ++i, ++t)
        {
            auto sec = t / sr;
            auto tb = std::fmod (sec, beat), t8 = std::fmod (sec, beat * 0.5);
            auto beatNo = (int) (sec / beat) % 4;
            auto bar = (int) (sec / (beat * 4)) % 4;
            double kick = std::exp (-tb * 10) * std::sin (6.2832 * (50 * tb + 70 * (1 - std::exp (-tb * 30)) / 30));
            auto noise = rnd.nextFloat() * 2.0 - 1.0;
            double snare = beatNo % 2 == 1 ? std::exp (-tb * 18) * noise : 0;
            double hat = std::exp (-t8 * 70) * (noise - prevNoise) * 0.4;
            prevNoise = noise;
            double pad = 0;
            if (chords)
                for (int k = 0; k < 3; ++k)
                {
                    auto f = 440.0 * std::pow (2.0, (prog[bar][k] - 69) / 12.0);
                    pad += 0.12 * (std::sin (6.2832 * f * sec) + 0.3 * std::sin (6.2832 * 2 * f * sec));
                }
            auto v = (float) (0.6 * kick + 0.3 * snare + 0.3 * hat + pad);
            l[(size_t) i] = v; r[(size_t) i] = v;
        }
        a.push (l.data(), r.data(), 512);
        a.frame();
    }
}

static void analysisTests()
{
    std::cout << "analysis" << std::endl;
    const double sr = 48000.0;
    {
        Analyzer a; a.prepare (sr);
        groove (a, sr, 120.0, 16.0, false);
        int kicks = 0, snares = 0;
        for (auto& h : a.hits) { kicks += h.kind == Hit::kick; snares += h.kind == Hit::snare; }
        if (juce::SystemStats::getEnvironmentVariable ("COSMOS_DEBUG", {}).isNotEmpty())
            for (auto& h : a.hits) if (h.kind == Hit::snare) std::cout << "        snare at beat " << std::fmod (h.time / sr / 0.5, 4.0) << " s=" << h.strength << std::endl;
        // the last 10 s hold 20 kicks and 10 snares
        check (kicks >= 17 && kicks <= 23, "kicks detected in last 10 s: " + juce::String (kicks));
        check (snares >= 7 && snares <= 13, "snares detected in last 10 s: " + juce::String (snares));
        check (std::abs (a.bpm - 120.0f) < 2.0f, "free tempo 120: " + juce::String (a.bpm, 1));
        int filled = 0;
        for (int st = 0; st < Analyzer::gridSteps; ++st) filled += (a.previousBar.hits[(size_t) st] & 1) != 0;
        check (filled >= 3 && filled <= 5, "previous bar holds 4 kicks in the step grid: " + juce::String (filled));
    }
    {
        Analyzer a; a.prepare (sr);
        groove (a, sr, 96.0, 16.0, false);
        check (std::abs (a.bpm - 96.0f) < 2.0f || std::abs (a.bpm - 192.0f) < 3.0f, "free tempo 96: " + juce::String (a.bpm, 1));
    }
    {
        Analyzer a; a.prepare (sr);
        a.setHostTiming (true, true, 120.0, 0.0, 0.0, 4, 4, 0);   // DAW timing: bars line up with the music
        groove (a, sr, 120.0, 24.0, true);
        check (a.key == 21 || a.key == 0, "key of Am-F-C-G (A min or C maj): " + music::keyName (a.key));
        int named = 0;
        for (auto& bc : a.chordStrip) named += bc.root >= 0;
        check (named >= 6, "bar chords recorded: " + juce::String (named));
        juce::StringArray strip;
        for (auto& bc : a.chordStrip) strip.add (music::chordName (bc.root, bc.type, -1, false));
        std::cout << "        chord strip: " << strip.joinIntoString (" ") << std::endl;
        int right = 0;
        static const int roots[4] = { 9, 5, 0, 7 };
        for (auto& bc : a.chordStrip)
            if (bc.root >= 0 && bc.bar >= 0) right += bc.root == roots[bc.bar % 4];
        check (right >= 5, "bar chords match Am F C G: " + juce::String (right) + "/8");
    }
    {
        Analyzer a; a.prepare (sr);
        // host timing: bar clock follows the ppq exactly
        a.setHostTiming (true, true, 120.0, 8.0, 8.0, 4, 4, 0);   // starts at bar 3 (ppq 8)
        std::vector<float> z (480, 0.0f);
        for (int i = 0; i < 100; ++i) a.push (z.data(), z.data(), 480);    // 1 s
        check (std::abs (a.barTime - 2.5) < 0.02, "host bar clock after 1 s: " + juce::String (a.barTime, 3));
    }
    {
        Analyzer a; a.prepare (sr);
        std::vector<float> l (480);
        double ph = 0;
        for (int b = 0; b < 60; ++b)
        {
            for (auto& v : l) { v = (float) (0.3 * std::sin (ph) + 0.1 * std::sin (2 * ph)); ph += 6.283185307 * 110.0 / sr; }
            a.push (l.data(), l.data(), 480);
            a.frame();
        }
        check (a.tunerMidi == 45 && std::abs (a.tunerCents) < 5.0f, "tuner A2 110 Hz: " + music::noteNameWithOctave (a.tunerMidi, false) + " " + juce::String (a.tunerCents, 1) + " ct");
    }
    {
        Analyzer a; a.prepare (sr);
        // wide low end: different noise on L and R -> low thirds read wide; mono signal reads narrow
        juce::Random rnd (1);
        std::vector<float> l (512), r (512);
        for (int b = 0; b < 120; ++b)
        {
            for (int i = 0; i < 512; ++i) { l[(size_t) i] = rnd.nextFloat() - 0.5f; r[(size_t) i] = rnd.nextFloat() - 0.5f; }
            a.push (l.data(), r.data(), 512);
            a.frame();
        }
        check (a.thirdWidth[3] > 0.35f && std::abs (a.correlation) < 0.2f, "decorrelated noise is wide: width " + juce::String (a.thirdWidth[3], 2) + " corr " + juce::String (a.correlation, 2));
        for (int b = 0; b < 120; ++b)
        {
            for (int i = 0; i < 512; ++i) { l[(size_t) i] = rnd.nextFloat() - 0.5f; r[(size_t) i] = l[(size_t) i]; }
            a.push (l.data(), r.data(), 512);
            a.frame();
        }
        check (a.thirdWidth[3] < 0.05f && a.correlation > 0.95f, "mono noise is narrow: width " + juce::String (a.thirdWidth[3], 2) + " corr " + juce::String (a.correlation, 2));
    }
    {
        check (music::chordName (7, 2, 11, false) == "G7/B", "chord naming G7/B");
        check (music::romanNumeral (0, 7, 2) == "V7", "roman V7 in C: " + music::romanNumeral (0, 7, 2));
        check (music::romanNumeral (21, 9, 1) == "i", "roman i in A minor: " + music::romanNumeral (21, 9, 1));
        check (music::keyName (15) == "Eb MIN", "flat spelling Eb minor: " + music::keyName (15));
    }
}

int main()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 }) loudnessTests (sr);
    analysisTests();
    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
