#include "CloneEngine.h"
#include "Classifier.h"
#include <juce_dsp/juce_dsp.h>
#include <numeric>
#include <set>
#include <map>

namespace eclipse
{

namespace
{
constexpr int fftOrder = 10, fftSize = 1 << fftOrder, hop = 256, nBins = fftSize / 2 + 1;
constexpr int nPrint = 24;                               // fingerprint bands

bool cancelled (const std::atomic<bool>* c) { return c != nullptr && c->load(); }

const std::vector<float>& hann()
{
    static const std::vector<float> w = []
    {
        std::vector<float> v (fftSize);
        for (int i = 0; i < fftSize; ++i) v[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);
        return v;
    }();
    return w;
}

std::vector<float> toMono (const juce::AudioBuffer<float>& b)
{
    std::vector<float> m ((size_t) b.getNumSamples(), 0.0f);
    const float g = 1.0f / (float) juce::jmax (1, b.getNumChannels());
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        auto* x = b.getReadPointer (c);
        for (int i = 0; i < b.getNumSamples(); ++i) m[(size_t) i] += x[i] * g;
    }
    return m;
}

/** Magnitude spectrogram, frame f centred on sample f * hop. */
struct Spectro
{
    int frames = 0;
    std::vector<float> m;
    float* row (int f)             { return m.data() + (size_t) f * nBins; }
    const float* row (int f) const { return m.data() + (size_t) f * nBins; }
};

Spectro magnitude (const std::vector<float>& x)
{
    Spectro s;
    s.frames = (int) (x.size() / hop) + 1;
    s.m.assign ((size_t) s.frames * nBins, 0.0f);
    juce::dsp::FFT fft (fftOrder);
    std::vector<float> buf (2 * fftSize);
    const auto& w = hann();
    for (int f = 0; f < s.frames; ++f)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        const juce::int64 start = (juce::int64) f * hop - fftSize / 2;
        for (int i = 0; i < fftSize; ++i)
        {
            auto k = start + i;
            if (k >= 0 && k < (juce::int64) x.size()) buf[(size_t) i] = x[(size_t) k] * w[(size_t) i];
        }
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        std::copy (buf.begin(), buf.begin() + nBins, s.row (f));
    }
    return s;
}

/** Percussive mask (0..1) per bin: median across frequency vs median across time. */
std::vector<float> percussiveMask (const Spectro& s, const std::atomic<bool>* cancel)
{
    const int F = s.frames;
    std::vector<float> H ((size_t) F * nBins);
    constexpr int tw = 11, fw = 8;                        // half widths: 23 frames (~130 ms), 17 bins
    std::vector<float> series ((size_t) F), win;
    win.reserve (2 * tw + 1);
    for (int b = 0; b < nBins; ++b)
    {
        if (cancelled (cancel)) return {};
        for (int f = 0; f < F; ++f) series[(size_t) f] = s.row (f)[b];
        for (int f = 0; f < F; ++f)
        {
            win.clear();
            for (int k = juce::jmax (0, f - tw); k <= juce::jmin (F - 1, f + tw); ++k) win.push_back (series[(size_t) k]);
            std::nth_element (win.begin(), win.begin() + (long) win.size() / 2, win.end());
            H[(size_t) f * nBins + (size_t) b] = win[win.size() / 2];
        }
    }
    std::vector<float> p ((size_t) nBins);
    for (int f = 0; f < F; ++f)
    {
        const float* r = s.row (f);
        for (int b = 0; b < nBins; ++b)
        {
            win.clear();
            for (int k = juce::jmax (0, b - fw); k <= juce::jmin (nBins - 1, b + fw); ++k) win.push_back (r[k]);
            std::nth_element (win.begin(), win.begin() + (long) win.size() / 2, win.end());
            p[(size_t) b] = win[win.size() / 2];
        }
        float* h = H.data() + (size_t) f * nBins;
        for (int b = 0; b < nBins; ++b)
        {
            const float pp = p[(size_t) b] * p[(size_t) b], hh = h[b] * h[b];
            h[b] = pp / (pp + hh + 1.0e-12f);
        }
    }
    return H;
}

/** Applies a mask (frames x bins) to one channel with a complex STFT and overlap-add. */
void applyMask (const float* x, float* y, int n, const std::vector<float>& mask, int frames)
{
    juce::dsp::FFT fft (fftOrder);
    std::vector<std::complex<float>> in ((size_t) fftSize), out ((size_t) fftSize);
    const auto& w = hann();
    std::fill (y, y + n, 0.0f);
    for (int f = 0; f < frames; ++f)
    {
        const juce::int64 start = (juce::int64) f * hop - fftSize / 2;
        for (int i = 0; i < fftSize; ++i)
        {
            auto k = start + i;
            in[(size_t) i] = { (k >= 0 && k < n) ? x[k] * w[(size_t) i] : 0.0f, 0.0f };
        }
        fft.perform (in.data(), out.data(), false);
        const float* m = mask.data() + (size_t) f * nBins;
        for (int b = 0; b < nBins; ++b)
        {
            out[(size_t) b] *= m[b];
            if (b > 0 && b < fftSize / 2) out[(size_t) (fftSize - b)] *= m[b];
        }
        fft.perform (out.data(), in.data(), true);
        for (int i = 0; i < fftSize; ++i)
        {
            auto k = start + i;
            if (k >= 0 && k < n) y[k] += in[(size_t) i].real() * w[(size_t) i] / 1.5f;   // Hann^2 at hop N/4 sums to 1.5
        }
    }
}

int binOf (double hz, double sr) { return juce::jlimit (1, nBins - 1, (int) std::round (hz * fftSize / sr)); }

float percentile (std::vector<float> v, float q)
{
    if (v.empty()) return 0;
    auto k = (size_t) juce::jlimit<double> (0, (double) v.size() - 1, q * (double) (v.size() - 1));
    std::nth_element (v.begin(), v.begin() + (long) k, v.end());
    return v[k];
}

enum Detector { dLow = 0, dMid, dHigh };

struct Cand
{
    juce::int64 pos = 0;
    int frame = 0;
    Detector det = dLow;
    float strength = 0;
    Family family = Family::perc;
    std::array<float, nPrint> print {};
    float decayMs = 0;
    bool keep = true;
    std::array<float, numFamilies> prob {};   // v2: the network's answer
};

/** Length of a one-shot of a family, in ms (min, max). */
juce::Range<float> lengthRange (Family f)
{
    switch (f)
    {
        case Family::kick:    return { 180, 900 };
        case Family::snare:   return { 160, 480 };
        case Family::clap:    return { 160, 480 };
        case Family::hat:     return { 70, 220 };
        case Family::openHat: return { 200, 750 };
        case Family::tom:     return { 200, 700 };
        case Family::perc:    return { 90, 380 };
        case Family::cymbal:  return { 600, 2200 };
        default:              return { 100, 1000 };
    }
}

int maxGroups (Family f)
{
    switch (f)
    {
        case Family::kick: return 4;   case Family::snare: return 3; case Family::clap: return 3; case Family::hat: return 4;
        case Family::openHat: return 2; case Family::tom: return 3;  case Family::perc: return 4; case Family::cymbal: return 2;
        default: return 2;
    }
}

float similarity (const std::array<float, nPrint>& a, const std::array<float, nPrint>& b, int lo = 0, int hi = nPrint)
{
    double ma = 0, mb = 0;
    for (int i = lo; i < hi; ++i) { ma += a[(size_t) i]; mb += b[(size_t) i]; }
    ma /= (hi - lo); mb /= (hi - lo);
    double ab = 0, aa = 0, bb = 0;
    for (int i = lo; i < hi; ++i)
    {
        const double x = a[(size_t) i] - ma, y = b[(size_t) i] - mb;
        ab += x * y; aa += x * x; bb += y * y;
    }
    return (float) (ab / std::sqrt (aa * bb + 1.0e-12));
}

/** Simple one-pole filters for the sample-accurate onset search. */
std::vector<float> bandEnvelope (const std::vector<float>& x, juce::int64 from, juce::int64 to, Detector d, double sr)
{
    from = juce::jmax<juce::int64> (0, from); to = juce::jmin<juce::int64> ((juce::int64) x.size(), to);
    std::vector<float> e ((size_t) juce::jmax<juce::int64> (0, to - from));
    const float aLp = std::exp (-juce::MathConstants<float>::twoPi * 160.0f / (float) sr);
    const float aHp = std::exp (-juce::MathConstants<float>::twoPi * (d == dHigh ? 5000.0f : 300.0f) / (float) sr);
    const float aEnv = std::exp (-1.0f / (0.0015f * (float) sr));
    float lp = 0, hpLp = 0, env = 0;
    for (juce::int64 i = from; i < to; ++i)
    {
        const float s = x[(size_t) i];
        lp = s + aLp * (lp - s);
        hpLp = s + aHp * (hpLp - s);
        const float v = d == dLow ? lp : s - hpLp;
        const float a = std::abs (v);
        env = a > env ? a : aEnv * env + (1 - aEnv) * a;
        e[(size_t) (i - from)] = env;
    }
    return e;
}

/** Band-limited signal, squared (for the onset search). */
std::vector<float> bandSquared (const std::vector<float>& x, juce::int64 from, juce::int64 to, Detector d, double sr)
{
    from = juce::jmax<juce::int64> (0, from); to = juce::jmin<juce::int64> ((juce::int64) x.size(), to);
    std::vector<float> e ((size_t) juce::jmax<juce::int64> (0, to - from));
    const float aLp = std::exp (-juce::MathConstants<float>::twoPi * 160.0f / (float) sr);
    const float aHp = std::exp (-juce::MathConstants<float>::twoPi * (d == dHigh ? 5000.0f : 300.0f) / (float) sr);
    float lp = 0, lp2 = 0, hpLp = 0;
    // warm the filters up a little before the window
    for (juce::int64 i = juce::jmax<juce::int64> (0, from - 512); i < to; ++i)
    {
        const float s = x[(size_t) i];
        lp = s + aLp * (lp - s); lp2 = lp + aLp * (lp2 - lp);
        hpLp = s + aHp * (hpLp - s);
        const float v = d == dLow ? lp2 : s - hpLp;
        if (i >= from) e[(size_t) (i - from)] = v * v;
    }
    return e;
}

/** Number of separate bursts in the first 45 ms (claps have 2-4). */
int countFlams (const std::vector<float>& x, juce::int64 pos, double sr)
{
    auto e = bandEnvelope (x, pos, pos + (juce::int64) (0.045 * sr), dMid, sr);
    if (e.empty()) return 0;
    const float mx = *std::max_element (e.begin(), e.end());
    if (mx <= 0) return 0;
    int n = 0; bool above = false; juce::int64 last = -100000;
    const auto minGap = (juce::int64) (0.004 * sr);
    for (size_t i = 0; i < e.size(); ++i)
    {
        if (! above && e[i] > 0.55f * mx && (juce::int64) i - last > minGap) { ++n; above = true; last = (juce::int64) i; }
        else if (above && e[i] < 0.3f * mx) above = false;
    }
    return n;
}

// ------------------------------------------------------------------ short STFT for CLEAN
struct Frames { int count = 0; std::vector<std::complex<float>> d; };

