#include <juce_audio_formats/juce_audio_formats.h>
#include "Analysis.h"

using namespace typebeats;

// Renders a little "type beat": chords (pad + bass) and a kick on every beat.
static juce::AudioBuffer<float> render (double sr, double bpm, std::vector<std::vector<int>> chords, int barsPerChord, int loops)
{
    auto beat = 60.0 / bpm;
    auto chordLen = beat * 4 * barsPerChord;
    auto total = chordLen * (double) chords.size() * loops;
    juce::AudioBuffer<float> b (2, (int) (total * sr));
    b.clear();
    juce::Random rnd (1);

    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        auto t = i / sr;
        auto ci = (int) (t / chordLen) % (int) chords.size();
        auto& notes = chords[(size_t) ci];
        float s = 0;
        for (size_t n = 1; n < notes.size(); ++n)
        {
            auto f = 440.0 * std::pow (2.0, (notes[n] - 69) / 12.0);
            s += 0.12f * (float) (std::sin (juce::MathConstants<double>::twoPi * f * t) + 0.3 * std::sin (juce::MathConstants<double>::twoPi * 2 * f * t));
        }
        auto fb = 440.0 * std::pow (2.0, (notes[0] - 69) / 12.0);
        s += 0.25f * (float) std::sin (juce::MathConstants<double>::twoPi * fb * t);

        auto tb = std::fmod (t, beat);
        auto kickEnv = std::exp (-tb * 18.0);
        s += 0.8f * (float) (kickEnv * std::sin (juce::MathConstants<double>::twoPi * (50 + 120 * std::exp (-tb * 40)) * tb));
        auto th = std::fmod (t + beat * 0.5, beat);
        s += 0.08f * (float) (std::exp (-th * 60.0) * (rnd.nextFloat() * 2 - 1));   // offbeat hat

        b.setSample (0, i, s); b.setSample (1, i, s);
    }
    return b;
}

int main (int argc, char** argv)
{
    int failures = 0;

    auto run = [&] (const char* label, double bpm, std::vector<std::vector<int>> chords, const char* expectKey, std::vector<juce::String> expectChords)
    {
        auto audio = render (44100.0, bpm, chords, 1, 4);
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto r = Analyzer::analyse (audio, 44100.0);
        auto ms = juce::Time::getMillisecondCounterHiRes() - t0;

        juce::String prog;
        for (size_t i = 0; i < r.chords.size() && i < 8; ++i)
            prog << r.chords[i].chord.name (r.prefersFlats()) << " ";
        std::cout << label << ": bpm " << r.bpm << "  key " << r.keys[0].name() << " " << r.keys[0].percent << "%  alts "
                  << r.keys[1].name() << " " << r.keys[1].percent << "%, " << r.keys[2].name() << " " << r.keys[2].percent
                  << "%   chords: " << prog << "  (" << (int) ms << " ms for " << (int) r.duration << " s)\n";

        if (std::abs (r.bpm - bpm) > 0.6) { ++failures; std::cout << "  FAIL bpm\n"; }
        if (r.keys[0].name() != expectKey) { ++failures; std::cout << "  FAIL key\n"; }
        for (size_t i = 0; i < expectChords.size(); ++i)
            if (i >= r.chords.size() || r.chords[i].chord.name (r.prefersFlats()) != expectChords[i]) { ++failures; std::cout << "  FAIL chord " << i << "\n"; break; }
    };

    // Am F C G (bass note first)
    run ("A minor trap 140", 140.0, { { 45, 69, 72, 76 }, { 41, 65, 69, 72 }, { 48, 67, 72, 76 }, { 43, 67, 71, 74 } }, "A MINOR", { "Am", "F", "C", "G" });
    // C G Am F at 96
    run ("C major pop 96", 96.0, { { 48, 64, 67, 72 }, { 43, 67, 71, 74 }, { 45, 69, 72, 76 }, { 41, 65, 69, 72 } }, "C MAJOR", { "C", "G", "Am", "F" });
    // Fm Db Ab Eb at 150
    run ("F minor 150", 150.0, { { 41, 68, 72, 77 }, { 37, 68, 73, 77 }, { 44, 68, 72, 75 }, { 39, 67, 70, 75 } }, "F MINOR", { "Fm", "Db", "Ab", "Eb" });

    if (argc > 1)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (juce::File (argv[1])));
        if (reader)
        {
            juce::AudioBuffer<float> b ((int) reader->numChannels, (int) reader->lengthInSamples);
            reader->read (&b, 0, b.getNumSamples(), 0, true, true);
            auto r = Analyzer::analyse (b, reader->sampleRate);
            std::cout << "file: bpm " << r.bpm << " key " << r.keys[0].name() << "\n";
        }
    }

    std::cout << (failures == 0 ? "ALL ANALYSIS TESTS PASSED\n" : "ANALYSIS TESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
