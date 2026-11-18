// Runs real recordings through LT NOVA and checks the things a professional saturator must get right.
// Usage: nova_real_test file1.wav file2.wav ...   (optional env LT_RENDER_DIR=/dir to write the results)
#include <juce_audio_formats/juce_audio_formats.h>
#include "NovaEngine.h"
#include <iostream>

using namespace nova;

static double rms (const juce::AudioBuffer<float>& b, int start = 0)
{
    double s = 0; long n = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = start; i < b.getNumSamples(); ++i) { s += (double) b.getSample (c, i) * b.getSample (c, i); ++n; }
    return std::sqrt (s / juce::jmax (1L, n));
}

static float maxStep (const juce::AudioBuffer<float>& b, int start = 0)
{
    float m = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = start + 1; i < b.getNumSamples(); ++i) m = juce::jmax (m, std::abs (b.getSample (c, i) - b.getSample (c, i - 1)));
    return m;
}

static juce::AudioBuffer<float> render (const juce::AudioBuffer<float>& in, double sr, const NovaSettings& s, double& seconds)
{
    NovaEngine e;
    const int block = 512;
    e.prepare (sr, block, in.getNumChannels());
    juce::AudioBuffer<float> out (in);
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < out.getNumSamples(); pos += block)
    {
        auto n = juce::jmin (block, out.getNumSamples() - pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), out.getNumChannels(), pos, n);
        e.process (view, s);
    }
    seconds = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
    return out;
}