Frames stft (const float* x, int n)
{
    Frames fr;
    fr.count = n / hop + 1;
    fr.d.resize ((size_t) fr.count * fftSize);
    juce::dsp::FFT fft (fftOrder);
    std::vector<std::complex<float>> in ((size_t) fftSize);
    const auto& w = hann();
    for (int f = 0; f < fr.count; ++f)
    {
        const int start = f * hop - fftSize / 2;
        for (int i = 0; i < fftSize; ++i)
        {
            const int k = start + i;
            in[(size_t) i] = { (k >= 0 && k < n) ? x[k] * w[(size_t) i] : 0.0f, 0.0f };
        }
        fft.perform (in.data(), fr.d.data() + (size_t) f * fftSize, false);
    }
    return fr;
}

void istft (const Frames& fr, float* y, int n)
{
    juce::dsp::FFT fft (fftOrder);
    std::vector<std::complex<float>> out ((size_t) fftSize);
    const auto& w = hann();
    std::fill (y, y + n, 0.0f);
    for (int f = 0; f < fr.count; ++f)
    {
        fft.perform (fr.d.data() + (size_t) f * fftSize, out.data(), true);
        const int start = f * hop - fftSize / 2;
        for (int i = 0; i < fftSize; ++i)
        {
            const int k = start + i;
            if (k >= 0 && k < n) y[k] += out[(size_t) i].real() * w[(size_t) i] / 1.5f;
        }
    }
}

void fadeEdges (juce::AudioBuffer<float>& b, int fadeIn, float tailFraction)
{
    const int n = b.getNumSamples();
    const int tail = juce::jmax (1, (int) ((float) n * tailFraction));
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        auto* x = b.getWritePointer (c);
        for (int i = 0; i < juce::jmin (fadeIn, n); ++i) x[i] *= (float) i / (float) fadeIn;
        for (int i = 0; i < tail; ++i)
        {
            const float t = (float) i / (float) tail;
            x[n - tail + i] *= 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * t);
        }
    }
}

juce::AudioBuffer<float> segment (const juce::AudioBuffer<float>& src, juce::int64 start, int len)
{
    juce::AudioBuffer<float> b (juce::jmax (1, src.getNumChannels()), len);
    b.clear();
    for (int c = 0; c < src.getNumChannels(); ++c)
        for (int i = 0; i < len; ++i)
        {
            const auto k = start + i;
            if (k >= 0 && k < src.getNumSamples()) b.setSample (c, i, src.getSample (c, (int) k));
        }
    return b;
}

/** Best lag (samples, within +-maxLag) that aligns b to a on their first window. */
int alignLag (const juce::AudioBuffer<float>& src, juce::int64 a, juce::int64 b, int window, int maxLag)
{
    double best = -1e30; int bestLag = 0;
    for (int lag = -maxLag; lag <= maxLag; lag += 2)
    {
        double s = 0;
        for (int i = 0; i < window; i += 2)
        {
            const auto ia = a + i, ib = b + i + lag;
            if (ia < 0 || ib < 0 || ia >= src.getNumSamples() || ib >= src.getNumSamples()) continue;
            float xa = 0, xb = 0;
            for (int c = 0; c < src.getNumChannels(); ++c) { xa += src.getSample (c, (int) ia); xb += src.getSample (c, (int) ib); }
            s += (double) xa * xb;
        }
        if (s > best) { best = s; bestLag = lag; }
    }
    return bestLag;
}

void normalisePeak (juce::AudioBuffer<float>& b, float target)
{
    const float mx = b.getMagnitude (0, b.getNumSamples());
    if (mx > 1.0e-6f) b.applyGain (target / mx);
}
} // namespace

// ================================================================== load
std::shared_ptr<juce::AudioBuffer<float>> CloneEngine::load (const juce::File& f, double& sr, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) { error = "This file can't be read."; return nullptr; }
    const auto maxLen = (juce::int64) (r->sampleRate * 60.0 * 10.0);           // 10 minutes
    const int len = (int) juce::jmin<juce::int64> (r->lengthInSamples, maxLen);
    if (len < 64) { error = "The file is empty."; return nullptr; }
    auto b = std::make_shared<juce::AudioBuffer<float>> ((int) juce::jlimit (1u, 2u, r->numChannels), len);
    r->read (b.get(), 0, len, 0, true, b->getNumChannels() > 1);
    sr = r->sampleRate;
    return b;
}

// ================================================================== drum stem
std::shared_ptr<juce::AudioBuffer<float>> CloneEngine::percussiveStem (const juce::AudioBuffer<float>& src, double sr, const std::atomic<bool>* cancel)
{
    auto mono = toMono (src);
    auto spec = magnitude (mono);
    auto mask = percussiveMask (spec, cancel);
    if (mask.empty()) return nullptr;
    // sharpen: a drum stem should drop the tonal parts harder than the detector does
    for (auto& m : mask) m = m * m * (3.0f - 2.0f * m);
    const int keepLow = binOf (140, sr);
    for (int f = 0; f < spec.frames; ++f)
        for (int b = 0; b < keepLow; ++b) mask[(size_t) f * nBins + (size_t) b] = juce::jmax (mask[(size_t) f * nBins + (size_t) b], 1.0f - (float) b / (float) keepLow * 0.3f);
    auto out = std::make_shared<juce::AudioBuffer<float>> (src.getNumChannels(), src.getNumSamples());
    for (int c = 0; c < src.getNumChannels(); ++c)
        applyMask (src.getReadPointer (c), out->getWritePointer (c), src.getNumSamples(), mask, spec.frames);
    return out;
}

// ================================================================== pads
PadSound CloneEngine::makePad (const juce::AudioBuffer<float>& from, const Analysis& an, int gi, int hitIndex)
{
    PadSound p;
    if (! juce::isPositiveAndBelow (gi, (int) an.groups.size())) return p;
    const auto& g = an.groups[(size_t) gi];
    if (g.hits.empty()) return p;
    hitIndex = juce::jlimit (0, (int) g.hits.size() - 1, hitIndex);
    const double sr = an.sampleRate;
    const int pre = (int) (0.001 * sr);
    const int len = juce::jmax (256, g.length);
    const auto& me = an.hits[(size_t) g.hits[(size_t) hitIndex]];

    auto raw = segment (from, me.pos - pre, len);

    // CLEAN: median magnitude of the aligned hits, phase of this hit
    std::vector<juce::int64> others;
    for (int k = 0; k < (int) g.hits.size() && (int) others.size() < 32; ++k)
        if (k != hitIndex) others.push_back (an.hits[(size_t) g.hits[(size_t) k]].pos);

    juce::AudioBuffer<float> clean (raw.getNumChannels(), len);
    if (others.size() >= 4)
    {
        const int window = (int) (0.03 * sr), maxLag = (int) (0.003 * sr);
        std::vector<juce::AudioBuffer<float>> segs;
        auto rms = [&] (const juce::AudioBuffer<float>& b) { double e = 0; for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < juce::jmin (window, b.getNumSamples()); ++i) e += b.getSample (c, i) * b.getSample (c, i); return std::sqrt (e) + 1.0e-9; };
        const double myRms = rms (raw);
        for (auto pos : others)
        {   // aligned and at the same level (velocity) as this hit
            auto sgm = segment (from, pos - pre + alignLag (from, me.pos, pos, window, maxLag), len);
            sgm.applyGain ((float) juce::jlimit (0.25, 4.0, myRms / rms (sgm)));
            segs.push_back (std::move (sgm));
        }

        for (int c = 0; c < raw.getNumChannels(); ++c)
        {
            auto mine = stft (raw.getReadPointer (c), len);
            std::vector<Frames> fr;
            for (auto& s : segs) fr.push_back (stft (s.getReadPointer (juce::jmin (c, s.getNumChannels() - 1)), len));
            std::vector<float> mags (fr.size() + 1);
            for (int f = 0; f < mine.count; ++f)
                for (int b = 0; b < nBins; ++b)
                {
                    const size_t idx = (size_t) f * fftSize + (size_t) b;
                    const float m0 = std::abs (mine.d[idx]);
                    mags[0] = m0;
                    for (size_t k = 0; k < fr.size(); ++k) mags[k + 1] = std::abs (fr[k].d[idx]);
                    // a low quantile: the drum is in every hit, the rest (voice, bass, chords) only in some
                    const auto q = (long) ((mags.size() - 1) / 4);
                    std::nth_element (mags.begin(), mags.begin() + q, mags.end());
                    const float med = mags[(size_t) q];
                    const float gain = m0 > 1.0e-9f ? juce::jmin (1.0f, med / m0) : 0.0f;
                    mine.d[idx] *= gain;
                    if (b > 0 && b < fftSize / 2) mine.d[(size_t) f * fftSize + (size_t) (fftSize - b)] *= gain;
                }
            istft (mine, clean.getWritePointer (c), len);
        }
    }
    else
        clean.makeCopyOf (raw);

    {   // CLEAN also takes out what can't belong to the drum: lows under hats and cymbals, sub under snares
        const float hz = g.family == Family::hat || g.family == Family::openHat || g.family == Family::cymbal ? 450.0f
                       : g.family == Family::snare || g.family == Family::clap ? 90.0f : g.family == Family::perc ? 60.0f : 0.0f;
        if (hz > 0)
        {
            const float a = std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr);
            for (int c = 0; c < clean.getNumChannels(); ++c)
            {
                auto* x = clean.getWritePointer (c);
                for (int pass = 0; pass < 2; ++pass)   // 24 dB/oct
                {
                    float l1 = 0, l2 = 0;
                    for (int i = 0; i < len; ++i) { l1 = x[i] + a * (l1 - x[i]); l2 = l1 + a * (l2 - l1); x[i] -= l2; }
                }
            }
        }
    }
    const int fadeIn = juce::jmax (1, pre);
    fadeEdges (raw, fadeIn, 0.25f);
    fadeEdges (clean, fadeIn, 0.25f);
    const float rawPeak = 0.89f;
    normalisePeak (raw, rawPeak);
    normalisePeak (clean, rawPeak);

    p.raw = std::make_shared<juce::AudioBuffer<float>> (std::move (raw));
    p.clean = std::make_shared<juce::AudioBuffer<float>> (std::move (clean));
    p.sampleRate = sr;
    p.group = gi;
    p.hitIndex = hitIndex;
    p.type = g.family;
    return p;
}

