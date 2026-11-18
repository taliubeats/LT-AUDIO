// LT VOCAL SUITE, whole plugin on real recordings: every factory preset over every file.
// Checks: no NaN/Inf, no clipping, sane loudness (presets must not jump in level), no clicks
// created by the plugin, CPU. Usage: vocalsuite_real_test vocal1.wav ... [--instruments inst1.wav ...]
// Files after --instruments (drums, mixes, brass) are only checked for safety (finite, no clipping, CPU).
// LT_RENDER_DIR=/dir writes the results as WAVs.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static float rmsDb (const juce::AudioBuffer<float>& b, int start, int n)
{
    double s = 0; int count = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = start; i < start + n && i < b.getNumSamples(); ++i) { s += (double) b.getSample (c, i) * b.getSample (c, i); ++count; }
    return (float) (10.0 * std::log10 (s / juce::jmax (1, count) + 1e-20));
}

// clicks: an isolated jump = second difference far above the local (+-2 ms) HF activity of the
// same signal. Loud bright material has consistently large differences, so it does not count.
static int countClicks (const juce::AudioBuffer<float>& b, double sr)
{
    int clicks = 0;
    auto w = (int) (sr * 0.002);
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        auto* d = b.getReadPointer (c);
        auto n = b.getNumSamples();
        std::vector<float> sd ((size_t) n, 0.0f);
        for (int i = 2; i < n; ++i) sd[(size_t) i] = std::abs (d[i] - 2.0f * d[i - 1] + d[i - 2]);
        // running sum of squares for a centred window
        std::vector<double> cs ((size_t) n + 1, 0.0);
        for (int i = 0; i < n; ++i) cs[(size_t) i + 1] = cs[(size_t) i] + (double) sd[(size_t) i] * sd[(size_t) i];
        int last = -1000000;
        for (int i = w; i < n - w; ++i)
        {
            auto v = sd[(size_t) i];
            if (v < 0.03f) continue;
            auto local = (cs[(size_t) (i + w)] - cs[(size_t) (i - w)] - (double) v * v) / (2.0 * w - 1);
            if (v > 8.0 * std::sqrt (local) && i - last > w) { ++clicks; last = i; }
        }
    }
    return clicks;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    auto renderDir = juce::SystemStats::getEnvironmentVariable ("LT_RENDER_DIR", {});
    int failures = 0;
    auto check = [&] (bool ok, const juce::String& what) { if (! ok) { ++failures; std::cout << "  FAIL " << what << std::endl; } };

    bool instruments = false;
    for (int f = 1; f < argc; ++f)
    {
        if (juce::String (argv[f]) == "--instruments") { instruments = true; continue; }
        juce::File file (argv[f]);
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr) { std::cout << "cannot read " << file.getFileName() << std::endl; return 2; }
        auto sr = reader->sampleRate;
        auto len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (sr * 20));
        juce::AudioBuffer<float> input (2, len);
        reader->read (&input, 0, len, 0, true, true);
        // normalise to a typical recording level (-18 dBFS RMS)
        input.applyGain (juce::Decibels::decibelsToGain (-18.0f - rmsDb (input, 0, len)));
        if (input.getMagnitude (0, len) > 0.95f) input.applyGain (0.95f / input.getMagnitude (0, len));
        auto inDb = rmsDb (input, 0, len);
        std::cout << "\n== " << file.getFileName() << "  (" << juce::String (len / sr, 1) << " s, in " << juce::String (inDb, 1) << " dB RMS)" << std::endl;

        std::unique_ptr<juce::AudioProcessor> plugin (createPluginFilter());
        const int block = 512;
        plugin->setPlayConfigDetails (2, 2, sr, block);
        plugin->prepareToPlay (sr, block);

        for (int pi = 0; pi < plugin->getNumPrograms(); ++pi)
        {
            plugin->setCurrentProgram (pi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);   // voice changes reconfigure asynchronously
            juce::AudioBuffer<float> out (input);
            juce::MidiBuffer midi;
            auto t0 = juce::Time::getMillisecondCounterHiRes();
            for (int pos = 0; pos < len; pos += block)
            {
                auto n = juce::jmin (block, len - pos);
                juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, n);
                plugin->processBlock (view, midi);
            }
            auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
            auto cpu = 100.0 * ms / (1000.0 * len / sr);

            bool finite = true;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < len; ++i) if (! std::isfinite (out.getSample (c, i))) finite = false;
            auto peak = out.getMagnitude (0, len);
            auto outDb = rmsDb (out, (int) sr, len - (int) sr);
            auto inRef = rmsDb (input, (int) sr, len - (int) sr);
            auto clicksIn = countClicks (input, sr), clicksOut = countClicks (out, sr);
            auto name = plugin->getProgramName (pi);
            std::cout << "  " << name.paddedRight (' ', 20) << " level " << juce::String (outDb - inRef, 1).paddedLeft (' ', 5) << " dB"
                      << "  peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1).paddedLeft (' ', 5) << " dBFS"
                      << "  clicks " << clicksOut << "/" << clicksIn << "  cpu " << juce::String (cpu, 1) << "%"
                      << "  latency " << plugin->getLatencySamples() << std::endl;
            check (finite, name + " finite");
            check (peak < 1.0f, name + " no clipping");
            if (! instruments)
            check (outDb - inRef > -4.0f && outDb - inRef < 4.5f, name + " level");   // presets must not jump in level
            // RAGE clips hard on purpose (square-ish edges at every glottal pulse): reported, not judged
            if (name != "Rage Vocal" && ! instruments)
                check (clicksOut <= clicksIn * 2 + 4, name + " clicks");
            check (cpu < 25.0, name + " cpu");

            if (renderDir.isNotEmpty())
            {
                auto dest = juce::File (renderDir).getChildFile (file.getFileNameWithoutExtension() + "_" + name.replaceCharacter (' ', '_').replaceCharacter ('&', 'n') + ".wav");
                dest.deleteFile();
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (dest), sr, 2, 24, {}, 0));
                if (w) w->writeFromAudioSampleBuffer (out, 0, len);
            }
        }
    }
    std::cout << (failures == 0 ? "\nALL VOCAL SUITE REAL-AUDIO TESTS PASSED" : "\nVOCAL SUITE REAL-AUDIO TESTS FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
