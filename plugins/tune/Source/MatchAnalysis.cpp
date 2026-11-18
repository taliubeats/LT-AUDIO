#include "MatchAnalysis.h"
#include "PitchDetector.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace tune
{

namespace
{
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (double sr, double fc, double q, bool high)
        {
            auto w = juce::MathConstants<double>::twoPi * fc / sr;
            auto cw = std::cos (w), alpha = std::sin (w) / (2.0 * q), a0 = 1.0 + alpha;
            b0 = (high ? (1.0 + cw) * 0.5 : (1.0 - cw) * 0.5) / a0;
            b1 = (high ? -(1.0 + cw) : (1.0 - cw)) / a0;
            b2 = b0;
            a1 = -2.0 * cw / a0;
            a2 = (1.0 - alpha) / a0;
        }
        float process (float x)
        {
            auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
    };

    float median (std::vector<float> v)
    {
        if (v.empty()) return 0.0f;
        auto mid = v.begin() + (std::ptrdiff_t) (v.size() / 2);
        std::nth_element (v.begin(), mid, v.end());
        return *mid;
    }

    float stdev (const float* x, int n)
    {
        if (n < 2) return 0.0f;
        double m = 0, s = 0;
        for (int i = 0; i < n; ++i) m += x[i];
        m /= n;
        for (int i = 0; i < n; ++i) s += (x[i] - m) * (x[i] - m);
        return (float) std::sqrt (s / (n - 1));
    }

    struct Note { int start, end; float pitch; };   // hop indices [start, end)
}

float MatchAnalysis::estimateKey (const std::array<double, 12>& h, int& key, int& scale)
{
    static constexpr double majorP[] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static constexpr double minorP[] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    auto corr = [&] (const double* prof, int k)
    {
        double mh = 0, mp = 0;
        for (int i = 0; i < 12; ++i) { mh += h[(size_t) i]; mp += prof[i]; }
        mh /= 12; mp /= 12;
        double num = 0, dh = 0, dp = 0;
        for (int i = 0; i < 12; ++i)
        {
            auto a = h[(size_t) ((i + k) % 12)] - mh, b = prof[i] - mp;
            num += a * b; dh += a * a; dp += b * b;
        }
        return dh > 0 && dp > 0 ? num / std::sqrt (dh * dp) : 0.0;
    };

    double best = -2, second = -2;
    for (int k = 0; k < 12; ++k)
        for (int m = 0; m < 2; ++m)
        {
            auto c = corr (m == 0 ? majorP : minorP, k);
            if (c > best) { second = best; best = c; key = k; scale = m == 0 ? 1 : 2; }
            else if (c > second) second = c;
        }
    return (float) juce::jlimit (0.0, 1.0, 0.5 * best + 2.0 * (best - second));
}

MatchResult MatchAnalysis::analyseFile (const juce::File& file, std::function<bool()> shouldExit)
{
    MatchResult r;
    r.fileName = file.getFileName();
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
    {
        r.error = "Can't read this file (use WAV, AIFF, FLAC, OGG or MP3).";
        return r;
    }
    auto len = (int) std::min (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 360.0));
    if (len < (int) (reader->sampleRate * 1.0))
    {
        r.error = "The file is too short.";
        return r;
    }
    juce::AudioBuffer<float> buf ((int) juce::jlimit (1u, 2u, reader->numChannels), len);
    reader->read (&buf, 0, len, 0, true, buf.getNumChannels() > 1);
    auto out = analyse (buf, reader->sampleRate, shouldExit);
    out.fileName = file.getFileName();
    return out;
}

