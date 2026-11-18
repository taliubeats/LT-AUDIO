// LT VOCAL SUITE chain on real vocals: dynamics, de-essing, gate, bypass, stability, CPU.
#include <juce_audio_formats/juce_audio_formats.h>
#include "VocalChain.h"
#include <iostream>

using namespace vocal;

static int failures = 0;
#define CHECK(c, msg) do { if (! (c)) { ++failures; std::cout << "  FAIL: " << msg << "\n"; } else std::cout << "  ok   " << msg << "\n"; } while (0)

static juce::AudioBuffer<float> run (const juce::AudioBuffer<float>& in, double sr, const ChainSettings& s, double& cpu, int& latency)
{
    VocalChain chain;
    chain.prepare (sr, 512, s.tune.voice, s.tune.hq);
    latency = chain.getLatency();
    juce::AudioBuffer<float> out (in);
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < out.getNumSamples(); pos += 512)
    {
        auto n = juce::jmin (512, out.getNumSamples() - pos);
        juce::AudioBuffer<float> v (out.getArrayOfWritePointers(), out.getNumChannels(), pos, n);
        chain.process (v, s);
    }
    cpu = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0 / (in.getNumSamples() / sr) * 100.0;
    return out;
}

// short-term loudness spread (dB between the 90th and 10th percentile of 50 ms RMS frames),
// measured on the frames where the *reference* signal is active, so make-up gain can't sneak breaths in
static double dynamicRange (const juce::AudioBuffer<float>& b, const juce::AudioBuffer<float>& ref, double sr, int offset)
{
    auto w = (int) (sr * 0.05);
    auto frameDb = [&] (const juce::AudioBuffer<float>& x, int p) { double s = 0; for (int i = 0; i < w; ++i) s += (double) x.getSample (0, p + i) * x.getSample (0, p + i); return 10 * std::log10 (s / w + 1e-12); };
    double refMax = -200;
    for (int p = 0; p + w < ref.getNumSamples(); p += w) refMax = std::max (refMax, frameDb (ref, p));
    std::vector<double> frames;
    for (int p = 0; p + w + offset < b.getNumSamples(); p += w)
        if (frameDb (ref, p) > refMax - 30) frames.push_back (frameDb (b, p + offset));
    if (frames.size() < 10) return 0;
    std::sort (frames.begin(), frames.end());
    return frames[frames.size() * 9 / 10] - frames[frames.size() / 10];
}

static double bandEnergy (const juce::AudioBuffer<float>& b, double sr, double lo, double hi)
{
    juce::dsp::FFT fft (13);
    const int N = 8192;
    std::vector<float> buf (N * 2);
    double e = 0;
    for (int p = 0; p + N < b.getNumSamples(); p += N)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = b.getSample (0, p + i) * (0.5f - 0.5f * std::cos (6.2831853f * i / (N - 1)));
        fft.performFrequencyOnlyForwardTransform (buf.data());
        for (int k = (int) (lo * N / sr); k < (int) (hi * N / sr) && k < N / 2; ++k) e += (double) buf[(size_t) k] * buf[(size_t) k];
    }
    return e;
}