// ================================================================== clone
CloneResult CloneEngine::clone (const juce::AudioBuffer<float>& sourceIn, double sr, const CloneSettings& set,
                                const std::atomic<bool>* cancel, Progress progress)
{
    CloneResult res;
    auto an = std::make_shared<Analysis>();
    res.analysis = an;
    an->sampleRate = sr;
    an->neural = set.ai && set.drumStem != nullptr;
    an->length = sourceIn.getNumSamples();
    auto report = [&] (float v, const char* what) { if (progress) progress (v, what); };

    // level-independent analysis
    juce::AudioBuffer<float> source (sourceIn);
    const float inPeak = sourceIn.getMagnitude (0, sourceIn.getNumSamples());
    normalisePeak (source, 1.0f);
    // v2: with the neural drum stem everything (detection and one-shots) comes from the drums only
    const bool neural = set.ai && set.drumStem != nullptr && set.drumStem->getNumSamples() == sourceIn.getNumSamples();
    if (neural)
    {
        source.makeCopyOf (*set.drumStem);
        if (inPeak > 1.0e-6f) source.applyGain (1.0f / inPeak);      // the same gain as the mix
    }

    auto mono = toMono (source);
    report (0.05f, "Analysing");
    auto spec = magnitude (mono);
    if (cancelled (cancel)) return res;
    report (0.15f, "Separating drums");
    auto mask = percussiveMask (spec, cancel);
    if (mask.empty()) return res;

    const std::vector<float> mixMono = mono;     // the low detector always listens to the mix (the stem keeps the bass)
    // the audio the one-shots come from
    if (set.ai && ! neural)
    {
        auto stem = std::make_shared<juce::AudioBuffer<float>> (source.getNumChannels(), source.getNumSamples());
        std::vector<float> sharp (mask);
        for (auto& m : sharp) m = m * m * (3.0f - 2.0f * m);
        // kick and 808 tails are tonal: under 140 Hz the stem keeps everything (CLEAN takes the bass out later)
        const int keepLow = binOf (140, sr);
        for (int f = 0; f < spec.frames; ++f)
            for (int b = 0; b < keepLow; ++b) sharp[(size_t) f * nBins + (size_t) b] = juce::jmax (sharp[(size_t) f * nBins + (size_t) b], 1.0f - (float) b / (float) keepLow * 0.3f);
        for (int c = 0; c < source.getNumChannels(); ++c)
            applyMask (source.getReadPointer (c), stem->getWritePointer (c), source.getNumSamples(), sharp, spec.frames);
        res.extractFrom = stem;
        mono = toMono (*stem);
    }
    else
        res.extractFrom = std::make_shared<juce::AudioBuffer<float>> (source);
    if (cancelled (cancel)) return res;
    report (0.45f, "Detecting hits");

    // percussive spectrogram, log compressed
    const int F = spec.frames;
    // how percussive the lows are, frame by frame (energy-weighted mask)
    const int lowLo = binOf (30, sr), lowHi = binOf (150, sr);
    std::vector<float> lowPerc ((size_t) F, 0.0f);
    for (int f = 0; f < F; ++f)
    {
        double e = 0, pe = 0;
        for (int k = lowLo; k < lowHi; ++k) { const double m2 = (double) spec.row (f)[k] * spec.row (f)[k]; e += m2; pe += m2 * mask[(size_t) f * nBins + (size_t) k]; }
        lowPerc[(size_t) f] = e > 0 ? (float) (pe / e) : 0.0f;
    }
    for (size_t i = 0; i < spec.m.size(); ++i) spec.m[i] *= mask[i];
    mask.clear(); mask.shrink_to_fit();
    std::vector<float> logP (spec.m.size());
    for (size_t i = 0; i < spec.m.size(); ++i) logP[i] = std::log1p (30.0f * spec.m[i]);

    const int bands[4] = { binOf (30, sr), binOf (150, sr), binOf (4500, sr), binOf (16000, sr) };
    std::array<std::vector<float>, 3> odf;
    for (auto& o : odf) o.assign ((size_t) F, 0.0f);
    for (int f = 1; f < F; ++f)
    {
        const float* a = logP.data() + (size_t) f * nBins;
        const float* b = logP.data() + (size_t) (f - 1) * nBins;
        for (int d = 0; d < 3; ++d)
        {
            float s = 0;
            for (int k = bands[d]; k < bands[d + 1]; ++k) s += juce::jmax (0.0f, a[k] - b[k]);
            odf[(size_t) d][(size_t) f] = s / (float) juce::jmax (1, bands[d + 1] - bands[d]);
        }
    }
    // the lows need a longer look than one FFT frame (a 50 Hz cycle is 20 ms): energy of the
    // low-passed signal over 23 ms, rising against the last 12 ms, weighted by how percussive it is
    std::vector<float> lowEnergy ((size_t) F, 0.0f);
    {
        const float a = std::exp (-juce::MathConstants<float>::twoPi * 150.0f / (float) sr);
        auto& e = lowEnergy;
        float l1 = 0, l2 = 0;
        std::vector<float> sq (mixMono.size());
        for (size_t i = 0; i < mixMono.size(); ++i) { l1 = mixMono[i] + a * (l1 - mixMono[i]); l2 = l1 + a * (l2 - l1); sq[i] = l2 * l2; }
        std::vector<double> cum (sq.size() + 1, 0.0);
        for (size_t i = 0; i < sq.size(); ++i) cum[i + 1] = cum[i] + sq[i];
        const int half = (int) (0.0115 * sr);
        for (int f = 0; f < F; ++f)
        {
            const auto c = (juce::int64) f * hop;
            const auto lo = juce::jlimit<juce::int64> (0, (juce::int64) sq.size(), c - half), hi = juce::jlimit<juce::int64> (0, (juce::int64) sq.size(), c + half);
            e[(size_t) f] = hi > lo ? (float) ((cum[(size_t) hi] - cum[(size_t) lo]) / (double) (hi - lo)) : 0.0f;
        }
        const float emax = juce::jmax (1.0e-12f, *std::max_element (e.begin(), e.end()));
        for (int f = 1; f < F; ++f)
        {
            const float now = e[(size_t) f] / emax;
            if (now < 1.0e-4f) { odf[0][(size_t) f] = 0; continue; }
            const float before = juce::jmin (f >= 2 ? e[(size_t) f - 2] : 0.0f, f >= 3 ? e[(size_t) f - 3] : 0.0f) / emax;   // silence before the file
            const float rise = std::log10 ((now + 1.0e-4f) / (before + 1.0e-4f));
            odf[0][(size_t) f] = juce::jmax (0.0f, rise) * std::sqrt (now) * (0.25f + 0.75f * lowPerc[(size_t) f]);
        }
    }

    // v3: the drum stem split into kick / snare / cymbals. Each detector listens to its own part: a kick
    // is only what is in the kick part, and the one-shots are cut from the parts (no snare in the kick).
    const bool pm = neural && set.parts[0] != nullptr && set.parts[1] != nullptr && set.parts[2] != nullptr
                 && set.parts[0]->getNumSamples() == sourceIn.getNumSamples();
    std::array<std::shared_ptr<juce::AudioBuffer<float>>, 3> partBuf {};
    std::array<std::vector<float>, 3> partMono;
    if (pm)
    {
        for (int p = 0; p < 3; ++p)
        {
            partBuf[(size_t) p] = std::make_shared<juce::AudioBuffer<float>> (*set.parts[(size_t) p]);
            if (inPeak > 1.0e-6f) partBuf[(size_t) p]->applyGain (1.0f / inPeak);
            partMono[(size_t) p] = toMono (*partBuf[(size_t) p]);
        }
        // kick: rising energy of the kick part (it holds only kicks, the whole band counts)
        {
            const float a = std::exp (-juce::MathConstants<float>::twoPi * 400.0f / (float) sr);
            float l1 = 0, l2 = 0;
            const auto& x = partMono[0];
            std::vector<double> cum (x.size() + 1, 0.0);
            for (size_t i = 0; i < x.size(); ++i) { l1 = x[i] + a * (l1 - x[i]); l2 = l1 + a * (l2 - l1); cum[i + 1] = cum[i] + (double) l2 * l2; }
            const int half = (int) (0.0115 * sr);
            for (int f = 0; f < F; ++f)
            {
                const auto c = (juce::int64) f * hop;
                const auto lo = juce::jlimit<juce::int64> (0, (juce::int64) x.size(), c - half), hi = juce::jlimit<juce::int64> (0, (juce::int64) x.size(), c + half);
                lowEnergy[(size_t) f] = hi > lo ? (float) ((cum[(size_t) hi] - cum[(size_t) lo]) / (double) (hi - lo)) : 0.0f;
            }
            const float emax = juce::jmax (1.0e-12f, *std::max_element (lowEnergy.begin(), lowEnergy.end()));
            for (int f = 1; f < F; ++f)
            {
                const float now = lowEnergy[(size_t) f] / emax;
                const float before = juce::jmin (f >= 2 ? lowEnergy[(size_t) f - 2] : 0.0f, f >= 3 ? lowEnergy[(size_t) f - 3] : 0.0f) / emax;
                odf[0][(size_t) f] = now < 1.0e-4f ? 0.0f : juce::jmax (0.0f, std::log10 ((now + 1.0e-4f) / (before + 1.0e-4f))) * std::sqrt (now);
            }
        }
        // snare and cymbals: spectral flux of their part
        for (int p = 1; p < 3; ++p)
        {
            auto ps = magnitude (partMono[(size_t) p]);
            const int lo = p == 1 ? binOf (120, sr) : binOf (4000, sr), hi = p == 1 ? binOf (12000, sr) : binOf (16000, sr);
            auto& o = odf[(size_t) p];
            std::fill (o.begin(), o.end(), 0.0f);
            for (int f = 1; f < juce::jmin (F, ps.frames); ++f)
            {
                float sum = 0;
                for (int k = lo; k < hi; ++k) sum += juce::jmax (0.0f, std::log1p (30.0f * ps.row (f)[k]) - std::log1p (30.0f * ps.row (f - 1)[k]));
                o[(size_t) f] = sum / (float) (hi - lo);
            }
        }
    }

    if (auto* dbg = std::getenv ("ECLIPSE_DEBUG_ODF"))
    {
        const double t0 = juce::String (dbg).getDoubleValue();
        for (int f = juce::jmax (0, (int) ((t0 - 0.08) * sr / hop)); f < juce::jmin (F, (int) ((t0 + 0.08) * sr / hop)); ++f)
            std::cerr << "odf t=" << f * hop / sr << " low=" << odf[0][(size_t) f] << " lowPerc=" << lowPerc[(size_t) f] << "\n";
    }
    // normalise each detector; a region that is (almost) silent stays quiet
    float globalRef = 0;
    std::array<float, 3> ref {};
    for (int d = 0; d < 3; ++d) { ref[(size_t) d] = percentile (odf[(size_t) d], 0.995f); globalRef = juce::jmax (globalRef, ref[(size_t) d]); }
    for (int d = 0; d < 3; ++d)
    {
        const float r = juce::jmax (ref[(size_t) d], globalRef * 0.08f, 1.0e-6f);
        for (auto& v : odf[(size_t) d]) v = juce::jmin (1.5f, v / r);
    }

    // band energy per frame (for decay and features)
    auto bandEnergy = [&] (int f, int lo, int hi)
    {
        if (f < 0 || f >= F) return 0.0f;
        const float* r = spec.row (f);
        float s = 0;
        for (int k = lo; k < hi; ++k) s += r[k] * r[k];
        return s;
    };

    // peak picking per detector
    const bool v2 = classifier::hasWeights() && set.useClassifier;     // the network filters what isn't a drum: listen more
    const float lambda = juce::jmap (juce::jlimit (0.0f, 1.0f, set.sensitivity), 0.42f, 0.05f) * (v2 ? 0.6f : 1.0f);
    const int minGap[3] = { (int) (0.07 * sr / hop), (int) (0.055 * sr / hop), (int) (0.035 * sr / hop) };
    const int medW = (int) (0.12 * sr / hop);
    std::vector<Cand> cands;
    std::vector<float> win;
    for (int d = 0; d < 3; ++d)
    {
        const auto& o = odf[(size_t) d];
        int last = -1000;
        for (int f = 1; f < F - 2; ++f)
        {
            const float v = o[(size_t) f];
            if (v < 0.06f || v < o[(size_t) f - 1] || v < o[(size_t) f + 1] || (f >= 2 && v < o[(size_t) f - 2]) || v < o[(size_t) f + 2]) continue;
            win.clear();
            for (int k = juce::jmax (0, f - medW); k <= juce::jmin (F - 1, f + medW); ++k) win.push_back (o[(size_t) k]);
            std::nth_element (win.begin(), win.begin() + (long) win.size() / 2, win.end());
            if (v < win[win.size() / 2] + lambda) continue;
            if (f - last < minGap[d])
            {   // keep the stronger of two close peaks
                if (! cands.empty() && cands.back().det == d && v > cands.back().strength) { cands.back().frame = f; cands.back().strength = v; last = f; }
                continue;
            }
            Cand c; c.frame = f; c.det = (Detector) d; c.strength = v;
            cands.push_back (c);
            last = f;
        }
    }
    if (cancelled (cancel)) return res;

    if (! v2)
    {   // v1 rules (with the network, it decides)
    // a snare also shows up in the highs, a kick's click in the mids: drop the weaker echo
    std::sort (cands.begin(), cands.end(), [] (const Cand& a, const Cand& b) { return a.frame < b.frame; });
    for (size_t i = 0; i < cands.size(); ++i)
        for (size_t j = i + 1; j < cands.size() && cands[j].frame - cands[i].frame <= 2; ++j)
        {
            auto& a = cands[i]; auto& b = cands[j];
            auto echo = [] (Cand& main, Cand& other, Detector mainDet, Detector otherDet, float ratio)
            {
                if (main.det == mainDet && other.det == otherDet && other.strength < main.strength * ratio) { other.keep = false; if (std::getenv ("ECLIPSE_DEBUG")) std::cerr << "drop echo det=" << (int) otherDet << " t=" << other.frame * 256 / 44100.0 << "\n"; }
            };
            echo (a, b, dMid, dHigh, 0.75f); echo (b, a, dMid, dHigh, 0.75f);
            echo (a, b, dLow, dMid, 0.55f);  echo (b, a, dLow, dMid, 0.55f);
        }
    // a mid hit at the same time as a high one: if there is (almost) no body under 400 Hz it is
    // just the hat seen by the mid detector
    {
        const int bA = binOf (150, sr), bB = binOf (400, sr), bC = binOf (16000, sr);
        auto newEnergy = [&] (int f, int lo, int hi)
        {
            double e = 0;
            for (int k = 0; k < 3; ++k)
            {
                const int fr = f + k;
                if (fr <= 0 || fr >= F) continue;
                for (int b = lo; b < hi; ++b) e += juce::jmax (0.0f, spec.row (fr)[b] * spec.row (fr)[b] - spec.row (f - 1)[b] * spec.row (f - 1)[b]);
            }
            return e;
        };
        for (size_t i = 0; i < cands.size(); ++i)
        {
            if (cands[i].det != dMid || ! cands[i].keep) continue;
            bool withHigh = false;
            for (size_t j = 0; j < cands.size(); ++j)
                if (cands[j].det == dHigh && cands[j].keep && std::abs (cands[j].frame - cands[i].frame) <= 2) withHigh = true;
            bool withLow = false;
            for (size_t j = 0; j < cands.size(); ++j)
                if (cands[j].det == dLow && cands[j].keep && std::abs (cands[j].frame - cands[i].frame) <= 2) withLow = true;
            if (! withHigh && ! withLow) continue;
            const double body = newEnergy (cands[i].frame, bA, bB), all = newEnergy (cands[i].frame, bA, bC) + 1.0e-12;
            const double upper = newEnergy (cands[i].frame, bB, binOf (5000, sr));
            if (withHigh && body / all < 0.12) cands[i].keep = false;      // only the hat
            if (withLow && upper / all < 0.06) cands[i].keep = false;      // only the kick's body
        }
    }
    // a snare's body (150-250 Hz) also reaches the low detector: with a mid hit at the same time
    // it only counts as a kick if the sub dominates
    {
        const int s0b = binOf (30, sr), s1b = binOf (110, sr), b0b = binOf (150, sr), b1b = binOf (400, sr);
        auto newE = [&] (int f, int lo, int hi)
        {
            double e = 0;
            for (int k = 0; k < 4; ++k)
            {
                const int fr = f + k;
                if (fr <= 0 || fr >= F) continue;
                for (int b = lo; b < hi; ++b) e += juce::jmax (0.0f, spec.row (fr)[b] * spec.row (fr)[b] - spec.row (f - 1)[b] * spec.row (f - 1)[b]);
            }
            return e;
        };
        std::vector<float> lowStr;
        for (auto& c : cands) if (c.det == dLow && c.keep) lowStr.push_back (c.strength);
        const float strongKick = lowStr.size() >= 4 ? percentile (lowStr, 0.8f) * 0.6f : 0.0f;
        for (auto& c : cands)
        {
            if (c.det != dLow || ! c.keep) continue;
            bool withMid = false;
            for (auto& o : cands) if (o.det == dMid && o.keep && std::abs (o.frame - c.frame) <= 2) withMid = true;
            if (withMid && newE (c.frame, s0b, s1b) < 0.12 * newE (c.frame, b0b, b1b)) { c.keep = false; if (std::getenv ("ECLIPSE_DEBUG")) std::cerr << "drop body t=" << c.frame * hop / sr << "\n"; }
        }
    }
    }
    // much weaker than the typical hit of its detector around it (+-6 s: songs get louder and softer)
    // = not a drum (a bass note, a vocal consonant)
    {
        const float factor = (0.2f + 0.25f * (1.0f - juce::jlimit (0.0f, 1.0f, set.sensitivity))) * (v2 ? 0.6f : 1.0f);
        const int win6 = (int) (6.0 * sr / hop);
        std::vector<float> floors (cands.size(), 0.0f);
        std::vector<float> st;
        for (size_t i = 0; i < cands.size(); ++i)
        {
            if (! cands[i].keep) continue;
            st.clear();
            for (auto& o : cands) if (o.det == cands[i].det && o.keep && std::abs (o.frame - cands[i].frame) < win6) st.push_back (o.strength);
            if (st.size() >= 4) floors[i] = percentile (st, 0.9f) * factor;
        }
        for (size_t i = 0; i < cands.size(); ++i)
            if (cands[i].keep && cands[i].strength < floors[i])
            {
                cands[i].keep = false;
                if (std::getenv ("ECLIPSE_DEBUG")) std::cerr << "drop weak det=" << (int) cands[i].det << " t=" << cands[i].frame * hop / sr << "\n";
            }
    }
    cands.erase (std::remove_if (cands.begin(), cands.end(), [] (const Cand& c) { return ! c.keep; }), cands.end());

    // features, sample-accurate onset, family
    const int b150 = binOf (150, sr), b400 = binOf (400, sr), b1k = binOf (1000, sr), b2k = binOf (2000, sr),
              b5k = binOf (5000, sr), b8k = binOf (8000, sr), b16k = binOf (16000, sr), b30 = binOf (30, sr);
    std::array<int, nPrint + 1> pb {};
    for (int i = 0; i <= nPrint; ++i) pb[(size_t) i] = binOf (30.0 * std::pow (16000.0 / 30.0, (double) i / nPrint), sr);
    const int maxDecayFrames = (int) (2.5 * sr / hop);

    for (auto& c : cands)
    {
        const int f0 = c.frame;
        // fingerprint: new energy in the first ~45 ms
        for (int i = 0; i < nPrint; ++i)
        {
            float e = 0;
            for (int k = 0; k < 8; ++k)
            {
                const int f = f0 + k;
                if (f >= F) break;
                for (int b = pb[(size_t) i]; b < juce::jmax (pb[(size_t) i] + 1, pb[(size_t) i + 1]); ++b)
                    e += spec.row (f)[b] * spec.row (f)[b] * (k < 3 ? 1.0f : 0.5f);
            }
            c.print[(size_t) i] = std::log10 (1.0e-9f + e);
        }
        // decay of the detector's region
        const int lo = c.det == dLow ? b30 : c.det == dMid ? b150 : b5k;
        const int hi = c.det == dLow ? b150 : c.det == dMid ? b5k : b16k;
        float peak = 0; int pf = f0;
        for (int k = 0; k < 4; ++k) { float e = bandEnergy (f0 + k, lo, hi); if (e > peak) { peak = e; pf = f0 + k; } }
        int df = pf;
        if (c.det == dLow)
        {   // kick tails are tonal: measure them on the lows of the mix, not on the percussive part
            float pk = 0;
            for (int k = 0; k < 6 && f0 + k < F; ++k) if (lowEnergy[(size_t) (f0 + k)] > pk) { pk = lowEnergy[(size_t) (f0 + k)]; pf = f0 + k; }
            df = pf;
            while (df < F - 1 && df - pf < maxDecayFrames && lowEnergy[(size_t) df] > pk * 0.05f) ++df;
        }
        else
            while (df < F - 1 && df - pf < maxDecayFrames && bandEnergy (df, lo, hi) > peak * 0.1f) ++df;
        c.decayMs = (float) ((df - pf) * hop * 1000.0 / sr);

        // sample-accurate onset
        const juce::int64 s0 = juce::jmax<juce::int64> (0, (juce::int64) f0 * hop - fftSize - (juce::int64) (0.01 * sr)), s1 = (juce::int64) f0 * hop + hop;
        {   // where the energy after (6 ms) jumps the most over the energy before (6 ms)
            auto sq = bandSquared (pm ? partMono[(size_t) c.det] : mono, s0, s1 + (juce::int64) (0.03 * sr), pm ? dMid : c.det, sr);
            const int W = (int) (0.006 * sr);
            std::vector<double> cum (sq.size() + 1, 0.0);
            for (size_t i = 0; i < sq.size(); ++i) cum[i + 1] = cum[i] + sq[i];
            const double floorE = cum.back() / (double) std::max<size_t> (1, sq.size()) * 0.01 * W + 1.0e-12;
            double best = -1; int at = -1;
            for (int k = W; k + W <= (int) sq.size(); ++k)
            {
                const double pre = cum[(size_t) k] - cum[(size_t) (k - W)], post = cum[(size_t) (k + W)] - cum[(size_t) k];
                const double r = (post + floorE) / (pre + floorE);
                if (r > best) { best = r; at = k; }
            }
            c.pos = at >= 0 ? juce::jmax<juce::int64> (0, s0 + at - (juce::int64) (0.001 * sr)) : (juce::int64) f0 * hop;
        }

        // family
        if (c.det == dLow) c.family = Family::kick;
        else if (c.det == dHigh) c.family = c.decayMs < 120 ? Family::hat : Family::openHat;   // no cymbals
        else
        {
            float eLm = 0, eMid = 0, eHm = 0;
            for (int k = 0; k < 3; ++k) { eLm += bandEnergy (f0 + k, b150, b400); eMid += bandEnergy (f0 + k, b400, b2k); eHm += bandEnergy (f0 + k, b2k, b5k); }
            const float tot = eLm + eMid + eHm + 1.0e-12f, lm = eLm / tot, hm = eHm / tot;
            // flatness 1k..8k = how noisy
            double lg = 0, ar = 0; int nb = 0;
            for (int b = b1k; b < b8k; ++b) { const double m = spec.row (juce::jmin (F - 1, f0 + 1))[b] + 1.0e-9; lg += std::log (m); ar += m; ++nb; }
            const float flat = nb > 0 ? (float) (std::exp (lg / nb) / (ar / nb)) : 0.0f;
            const int flams = countFlams (mono, c.pos, sr);
            if (flat < 0.12f && lm > 0.45f && c.decayMs > 100) c.family = Family::tom;
            else if (flat < 0.1f) c.family = Family::perc;
            else if (flams >= 2 && lm < 0.35f) c.family = Family::clap;
            else if (lm < 0.12f && hm > 0.45f) c.family = Family::clap;
            else c.family = Family::snare;
        }
    }
    // v3: a part also gets a little of the others (the kick's click in the snare part, the snare's top in the
    // cymbals part). A hit whose own part is much quieter than another part at that moment is that leak.
    if (pm)
    {
        auto energy = [&] (int p, juce::int64 pos)
        {
            const auto& x = partMono[(size_t) p];
            const auto a = juce::jmax<juce::int64> (0, pos), b = juce::jmin<juce::int64> ((juce::int64) x.size(), pos + (juce::int64) (0.03 * sr));
            double e = 0; for (auto i = a; i < b; ++i) e += (double) x[(size_t) i] * x[(size_t) i];
            return e;
        };
        for (auto& c : cands)
        {
            if (c.det == dLow) continue;
            const double own = energy (c.det, c.pos);
            const double kick = energy (0, c.pos), snare = energy (1, c.pos);
            if (c.det == dMid && own < 0.12 * kick) c.keep = false;
            if (c.det == dHigh && (own < 0.06 * kick || own < 0.12 * snare)) c.keep = false;
        }
        cands.erase (std::remove_if (cands.begin(), cands.end(), [] (const Cand& c) { return ! c.keep; }), cands.end());
    }

    // v2: the trained network says what each hit is, among what its detector can hear:
    // lows = kick / tom / perc, mids = snare / clap / tom / perc, highs = hat / open hat / cymbal / snare / clap
    if (classifier::hasWeights() && set.useClassifier)
    {
        const auto& from = *res.extractFrom;
        for (auto& c : cands)
        {
            const auto p = classifier::predict (classifier::features (pm ? *partBuf[(size_t) c.det] : from, c.pos, sr));
            c.prob = p;
            auto P = [&] (Family f) { return p[(size_t) f]; };
            std::vector<Family> allowed;
            if (pm)
            {   // the part already says what it is; the network only splits snare / clap / perc and hat / open hat
                if (c.det == dLow) { c.family = Family::kick; continue; }
                Family best = c.det == dMid ? Family::snare : Family::hat;
                if (c.det == dMid)
                {
                    if (P (Family::clap) > P (Family::snare) * 1.15f) best = Family::clap;
                    if (P (Family::perc) > 0.8f && P (Family::perc) > 2.0f * (P (Family::snare) + P (Family::clap))) best = Family::perc;
                }
                else best = (P (Family::openHat) > P (Family::hat) * 1.3f && c.decayMs >= 110) ? Family::openHat : Family::hat;
                c.family = best;
                continue;
            }
            if (c.det == dLow)       allowed = { Family::kick, Family::tom, Family::perc };
            else if (c.det == dMid)  allowed = { Family::snare, Family::clap, Family::tom, Family::perc };
            else                     allowed = { Family::hat, Family::openHat, Family::snare, Family::clap };   // no cymbals (they catch pieces of voice)
            // the same hit seen by the wrong detector (a hat in the mids, a snare in the lows): drop it
            if (c.det == dMid && P (Family::hat) + P (Family::openHat) > 0.7f && P (Family::snare) + P (Family::clap) < 0.25f) { c.keep = false; continue; }
            if (c.det == dLow && P (Family::kick) < 0.2f && P (Family::snare) + P (Family::clap) + P (Family::hat) > 0.7f) { c.keep = false; continue; }
            if (c.det == dMid && P (Family::kick) > 0.75f && P (Family::snare) + P (Family::clap) < 0.12f) { c.keep = false; continue; }   // the kick's click
            if (c.det == dMid)
            {   // with a kick at the same time, a mid hit that sounds more like kick than snare / clap is that kick
                bool withLow = false;
                for (auto& o : cands) if (o.det == dLow && std::abs (o.frame - c.frame) <= 2) withLow = true;
                if (withLow && P (Family::kick) > 1.2f * (P (Family::snare) + P (Family::clap))) { c.keep = false; continue; }
            }
            Family best = allowed[0];
            float bp = -1, second = -1;
            for (auto f : allowed)
            {
                float v = P (f);
                if (f == Family::perc) v = v > 0.85f ? v * 0.5f : 0.0f;                         // rarer in songs: they need to be clear
                if (f == Family::tom || f == Family::cymbal) v *= 0.6f;
                if (v > bp) { second = bp; bp = v; best = f; } else if (v > second) second = v;
            }
            if (bp < 0.08f) { c.keep = false; continue; }        // not a drum (a vocal, a bass note)
            // a high hit at the same time as a kick / snare: the louder drum masks the hat, so a little
            // "hat" in the network's answer is enough; only the snare's own top is dropped
            if (c.det == dHigh)
            {
                bool other = false;
                for (auto& o : cands) if (o.det != dHigh && std::abs (o.frame - c.frame) <= 2) other = true;
                const float hatP = P (Family::hat) + P (Family::openHat) + P (Family::cymbal);
                if (other)
                {
                    if (hatP < 0.1f) { c.keep = false; continue; }
                    best = P (Family::openHat) > P (Family::hat) * 1.5f ? Family::openHat : Family::hat;
                }
                else if (best == Family::snare || best == Family::clap)
                {
                    bool midToo = false;
                    for (auto& o : cands) if (o.det == dMid && std::abs (o.frame - c.frame) <= 2) midToo = true;
                    if (midToo) { c.keep = false; continue; }
                }
                // an open hat rings: a short one is a closed hat
                if (best == Family::openHat && c.decayMs < 110) best = Family::hat;
            }
            c.family = best;
        }
        cands.erase (std::remove_if (cands.begin(), cands.end(), [] (const Cand& c) { return ! c.keep; }), cands.end());
    }

    // a note that is already sounding when the file starts (a bass) is not a kick: a kick dies away
    cands.erase (std::remove_if (cands.begin(), cands.end(), [&] (const Cand& c)
    {
        return c.det == dLow && c.frame < 4 && bandEnergy (c.frame + (int) (0.35 * sr / hop), b30, b150) > 0.35f * bandEnergy (c.frame + 1, b30, b150);
    }), cands.end());
    if (std::getenv ("ECLIPSE_DEBUG") != nullptr)
        for (auto& c : cands)
            std::cerr << "cand t=" << (double) c.pos / sr << " det=" << (int) c.det << " str=" << c.strength
                      << " fam=" << familyName (c.family) << " decay=" << c.decayMs << "\n";
    if (cancelled (cancel)) return res;
    report (0.6f, "Grouping sounds");

    // v2: the same drum repeats all through a song. Hits that sound the same (per detector) vote together:
    // the average answer of the network over the group decides the family of all of them.
    if (classifier::hasWeights() && set.useClassifier)
    {
        for (int d = 0; d < 3; ++d)
        {
            std::vector<int> idx;
            for (int i = 0; i < (int) cands.size(); ++i) if (cands[(size_t) i].det == d) idx.push_back (i);
            std::sort (idx.begin(), idx.end(), [&] (int a, int b) { return cands[(size_t) a].strength > cands[(size_t) b].strength; });
            const int rLo = d == dLow ? 0 : d == dHigh ? 16 : 5, rHi = d == dLow ? 8 : d == dHigh ? nPrint : 20;
            struct V { std::array<float, nPrint> centroid {}; float decay = 0; std::vector<int> members; };
            std::vector<V> vs;
            for (int i : idx)
            {
                const auto& c = cands[(size_t) i];
                int best = -1; float bestSim = -2;
                for (int g = 0; g < (int) vs.size(); ++g)
                {
                    const float sm = similarity (c.print, vs[(size_t) g].centroid, rLo, rHi) - 0.12f * std::abs (std::log ((c.decayMs + 20) / (vs[(size_t) g].decay + 20)));
                    if (sm > bestSim) { bestSim = sm; best = g; }
                }
                if (best >= 0 && bestSim > 0.82f)
                {
                    auto& g = vs[(size_t) best];
                    const float n = (float) g.members.size();
                    for (int k = 0; k < nPrint; ++k) g.centroid[(size_t) k] = (g.centroid[(size_t) k] * n + c.print[(size_t) k]) / (n + 1);
                    g.decay = (g.decay * n + c.decayMs) / (n + 1);
                    g.members.push_back (i);
                }
                else { V g; g.centroid = c.print; g.decay = c.decayMs; g.members.push_back (i); vs.push_back (std::move (g)); }
            }
            for (auto& g : vs)
            {
                if (g.members.size() < 3) continue;
                if (pm && d == dMid)
                {   // a "snare" that only ever plays with the kick is the kick leaking into the snare part
                    int onKick = 0;
                    for (int m : g.members)
                        for (auto& o : cands) if (o.det == dLow && o.keep && std::abs (o.frame - cands[(size_t) m].frame) <= 5) { ++onKick; break; }
                    if (onKick >= (int) g.members.size() * 80 / 100)
                    {
                        for (int m : g.members) cands[(size_t) m].keep = false;
                        continue;
                    }
                }
                if (d == dHigh)
                {   // a "hat" that is (almost) always on top of a kick or snare is the top of that drum, not a hat
                    int together = 0; float hatSum = 0;
                    for (int m : g.members)
                    {
                        const auto& c = cands[(size_t) m];
                        hatSum += c.prob[(size_t) Family::hat] + c.prob[(size_t) Family::openHat];
                        for (auto& o : cands) if (o.det != dHigh && o.keep && std::abs (o.frame - c.frame) <= (pm ? 5 : 2)) { ++together; break; }
                    }
                    if ((together >= (int) g.members.size() * 80 / 100 && hatSum / (float) g.members.size() < 0.65f)
                        || (pm && together >= (int) g.members.size() * 90 / 100))     // the bright top of the kick / snare in the cymbals part
                    {
                        for (int m : g.members) cands[(size_t) m].keep = false;
                        continue;
                    }
                    if (pm && together >= (int) g.members.size() * 55 / 100)
                    {   // mostly on the kick / snare: those are the drum's top; keep only the hats that play on their own
                        std::vector<int> free;
                        for (int m : g.members)
                        {
                            const auto& c = cands[(size_t) m];
                            bool with = false;
                            for (auto& o : cands) if (o.det != dHigh && o.keep && std::abs (o.frame - c.frame) <= 5) { with = true; break; }
                            if (with) cands[(size_t) m].keep = false; else free.push_back (m);
                        }
                        g.members = std::move (free);
                        if (g.members.size() < 3) { for (int m : g.members) cands[(size_t) m].keep = false; continue; }
                    }
                }
                std::array<float, numFamilies> mean {};
                std::array<int, numFamilies> votes {};
                for (int m : g.members)
                {
                    for (int k = 0; k < numFamilies; ++k) mean[(size_t) k] += cands[(size_t) m].prob[(size_t) k] / (float) g.members.size();
                    votes[(size_t) cands[(size_t) m].family]++;
                }
                std::vector<Family> allowed;
                if (pm && d == dLow) allowed = { Family::kick };
                else if (pm && d == dMid) allowed = { Family::snare, Family::clap, Family::perc };
                else if (d == dLow)  allowed = { Family::kick, Family::tom, Family::perc };
                else if (d == dMid)  allowed = { Family::snare, Family::clap, Family::tom, Family::perc };
                else                 allowed = { Family::hat, Family::openHat };
                Family best = allowed[0]; float bp = -1;
                for (auto f : allowed)
                {
                    float v = mean[(size_t) f] + 0.25f * (float) votes[(size_t) f] / (float) g.members.size();
                    if (f == Family::perc) v *= 0.25f;
                    if (f == Family::tom || f == Family::cymbal) v *= 0.7f;
                    if (f == Family::openHat && g.decay < 110) v *= 0.4f;
                    if (v > bp) { bp = v; best = f; }
                }
                for (int m : g.members) cands[(size_t) m].family = best;
            }
        }
    }

    cands.erase (std::remove_if (cands.begin(), cands.end(), [] (const Cand& c) { return ! c.keep; }), cands.end());
    // hits
    float strongest[3] {};
    for (auto& c : cands) strongest[c.det] = juce::jmax (strongest[c.det], c.strength);
    for (auto& c : cands)
    {
        Hit h; h.pos = c.pos; h.family = c.family; h.strength = juce::jlimit (0.0f, 1.0f, c.strength / juce::jmax (1.0e-6f, strongest[c.det]));
        an->hits.push_back (h);
    }

    std::vector<std::pair<juce::int64, bool>> others;     // every hit: position, is it a hat
    for (auto& c : cands) others.push_back ({ c.pos, c.family == Family::hat || c.family == Family::openHat || c.family == Family::cymbal });
    std::sort (others.begin(), others.end());

    // groups per family (leader clustering, strongest first)
    for (int fi = 0; fi < numFamilies; ++fi)
    {
        const auto fam = (Family) fi;
        std::vector<int> idx;
        for (int i = 0; i < (int) cands.size(); ++i) if (cands[(size_t) i].family == fam) idx.push_back (i);
        if (idx.empty()) continue;
        std::sort (idx.begin(), idx.end(), [&] (int a, int b) { return cands[(size_t) a].strength > cands[(size_t) b].strength; });

        // compare only the drum's own region: a hat on top of a kick doesn't make it another kick
        const bool lowFam = fam == Family::kick, highFam = fam == Family::hat || fam == Family::openHat || fam == Family::cymbal;
        const int rLo = lowFam ? 0 : highFam ? 16 : 5, rHi = lowFam ? 8 : highFam ? nPrint : 20;
        auto sim = [rLo, rHi] (const std::array<float, nPrint>& a, const std::array<float, nPrint>& b) { return similarity (a, b, rLo, rHi); };
        struct G { std::array<float, nPrint> centroid {}; float decay = 0; std::vector<int> members; };
        std::vector<G> gs;
        const float thr = 0.86f;
        for (int i : idx)
        {
            const auto& c = cands[(size_t) i];
            int best = -1; float bestSim = -2;
            for (int g = 0; g < (int) gs.size(); ++g)
            {
                const float s = sim (c.print, gs[(size_t) g].centroid)
                              - 0.12f * std::abs (std::log ((c.decayMs + 20) / (gs[(size_t) g].decay + 20)));
                if (s > bestSim) { bestSim = s; best = g; }
            }
            if (best >= 0 && bestSim > thr)
            {
                auto& g = gs[(size_t) best];
                const float n = (float) g.members.size();
                for (int k = 0; k < nPrint; ++k) g.centroid[(size_t) k] = (g.centroid[(size_t) k] * n + c.print[(size_t) k]) / (n + 1);
                g.decay = (g.decay * n + c.decayMs) / (n + 1);
                g.members.push_back (i);
            }
            else
            {
                G g; g.centroid = c.print; g.decay = c.decayMs; g.members.push_back (i);
                gs.push_back (std::move (g));
            }
        }
        // biggest groups win; tiny ones (one-off noises) are dropped unless the source is short
        std::sort (gs.begin(), gs.end(), [] (const G& a, const G& b) { return a.members.size() > b.members.size(); });
        // the same drum with something else on top (a hat over the snare) is still the same sound
        for (size_t i = 0; i < gs.size(); ++i)
            for (size_t j = i + 1; j < gs.size();)
            {
                const float sm = sim (gs[i].centroid, gs[j].centroid) - 0.12f * std::abs (std::log ((gs[i].decay + 20) / (gs[j].decay + 20)));
                if (sm > 0.8f || (sm > 0.72f && gs[j].members.size() * 2 <= gs[i].members.size() + 2))
                {
                    const float a = (float) gs[i].members.size(), b = (float) gs[j].members.size();
                    for (int k = 0; k < nPrint; ++k) gs[i].centroid[(size_t) k] = (gs[i].centroid[(size_t) k] * a + gs[j].centroid[(size_t) k] * b) / (a + b);
                    gs[i].decay = (gs[i].decay * a + gs[j].decay * b) / (a + b);
                    gs[i].members.insert (gs[i].members.end(), gs[j].members.begin(), gs[j].members.end());
                    gs.erase (gs.begin() + (long) j);
                }
                else ++j;
            }
        std::sort (gs.begin(), gs.end(), [] (const G& a, const G& b) { return a.members.size() > b.members.size(); });
        // only sounds that really repeat get a pad (a loop or a one-shot: everything)
        const size_t minSize = idx.size() < 6 ? (size_t) juce::jmin (2, (int) idx.size()) : (size_t) juce::jmax (4, (int) (idx.size() * 0.03));
        const bool hatLike = fam == Family::hat || fam == Family::openHat || fam == Family::cymbal;
        const auto lr = lengthRange (fam);
        const auto wanted = [&] (const G& g) { return (juce::int64) (juce::jlimit (lr.getStart(), lr.getEnd(), g.decay * 1.4f + 40.0f) * sr / 1000.0); };
        // time to the next other drum (hats don't count for kicks and snares: they sit on top all the time)
        auto gapAfter = [&] (const Cand& c)
        {
            juce::int64 gap = std::numeric_limits<juce::int64>::max();
            for (auto& o : others)
            {
                if (o.first <= c.pos + (juce::int64) (0.008 * sr)) continue;
                if (! hatLike && o.second) continue;
                gap = o.first - c.pos;
                break;
            }
            return gap;
        };
        int made = 0;
        for (auto& g : gs)
        {
            if (made >= maxGroups (fam) || g.members.size() < minSize) break;
            Group out; out.family = fam;
            if (pm) out.source = fam == Family::kick ? 0 : hatLike ? 2 : 1;
            const auto want = wanted (g);
            // HIT 1 = the cleanest: strong, nothing at the same time, and room after it (no snare inside the kick)
            std::vector<std::pair<float, int>> scored;
            std::vector<juce::int64> gaps;
            for (int m : g.members)
            {
                const auto& c = cands[(size_t) m];
                float pen = 0;
                const auto lo = std::lower_bound (others.begin(), others.end(), std::make_pair (c.pos - (juce::int64) (0.005 * sr), false));
                for (auto it = lo; it != others.end() && it->first < c.pos + (juce::int64) (0.005 * sr); ++it)
                    if (it->first != c.pos && (! it->second || hatLike)) pen = 0.4f;
                const auto gap = gapAfter (c);
                gaps.push_back (gap);
                const float room = gap >= want ? 1.0f : (float) gap / (float) want;
                scored.push_back ({ c.strength * (1.0f - pen) * (0.3f + 0.7f * room) * (0.6f + 0.4f * sim (c.print, g.centroid)), m });
            }
            std::sort (scored.begin(), scored.end(), [] (auto& x, auto& y) { return x.first > y.first; });
            for (auto& sc : scored) { out.hits.push_back (sc.second); an->hits[(size_t) sc.second].group = (int) an->groups.size(); }
            // the one-shot ends before the next drum of most hits (so the kick doesn't carry the snare)
            std::sort (gaps.begin(), gaps.end());
            const auto typicalGap = gaps[gaps.size() / 4];
            auto len = want;
            if (typicalGap != std::numeric_limits<juce::int64>::max())
                len = juce::jlimit ((juce::int64) (lr.getStart() * 0.55 * sr / 1000.0), want, typicalGap - (juce::int64) (0.006 * sr));
            out.length = (int) len;
            an->groups.push_back (std::move (out));
            ++made;
        }
    }
    if (cancelled (cancel)) return res;
    report (0.7f, "Finding the tempo");

    // ---------------------------------------------------------- tempo
    std::vector<float> all ((size_t) F, 0.0f);
    for (int f = 0; f < F; ++f) all[(size_t) f] = odf[0][(size_t) f] * 1.2f + odf[1][(size_t) f] + odf[2][(size_t) f] * 0.6f;
    const double fps = sr / hop;
    auto at = [&] (double fr) { const int i = (int) fr; const double t = fr - i; if (i < 0 || i + 1 >= F) return 0.0; return all[(size_t) i] * (1 - t) + all[(size_t) i + 1] * t; };
    auto score = [&] (double bpm)
    {
        const double lag = 60.0 / bpm * fps;
        double s = 0;
        for (int f = 0; f + (int) (lag * 2) + 2 < F; f += 1)
            s += all[(size_t) f] * (at (f + lag) + 0.5 * at (f + lag * 2) + 0.25 * at (f + lag * 0.5));
        const double prior = std::exp (-0.5 * std::pow (std::log2 (bpm / 110.0) / 0.9, 2.0));
        // the backbeat (snares, claps) repeats every bar only at the right tempo
        double bar = 0, norm = 0;
        const auto& mid = odf[1];
        for (int f = 0; f + (int) (lag * 4) + 2 < F; f += 1)
        {
            const double fr = f + lag * 4; const int i0 = (int) fr; const double t = fr - i0;
            bar += mid[(size_t) f] * (mid[(size_t) i0] * (1 - t) + mid[(size_t) i0 + 1] * t);
            norm += mid[(size_t) f] * mid[(size_t) f];
        }
        return s * (0.6 + 0.4 * prior) * (0.7 + 0.6 * (norm > 0 ? bar / norm : 0.0));
    };
    // the hats' grid: the most common distance between hats is a 16th or an 8th
    double tatum = 0;
    {
        std::vector<double> hp;
        for (auto& h : an->hits) if (h.family == Family::hat || h.family == Family::openHat) hp.push_back ((double) h.pos / sr);
        std::sort (hp.begin(), hp.end());
        std::vector<double> hist (400, 0.0);        // 1 ms bins, 0-400 ms
        for (size_t i = 1; i < hp.size(); ++i)
        {
            const double d = hp[i] - hp[i - 1];
            if (d > 0.06 && d < 0.4) for (int k = -3; k <= 3; ++k) { const int b = (int) std::round (d * 1000) + k; if (b >= 0 && b < 400) hist[(size_t) b] += 1.0 - std::abs (k) / 4.0; }
        }
        const auto it = std::max_element (hist.begin(), hist.end());
        if (hp.size() > 32 && *it > hp.size() * 0.15) tatum = (double) std::distance (hist.begin(), it) / 1000.0;
    }
    auto gridFits = [tatum] (double b)
    {
        if (tatum <= 0) return 1.0;
        const double s16 = 60.0 / b / 4, s8 = s16 * 2, t3 = 60.0 / b / 6;    // 16ths, 8ths, 16th triplets
        for (double g : { s16, s8, t3 }) if (std::abs (tatum - g) / g < 0.04) return g == t3 ? 1.1 : 1.35;
        return 1.0;
    };
    double bestBpm = 120, best = -1;
    for (double b = 60; b <= 200; b += 0.5) { const double s = score (b) * gridFits (b); if (s > best) { best = s; bestBpm = b; } }
    for (double b = bestBpm - 0.5; b <= bestBpm + 0.5; b += 0.05) { const double s = score (b) * gridFits (b); if (s > best) { best = s; bestBpm = b; } }
    if (std::abs (bestBpm - std::round (bestBpm)) < 0.3) bestBpm = std::round (bestBpm);
    an->bpm = bestBpm;

    // phase of the 16th grid from all hits, then the beat and the downbeat from the kicks
    const double beat = 60.0 / bestBpm * sr, sixteenth = beat / 4;
    double re = 0, im = 0;
    for (auto& h : an->hits)
    {
        const double w = h.family == Family::kick ? 1.0 : h.family == Family::hat ? 0.6 : 0.4;
        const double ph = juce::MathConstants<double>::twoPi * (double) h.pos / sixteenth;
        re += w * h.strength * std::cos (ph); im += w * h.strength * std::sin (ph);
    }
    const double phase16 = std::fmod (std::atan2 (im, re) / juce::MathConstants<double>::twoPi * sixteenth + sixteenth, sixteenth);
    const double barLen = beat * 4;
    double bestOff = phase16, bestKick = -1;
    std::array<double, 16> scores {};
    for (int s = 0; s < 16; ++s)
    {
        const double off = phase16 + s * sixteenth;
        double k = 0;
        for (auto& h : an->hits)
        {
            if (h.family != Family::kick && h.family != Family::snare && h.family != Family::clap) continue;
            const double posInBar = std::fmod ((double) h.pos - off + barLen * 1000, barLen) / sixteenth;   // 0..16
            const int step = (int) std::round (posInBar) % 16;
            if (std::abs (posInBar - std::round (posInBar)) > 0.35) continue;
            if (h.family == Family::kick) k += (step == 0 ? 2.0 : step == 8 ? 0.8 : step % 4 == 0 ? 0.5 : 0.0) * h.strength;
            else k += (step == 4 || step == 12 ? 0.6 : step == 8 ? 0.4 : 0.0) * h.strength;
        }
        scores[(size_t) s] = k;
        if (k > bestKick) { bestKick = k; bestOff = off; }
    }
    {   // several downbeats fit about as well (a kick on every beat): the bar starts where the music starts
        juce::int64 firstKick = -1;
        for (auto& h : an->hits) if (h.family == Family::kick && h.strength > 0.5f && (firstKick < 0 || h.pos < firstKick)) firstKick = h.pos;
        if (firstKick >= 0)
            for (int s = 0; s < 16; ++s)
            {
                const double off = phase16 + s * sixteenth;
                const double rel = std::fmod ((double) firstKick - off + barLen * 1000, barLen);
                if (scores[(size_t) s] >= bestKick * 0.85 && (rel < sixteenth * 0.5 || rel > barLen - sixteenth * 0.5)) { bestOff = off; break; }
            }
    }
    an->beatOffset = (juce::int64) std::fmod (bestOff, barLen);

    // ---------------------------------------------------------- drum sections (highlighted)
    {
        const int secs = (int) (an->length / sr) + 1;
        std::vector<float> dens ((size_t) secs, 0.0f);
        for (auto& h : an->hits) dens[(size_t) juce::jmin (secs - 1, (int) (h.pos / sr))] += h.strength;
        std::vector<float> sm ((size_t) secs);
        for (int i = 0; i < secs; ++i) { float s = 0; int n = 0; for (int k = i - 2; k <= i + 2; ++k) if (k >= 0 && k < secs) { s += dens[(size_t) k]; ++n; } sm[(size_t) i] = s / (float) n; }
        const float mx = sm.empty() ? 0 : *std::max_element (sm.begin(), sm.end());
        int start = -1;
        for (int i = 0; i <= secs; ++i)
        {
            const bool on = i < secs && sm[(size_t) i] > mx * 0.45f;
            if (on && start < 0) start = i;
            if (! on && start >= 0)
            {
                if (i - start >= 3) an->drumSections.push_back ({ (juce::int64) (start * sr), (juce::int64) juce::jmin ((double) an->length, i * sr) });
                start = -1;
            }
        }
    }

    // ---------------------------------------------------------- kits (a song can change drums)
    // windows of 4 s; a new kit = a different set of sounds that lasts 16 s in a row and has sounds of its own
    std::vector<std::set<int>> kitGroups;            // groups of each kit
    std::vector<int> kitOfWindow;
    const double winS = 4.0;
    const int nWin = (int) std::ceil ((double) an->length / sr / winS);
    {
        std::vector<std::map<int, int>> wg ((size_t) nWin);
        for (auto& h : an->hits) if (h.group >= 0) wg[(size_t) juce::jmin (nWin - 1, (int) ((double) h.pos / sr / winS))][h.group]++;
        std::vector<std::set<int>> win ((size_t) nWin);
        for (int w = 0; w < nWin; ++w) for (auto& [g, n] : wg[(size_t) w]) if (n >= 2) win[(size_t) w].insert (g);
        auto jacc = [] (const std::set<int>& x, const std::set<int>& y)
        {
            size_t inter = 0; for (int v : x) inter += y.count (v);
            const size_t uni = x.size() + y.size() - inter;
            return uni == 0 ? 1.0f : (float) inter / (float) uni;
        };
        kitOfWindow.assign ((size_t) nWin, -1);
        struct Seg { int kit; std::map<int, int> counts; int windows = 0; std::set<int> sig() const
        { std::set<int> s; for (auto& [g, n] : counts) if (n * 4 >= windows) s.insert (g); return s; } };
        std::vector<Seg> kits;
        int cur = -1;
        for (int w = 0; w < nWin; ++w)
        {
            if (win[(size_t) w].size() < 2) { kitOfWindow[(size_t) w] = cur; continue; }
            if (cur < 0) { kits.push_back ({ 0, {}, 0 }); cur = 0; }
            const auto sig = kits[(size_t) cur].sig();
            // a different kit: sounds of its own appear and sounds of this kit are gone, for 16 s in a row
            auto differs = [&] (const std::set<int>& x, std::set<int>& ownOut)
            {
                int own = 0, lost = 0;
                ownOut.clear();
                for (int g : x) if (! sig.count (g)) { ++own; ownOut.insert (g); }
                for (int g : sig) lost += x.count (g) ? 0 : 1;
                return own >= 1 && lost >= 1 && own + lost >= 2 && jacc (x, sig) < 0.6f;
            };
            if (std::getenv ("ECLIPSE_DEBUG"))
            {
                std::cerr << "win " << w << " kit " << cur << " groups:";
                for (int g : win[(size_t) w]) std::cerr << ' ' << g << familyName (an->groups[(size_t) g].family);
                std::cerr << "\n";
            }
            bool change = false;
            std::set<int> own0, ownK;
            if (set.splitKits && kits[(size_t) cur].windows >= 2 && w + 3 < nWin && differs (win[(size_t) w], own0))
            {
                change = true;
                for (int k = w + 1; k < w + 4 && change; ++k)
                {
                    change = win[(size_t) k].size() >= 2 && differs (win[(size_t) k], ownK);
                    bool shared = false;
                    for (int g : ownK) shared |= own0.count (g) > 0;
                    change = change && shared;
                }
            }
            if (change)
            {   // back to a kit we already had (verse / chorus)?
                int found = -1;
                for (int k = 0; k < (int) kits.size(); ++k) if (k != cur && jacc (win[(size_t) w], kits[(size_t) k].sig()) >= 0.45f) found = k;
                if (found < 0 && (int) kits.size() < numBanks) { kits.push_back ({ (int) kits.size(), {}, 0 }); found = (int) kits.size() - 1; }
                if (found >= 0) cur = found;
            }
            kitOfWindow[(size_t) w] = cur;
            for (int g : win[(size_t) w]) kits[(size_t) cur].counts[g]++;
            kits[(size_t) cur].windows++;
        }
        // the main kit (the one that plays the longest) is kit A
        {
            std::vector<int> order ((size_t) kits.size());
            std::iota (order.begin(), order.end(), 0);
            std::stable_sort (order.begin(), order.end(), [&] (int x, int y) { return kits[(size_t) x].windows > kits[(size_t) y].windows; });
            std::vector<int> newIndex ((size_t) kits.size());
            for (size_t i = 0; i < order.size(); ++i) newIndex[(size_t) order[i]] = (int) i;
            for (auto& k : kitOfWindow) if (k >= 0) k = newIndex[(size_t) k];
            std::vector<Seg> sorted;
            for (int i : order) sorted.push_back (kits[(size_t) i]);
            kits = std::move (sorted);
        }
        for (auto& k : kits) kitGroups.push_back (k.sig());
        if (kitGroups.empty()) kitGroups.push_back ({});
        // windows before the first drums belong to the first kit
        for (auto& k : kitOfWindow) if (k < 0) k = 0;
        // groups no kit claimed go to the kit where they were heard the most
        for (int g = 0; g < (int) an->groups.size(); ++g)
        {
            bool inAny = false;
            for (auto& k : kitGroups) inAny |= k.count (g) > 0;
            if (inAny) continue;
            std::vector<int> n (kitGroups.size(), 0);
            for (int hi : an->groups[(size_t) g].hits) n[(size_t) kitOfWindow[(size_t) juce::jmin (nWin - 1, (int) ((double) an->hits[(size_t) hi].pos / sr / winS))]]++;
            kitGroups[(size_t) std::distance (n.begin(), std::max_element (n.begin(), n.end()))].insert (g);
        }
        // sections for the source view (A, B... under the waveform)
        if (kitGroups.size() > 1)
            for (int w = 0; w < nWin; ++w)
            {
                const auto r = juce::Range<juce::int64> ((juce::int64) (w * winS * sr), juce::jmin (an->length, (juce::int64) ((w + 1) * winS * sr)));
                if (! an->kitSections.empty() && an->kitSections.back().second == kitOfWindow[(size_t) w] && an->kitSections.back().first.getEnd() == r.getStart())
                    an->kitSections.back().first.setEnd (r.getEnd());
                else an->kitSections.push_back ({ r, kitOfWindow[(size_t) w] });
            }
    }
    an->numKits = (int) kitGroups.size();
    for (int p = 0; p < 3; ++p) res.parts[(size_t) p] = partBuf[(size_t) p];
    an->parts = pm;
    auto kitAt = [&] (juce::int64 pos) { return kitOfWindow.empty() ? 0 : kitOfWindow[(size_t) juce::jlimit (0, nWin - 1, (int) ((double) pos / sr / winS))]; };

    // ---------------------------------------------------------- pads (one bank per kit)
    report (0.8f, "Isolating sounds");
    static const std::array<std::vector<int>, numFamilies> preferred {{
        { 0, 1, 5 }, { 2, 4 }, { 3, 14 }, { 6, 8 }, { 10 }, { 7, 9 }, { 11, 12, 15 }, { 13 } }};
    std::vector<std::vector<int>> padIn (kitGroups.size(), std::vector<int> (an->groups.size(), -1));   // kit -> group -> pad
    res.padOfGroup.assign (an->groups.size(), -1);
    {
        std::array<bool, numPads> used {};
        std::vector<int> rank (an->groups.size(), 0);
        std::array<int, numFamilies> count {};
        for (size_t gi = 0; gi < an->groups.size(); ++gi) rank[gi] = count[(size_t) an->groups[gi].family]++;
        for (int k = 0; k < (int) kitGroups.size(); ++k)
        {
            std::vector<int> order (kitGroups[(size_t) k].begin(), kitGroups[(size_t) k].end());
            if (kitGroups.size() == 1) { order.resize (an->groups.size()); std::iota (order.begin(), order.end(), 0); }
            std::stable_sort (order.begin(), order.end(), [&] (int x, int y) { return rank[(size_t) x] < rank[(size_t) y]; });
            const int base = k * padsPerBank;
            // 1. the standard places (C1 kick, D1 snare, D#1 clap, F#1 hat, A#1 open hat...)
            for (int gi : order)
                for (int slot : preferred[(size_t) an->groups[(size_t) gi].family])
                    if (! used[(size_t) (base + slot)]) { used[(size_t) (base + slot)] = true; padIn[(size_t) k][(size_t) gi] = base + slot; break; }
            // 2. the rest: free pads of this bank that no family of the kit wants, then any free pad
            std::array<bool, padsPerBank> wanted {};
            for (int gi : order) for (int slot : preferred[(size_t) an->groups[(size_t) gi].family]) wanted[(size_t) slot] = true;
            for (int gi : order)
            {
                if (padIn[(size_t) k][(size_t) gi] >= 0) continue;
                for (int pass = 0; pass < 3 && padIn[(size_t) k][(size_t) gi] < 0; ++pass)
                    for (int slot = pass < 2 ? base : 0; slot < (pass < 2 ? base + padsPerBank : numPads); ++slot)
                        if (! used[(size_t) slot] && (pass > 0 || ! wanted[(size_t) (slot - base)]))
                        { used[(size_t) slot] = true; padIn[(size_t) k][(size_t) gi] = slot; break; }
            }
        }
        for (int g = 0; g < (int) an->groups.size(); ++g)
            for (int k = 0; k < (int) kitGroups.size() && res.padOfGroup[(size_t) g] < 0; ++k) res.padOfGroup[(size_t) g] = padIn[(size_t) k][(size_t) g];
    }
    {
        std::vector<std::array<int, numFamilies>> seen (kitGroups.size());
        std::vector<PadSound> made (an->groups.size());
        int done = 0;
        for (int gi = 0; gi < (int) an->groups.size(); ++gi)
        {
            if (res.padOfGroup[(size_t) gi] < 0) continue;
            const int srcIdx = an->groups[(size_t) gi].source;
            made[(size_t) gi] = makePad (srcIdx >= 0 && partBuf[(size_t) srcIdx] ? *partBuf[(size_t) srcIdx] : *res.extractFrom, *an, gi, 0);
            if (cancelled (cancel)) return res;
            report (0.8f + 0.15f * (float) (++done) / (float) an->groups.size(), "Isolating sounds");
        }
        // sounds shared by two kits are on both banks
        for (int k = 0; k < (int) kitGroups.size(); ++k)
            for (int gi = 0; gi < (int) an->groups.size(); ++gi)
            {
                const int pad = padIn[(size_t) k][(size_t) gi];
                if (pad < 0 || made[(size_t) gi].empty()) continue;
                const auto fam = an->groups[(size_t) gi].family;
                auto p = made[(size_t) gi];
                const int n = ++seen[(size_t) k][(size_t) fam];
                p.name = juce::String (familyName (fam)) + (n > 1 ? " " + juce::String (n) : juce::String());
                res.pads[(size_t) pad] = std::move (p);
            }
    }

    // ---------------------------------------------------------- patterns (per kit)
    auto jaccI = [] (const std::set<int>& x, const std::set<int>& y)
    {
        size_t inter = 0; for (int v : x) inter += y.count (v);
        const size_t uni = x.size() + y.size() - inter;
        return uni == 0 ? 1.0f : (float) inter / (float) uni;
    };
    std::vector<std::pair<int, Pattern>> allFound;
    for (int kit = 0; kit < (int) kitGroups.size(); ++kit)
    {
        std::map<int, std::vector<std::pair<int, float>>> bars;          // bar -> (pad*64+tick, strength)
        for (auto& h : an->hits)
        {
            if (h.group < 0 || kitAt (h.pos) != kit) continue;
            int pad = padIn[(size_t) kit][(size_t) h.group];
            if (pad < 0) pad = res.padOfGroup[(size_t) h.group];
            if (pad < 0) continue;
            const double rel = (double) (h.pos - an->beatOffset) / barLen;
            if (rel < -0.02) continue;
            int bar = (int) std::floor (rel);
            const int q = h.family == Family::hat ? 2 : 4;
            int tick = (int) std::round ((rel - bar) * 64.0 / q) * q;
            if (tick >= 64) { tick -= 64; ++bar; }
            bars[juce::jmax (0, bar)].push_back ({ pad * 64 + tick, h.strength });
        }
        std::vector<int> barIds;
        std::vector<std::set<int>> sig, famSig;   // famSig: the same with the family instead of the pad (to compare bars)
        std::map<int, int> famOfPad;
        for (int g = 0; g < (int) an->groups.size(); ++g)
        {
            const int pd = padIn[(size_t) kit][(size_t) g] >= 0 ? padIn[(size_t) kit][(size_t) g] : res.padOfGroup[(size_t) g];
            if (pd >= 0) famOfPad[pd] = (int) an->groups[(size_t) g].family;
        }
        for (auto& [b, v] : bars)
        {
            if (v.size() < 3) continue;
            std::set<int> st, sf;
            for (auto& e : v) { st.insert (e.first); sf.insert (famOfPad[e.first / 64] * 64 + (e.first % 64) / 4 * 4); }
            barIds.push_back (b); sig.push_back (std::move (st)); famSig.push_back (std::move (sf));
        }
        const int nb = (int) sig.size();
        std::vector<int> cluster ((size_t) nb, -1), medoids;
        std::vector<int> support ((size_t) nb, 0);
        for (int i = 0; i < nb; ++i) for (int j = 0; j < nb; ++j) if (jaccI (famSig[(size_t) i], famSig[(size_t) j]) >= 0.6f) ++support[(size_t) i];
        std::vector<int> order ((size_t) nb);
        std::iota (order.begin(), order.end(), 0);
        std::sort (order.begin(), order.end(), [&] (int x, int y) { return support[(size_t) x] > support[(size_t) y]; });
        for (int i : order)
        {
            if (cluster[(size_t) i] >= 0) continue;
            const int c = (int) medoids.size();
            medoids.push_back (i);
            for (int j = 0; j < nb; ++j) if (cluster[(size_t) j] < 0 && jaccI (famSig[(size_t) i], famSig[(size_t) j]) >= 0.6f) cluster[(size_t) j] = c;
        }
        std::map<int, int> clusterOfBar;
        for (int i = 0; i < nb; ++i) clusterOfBar[barIds[(size_t) i]] = cluster[(size_t) i];
        auto makeBar = [&] (int c, int barOffsetTicks, Pattern& p)
        {
            // consensus of the bars of the cluster: a note is in the pattern if it is in at least half of them
            std::map<int, std::pair<float, int>> acc;
            int members = 0;
            for (int i = 0; i < nb; ++i)
            {
                if (cluster[(size_t) i] != c) continue;
                ++members;
                std::set<int> seenKey;
                for (auto& e : bars[barIds[(size_t) i]])
                    if (seenKey.insert (e.first).second) { acc[e.first].first += e.second; acc[e.first].second++; }
            }
            std::vector<int> keys;
            if (members >= 3) { for (auto& [key, av] : acc) if (av.second * 2 >= members) keys.push_back (key); }
            else keys.assign (sig[(size_t) medoids[(size_t) c]].begin(), sig[(size_t) medoids[(size_t) c]].end());
            for (int key : keys)
            {
                auto& av = acc[key];
                Note n;
                n.pad = key / 64; n.tick = key % 64 + barOffsetTicks;
                n.length = 4;
                n.velocity = juce::jlimit (0.2f, 1.0f, 0.35f + 0.65f * (av.second > 0 ? av.first / (float) av.second : 0.6f));
                p.notes.push_back (n);
            }
        };
        std::vector<std::pair<int, Pattern>> found;
        std::set<std::pair<int, int>> donePairs;
        for (int c = 0; c < (int) medoids.size() && found.size() < 5; ++c)
        {
            int count = 0;
            std::map<int, int> next;
            for (int i = 0; i < nb; ++i)
            {
                if (cluster[(size_t) i] != c) continue;
                ++count;
                auto it = clusterOfBar.find (barIds[(size_t) i] + 1);
                if (it != clusterOfBar.end()) next[it->second]++;
            }
            if (count < 2 && ! found.empty()) continue;
            int nextC = -1, nextN = 0;
            for (auto& [k2, v] : next) if (v > nextN) { nextN = v; nextC = k2; }
            Pattern p;
            p.kit = kit;
            if (nextC >= 0 && nextC != c && nextN >= juce::jmax (2, count * 6 / 10))
            {
                if (donePairs.count ({ nextC, c })) continue;
                donePairs.insert ({ c, nextC });
                p.bars = 2;
                makeBar (c, 0, p);
                makeBar (nextC, 64, p);
            }
            else { p.bars = 1; makeBar (c, 0, p); }
            found.push_back ({ count, std::move (p) });
        }
        // the main groove first: how often it plays, but a sparse bar (an intro with 3 notes) counts less
        for (auto& f : found) { const float d = juce::jmin (1.0f, (float) f.second.notes.size() / (10.0f * (float) f.second.bars)); f.first = (int) (f.first * d * d * 100); }
        std::stable_sort (found.begin(), found.end(), [] (auto& x, auto& y) { return x.first > y.first; });
        // the kit's best patterns first; up to 5 in all
        const size_t take = kitGroups.size() == 1 ? 5 : std::max<size_t> (1, 5 / kitGroups.size());
        for (size_t i = 0; i < found.size() && i < take; ++i) allFound.push_back (std::move (found[i]));
    }
    // all the kits together: the main groove of the song first
    std::stable_sort (allFound.begin(), allFound.end(), [] (auto& x, auto& y) { return x.first > y.first; });
    for (auto& f : allFound)
    {
        f.second.name = "Original";
        an->patterns.push_back (std::move (f.second));
    }
    report (1.0f, "Done");
    return res;
}

} // namespace eclipse