MatchResult MatchAnalysis::analyse (const juce::AudioBuffer<float>& audio, double sr, std::function<bool()> shouldExit)
{
    MatchResult r;
    const int n = audio.getNumSamples();
    if (n < (int) sr || sr < 8000.0)
    {
        r.error = "The file is too short.";
        return r;
    }
    r.seconds = n / sr;

    // ---- 1. "isolate" the voice: centre channel, voice band
    std::vector<float> voice ((size_t) n);
    {
        Biquad hp1, hp2, lp1, lp2;
        hp1.set (sr, 120.0, 0.5412, true); hp2.set (sr, 120.0, 1.3066, true);
        lp1.set (sr, 4500.0, 0.5412, false); lp2.set (sr, 4500.0, 1.3066, false);
        const int chans = audio.getNumChannels();
        for (int i = 0; i < n; ++i)
        {
            float m = 0.0f;
            for (int c = 0; c < chans; ++c) m += audio.getSample (c, i);
            m /= (float) chans;
            voice[(size_t) i] = lp2.process (lp1.process (hp2.process (hp1.process (m))));
        }
    }
    float peak = 0.0f;
    for (auto v : voice) peak = juce::jmax (peak, std::abs (v));
    if (peak < 1.0e-4f)
    {
        r.error = "The file is silent.";
        return r;
    }

    // ---- 2. pitch curve
    PitchDetector det;
    det.prepare (sr, 70.0f, 1100.0f, 1.5f);
    det.threshold = 0.12f;
    det.gateDb = juce::Decibels::gainToDecibels (peak) - 40.0f;
    const int hop = juce::jmax (1, (int) std::round (sr * MatchResult::hopSeconds));
    const int hops = n / hop;
    r.pitch.assign ((size_t) hops, 0.0f);
    r.envelope.assign ((size_t) hops, 0.0f);
    const auto latencyHops = juce::roundToInt (det.getWindowSamples() * 0.5 / hop);

    for (int h = 0; h < hops; ++h)
    {
        if (shouldExit && (h & 255) == 0 && shouldExit()) { r.error = "Cancelled."; return r; }
        float pk = 0.0f;
        for (int i = h * hop; i < (h + 1) * hop; ++i)
        {
            det.push (voice[(size_t) i]);
            for (int c = 0; c < audio.getNumChannels(); ++c) pk = juce::jmax (pk, std::abs (audio.getSample (c, i)));
        }
        r.envelope[(size_t) h] = pk;
        auto res = det.analyse();
        auto at = h - latencyHops;   // the window is centred behind "now"
        if (res.voiced && res.clarity > 0.75f && at >= 0)
            r.pitch[(size_t) at] = 69.0f + 12.0f * std::log2 (res.hz / 440.0f);
    }
    float envMax = 0.0f;
    for (auto e : r.envelope) envMax = juce::jmax (envMax, e);
    if (envMax > 0.0f) for (auto& e : r.envelope) e /= envMax;

    // remove isolated blips and octave spikes (median of 5 over voiced frames)
    {
        auto p = r.pitch;
        for (int i = 2; i + 2 < hops; ++i)
        {
            if (p[(size_t) i] <= 0.0f) continue;
            int voicedNear = 0;
            std::vector<float> w;
            for (int k = -2; k <= 2; ++k) if (p[(size_t) (i + k)] > 0.0f) { ++voicedNear; w.push_back (p[(size_t) (i + k)]); }
            if (voicedNear < 3) { r.pitch[(size_t) i] = 0.0f; continue; }
            auto med = median (w);
            if (std::abs (p[(size_t) i] - med) > 2.5f) r.pitch[(size_t) i] = med;
        }
    }

    int voicedCount = 0;
    std::array<double, 12> hist {};
    double devSum = 0.0;
    for (auto m : r.pitch)
        if (m > 0.0f)
        {
            ++voicedCount;
            auto nearest = std::round (m);
            hist[(size_t) (((int) nearest % 12 + 12) % 12)] += 1.0;
            devSum += std::abs (m - nearest) * 100.0;
        }
    r.voicedFraction = (float) voicedCount / (float) juce::jmax (1, hops);
    if (voicedCount < (int) (1.5 / MatchResult::hopSeconds))
    {
        r.error = "No clear voice found. Try a vocal stem (acapella).";
        return r;
    }

    // ---- 3. key & scale
    r.keyConfidence = estimateKey (hist, r.key, r.scale);

    // ---- 4. notes: stable runs of pitch
    std::vector<Note> notes;
    {
        int i = 0;
        while (i < hops)
        {
            if (r.pitch[(size_t) i] <= 0.0f) { ++i; continue; }
            int j = i;
            float ref = r.pitch[(size_t) i];
            while (j < hops && r.pitch[(size_t) j] > 0.0f && std::abs (r.pitch[(size_t) j] - ref) < 0.6f)
            {
                ref += (r.pitch[(size_t) j] - ref) * 0.2f;
                ++j;
            }
            if ((j - i) * MatchResult::hopSeconds >= 0.06)
            {
                std::vector<float> seg (r.pitch.begin() + i, r.pitch.begin() + j);
                notes.push_back ({ i, j, median (seg) });
            }
            i = juce::jmax (j, i + 1);
        }
    }

    // ---- 5. retune speed: how long the pitch takes to move between two connected notes
    std::vector<float> transitions;
    for (size_t k = 1; k < notes.size(); ++k)
    {
        auto& a = notes[k - 1];
        auto& b = notes[k];
        auto jump = b.pitch - a.pitch;
        if (std::abs (jump) < 0.8f || b.start - a.end > (int) (0.03 / MatchResult::hopSeconds)) continue;
        bool connected = true;
        for (int i = a.end; i < b.start; ++i) connected = connected && r.pitch[(size_t) i] > 0.0f;
        if (! connected) continue;

        // 15% -> 85% of the jump, searched around the boundary
        const int from = juce::jmax (a.start, a.end - (int) (0.15 / MatchResult::hopSeconds));
        const int to = juce::jmin (b.end, b.start + (int) (0.15 / MatchResult::hopSeconds));
        int t15 = -1, t85 = -1;
        for (int i = from; i < to; ++i)
        {
            auto f = (r.pitch[(size_t) i] - a.pitch) / jump;
            if (t15 < 0 && f >= 0.15f) t15 = i;
            if (t85 < 0 && f >= 0.85f) { t85 = i; break; }
        }
        if (t15 >= 0 && t85 >= t15) transitions.push_back ((float) ((t85 - t15 + 1) * MatchResult::hopSeconds));
    }

    // ---- 6. how steady the long notes are (vibrato removed with a 150 ms moving average)
    std::vector<float> sdLong, sdShort, vibDepth;
    int vibratoNotes = 0, longNotes = 0;
    for (auto& nt : notes)
    {
        const int len = nt.end - nt.start;
        const auto secs = len * MatchResult::hopSeconds;
        std::vector<float> cents ((size_t) len);
        for (int i = 0; i < len; ++i) cents[(size_t) i] = (r.pitch[(size_t) (nt.start + i)] - nt.pitch) * 100.0f;

        if (secs < 0.25)
        {
            sdShort.push_back (stdev (cents.data(), len));
            continue;
        }
        ++longNotes;
        const int win = juce::jmax (3, (int) (0.15 / MatchResult::hopSeconds));
        std::vector<float> trendV ((size_t) len), detr ((size_t) len);
        for (int i = 0; i < len; ++i)
        {
            double s = 0; int c = 0;
            for (int k = juce::jmax (0, i - win / 2); k <= juce::jmin (len - 1, i + win / 2); ++k) { s += cents[(size_t) k]; ++c; }
            trendV[(size_t) i] = (float) (s / c);
            detr[(size_t) i] = cents[(size_t) i] - trendV[(size_t) i];
        }
        sdLong.push_back (stdev (trendV.data(), len));

        // vibrato: periodic 4..8 Hz movement
        const int lagLo = (int) (1.0 / (8.0 * MatchResult::hopSeconds)), lagHi = (int) (1.0 / (4.0 * MatchResult::hopSeconds));
        double e0 = 0;
        for (auto v : detr) e0 += (double) v * v;
        double bestC = 0;
        for (int lag = lagLo; lag <= lagHi && lag < len - 4; ++lag)
        {
            double c = 0;
            for (int i = 0; i + lag < len; ++i) c += (double) detr[(size_t) i] * detr[(size_t) (i + lag)];
            if (e0 > 0) bestC = juce::jmax (bestC, c / e0 * (double) len / (double) (len - lag));
        }
        auto depth = stdev (detr.data(), len) * 1.414f;
        vibDepth.push_back (depth);
        if (bestC > 0.35 && depth > 12.0f) ++vibratoNotes;
    }

    auto steadyCents = sdLong.empty() ? median (sdShort) : median (sdLong);
    auto meanDev = (float) (devSum / voicedCount);

    auto t = transitions.empty() ? 0.06f : median (transitions);
    r.retuneMs = juce::jlimit (0.0f, 400.0f, (t / 1.73f - 0.008f) * 1000.0f);
    if (meanDev < 7.0f && steadyCents < 6.0f) r.retuneMs = juce::jmin (r.retuneMs, 12.0f);   // snapped & flat = hard tune
    r.retuneMs = std::round (r.retuneMs);
    r.hard = r.retuneMs <= 25.0f;

    // FLEX: how far the notes are allowed to sit from the exact pitch
    r.flex = std::round (juce::jlimit (0.0f, 100.0f, (meanDev - 6.0f) / 20.0f * 100.0f));
    // HUMANIZE: long notes drift more than short ones
    auto sdS = sdShort.empty() ? steadyCents : median (sdShort);
    r.humanize = std::round (juce::jlimit (0.0f, 100.0f, (steadyCents - sdS * 0.6f) / 12.0f * 100.0f));
    if (r.hard) r.humanize = juce::jmin (r.humanize, 15.0f);

    // VIBRATO: kept when most long notes wobble, flattened when they are flat
    r.vibratoDepthCents = vibDepth.empty() ? 0.0f : median (vibDepth);
    if (longNotes == 0) r.vibrato = 0.0f;
    else if (vibratoNotes * 2 >= longNotes) r.vibrato = 0.0f;
    else r.vibrato = r.vibratoDepthCents < 6.0f ? -100.0f : -60.0f;

    // ---- 7. formant: spectral centroid of voiced frames compared with a typical voice
    {
        constexpr int order = 11, size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) size * 2);
        std::vector<float> centroids;
        const int step = (int) (0.05 * sr);
        for (int pos = 0; pos + size < n && centroids.size() < 400; pos += step)
        {
            auto h = (pos + size / 2) / hop;
            if (h >= hops || r.pitch[(size_t) h] <= 0.0f) continue;
            std::fill (data.begin(), data.end(), 0.0f);
            for (int i = 0; i < size; ++i)
                data[(size_t) i] = voice[(size_t) (pos + i)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size));
            fft.performFrequencyOnlyForwardTransform (data.data());
            double num = 0, den = 0;
            for (int b = (int) (300.0 * size / sr); b < (int) (4000.0 * size / sr); ++b)
            {
                auto f = b * sr / size;
                num += f * data[(size_t) b];
                den += data[(size_t) b];
            }
            if (den > 0) centroids.push_back ((float) (num / den));
        }
        if (! centroids.empty())
        {
            auto c = median (centroids);
            auto st = 12.0f * std::log2 (c / 1650.0f) * 0.25f;
            r.formant = std::round (juce::jlimit (-2.0f, 2.0f, st) * 2.0f) / 2.0f;
        }
    }

    r.ok = true;
    return r;
}

} // namespace tune
