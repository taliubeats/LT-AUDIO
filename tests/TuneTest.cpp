// Tests for LT TUNE: pitch detection, correction to the scale (out-of-tune sines -> tuned output),
// retune speed, transpose, mix/latency, formant preservation, stability and MATCH REFERENCE analysis.
#include "TuneEngine.h"
#include "MatchAnalysis.h"
#include <juce_dsp/juce_dsp.h>

using namespace tune;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl;
    if (! ok) ++failures;
}

static float cents (float hz, float ref) { return 1200.0f * std::log2 (hz / ref); }
static float midiToHz (float m) { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }

/** Frequency of a (near) sine from its zero crossings - independent of the YIN detector. */
static float zeroCrossingHz (const float* x, int n, double sr)
{
    double first = -1, last = -1;
    int count = 0;
    for (int i = 1; i < n; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            auto t = (i - 1) + (double) (-x[i - 1]) / (double) (x[i] - x[i - 1]);
            if (first < 0) first = t;
            last = t;
            ++count;
        }
    return count > 1 ? (float) ((count - 1) * sr / (last - first)) : 0.0f;
}

static juce::AudioBuffer<float> sine (float hz, double seconds, double sr, int channels = 1, float amp = 0.5f)
{
    juce::AudioBuffer<float> b (channels, (int) (sr * seconds));
    for (int i = 0; i < b.getNumSamples(); ++i)
        for (int c = 0; c < channels; ++c)
            b.setSample (c, i, amp * std::sin (juce::MathConstants<float>::twoPi * hz * (float) i / (float) sr));
    return b;
}

/** A "voice": pulse train through two vowel resonances (formants at f1, f2). */
static juce::AudioBuffer<float> vowel (float hz, double seconds, double sr, float f1 = 700.0f, float f2 = 1200.0f)
{
    juce::AudioBuffer<float> b (1, (int) (sr * seconds));
    double phase = 0;
    float y1[2] {}, y2[2] {};
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        phase += hz / sr;
        float x = 0.0f;
        if (phase >= 1.0) { phase -= 1.0; x = 1.0f; }
        float out = 0.0f;
        const float fs[2] = { f1, f2 };
        for (int k = 0; k < 2; ++k)
        {
            auto rr = 0.985f, w = juce::MathConstants<float>::twoPi * fs[k] / (float) sr;
            auto y = x + 2.0f * rr * std::cos (w) * y1[k] - rr * rr * y2[k];
            y2[k] = y1[k]; y1[k] = y;
            out += y * 0.02f;
        }
        b.setSample (0, i, out);
    }
    return b;
}

static juce::AudioBuffer<float> run (TuneEngine& e, const juce::AudioBuffer<float>& input, const TuneSettings& s, int block = 256)
{
    auto b = input;
    for (int start = 0; start < b.getNumSamples(); start += block)
    {
        juce::AudioBuffer<float> v (b.getArrayOfWritePointers(), b.getNumChannels(), start, juce::jmin (block, b.getNumSamples() - start));
        e.process (v, s);
    }
    return b;
}

static float outputHz (const juce::AudioBuffer<float>& b, double sr, double from, double to)
{
    auto i0 = (int) (from * sr), i1 = (int) (to * sr);
    return zeroCrossingHz (b.getReadPointer (0) + i0, i1 - i0, sr);
}

/** Pitch of a harmonic signal measured with a fresh wide-range detector. */
static float yinHz (const juce::AudioBuffer<float>& b, double sr, double at)
{
    PitchDetector d;
    d.prepare (sr, 60.0f, 1200.0f, 2.0f);
    for (int i = (int) ((at - 0.1) * sr); i < (int) (at * sr); ++i) d.push (b.getSample (0, i));
    return d.analyse().hz;
}

static TuneSettings settings (int key, int scale)
{
    TuneSettings s;
    s.mask = Scales::mask (key, scale);
    s.retuneMs = 0.0f;
    s.voice = Voices::tenor;
    return s;
}

static float centroid (const juce::AudioBuffer<float>& b, double sr, double from)
{
    constexpr int order = 13, size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> d ((size_t) size * 2, 0.0f);
    auto i0 = (int) (from * sr);
    for (int i = 0; i < size; ++i)
        d[(size_t) i] = b.getSample (0, i0 + i) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / size));
    fft.performFrequencyOnlyForwardTransform (d.data());
    double num = 0, den = 0;
    for (int k = 1; k < size / 2; ++k)
    {
        auto f = k * sr / size;
        if (f > 5000) break;
        num += f * d[(size_t) k] * d[(size_t) k];
        den += d[(size_t) k] * d[(size_t) k];
    }
    return (float) (num / den);
}