int main (int argc, char** argv)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    int failures = 0;
    auto renderDir = juce::SystemStats::getEnvironmentVariable ("LT_RENDER_DIR", {});

    for (int a = 1; a < argc; ++a)
    {
        juce::File f (argv[a]);
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (! r) { std::cout << "cannot read " << f.getFileName() << "\n"; ++failures; continue; }
        juce::AudioBuffer<float> in ((int) r->numChannels, (int) juce::jmin ((juce::int64) r->sampleRate * 30, r->lengthInSamples));
        r->read (&in, 0, in.getNumSamples(), 0, true, true);
        auto sr = r->sampleRate;
        auto inRms = rms (in);
        auto inStep = maxStep (in);
        std::cout << "\n== " << f.getFileNameWithoutExtension() << "  (" << juce::String (in.getNumSamples() / sr, 1) << " s, "
                  << juce::String (juce::Decibels::gainToDecibels ((float) inRms), 1) << " dBFS RMS)\n";

        // 1) MIX 0 must be the input delayed by exactly the reported latency
        {
            NovaSettings s; s.drive = 1.0f; s.mix = 0.0f; s.autoGain = false;
            double secs; auto out = render (in, sr, s, secs);
            NovaEngine probe; probe.prepare (sr, 512, in.getNumChannels());
            auto lat = probe.getLatencySamples();
            double err = 0;
            for (int i = lat; i < in.getNumSamples(); ++i)
                err = juce::jmax (err, (double) std::abs (out.getSample (0, i) - in.getSample (0, i - lat)));
            std::cout << "  mix 0: max error vs delayed input " << err << " (latency " << lat << ")\n";
            if (err > 1.0e-4) { ++failures; std::cout << "  FAIL: mix 0 is not transparent\n"; }
        }

        // 2) every type at three drive levels
        for (int t = 0; t < (int) SatType::count; ++t)
        {
            juce::String line = "  " + SatTypes::names()[t].paddedRight (' ', 12);
            for (auto drive : { 0.3f, 0.6f, 1.0f })
            {
                NovaSettings s; s.type = t; s.drive = drive; s.autoGain = true; s.character = 0.4f; s.warmth = 0.3f; s.air = 0.2f;
                double secs; auto out = render (in, sr, s, secs);
                auto settle = (int) (sr * 1.0);                       // auto gain settles in ~1 s
                bool finite = true;
                for (int c = 0; c < out.getNumChannels() && finite; ++c)
                    for (int i = 0; i < out.getNumSamples(); ++i) if (! std::isfinite (out.getSample (c, i))) { finite = false; break; }
                auto outRms = rms (out, settle), refRms = rms (in, settle);
                auto levelDb = 20.0 * std::log10 (outRms / juce::jmax (1.0e-9, refRms));
                auto peak = out.getMagnitude (0, out.getNumSamples());
                auto step = maxStep (out, settle);
                auto cpu = secs / (in.getNumSamples() / sr) * 100.0;
                line << "  d" << juce::roundToInt (drive * 100) << ": " << juce::String (levelDb, 1) << " dB pk " << juce::String (peak, 2)
                     << " cpu " << juce::String (cpu, 2) << "%";

                if (! finite)                     { ++failures; line << " [NaN]"; }
                if (std::abs (levelDb) > 2.0)     { ++failures; line << " [LEVEL]"; }
                if (peak > 4.0f)                  { ++failures; line << " [PEAK]"; }
                if (step > inStep * 6.0f + 0.5f)  { ++failures; line << " [CLICK]"; }
                if (cpu > 5.0)                    { ++failures; line << " [CPU]"; }

                if (renderDir.isNotEmpty() && drive == 0.6f)
                {
                    auto outFile = juce::File (renderDir).getChildFile (f.getFileNameWithoutExtension() + "_" + SatTypes::names()[t] + ".wav");
                    outFile.deleteFile();
                    juce::WavAudioFormat wav;
                    std::unique_ptr<juce::OutputStream> os (outFile.createOutputStream());
                    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned) out.getNumChannels(), 24, {}, 0));
                    if (w) { os.release(); w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples()); }
                }
            }
            std::cout << line << "\n";
        }
    }

    // 3) aliasing: a 9 kHz sine through hard drive must not create strong tones below 9 kHz
    {
        const double sr = 44100.0;
        juce::AudioBuffer<float> in (1, (int) sr * 2);
        for (int i = 0; i < in.getNumSamples(); ++i) in.setSample (0, i, 0.5f * (float) std::sin (2 * juce::MathConstants<double>::pi * 9000.0 * i / sr));
        for (auto t : { (int) SatType::transistor, (int) SatType::diode })
        {
            NovaSettings s; s.type = t; s.drive = 1.0f; s.autoGain = false; s.tone = 0; s.air = 0; s.warmth = 0;
            double secs; auto out = render (in, sr, s, secs);
            // energy of the folded 3rd harmonic (27 kHz -> 44.1 - 27 = 17.1 kHz) and 5th (45 kHz -> 0.9 kHz)
            auto tone = [&] (double fHz)
            {
                double re = 0, im = 0; int n = 32768, off = out.getNumSamples() - n;
                for (int i = 0; i < n; ++i)
                {
                    auto w = 0.5 - 0.5 * std::cos (2 * juce::MathConstants<double>::pi * i / (n - 1));
                    auto x = out.getSample (0, off + i) * w;
                    re += x * std::cos (2 * juce::MathConstants<double>::pi * fHz * i / sr);
                    im -= x * std::sin (2 * juce::MathConstants<double>::pi * fHz * i / sr);
                }
                return std::sqrt (re * re + im * im);
            };
            auto fund = tone (9000.0), alias5 = tone (900.0), alias3 = tone (17100.0);
            auto worst = 20.0 * std::log10 (juce::jmax (alias5, alias3) / fund);
            std::cout << "\naliasing " << SatTypes::names()[t] << ": worst folded tone " << juce::String (worst, 1) << " dB below the fundamental\n";
            if (worst > -60.0) { ++failures; std::cout << "  FAIL: audible aliasing\n"; }
        }
    }

    std::cout << (failures == 0 ? "\nALL NOVA REAL-AUDIO TESTS PASSED\n" : "\nNOVA REAL-AUDIO TESTS FAILED (" + std::to_string (failures) + ")\n");
    return failures == 0 ? 0 : 1;
}