int main (int argc, char** argv)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    // de-esser on a controlled signal: a sung vowel with loud "s" bursts every 0.5 s
    {
        const double sr = 44100.0;
        juce::AudioBuffer<float> sig (2, (int) sr * 4);
        juce::Random rnd (2);
        juce::dsp::IIR::Filter<float> bp1, bp2;
        bp1.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, 7000.0, 1.2f);
        bp2.coefficients = bp1.coefficients;
        for (int i = 0; i < sig.getNumSamples(); ++i)
        {
            auto t = i / sr;
            auto vowel = 0.15f * (float) (std::sin (6.2832 * 220 * t) + 0.5 * std::sin (6.2832 * 440 * t) + 0.3 * std::sin (6.2832 * 880 * t));
            auto inS = std::fmod (t, 0.5) < 0.12;
            auto hiss = inS ? bp2.processSample (bp1.processSample (rnd.nextFloat() * 2 - 1)) * 0.9f : 0.0f;
            sig.setSample (0, i, vowel + hiss); sig.setSample (1, i, vowel + hiss);
        }
        ChainSettings off; off.tuneOn = false; off.comp = 0; off.sat.mix = 0; off.delay.mix = 0; off.reverb.mix = 0; off.gateDb = -80; off.deEss = 0;
        off.mudCut = 0; off.presence = 0; off.air = 0; off.lowCutHz = 40;
        ChainSettings on = off; on.deEss = 0.7f;
        double cpu; int lat;
        auto a = run (sig, sr, off, cpu, lat), b = run (sig, sr, on, cpu, lat);
        auto sDb = 10 * std::log10 (bandEnergy (b, sr, 5000, 10000) / bandEnergy (a, sr, 5000, 10000));
        auto vDb = 10 * std::log10 (bandEnergy (b, sr, 150, 1500) / bandEnergy (a, sr, 150, 1500));
        std::cout << "\n== synthetic sss: sibilance " << juce::String (sDb, 1) << " dB, vowel " << juce::String (vDb, 2) << " dB\n";
        CHECK (sDb < -3.0, "de-esser turns the s bursts down");
        CHECK (std::abs (vDb) < 0.3, "de-esser leaves the vowel alone");
    }

    for (int a = 1; a < argc; ++a)
    {
        juce::File f (argv[a]);
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (! r) continue;
        auto sr = r->sampleRate;
        juce::AudioBuffer<float> in (2, (int) juce::jmin ((juce::int64) (sr * 25), r->lengthInSamples));
        r->read (&in, 0, in.getNumSamples(), 0, true, true);
        std::cout << "\n== " << f.getFileNameWithoutExtension() << "\n";

        ChainSettings base;            // a typical modern vocal
        base.tune.mask = tune::Scales::mask (9, tune::Scales::minor);
        base.tune.retuneMs = 30.0f;
        base.comp = 0.55f; base.deEss = 0.5f; base.gateDb = -55.0f;
        base.sat.type = (int) nova::SatType::tube; base.sat.drive = 0.25f; base.sat.mix = 0.6f; base.sat.autoGain = true;
        base.delay.mix = 0.15f; base.delay.delaySeconds = 0.375; base.reverb.mix = 0.18f; base.reverb.decay = 1.8f;

        double cpu; int lat;
        auto out = run (in, sr, base, cpu, lat);
        bool finite = true;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < out.getNumSamples(); ++i) finite &= std::isfinite (out.getSample (c, i));
        CHECK (finite, "output is finite");
        auto peak = out.getMagnitude (0, out.getNumSamples());
        CHECK (peak < 1.6f, "peak stays sane (" + std::to_string (peak) + ")");
        std::cout << "  latency " << lat << " samples (" << juce::String (lat / sr * 1000.0, 1) << " ms), cpu " << juce::String (cpu, 1) << " %\n";
        CHECK (cpu < 20.0, "cpu under 20 % of one core");

        // compressor tightens dynamics
        ChainSettings dryish = base; dryish.delay.mix = 0; dryish.reverb.mix = 0; dryish.tuneOn = false; dryish.sat.mix = 0; dryish.gateDb = -80; dryish.deEss = 0;
        ChainSettings noComp = dryish; noComp.comp = 0;
        auto a1 = run (in, sr, noComp, cpu, lat), a2 = run (in, sr, dryish, cpu, lat);
        auto dr1 = dynamicRange (a1, in, sr, lat), dr2 = dynamicRange (a2, in, sr, lat);
        std::cout << "  dynamic range: no comp " << juce::String (dr1, 1) << " dB, comp " << juce::String (dr2, 1) << " dB\n";
        CHECK (dr2 < dr1 - 1.0 && dr2 > dr1 * 0.35, "compressor controls dynamics without flattening");

        // de-esser lowers sibilance band relative to the body
        ChainSettings noEss = noComp, ess = noComp; ess.deEss = 1.0f;
        auto b1 = run (in, sr, noEss, cpu, lat), b2 = run (in, sr, ess, cpu, lat);
        auto ratio1 = bandEnergy (b1, sr, 5000, 10000) / bandEnergy (b1, sr, 200, 2000);
        auto ratio2 = bandEnergy (b2, sr, 5000, 10000) / bandEnergy (b2, sr, 200, 2000);
        auto essDb = 10 * std::log10 (ratio2 / ratio1);
        std::cout << "  de-esser: sibilance band " << juce::String (essDb, 1) << " dB vs body\n";
        CHECK (essDb < 0.05 && essDb > -9.0, "de-esser never adds sibilance and stays musical");

        // bypass = delayed input
        ChainSettings bp = base; bp.bypass = true;
        auto c1 = run (in, sr, bp, cpu, lat);
        double err = 0;
        for (int i = lat + (int) (sr * 0.1); i < in.getNumSamples(); ++i) err = juce::jmax (err, (double) std::abs (c1.getSample (0, i) - in.getSample (0, i - lat)));
        CHECK (err < 1.0e-4, "bypass is the untouched input (err " + std::to_string (err) + ")");

        // gate: add hiss to the input and check silent parts get quieter
        auto tail = (int) (sr * 2.0);
        juce::AudioBuffer<float> noisy (2, in.getNumSamples() + tail);
        noisy.clear();
        for (int c = 0; c < 2; ++c) noisy.copyFrom (c, 0, in, c, 0, in.getNumSamples());
        juce::Random rnd (5);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < noisy.getNumSamples(); ++i) noisy.addSample (c, i, (rnd.nextFloat() * 2 - 1) * 0.0015f);
        ChainSettings g0 = noComp, g1 = noComp; g1.gateDb = -45.0f;
        auto n0 = run (noisy, sr, g0, cpu, lat), n1 = run (noisy, sr, g1, cpu, lat);
        // energy of the quietest 10 % of frames
        auto floorDb = [&] (const juce::AudioBuffer<float>& b)
        {
            // the last second of the noise-only tail
            double s = 0; auto start = b.getNumSamples() - (int) sr;
            for (int i = start; i < b.getNumSamples(); ++i) s += (double) b.getSample (0, i) * b.getSample (0, i);
            return 10 * std::log10 (s / sr + 1e-12);
        };
        auto f0 = floorDb (n0), f1 = floorDb (n1);
        std::cout << "  noise floor: gate off " << juce::String (f0, 1) << " dB, gate on " << juce::String (f1, 1) << " dB\n";
        CHECK (f1 < f0 - 6.0, "gate lowers the noise in the gaps");

        if (auto dir = juce::SystemStats::getEnvironmentVariable ("LT_RENDER_DIR", {}); dir.isNotEmpty())
        {
            auto o = juce::File (dir).getChildFile (f.getFileNameWithoutExtension() + "_VOCALSUITE.wav");
            o.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> os (o.createOutputStream());
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
            if (w) { os.release(); w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples()); }
        }
    }

    std::cout << (failures == 0 ? "\nALL VOCAL CHAIN TESTS PASSED\n" : "\nVOCAL CHAIN TESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