int main()
{
    constexpr double sr = 48000.0;

    std::cout << "Pitch detection" << std::endl;
    {
        struct Case { float hz; int voice; };
        for (auto c : { Case { 82.4f, Voices::bass }, Case { 110.0f, Voices::tenor }, Case { 220.0f, Voices::tenor },
                        Case { 440.0f, Voices::alto }, Case { 880.0f, Voices::soprano } })
        {
            PitchDetector d;
            d.prepare (sr, Voices::minHz (c.voice), Voices::maxHz (c.voice), 1.0f);
            auto b = sine (c.hz, 0.2, sr);
            for (int i = 0; i < b.getNumSamples(); ++i) d.push (b.getSample (0, i));
            auto r = d.analyse();
            check (r.voiced && std::abs (cents (r.hz, c.hz)) < 2.0f,
                   juce::String (c.hz) + " Hz sine detected (" + juce::String (r.hz, 2) + " Hz)");
        }
        {
            PitchDetector d;
            d.prepare (sr, 100.0f, 700.0f, 1.0f);
            auto b = vowel (155.0f, 0.2, sr);
            for (int i = 0; i < b.getNumSamples(); ++i) d.push (b.getSample (0, i));
            auto r = d.analyse();
            check (r.voiced && std::abs (cents (r.hz, 155.0f)) < 4.0f, "vowel-like pulse train at 155 Hz (" + juce::String (r.hz, 2) + " Hz)");
        }
        {
            PitchDetector d;
            d.prepare (sr, 100.0f, 700.0f, 1.0f);
            juce::Random rnd (3);
            for (int i = 0; i < 9600; ++i) d.push ((rnd.nextFloat() - 0.5f) * 0.5f);
            check (! d.analyse().voiced, "white noise is unvoiced");
            for (int i = 0; i < 9600; ++i) d.push (0.0f);
            check (! d.analyse().voiced, "silence is unvoiced");
        }
    }

    std::cout << "Correction" << std::endl;
    {
        // A4 +40 cents in A minor -> A4
        TuneEngine e;
        auto s = settings (9, Scales::minor);
        e.prepare (sr, 256, s.voice, false);
        auto in = midiToHz (69.4f);
        auto out = run (e, sine (in, 1.0, sr), s);
        auto hz = outputHz (out, sr, 0.4, 0.95);
        check (std::abs (cents (hz, 440.0f)) < 4.0f, "A4 +40 cents -> 440 Hz (" + juce::String (hz, 2) + " Hz)");
        check (e.getTargetNote() == 69, "target note is A4");
        check (std::abs (e.getInputMidi() - 69.4f) < 0.03f, "UI shows the sung pitch (+40 cents)");
    }
    {
        // 210 Hz (between G#3 and A3) chromatic, HQ, 44.1 kHz
        TuneEngine e;
        auto s = settings (0, Scales::chromatic);
        s.hq = true;
        const double sr2 = 44100.0;
        e.prepare (sr2, 512, s.voice, true);
        auto out = run (e, sine (midiToHz (56.7f), 1.0, sr2), s, 512);
        auto hz = outputHz (out, sr2, 0.4, 0.95);
        check (std::abs (cents (hz, midiToHz (57.0f))) < 4.0f, "HQ 44.1 kHz: -30 cents chromatic -> A3 (" + juce::String (hz, 2) + " Hz)");
    }
    {
        // F# +30 cents is not in A minor -> goes to G
        TuneEngine e;
        auto s = settings (9, Scales::minor);
        e.prepare (sr, 256, s.voice, false);
        auto out = run (e, sine (midiToHz (66.3f), 1.0, sr), s);
        auto hz = outputHz (out, sr, 0.4, 0.95);
        check (std::abs (cents (hz, midiToHz (67.0f))) < 4.0f, "F#4 +30 in A minor -> G4 (" + juce::String (hz, 2) + " Hz)");
    }
    {
        // custom mask: only C allowed -> D goes to C (2 semitones down)
        TuneEngine e;
        TuneSettings s;
        s.retuneMs = 0;
        s.mask = {};
        s.mask[0] = true;
        e.prepare (sr, 256, s.voice, false);
        auto out = run (e, sine (midiToHz (61.6f), 1.0, sr), s);
        auto hz = outputHz (out, sr, 0.4, 0.95);
        check (std::abs (cents (hz, midiToHz (60.0f))) < 5.0f, "EDIT NOTES: only C on -> C#+60 goes to C4 (" + juce::String (hz, 2) + " Hz)");
    }
    {
        // DETUNE reference 432 Hz
        TuneEngine e;
        auto s = settings (9, Scales::minor);
        s.refHz = 432.0f;
        e.prepare (sr, 256, s.voice, false);
        auto out = run (e, sine (445.0f, 1.0, sr), s);
        auto hz = outputHz (out, sr, 0.4, 0.95);
        check (std::abs (cents (hz, 432.0f)) < 4.0f, "DETUNE 432 Hz -> tuned to 432 (" + juce::String (hz, 2) + " Hz)");
    }

    std::cout << "Retune speed, flex, transpose" << std::endl;
    {
        TuneEngine e;
        auto s = settings (9, Scales::minor);
        s.retuneMs = 250.0f;
        e.prepare (sr, 256, s.voice, false);
        auto out = run (e, sine (midiToHz (69.4f), 2.0, sr), s);
        auto lat = e.getLatency() / sr;
        auto early = cents (outputHz (out, sr, lat + 0.05, lat + 0.10), 440.0f);
        auto late = cents (outputHz (out, sr, 1.6, 1.95), 440.0f);
        std::cout << "     early " << early << " cents, late " << late << " cents" << std::endl;
        check (early > 15.0f, "slow retune: still out of tune right after the onset");
        check (std::abs (late) < 4.0f, "slow retune: in tune after a while");
    }
    {
        TuneEngine e;
        auto s = settings (9, Scales::minor);
        s.flex = 1.0f;
        e.prepare (sr, 256, s.voice, false);
        auto out = run (e, sine (midiToHz (69.45f), 1.0, sr), s);
        auto c = cents (outputHz (out, sr, 0.4, 0.95), 440.0f);
        check (c > 35.0f, "FLEX TUNE 100: a note far from the target is left natural (" + juce::String (c, 1) + " cents)");
    }
    {
        TuneEngine e;
        auto s = settings (0, Scales::chromatic);
        s.transpose = 12.0f;
        e.prepare (sr, 256, s.voice, false);
        auto out = run (e, vowel (220.0f, 1.0, sr), s);
        auto hz = yinHz (out, sr, 0.8);
        check (std::abs (cents (hz, 440.0f)) < 5.0f, "TRANSPOSE +12: 220 -> 440 Hz (" + juce::String (hz, 2) + " Hz, engine in " + juce::String (e.getInputMidi(), 2) + " out " + juce::String (e.getOutputMidi(), 2) + ")");
    }
    {
        TuneEngine e;
        auto s = settings (0, Scales::chromatic);
        s.transpose = -7.0f;
        e.prepare (sr, 256, s.voice, false);
        auto out = run (e, vowel (330.0f, 1.0, sr), s);
        auto hz = yinHz (out, sr, 0.8);
        check (std::abs (cents (hz, 330.0f * std::pow (2.0f, -7.0f / 12.0f))) < 5.0f, "TRANSPOSE -7: 330 -> 220 Hz (" + juce::String (hz, 2) + " Hz, engine in " + juce::String (e.getInputMidi(), 2) + " out " + juce::String (e.getOutputMidi(), 2) + ")");
    }

    std::cout << "Mix, latency, level" << std::endl;
    {
        TuneEngine e;
        auto s = settings (9, Scales::minor);
        s.mix = 0.0f;
        e.prepare (sr, 256, s.voice, false);
        auto in = sine (447.0f, 0.5, sr, 2);
        auto out = run (e, in, s);
        auto L = e.getLatency();
        float err = 0.0f;
        for (int i = L; i < in.getNumSamples(); ++i)
            for (int c = 0; c < 2; ++c) err = juce::jmax (err, std::abs (out.getSample (c, i) - in.getSample (c, i - L)));
        check (err < 1.0e-5f, "MIX 0 = dry signal delayed by the reported latency (" + juce::String (L) + " samples)");
        check (L == TuneEngine::latencyFor (sr, s.voice, false), "latency matches latencyFor()");
        auto ms = 1000.0 * L / sr;
        std::cout << "     latency tenor LL " << ms << " ms, HQ " << 1000.0 * TuneEngine::latencyFor (sr, s.voice, true) / sr
                  << " ms, soprano LL " << 1000.0 * TuneEngine::latencyFor (sr, Voices::soprano, false) / sr << " ms" << std::endl;
        check (ms < 25.0, "LOW LATENCY for tenor is under 25 ms");
    }
    {
        TuneEngine e;
        auto s = settings (9, Scales::minor);
        e.prepare (sr, 256, s.voice, false);
        auto in = sine (447.0f, 1.0, sr, 1, 0.5f);
        auto out = run (e, in, s);
        auto rmsIn = in.getRMSLevel (0, 24000, 20000), rmsOut = out.getRMSLevel (0, 24000, 20000);
        check (std::abs (juce::Decibels::gainToDecibels (rmsOut / rmsIn)) < 1.0f,
               "corrected level within 1 dB of the input (" + juce::String (juce::Decibels::gainToDecibels (rmsOut / rmsIn), 2) + " dB)");
    }
    {
        // bypass passes the (delayed) input
        TuneEngine e;
        auto s = settings (9, Scales::minor);
        s.bypass = true;
        e.prepare (sr, 256, s.voice, false);
        auto in = sine (447.0f, 0.5, sr);
        auto out = run (e, in, s);
        auto hz = outputHz (out, sr, 0.2, 0.45);
        check (std::abs (hz - 447.0f) < 0.5f, "BYPASS leaves the pitch alone");
    }

    std::cout << "Formants" << std::endl;
    {
        auto in = vowel (150.0f, 1.2, sr);
        auto c0 = centroid (in, sr, 0.5);
        for (int preserve = 1; preserve >= 0; --preserve)
        {
            TuneEngine e;
            auto s = settings (0, Scales::chromatic);
            s.transpose = 5.0f;
            s.preserve = preserve == 1;
            e.prepare (sr, 256, s.voice, false);
            auto out = run (e, in, s);
            auto c = centroid (out, sr, 0.5);
            auto moved = 1200.0f * std::log2 (c / c0);
            std::cout << "     centroid in " << c0 << " Hz, out " << c << " Hz (" << moved << " cents)" << std::endl;
            if (preserve) check (std::abs (moved) < 200.0f, "PRESERVE on: formants stay put when shifting +5");
            else          check (moved > 300.0f, "PRESERVE off: formants move up with the pitch");
        }
        TuneEngine e;
        auto s = settings (0, Scales::chromatic);
        s.formant = 4.0f;
        e.prepare (sr, 256, s.voice, false);
        auto out = run (e, in, s);
        auto c = centroid (out, sr, 0.5);
        auto hz = zeroCrossingHz (out.getReadPointer (0) + 24000, 20000, sr);
        juce::ignoreUnused (hz);
        check (1200.0f * std::log2 (c / c0) > 200.0f, "FORMANT +4 moves the formants up");
    }

    std::cout << "Stability" << std::endl;
    {
        juce::Random rnd (11);
        bool finite = true;
        float maxOut = 0.0f;
        for (int round = 0; round < 24; ++round)
        {
            TuneEngine e;
            TuneSettings s;
            s.voice = rnd.nextInt (Voices::count);
            s.hq = rnd.nextBool();
            s.retuneMs = rnd.nextFloat() * 400.0f;
            s.humanize = rnd.nextFloat();
            s.flex = rnd.nextFloat();
            s.vibrato = rnd.nextFloat() * 2.0f - 1.0f;
            s.formant = rnd.nextFloat() * 24.0f - 12.0f;
            s.preserve = rnd.nextBool();
            s.transpose = (float) (rnd.nextInt (25) - 12);
            s.refHz = 430.0f + rnd.nextFloat() * 20.0f;
            s.mask = Scales::mask (rnd.nextInt (12), rnd.nextInt (Scales::count));
            const double rates[] = { 44100, 48000, 96000, 192000 };
            auto r = rates[rnd.nextInt (4)];
            e.prepare (r, 1024, s.voice, s.hq);
            juce::AudioBuffer<float> b (2, (int) (r * 0.6));
            auto kind = round % 4;
            auto f = 70.0f + rnd.nextFloat() * 900.0f;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                float x = kind == 0 ? (rnd.nextFloat() - 0.5f)
                        : kind == 1 ? 0.0f
                        : kind == 2 ? (std::fmod ((float) i * f / (float) r, 1.0f) - 0.5f)
                                    : 0.9f * std::sin ((float) i * (f + 200.0f * std::sin ((float) i / 5000.0f)) * 6.283f / (float) r);
                b.setSample (0, i, x);
                b.setSample (1, i, x * 0.7f);
            }
            int pos = 0;
            while (pos < b.getNumSamples())
            {
                auto n = juce::jmin (1 + rnd.nextInt (1024), b.getNumSamples() - pos);
                juce::AudioBuffer<float> v (b.getArrayOfWritePointers(), 2, pos, n);
                e.process (v, s);
                pos += n;
            }
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    auto y = b.getSample (c, i);
                    finite = finite && std::isfinite (y);
                    maxOut = juce::jmax (maxOut, std::abs (y));
                }
        }
        check (finite, "random settings / signals / rates / block sizes: no NaN or Inf");
        check (maxOut < 4.0f, "output stays bounded (peak " + juce::String (maxOut, 2) + ")");
    }

    std::cout << "MATCH REFERENCE" << std::endl;
    {
        // a hard-tuned melody in F# minor: notes snap instantly and are perfectly flat
        const int melody[] = { 66, 68, 69, 71, 73, 74, 76, 78, 76, 73, 69, 66, 64, 66, 69, 73, 71, 69, 68, 66 };
        const double noteSec = 0.35;
        juce::AudioBuffer<float> song (2, (int) (sr * noteSec * 20 * 2));
        song.clear();
        double phase = 0;
        juce::Random rnd (5);
        for (int rep = 0; rep < 2; ++rep)
            for (int k = 0; k < 20; ++k)
            {
                auto start = (int) ((rep * 20 + k) * noteSec * sr);
                auto f = midiToHz ((float) melody[k] - 12.0f);
                for (int i = 0; i < (int) (noteSec * sr); ++i)
                {
                    phase += f / sr;
                    auto env = juce::jmin (1.0f, (float) i / 200.0f);
                    auto v = 0.4f * env * (float) (std::sin (6.2831853 * phase) + 0.4 * std::sin (2 * 6.2831853 * phase) + 0.2 * std::sin (3 * 6.2831853 * phase));
                    auto bed = 0.02f * (rnd.nextFloat() - 0.5f);   // a little stereo noise
                    song.setSample (0, start + i, v + bed);
                    song.setSample (1, start + i, v - bed);
                }
            }
        auto r = MatchAnalysis::analyse (song, sr);
        check (r.ok, "analysis completes" + (r.ok ? juce::String() : " (" + r.error + ")"));
        check (r.key == 6 && r.scale == Scales::minor, "key & scale: F# Minor (got " + Scales::display (r.key, r.scale) + ")");
        check (r.hard && r.retuneMs <= 25.0f, "hard-tuned melody -> fast retune (" + juce::String (r.retuneMs) + " ms)");
        check (r.vibrato < 0.0f, "flat notes -> vibrato flattened");
        check (r.flex < 20.0f, "notes exactly on pitch -> low flex (" + juce::String (r.flex) + ")");
        check (! r.pitch.empty() && ! r.envelope.empty(), "pitch preview curve and waveform are filled");

        // a natural singer: glides between notes, 5.5 Hz vibrato on long notes, sits a bit off
        juce::AudioBuffer<float> nat (1, (int) (sr * 8.0));
        phase = 0;
        const int notes[] = { 60, 64, 67, 65, 64, 62, 59, 60 };   // C major
        double m = 60;
        for (int i = 0; i < nat.getNumSamples(); ++i)
        {
            auto t = i / sr;
            auto k = juce::jmin (7, (int) (t / 1.0));
            m += ((notes[k] + 0.12) - m) * (1.0 - std::exp (-1.0 / (sr * 0.06)));   // ~60 ms glide
            auto vib = 0.35 * std::sin (6.2831853 * 5.5 * t) * juce::jlimit (0.0, 1.0, (std::fmod (t, 1.0) - 0.25) / 0.2);
            auto f = 440.0 * std::pow (2.0, (m + vib - 69.0 - 12.0) / 12.0);
            phase += f / sr;
            nat.setSample (0, i, (float) (0.4 * std::sin (6.2831853 * phase) + 0.15 * std::sin (2 * 6.2831853 * phase)));
        }
        auto n2 = MatchAnalysis::analyse (nat, sr);
        check (n2.ok, "natural voice analysed");
        check (n2.key == 0 && n2.scale == Scales::major, "key & scale: C Major (got " + Scales::display (n2.key, n2.scale) + ")");
        check (! n2.hard && n2.retuneMs > 25.0f, "gliding voice with vibrato -> soft retune (" + juce::String (n2.retuneMs) + " ms)");
        check (n2.vibrato >= 0.0f, "vibrato kept (depth " + juce::String (n2.vibratoDepthCents, 1) + " cents)");

        juce::AudioBuffer<float> silence (2, (int) (sr * 3));
        silence.clear();
        check (! MatchAnalysis::analyse (silence, sr).ok, "silence -> clear error instead of a result");
    }

    std::cout << (failures == 0 ? "All TUNE tests passed." : juce::String (failures) + " test(s) failed.") << std::endl;
    return failures == 0 ? 0 : 1;
}
