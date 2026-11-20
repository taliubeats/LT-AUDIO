#include "Analysis.h"

namespace cosmos
{

//============================================================================== music
namespace music
{
    static const char* sharpNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatNames[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

    juce::String noteName (int pc, bool flats)
    {
        pc = ((pc % 12) + 12) % 12;
        return flats ? flatNames[pc] : sharpNames[pc];
    }

    juce::String noteNameWithOctave (int midi, bool flats)
    {
        return midi < 0 ? juce::String ("-") : noteName (midi, flats) + juce::String (midi / 12 - 1);
    }

    bool keyUsesFlats (int key)
    {
        if (key < 0) return false;
        auto pc = key % 12;
        if (key < 12) return pc == 5 || pc == 10 || pc == 3 || pc == 8 || pc == 1 || pc == 6;   // F Bb Eb Ab Db Gb
        return pc == 2 || pc == 7 || pc == 0 || pc == 5 || pc == 10 || pc == 3;                  // Dm Gm Cm Fm Bbm Ebm
    }

    juce::String keyName (int key)
    {
        if (key < 0) return "-";
        return noteName (key % 12, keyUsesFlats (key)) + (key < 12 ? " MAJ" : " MIN");
    }

    const std::vector<ChordType>& chordTypes()
    {
        static const std::vector<ChordType> t {
            { "",     { 0, 4, 7, 0 },  3 },
            { "m",    { 0, 3, 7, 0 },  3 },
            { "7",    { 0, 4, 7, 10 }, 4 },
            { "maj7", { 0, 4, 7, 11 }, 4 },
            { "m7",   { 0, 3, 7, 10 }, 4 },
            { "dim",  { 0, 3, 6, 0 },  3 },
            { "m7b5", { 0, 3, 6, 10 }, 4 },
            { "aug",  { 0, 4, 8, 0 },  3 },
            { "sus4", { 0, 5, 7, 0 },  3 },
            { "sus2", { 0, 2, 7, 0 },  3 },
        };
        return t;
    }

    juce::String chordName (int root, int type, int bassPc, bool flats)
    {
        if (root < 0) return "-";
        auto s = noteName (root, flats) + chordTypes()[(size_t) type].suffix;
        if (bassPc >= 0 && bassPc != root) s << "/" << noteName (bassPc, flats);
        return s;
    }

    juce::String romanNumeral (int key, int root, int type)
    {
        if (key < 0 || root < 0) return "";
        static const char* major[] = { "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII" };
        static const char* minor[] = { "I", "bII", "II", "III", "#III", "IV", "bV", "V", "VI", "#VI", "VII", "#VII" };
        auto interval = ((root - key % 12) % 12 + 12) % 12;
        juce::String n = key < 12 ? major[interval] : minor[interval];
        auto t = juce::String (chordTypes()[(size_t) type].suffix);
        auto minorQuality = t.startsWith ("m") && ! t.startsWith ("maj");
        if (minorQuality || t.startsWith ("dim"))
            n = n.toLowerCase();
        if (t == "dim") n << juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
        else if (t == "m7b5") n << juce::String (juce::CharPointer_UTF8 ("\xc3\xb8"));
        else if (t == "aug") n << "+";
        else if (t.endsWith ("7")) n << (t == "maj7" ? "M7" : "7");
        return n;
    }
}

//============================================================================== Analyzer
Analyzer::Analyzer()
{
    window.resize (fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
    work.resize (fftSize * 2);
    mag.resize (numBins);
    magL.resize (numBins);
    magR.resize (numBins);
    cplxL.resize (fftSize * 2);
    cplxR.resize (fftSize * 2);
    bigWindow.resize (fftSize * 2);
    for (int i = 0; i < fftSize * 2; ++i)
        bigWindow[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize * 2 - 1));
    bigWork.resize (fftSize * 4);
    bigMag.resize (fftSize + 1);
    for (auto& h : hist) h.assign (historySize, 0.0f);
    spectrumDb.assign (numBins, -120.0f);
    peakDb.assign (numBins, -120.0f);
    leftDb.assign (numBins, -120.0f);
    rightDb.assign (numBins, -120.0f);
    noveltyRing.assign (512, 0.0f);
    prepare (48000.0);
}

void Analyzer::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    hopLength = juce::jmax (256, (int) std::round (sr * 1024.0 / 48000.0));
    hopRate = sr / hopLength;
    waveColLength = juce::jmax (64, (int) std::round (sr * 480.0 / 48000.0));   // ~10 s across a wide panel
    clear();
}

void Analyzer::clear()
{
    for (auto& h : hist) std::fill (h.begin(), h.end(), 0.0f);
    histWrite = 0;
    hopFill = 0;
    std::fill (spectrumDb.begin(), spectrumDb.end(), -120.0f);
    std::fill (peakDb.begin(), peakDb.end(), -120.0f);
    std::fill (leftDb.begin(), leftDb.end(), -120.0f);
    std::fill (rightDb.begin(), rightDb.end(), -120.0f);
    logDb.fill (-120.0f); logRaw.fill (-120.0f);
    thirdDb.fill (-120.0f); thirdWidth.fill (0.0f);
    wave = {};
    waveWrite = 0; waveFill = 0; waveAcc = {};
    momentaryHistory.fill (-200.0f); shortHistory.fill (-200.0f); loudWrite = 0;
    hits.clear();
    hitFlash = {};
    bandPrev = bandPrev2 = fluxMean = fluxDev = {};
    std::fill (noveltyRing.begin(), noveltyRing.end(), 0.0f);
    chroma = chromaKey = chromaFast = chromaSlow = {};
    key = chordRoot = bassPc = bassMidi = -1;
    chordText = "-";
    chordStrip = {};
    barChordVotes = {};
    kickSpectrum.fill (-90.0f); bassSpectrum.fill (-90.0f);
    kickEnv = {}; bassEnv = {}; envWrite = 0;
    currentBar = {}; previousBar = {};
    freeBeats = 0; freeBpm = 0; bpm = 0;
    downbeatScore = {};
    downbeatOffset = 0;
    lastBarIndex = lastBeatIndex = -1;
    barTime = -1.0;
}

float Analyzer::logBandFrequency (float band) noexcept
{
    return 25.0f * std::pow (800.0f, band / (float) (numLogBands - 1));   // 25 Hz .. 20 kHz
}

float Analyzer::thirdFrequency (int i) noexcept
{
    return 25.0f * std::pow (2.0f, (float) i / 3.0f);
}

void Analyzer::copyHistory (int channel, float* dest, int n) const
{
    auto& h = hist[(size_t) (channel & 1)];
    n = juce::jmin (n, historySize);
    auto start = (histWrite - n + historySize) % historySize;
    auto first = juce::jmin (n, historySize - start);
    std::copy (h.begin() + start, h.begin() + start + first, dest);
    if (first < n) std::copy (h.begin(), h.begin() + (n - first), dest + first);
}

void Analyzer::setHostTiming (bool valid, bool isPlaying, double newBpm, double ppq, double barStart, int num, int den,
                              juce::int64 at)
{
    hostValid = valid && newBpm > 0;
    playing = isPlaying;
    hostBpm = newBpm;
    hostPpq = ppq;
    hostBarStart = barStart;
    hostNum = juce::jmax (1, num);
    hostDen = juce::jmax (1, den);
    hostAt = at;
}

void Analyzer::pushLoudness (float m, float s)
{
    momentaryHistory[(size_t) loudWrite] = m;
    shortHistory[(size_t) loudWrite] = s;
    loudWrite = (loudWrite + 1) % loudHistory;
}

//============================================================================== audio in
void Analyzer::push (const float* left, const float* right, int n)
{
    const float rmsCoeff = (float) std::exp (-1.0 / (0.3 * sr));
    const float lowCoeff = (float) std::exp (-juce::MathConstants<double>::twoPi * 200.0 / sr);

    for (int i = 0; i < n; ++i)
    {
        auto l = left[i], r = right != nullptr ? right[i] : left[i];
        hist[0][(size_t) histWrite] = l;
        hist[1][(size_t) histWrite] = r;
        histWrite = (histWrite + 1) % historySize;
        ++totalSamples;

        blockPeak[0] = juce::jmax (blockPeak[0], std::abs (l));
        blockPeak[1] = juce::jmax (blockPeak[1], std::abs (r));
        rms[0] = rms[0] * rmsCoeff + l * l * (1.0f - rmsCoeff);
        rms[1] = rms[1] * rmsCoeff + r * r * (1.0f - rmsCoeff);

        auto m = 0.5f * (l + r);
        lowState = m + (lowState - m) * lowCoeff;
        auto& w = waveAcc;
        w.minL = juce::jmin (w.minL, l); w.maxL = juce::jmax (w.maxL, l);
        w.minR = juce::jmin (w.minR, r); w.maxR = juce::jmax (w.maxR, r);
        w.low = juce::jmax (w.low, std::abs (lowState));
        w.high = juce::jmax (w.high, std::abs (m - lowState));
        if (++waveFill >= waveColLength)
        {
            wave[(size_t) waveWrite] = w;
            waveWrite = (waveWrite + 1) % numWaveCols;
            waveAcc = {};
            waveFill = 0;
        }

        if (++hopFill >= hopLength)
        {
            hopFill = 0;
            hop();
        }
    }
    phase = (double) totalSamples / sr;
}

void Analyzer::computeSpectrum (const float* x, float* outMag, std::vector<float>* complexOut)
{
    for (int i = 0; i < fftSize; ++i) work[(size_t) i] = x[i] * window[(size_t) i];
    std::fill (work.begin() + fftSize, work.end(), 0.0f);
    fft.performRealOnlyForwardTransform (work.data(), true);
    const float norm = 4.0f / (float) fftSize;   // Hann coherent gain 0.5, single-sided x2
    for (int k = 0; k < numBins; ++k)
    {
        auto re = work[(size_t) (2 * k)], im = work[(size_t) (2 * k + 1)];
        outMag[k] = std::sqrt (re * re + im * im) * norm;
    }
    if (complexOut != nullptr) std::copy (work.begin(), work.end(), complexOut->begin());
}

static inline float toDb (float lin) { return lin > 1.0e-7f ? 20.0f * std::log10 (lin) : -140.0f; }

void Analyzer::hop()
{
    const int big = fftSize * 2;
    std::vector<float>& buf = yinBuf;   // reused scratch
    buf.resize ((size_t) big);
    {
        std::vector<float> r ((size_t) big);
        copyHistory (0, buf.data(), big);
        copyHistory (1, r.data(), big);
        for (int i = 0; i < big; ++i) buf[(size_t) i] = 0.5f * (buf[(size_t) i] + r[(size_t) i]);
    }
    computeSpectrum (buf.data() + fftSize, mag.data(), nullptr);   // newest 4096 for onsets
    if (++hopCount % 2 == 0)
    {
        for (int i = 0; i < big; ++i) bigWork[(size_t) i] = buf[(size_t) i] * bigWindow[(size_t) i];
        std::fill (bigWork.begin() + big, bigWork.end(), 0.0f);
        bigFft.performRealOnlyForwardTransform (bigWork.data(), true);
        for (int k = 0; k <= fftSize; ++k)
            bigMag[(size_t) k] = std::sqrt (bigWork[(size_t) (2 * k)] * bigWork[(size_t) (2 * k)] + bigWork[(size_t) (2 * k + 1)] * bigWork[(size_t) (2 * k + 1)]) * (2.0f / (float) fftSize);
    }

    // octave bands
    for (int o = 0; o < numOctaves; ++o)
    {
        auto c = 31.25f * std::pow (2.0f, (float) o);
        auto lo = juce::jmax (1, (int) (c / 1.414f / binFrequency (1))), hi = juce::jmin (numBins - 1, (int) (c * 1.414f / binFrequency (1)) + 1);
        float p = 0;
        for (int k = lo; k <= hi; ++k) p += mag[(size_t) k] * mag[(size_t) k];
        octaveDb[(size_t) o] = 10.0f * std::log10 (p + 1.0e-12f);
    }

    updateBarClock();
    detectOnsets (mag.data());
    if (hopCount % 2 == 0) updateChroma();

    // low end: spectra and envelopes
    auto bandLevel = [this] (float lo, float hi)
    {
        float p = 0;
        for (int k = juce::jmax (1, (int) (lo / binFrequency (1))); k <= juce::jmin (numBins - 1, (int) (hi / binFrequency (1))); ++k)
            p += mag[(size_t) k] * mag[(size_t) k];
        return 10.0f * std::log10 (p + 1.0e-12f);
    };
    auto kickBand = bandLevel (40.0f, 100.0f), bassBand = bandLevel (100.0f, 300.0f);
    kickEnv[(size_t) envWrite] = juce::jlimit (0.0f, 1.0f, (kickBand + 60.0f) / 60.0f);
    bassEnv[(size_t) envWrite] = juce::jlimit (0.0f, 1.0f, (bassBand + 60.0f) / 60.0f);
    envWrite = (envWrite + 1) % envLength;

    auto sinceKick = (double) (totalSamples - lastKickTime) / sr;
    if (pendingKickCapture > 0 && --pendingKickCapture == 0)
        for (int k = 0; k < lowBins; ++k)
            kickSpectrum[(size_t) k] = kickSpectrum[(size_t) k] * 0.6f + toDb (mag[(size_t) k]) * 0.4f;
    if (sinceKick > 0.22 && sinceKick < 0.8 && pendingKickCapture == 0)
        for (int k = 0; k < lowBins; ++k)
            bassSpectrum[(size_t) k] = bassSpectrum[(size_t) k] * 0.94f + toDb (mag[(size_t) k]) * 0.06f;
    if (sinceKick > 0.06 && sinceKick < 0.14) duckPost = duckPost * 0.85f + bassBand * 0.15f;
    if (sinceKick > 0.3 && sinceKick < 0.8)  duckPre = duckPre * 0.95f + bassBand * 0.05f;
    if (sinceKick < 2.0) duckDb = juce::jmax (0.0f, duckPre - duckPost);
    kickDb = *std::max_element (kickSpectrum.begin() + 1, kickSpectrum.end());
    bassDb = *std::max_element (bassSpectrum.begin() + 1, bassSpectrum.end());

    // bar grid
    if (barTime >= 0.0)
    {
        auto step = juce::jlimit (0, gridSteps - 1, (int) ((barTime - std::floor (barTime)) * gridSteps));
        for (int o = 0; o < numOctaves; ++o)
        {
            auto v = juce::jlimit (0.0f, 1.0f, (octaveDb[(size_t) o] + 66.0f) / 60.0f);
            auto& e = currentBar.energy[(size_t) step][(size_t) o];
            e = juce::jmax (e, v);
        }
    }

    if (--tempoCountdown <= 0)
    {
        tempoCountdown = (int) (hopRate * 0.5);
        updateTempo();
    }
}

void Analyzer::detectOnsets (const float* m)
{
    // 0 kick, 1 snare body, 2 snare crack, 3 hats. A snare needs body AND crack together, so
    // kick leakage (no crack) and hats (no body) do not read as snares.
    struct Band { float lo, hi, minFlux; };
    static const Band bands[4] = { { 35.0f, 110.0f, 3.5f }, { 260.0f, 750.0f, 3.0f }, { 1500.0f, 5000.0f, 3.0f }, { 7000.0f, 16000.0f, 3.0f } };
    auto binHz = binFrequency (1);
    float total = 0, all = 0;
    for (int k = 1; k < numBins; ++k) all += m[k] * m[k];
    const auto eAll = 10.0f * std::log10 (all + 1.0e-12f);
    std::array<float, 4> flux {};
    std::array<bool, 4> over {};
    for (int b = 0; b < 4; ++b)
    {
        auto& d = bands[b];
        float p = 0;
        for (int k = juce::jmax (1, (int) (d.lo / binHz)); k <= juce::jmin (numBins - 1, (int) (d.hi / binHz)); ++k) p += m[k] * m[k];
        auto e = 10.0f * std::log10 (p + 1.0e-12f);
        auto f = juce::jmax (0.0f, e - juce::jmax (bandPrev[(size_t) b], bandPrev2[(size_t) b]));
        bandPrev2[(size_t) b] = bandPrev[(size_t) b];
        bandPrev[(size_t) b] = e;
        flux[(size_t) b] = f;
        total += juce::jmin (f, 20.0f) / 20.0f;
        auto thr = fluxMean[(size_t) b] + 2.0f * fluxDev[(size_t) b] + d.minFlux;
        fluxMean[(size_t) b] = fluxMean[(size_t) b] * 0.97f + f * 0.03f;
        fluxDev[(size_t) b] = fluxDev[(size_t) b] * 0.97f + std::abs (f - fluxMean[(size_t) b]) * 0.03f;
        over[(size_t) b] = f > thr && e > -78.0f && e > eAll - 30.0f;   // a band must carry real energy, not leakage
    }
    const float minGap[3] = { 0.11f, 0.11f, 0.06f };
    const bool fired[3] = { over[0], over[1] && over[2], over[3] };
    const float strength[3] = { flux[0], 0.5f * (flux[1] + flux[2]), flux[3] };
    for (int k = 0; k < 3; ++k)
    {
        auto gap = (double) (totalSamples - lastHit[(size_t) k]) / sr;
        if (fired[k] && gap > minGap[k])
        {
            lastHit[(size_t) k] = totalSamples;
            addHit (k, juce::jlimit (0.2f, 1.0f, 0.3f + strength[k] / 18.0f));
        }
    }

    onsetFlux = juce::jlimit (0.0f, 1.0f, total * 0.75f);
    noveltyRing[(size_t) noveltyWrite] = total;
    noveltyWrite = (noveltyWrite + 1) % (int) noveltyRing.size();
}

void Analyzer::addHit (int kind, float strength)
{
    Hit h;
    h.kind = kind;
    h.strength = strength;
    h.time = totalSamples;
    h.barTime = barTime;
    hits.push_back (h);
    auto oldest = totalSamples - (juce::int64) (sr * 10.0);
    while (! hits.empty() && hits.front().time < oldest) hits.erase (hits.begin());
    hitFlash[(size_t) kind] = juce::jmax (hitFlash[(size_t) kind], strength);

    if (kind == Hit::kick)
    {
        lastKickTime = totalSamples;
        pendingKickCapture = 2;
        beatKickAcc[0] += strength;
    }
    if (barTime >= 0.0)
    {
        // hits are quantised to the nearest step (detection lands a few ms either side of the grid)
        auto pos = (barTime - std::floor (barTime)) * gridSteps;
        auto step = (int) std::round (pos);
        if (step >= gridSteps) previousBarPending |= 1 << kind;   // belongs to step 0 of the next bar
        else currentBar.hits[(size_t) step] |= 1 << kind;
    }
}

void Analyzer::updateChroma()
{
    const float* m = bigMag.data();
    const int nb = fftSize + 1;
    auto binHz = (float) (sr / (fftSize * 2));
    std::array<float, 12> frameC {}, lowC {};
    float energySum = 0, top = 0;
    auto kLo = juce::jmax (2, (int) (55.0f / binHz)), kHi = juce::jmin (nb - 2, (int) (4200.0f / binHz));
    for (int k = kLo; k <= kHi; ++k) { energySum += m[k] * m[k]; top = juce::jmax (top, m[k]); }
    // spectral peaks with interpolated frequency -> pitch classes
    for (int k = kLo; k <= kHi; ++k)
    {
        if (! (m[k] > m[k - 1] && m[k] >= m[k + 1]) || m[k] < top * 0.006f) continue;
        auto a = std::log (m[k - 1] + 1.0e-9f), b = std::log (m[k] + 1.0e-9f), c = std::log (m[k + 1] + 1.0e-9f);
        auto off = 0.5f * (a - c) / (a - 2 * b + c - 1.0e-9f);
        auto f = ((float) k + juce::jlimit (-0.5f, 0.5f, off)) * binHz;
        auto midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
        auto nearest = std::round (midi);
        auto w = juce::jmax (0.0f, 1.0f - 2.5f * std::abs (midi - nearest));
        if (w <= 0) continue;
        auto pc = ((int) nearest % 12 + 12) % 12;
        frameC[(size_t) pc] += std::pow (m[k], 0.6f) * w;
        if (f < 260.0f) lowC[(size_t) pc] += m[k] * w;
    }
    auto level = 10.0f * std::log10 (energySum + 1.0e-12f);
    if (level < -62.0f)
    {
        for (auto& c : chromaFast) c *= 0.97f;
        if (level < -80.0f) { chordText = "-"; chordRoot = -1; bassMidi = -1; }
        return;
    }
    auto mx = *std::max_element (frameC.begin(), frameC.end());
    if (mx <= 0) return;
    const float fastA = (float) (1.0 / (hopRate * 0.5 * 0.25)), slowA = (float) (1.0 / (hopRate * 0.5 * 12.0));
    for (int i = 0; i < 12; ++i)
    {
        auto v = frameC[(size_t) i] / mx;
        chromaFast[(size_t) i] += (v - chromaFast[(size_t) i]) * fastA;
        chromaSlow[(size_t) i] += (v - chromaSlow[(size_t) i]) * slowA;
    }
    auto fmx = juce::jmax (1.0e-6f, *std::max_element (chromaFast.begin(), chromaFast.end()));
    auto smx = juce::jmax (1.0e-6f, *std::max_element (chromaSlow.begin(), chromaSlow.end()));
    for (int i = 0; i < 12; ++i) { chroma[(size_t) i] = chromaFast[(size_t) i] / fmx; chromaKey[(size_t) i] = chromaSlow[(size_t) i] / smx; }

    // key (Krumhansl-Kessler profiles)
    static const float majorP[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float minorP[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
    auto corr = [this] (const float* prof, int tonic)
    {
        float mx2 = 0, my = 0;
        for (int i = 0; i < 12; ++i) { mx2 += chromaKey[(size_t) i]; my += prof[i]; }
        mx2 /= 12; my /= 12;
        float sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i)
        {
            auto x = chromaKey[(size_t) ((i + tonic) % 12)] - mx2, y = prof[i] - my;
            sxy += x * y; sxx += x * x; syy += y * y;
        }
        return sxy / std::sqrt (sxx * syy + 1.0e-12f);
    };
    float best = -2, second = -2;
    int bestKey = -1;
    for (int k = 0; k < 24; ++k)
    {
        auto c = corr (k < 12 ? majorP : minorP, k % 12);
        if (c > best) { second = best; best = c; bestKey = k; }
        else if (c > second) second = c;
    }
    key = best > 0.3f ? bestKey : -1;
    keyConfidence = juce::jlimit (0.0f, 1.0f, (best - second) * 5.0f + (best - 0.3f));

    // chord
    float norm = 0;
    for (auto c : chroma) norm += c * c;
    norm = std::sqrt (norm) + 1.0e-6f;
    auto& types = music::chordTypes();
    float bestScore = 0;
    int br = -1, bt = 0;
    for (int r = 0; r < 12; ++r)
        for (int t = 0; t < (int) types.size(); ++t)
        {
            auto& ct = types[(size_t) t];
            float s = 0;
            for (int i = 0; i < ct.size; ++i) s += chroma[(size_t) ((r + ct.intervals[(size_t) i]) % 12)] * (i == 0 ? 1.15f : 1.0f);
            s /= norm * std::sqrt ((float) ct.size);
            if (ct.size == 4) s *= 0.96f;
            if (t >= 5) s *= 0.97f;   // rarer qualities need a clearer picture
            if (key >= 0)
            {
                static const int majorScale[] = { 0, 2, 4, 5, 7, 9, 11 }, minorScale[] = { 0, 2, 3, 5, 7, 8, 10 };
                auto* sc = key < 12 ? majorScale : minorScale;
                auto iv = ((r - key % 12) % 12 + 12) % 12;
                if (std::find (sc, sc + 7, iv) != sc + 7) s *= 1.03f;
            }
            if (s > bestScore) { bestScore = s; br = r; bt = t; }
        }
    if (bestScore < 0.62f) br = -1;

    if (br == candidateRoot && bt == candidateType) ++candidateCount;
    else { candidateRoot = br; candidateType = bt; candidateCount = 1; }
    if (candidateCount >= 4) { stableRoot = br; stableType = bt; }

    auto lowMax = std::max_element (lowC.begin(), lowC.end());
    bassPc = *lowMax > 1.0e-4f ? (int) (lowMax - lowC.begin()) : -1;
    // bass note with octave from the strongest low bin
    {
        int bestK = -1; float bestM = 0;
        for (int k = juce::jmax (2, (int) (30.0f / binHz)); k < (int) (260.0f / binHz); ++k)
            if (m[k] > bestM && m[k] >= m[k - 1] && m[k] >= m[k + 1]) { bestM = m[k]; bestK = k; }
        if (bestK > 0 && toDb (bestM) > -60.0f)
        {
            auto a = m[bestK - 1], b = m[bestK], c = m[bestK + 1];
            auto off = 0.5f * (a - c) / (a - 2 * b + c + 1.0e-12f);
            auto f = (bestK + off) * binHz;
            bassMidi = (int) std::round (69.0f + 12.0f * std::log2 (f / 440.0f));
        }
        else bassMidi = -1;
    }

    chordRoot = stableRoot;
    chordType = stableType;
    int slash = -1;
    if (chordRoot >= 0 && bassPc >= 0)
    {
        auto& ct = types[(size_t) chordType];
        for (int i = 1; i < ct.size; ++i)
            if ((chordRoot + ct.intervals[(size_t) i]) % 12 == bassPc) slash = bassPc;
    }
    chordText = music::chordName (chordRoot, chordType, slash, music::keyUsesFlats (key));
    if (chordRoot >= 0) ++barChordVotes[(size_t) (chordRoot * 16 + chordType)];
}

void Analyzer::updateTempo()
{
    if (hostValid) return;
    const int n = (int) noveltyRing.size();
    const int span = juce::jmin (n - 1, (int) (hopRate * 8.0));
    std::vector<float> env ((size_t) span);
    float mean = 0;
    for (int i = 0; i < span; ++i)
    {
        env[(size_t) i] = noveltyRing[(size_t) ((noveltyWrite - span + i + n) % n)];
        mean += env[(size_t) i];
    }
    mean /= (float) span;
    float var = 0;
    for (auto& e : env) { e -= mean; var += e * e; }
    if (var < 1.0e-4f) { if (freeBpm > 0 && (hits.empty() || hits.back().time < totalSamples - (juce::int64) (sr * 4))) freeBpm = 0; return; }

    auto at = [&env, span] (float idx)
    {
        if (idx < 0 || idx >= (float) (span - 1)) return 0.0f;
        auto i = (int) idx; auto f = idx - (float) i;
        return env[(size_t) i] * (1 - f) + env[(size_t) i + 1] * f;
    };
    auto acf = [&] (float lag)
    {
        float s = 0;
        for (int i = (int) std::ceil (lag); i < span; ++i) s += env[(size_t) i] * at ((float) i - lag);
        return s / (float) (span - lag);
    };
    float bestScore = -1.0e9f, best = 0;
    for (float b = 70.0f; b <= 180.0f; b += 0.5f)
    {
        auto lag = (float) (60.0 * hopRate / b);
        auto prior = std::exp (-0.5f * std::pow (std::log2 (b / 110.0f) / 0.8f, 2.0f));
        auto s = (acf (lag) + 0.5f * acf (lag * 2.0f) + 0.25f * acf (lag * 0.5f)) * (0.6f + 0.4f * prior);
        if (s > bestScore) { bestScore = s; best = b; }
    }
    if (bestScore <= 0) return;
    if (freeBpm <= 0 || std::abs (best - freeBpm) / freeBpm < 0.03f) freeBpm = freeBpm <= 0 ? best : freeBpm * 0.8f + best * 0.2f;
    else if (std::abs (best - freeBpmCandidate) / best < 0.03f) freeBpm = best;
    freeBpmCandidate = best;

    // beat phase: nudge the running beat counter towards where the novelty peaks sit
    auto period = (float) (60.0 * hopRate / freeBpm);
    float bestPhase = 0, bestSum = -1.0e9f;
    for (float ph = 0; ph < period; ph += 0.5f)
    {
        float s = 0;
        for (float t = (float) (span - 1) - ph; t > 0; t -= period) s += at (t);
        if (s > bestSum) { bestSum = s; bestPhase = ph; }
    }
    // the last beat was bestPhase hops ago -> target fractional beat position now
    auto target = bestPhase / period;
    auto frac = (float) (freeBeats - std::floor (freeBeats));
    auto err = target - frac;
    err -= std::round (err);
    freeBeats += err * 0.5f;
}

void Analyzer::updateBarClock()
{
    if (hostValid && playing)
    {
        bpm = (float) hostBpm;
        hostTiming = true;
        auto ppqNow = hostPpq + (double) (totalSamples - hostAt) / sr * hostBpm / 60.0;
        auto barLen = hostNum * 4.0 / hostDen;
        beatsPerBar = hostNum;
        barTime = std::round (hostBarStart / barLen) + (ppqNow - hostBarStart) / barLen;
    }
    else
    {
        hostTiming = false;
        bpm = freeBpm;
        beatsPerBar = 4;
        if (freeBpm > 0)
        {
            auto before = (juce::int64) std::floor (freeBeats);
            freeBeats += hopLength / (60.0 / freeBpm * sr);
            auto now = (juce::int64) std::floor (freeBeats);
            if (now != before)
            {
                // learn which beat of four is the downbeat: the one with the most kick weight
                downbeatScore[(size_t) (before % 4)] = downbeatScore[(size_t) (before % 4)] * 0.9f + beatKickAcc[0];
                beatKickAcc[0] = 0;
            }
            // move the downbeat only when another beat clearly carries more kick (no flip-flopping on four-on-the-floor)
            int best = (int) (std::max_element (downbeatScore.begin(), downbeatScore.begin() + 4) - downbeatScore.begin());
            if (downbeatScore[(size_t) best] > downbeatScore[(size_t) downbeatOffset] * 1.35f + 0.05f) downbeatOffset = best;
            barTime = juce::jmax (0.0, (freeBeats - downbeatOffset) / 4.0);
        }
        else barTime = -1.0;
    }

    if (barTime >= 0.0)
    {
        auto beat = (juce::int64) std::floor (barTime * beatsPerBar);
        if (beat != lastBeatIndex) { beatPulse = 1.0f; lastBeatIndex = beat; }
        auto bar = (juce::int64) std::floor (barTime);
        // free tempo nudges the clock a little back and forth: only a forward step (or a real jump,
        // like a DAW loop) starts a new bar
        auto newBar = bar > lastBarIndex || (hostTiming && bar < lastBarIndex) || lastBarIndex < 0;
        if (! hostTiming && bar < lastBarIndex) barTime = juce::jmax (barTime, (double) lastBarIndex);
        if (newBar && bar != lastBarIndex)
        {
            if (lastBarIndex >= 0)
            {
                previousBar = currentBar;
                // chord of the finished bar = most votes
                auto best = std::max_element (barChordVotes.begin(), barChordVotes.end());
                BarChord bc;
                bc.bar = lastBarIndex;
                if (*best > 0) { auto i = (int) (best - barChordVotes.begin()); bc.root = i / 16; bc.type = i % 16; }
                std::rotate (chordStrip.begin(), chordStrip.begin() + 1, chordStrip.end());
                chordStrip.back() = bc;
            }
            barChordVotes = {};
            currentBar = {};
            currentBar.index = bar;
            currentBar.hits[0] |= previousBarPending;
            previousBarPending = 0;
            lastBarIndex = bar;
        }
    }
}

//============================================================================== display frame
void Analyzer::frame()
{
    ++frameCount;
    std::vector<float> l (fftSize), r (fftSize);
    copyHistory (0, l.data(), fftSize);
    copyHistory (1, r.data(), fftSize);
    computeSpectrum (l.data(), magL.data(), &cplxL);
    computeSpectrum (r.data(), magR.data(), &cplxR);

    const float norm = 4.0f / (float) fftSize;
    for (int k = 0; k < numBins; ++k)
    {
        auto re = 0.5f * (cplxL[(size_t) (2 * k)] + cplxR[(size_t) (2 * k)]);
        auto im = 0.5f * (cplxL[(size_t) (2 * k + 1)] + cplxR[(size_t) (2 * k + 1)]);
        auto mono = toDb (std::sqrt (re * re + im * im) * norm);
        auto smooth = [] (float& cur, float target) { cur += (target - cur) * (target > cur ? 0.7f : 0.16f); };
        smooth (spectrumDb[(size_t) k], mono);
        smooth (leftDb[(size_t) k], toDb (magL[(size_t) k]));
        smooth (rightDb[(size_t) k], toDb (magR[(size_t) k]));
        peakDb[(size_t) k] = juce::jmax (spectrumDb[(size_t) k], peakDb[(size_t) k] - 0.35f);
    }

    // log bands: max of the bins inside the band (interpolated where bands are narrower than a bin)
    auto binHz = binFrequency (1);
    for (int b = 0; b < numLogBands; ++b)
    {
        auto f0 = logBandFrequency ((float) b - 0.5f), f1 = logBandFrequency ((float) b + 0.5f);
        auto k0 = f0 / binHz, k1 = f1 / binHz;
        float v;
        if ((int) k1 - (int) k0 < 1)
        {
            auto kc = juce::jlimit (0.0f, (float) (numBins - 2), 0.5f * (k0 + k1));
            auto i = (int) kc; auto f = kc - (float) i;
            v = spectrumDb[(size_t) i] * (1 - f) + spectrumDb[(size_t) i + 1] * f;
        }
        else
        {
            v = -140.0f;
            for (int k = (int) std::ceil (k0); k <= juce::jmin (numBins - 1, (int) k1); ++k) v = juce::jmax (v, spectrumDb[(size_t) k]);
        }
        logRaw[(size_t) b] = v;
        logDb[(size_t) b] += (v - logDb[(size_t) b]) * 0.5f;
    }

    // thirds: level and stereo width (side / (mid + side))
    for (int t = 0; t < numThirds; ++t)
    {
        auto c = thirdFrequency (t);
        auto lo = juce::jmax (1, (int) (c / 1.122f / binHz)), hi = juce::jmin (numBins - 1, juce::jmax (lo, (int) (c * 1.122f / binHz)));
        float pm = 0, ps = 0;
        for (int k = lo; k <= hi; ++k)
        {
            auto lr = cplxL[(size_t) (2 * k)], li = cplxL[(size_t) (2 * k + 1)];
            auto rr = cplxR[(size_t) (2 * k)], ri = cplxR[(size_t) (2 * k + 1)];
            auto mr = 0.5f * (lr + rr), mi = 0.5f * (li + ri), sr2 = 0.5f * (lr - rr), si = 0.5f * (li - ri);
            pm += mr * mr + mi * mi;
            ps += sr2 * sr2 + si * si;
        }
        auto level = 10.0f * std::log10 ((pm + ps) * norm * norm + 1.0e-14f);
        thirdDb[(size_t) t] += (level - thirdDb[(size_t) t]) * 0.3f;
        auto w = (pm + ps) > 1.0e-10f ? ps / (pm + ps) : 0.0f;
        thirdWidth[(size_t) t] += (w - thirdWidth[(size_t) t]) * 0.12f;
    }

    // correlation and balance over the same window
    {
        double slr = 0, sll = 0, srr = 0;
        for (int i = 0; i < fftSize; ++i) { slr += l[(size_t) i] * r[(size_t) i]; sll += l[(size_t) i] * l[(size_t) i]; srr += r[(size_t) i] * r[(size_t) i]; }
        auto c = (sll > 1.0e-9 && srr > 1.0e-9) ? (float) (slr / std::sqrt (sll * srr)) : 1.0f;
        correlation += (c - correlation) * 0.15f;
        auto bal = (sll + srr) > 1.0e-9 ? (float) ((srr - sll) / (sll + srr)) : 0.0f;
        balance += (bal - balance) * 0.15f;
    }

    for (int c = 0; c < 2; ++c)
    {
        auto db = 10.0f * std::log10 (rms[(size_t) c] + 1.0e-12f);
        vu[(size_t) c] += (db - vu[(size_t) c]) * 0.12f;
    }

    // energies for generative visuals
    auto bandAvg = [this] (int a, int b)
    {
        float s = 0;
        for (int i = a; i < b; ++i) s += juce::jlimit (0.0f, 1.0f, (logDb[(size_t) i] + 72.0f) / 60.0f);
        return s / (float) (b - a);
    };
    auto smoothE = [] (float& e, float t) { e += (t - e) * (t > e ? 0.5f : 0.08f); };
    smoothE (energyLow, bandAvg (0, 40));
    smoothE (energyMid, bandAvg (40, 110));
    smoothE (energyHigh, bandAvg (110, numLogBands));
    smoothE (energy, juce::jlimit (0.0f, 1.0f, (toDb (std::sqrt (0.5f * (rms[0] + rms[1]))) + 48.0f) / 42.0f));

    peak = blockPeak;
    blockPeak = {};
    for (auto& h : hitFlash) h *= 0.86f;
    beatPulse *= 0.88f;

    if (frameCount % 2 == 0) runTuner();
}

void Analyzer::runTuner()
{
    const int dec = juce::jmax (1, (int) std::round (sr / 12000.0));
    const double rate = sr / dec;
    const int W = 1024, tauMax = juce::jmin (700, (int) (rate / 22.0)), tauMin = juce::jmax (2, (int) (rate / 1800.0));
    const int need = (W + tauMax) * dec;
    std::vector<float> l ((size_t) need), r ((size_t) need), x ((size_t) (W + tauMax));
    copyHistory (0, l.data(), need);
    copyHistory (1, r.data(), need);
    double energySum = 0;
    for (int i = 0; i < W + tauMax; ++i)
    {
        float s = 0;
        for (int j = 0; j < dec; ++j) s += l[(size_t) (i * dec + j)] + r[(size_t) (i * dec + j)];
        x[(size_t) i] = s / (2.0f * dec);
        energySum += x[(size_t) i] * x[(size_t) i];
    }
    if (energySum / (W + tauMax) < 1.0e-6) { tunerClarity *= 0.8f; if (tunerClarity < 0.1f) tunerMidi = -1; return; }

    yinDiff.assign ((size_t) tauMax + 1, 0.0f);
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        float s = 0;
        for (int i = 0; i < W; ++i) { auto d = x[(size_t) i] - x[(size_t) (i + tau)]; s += d * d; }
        yinDiff[(size_t) tau] = s;
    }
    float running = 0;
    yinDiff[0] = 1;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        running += yinDiff[(size_t) tau];
        yinDiff[(size_t) tau] = running > 0 ? yinDiff[(size_t) tau] * tau / running : 1.0f;
    }
    int found = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
        if (yinDiff[(size_t) tau] < 0.15f)
        {
            while (tau + 1 < tauMax && yinDiff[(size_t) tau + 1] < yinDiff[(size_t) tau]) ++tau;
            found = tau;
            break;
        }
    if (found < 0) { tunerClarity *= 0.85f; if (tunerClarity < 0.1f) tunerMidi = -1; return; }

    auto a = yinDiff[(size_t) found - 1], b = yinDiff[(size_t) found], c = yinDiff[(size_t) found + 1];
    auto off = 0.5f * (a - c) / (a - 2 * b + c + 1.0e-12f);
    auto hz = (float) (rate / (found + juce::jlimit (-0.5f, 0.5f, off)));
    tunerHz = tunerHz > 0 && std::abs (hz - tunerHz) / tunerHz < 0.03f ? tunerHz * 0.7f + hz * 0.3f : hz;
    auto midi = 69.0f + 12.0f * std::log2 (tunerHz / 440.0f);
    tunerMidi = (int) std::round (midi);
    tunerCents = (midi - (float) tunerMidi) * 100.0f;
    tunerClarity = 1.0f - b;
}

} // namespace cosmos
