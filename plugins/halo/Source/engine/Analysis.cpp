#include "Analysis.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>

#include <world/cheaptrick.h>
#include <world/d4c.h>
#include <world/harvest.h>
#include <world/stonemask.h>

namespace halo
{

namespace
{
    const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    constexpr int majorSteps[] = { 0, 2, 4, 5, 7, 9, 11 };
    constexpr int minorSteps[] = { 0, 2, 3, 5, 7, 8, 10 };
    constexpr double majorProfile[] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    constexpr double minorProfile[] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    constexpr double pi = 3.14159265358979323846;

    double median (std::vector<double> v)
    {
        if (v.empty()) return 0.0;
        auto mid = v.begin() + (long) (v.size() / 2);
        std::nth_element (v.begin(), mid, v.end());
        return *mid;
    }

    std::vector<double> medianFilter (const std::vector<double>& x, int k)
    {
        if (k < 3) return x;
        std::vector<double> out (x.size());
        auto h = k / 2;
        std::vector<double> win;
        for (int i = 0; i < (int) x.size(); ++i)
        {
            win.clear();
            for (int j = i - h; j <= i + h; ++j)
                win.push_back (j < 0 || j >= (int) x.size() ? 0.0 : x[(size_t) j]);   // zero padding, like scipy
            out[(size_t) i] = median (win);
        }
        return out;
    }

    double correlation (const double* a, const double* b, int n)
    {
        double ma = 0, mb = 0;
        for (int i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; }
        ma /= n; mb /= n;
        double num = 0, da = 0, db = 0;
        for (int i = 0; i < n; ++i)
        {
            num += (a[i] - ma) * (b[i] - mb);
            da += (a[i] - ma) * (a[i] - ma);
            db += (b[i] - mb) * (b[i] - mb);
        }
        return num / std::sqrt (da * db + 1e-12);
    }
}

double hzToMidi (double hz)    { return hz > 0.0 ? 69.0 + 12.0 * std::log2 (hz / 440.0) : 0.0; }
double midiToHz (double midi)  { return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0); }
std::string pitchClassName (int pc) { return names[((pc % 12) + 12) % 12]; }
std::string noteName (int midi)     { return pitchClassName (midi) + std::to_string (midi / 12 - 1); }

const int* Key::steps() const { return minor ? minorSteps : majorSteps; }

std::array<int, 7> Key::pitchClasses() const
{
    std::array<int, 7> pcs {};
    for (int i = 0; i < 7; ++i) pcs[(size_t) i] = (tonic + steps()[i]) % 12;
    return pcs;
}

bool Key::contains (int pc) const
{
    pc = ((pc % 12) + 12) % 12;
    for (auto p : pitchClasses()) if (p == pc) return true;
    return false;
}

std::string Key::name() const { return pitchClassName (tonic) + (minor ? " Minor" : " Major"); }

// ------------------------------------------------------------------ notes

double estimateTuning (const std::vector<double>& midi)
{
    std::complex<double> acc { 0.0, 0.0 };
    int n = 0;
    for (auto m : midi)
        if (m > 0) { acc += std::polar (1.0, 2.0 * pi * (m - std::round (m))); ++n; }
    return n == 0 ? 0.0 : std::arg (acc / (double) n) / (2.0 * pi);
}

std::vector<Note> segmentNotes (const std::vector<double>& midi, double tuning, double minNoteMs)
{
    const int n = (int) midi.size();
    std::vector<double> smooth ((size_t) n, 0.0);
    for (int i = 0; i < n;)
    {
        if (midi[(size_t) i] <= 0) { ++i; continue; }
        int j = i;
        while (j < n && midi[(size_t) j] > 0) ++j;
        std::vector<double> run (midi.begin() + i, midi.begin() + j);
        int k = std::min (9, ((int) run.size() / 2) * 2 - 1);
        auto f = k >= 3 ? medianFilter (run, k) : run;
        std::copy (f.begin(), f.end(), smooth.begin() + i);
        i = j;
    }

    std::vector<int> q ((size_t) n, -1);
    for (int i = 0; i < n; ++i)
        if (midi[(size_t) i] > 0) q[(size_t) i] = (int) std::lround (smooth[(size_t) i] - tuning);

    struct Seg { int s, e, p; };
    std::vector<Seg> segs;
    for (int i = 0; i < n;)
    {
        int j = i;
        while (j < n && q[(size_t) j] == q[(size_t) i]) ++j;
        if (q[(size_t) i] >= 0) segs.push_back ({ i, j, q[(size_t) i] });
        i = j;
    }

    const int minLen = (int) (minNoteMs / frameMs);
    for (bool changed = true; changed;)
    {
        changed = false;
        for (size_t idx = 0; idx < segs.size(); ++idx)
        {
            auto sg = segs[idx];
            if (sg.e - sg.s >= minLen) continue;
            Seg* prev = idx > 0 && segs[idx - 1].e == sg.s ? &segs[idx - 1] : nullptr;
            Seg* next = idx + 1 < segs.size() && segs[idx + 1].s == sg.e ? &segs[idx + 1] : nullptr;
            if (prev == nullptr && next == nullptr)
            {
                if (sg.e - sg.s < minLen / 2) { segs.erase (segs.begin() + (long) idx); changed = true; break; }
                continue;
            }
            Seg* tgt = prev;
            if (tgt == nullptr || (next != nullptr && std::abs (next->p - sg.p) < std::abs (prev->p - sg.p)))
                tgt = next;
            tgt->s = std::min (tgt->s, sg.s);
            tgt->e = std::max (tgt->e, sg.e);
            segs.erase (segs.begin() + (long) idx);
            changed = true;
            break;
        }
    }

    std::vector<Seg> merged;
    for (auto& s : segs)
    {
        if (! merged.empty() && merged.back().e == s.s && merged.back().p == s.p) merged.back().e = s.e;
        else merged.push_back (s);
    }

    std::vector<Note> notes;
    for (auto& s : merged)
    {
        double sum = 0; int c = 0;
        for (int i = s.s; i < s.e; ++i) if (midi[(size_t) i] > 0) { sum += midi[(size_t) i]; ++c; }
        Note nt;
        nt.start = s.s; nt.end = s.e; nt.midi = s.p;
        nt.mean = c ? sum / c : s.p + tuning;
        notes.push_back (nt);
    }
    return notes;
}

// ------------------------------------------------------------------ key + chords

Key estimateKey (const std::vector<Note>& notes)
{
    double hist[12] {};
    for (auto& n : notes) hist[((n.midi % 12) + 12) % 12] += n.frames();
    double total = 0;
    for (auto h : hist) total += h;
    if (total <= 0) return { 0, false, 0.0f };

    std::vector<double> scores;
    Key best;
    double bestR = -2;
    for (int t = 0; t < 12; ++t)
        for (int m = 0; m < 2; ++m)
        {
            double prof[12];
            for (int i = 0; i < 12; ++i) prof[(i + t) % 12] = (m ? minorProfile : majorProfile)[i];
            auto r = correlation (hist, prof, 12);
            scores.push_back (r);
            if (r > bestR) { bestR = r; best.tonic = t; best.minor = m == 1; }
        }
    std::sort (scores.begin(), scores.end(), std::greater<>());
    best.confidence = (float) std::clamp ((scores[0] - scores[1]) * 5.0 + 0.5, 0.0, 1.0);
    return best;
}

void estimateChords (std::vector<Note>& notes, const Key& key, double changePenalty)
{
    if (notes.empty()) return;
    auto pcs = key.pitchClasses();
    struct Chord { std::array<int, 3> pcs; std::string name; bool dim; };
    std::vector<Chord> chords;
    for (int d = 0; d < 7; ++d)
    {
        int root = pcs[(size_t) d], third = pcs[(size_t) ((d + 2) % 7)], fifth = pcs[(size_t) ((d + 4) % 7)];
        auto i3 = ((third - root) % 12 + 12) % 12, i5 = ((fifth - root) % 12 + 12) % 12;
        std::string q = i3 == 4 ? "" : "m";
        bool dim = i5 == 6;
        if (dim) q = "dim";
        chords.push_back ({ { root, third, fifth }, pitchClassName (root) + q, dim });
    }
    double prior[7];
    const double pMaj[] = { 0.6, 0.0, 0.1, 0.4, 0.5, 0.2, 0.0 };
    const double pMin[] = { 0.6, 0.0, 0.4, 0.4, 0.4, 0.4, 0.3 };
    for (int k = 0; k < 7; ++k) prior[k] = (key.minor ? pMin : pMaj)[k] - (chords[(size_t) k].dim ? 1.5 : 0.0);

    const int K = 7, T = (int) notes.size();
    std::vector<std::array<double, 7>> emis ((size_t) T);
    for (int t = 0; t < T; ++t)
    {
        auto& n = notes[(size_t) t];
        double w = std::min (n.ms() / 250.0, 2.0) + 0.2;
        int pc = ((n.midi % 12) + 12) % 12;
        for (int k = 0; k < K; ++k)
        {
            auto& c = chords[(size_t) k].pcs;
            double e = pc == c[0] ? 1.2 * w : (pc == c[1] || pc == c[2]) ? 1.0 * w : -0.6 * w;
            emis[(size_t) t][(size_t) k] = e + prior[k] * 0.3;
        }
    }
    std::vector<std::array<int, 7>> back ((size_t) T);
    auto score = emis[0];
    for (int t = 1; t < T; ++t)
    {
        double gap = (notes[(size_t) t].start - notes[(size_t) t - 1].end) * frameSec;
        double pen = changePenalty * (gap > 0.35 ? 0.4 : 1.0);
        std::array<double, 7> ns {};
        for (int k = 0; k < K; ++k)
        {
            int bi = 0; double bv = -1e18;
            for (int j = 0; j < K; ++j)
            {
                double v = score[(size_t) j] - (j == k ? 0.0 : pen);
                if (v > bv) { bv = v; bi = j; }
            }
            back[(size_t) t][(size_t) k] = bi;
            ns[(size_t) k] = bv + emis[(size_t) t][(size_t) k];
        }
        score = ns;
    }
    int k = (int) (std::max_element (score.begin(), score.end()) - score.begin());
    for (int t = T - 1; t >= 0; --t)
    {
        notes[(size_t) t].chord = chords[(size_t) k].pcs;
        notes[(size_t) t].chordName = chords[(size_t) k].name;
        if (t > 0) k = back[(size_t) t][(size_t) k];
    }
}

// ------------------------------------------------------------------ vibrato

void estimateVibrato (const std::vector<double>& midi, const std::vector<Note>& notes, double& depth, double& rate)
{
    std::vector<double> depths, rates;
    const double fs = 1000.0 / frameMs;
    for (auto& n : notes)
    {
        if (n.ms() < 400) continue;
        std::vector<double> seg;
        for (int i = n.start; i < n.end; ++i) if (midi[(size_t) i] > 0) seg.push_back (midi[(size_t) i]);
        if (seg.size() < 60) continue;
        std::vector<double> x;
        for (int i = 20; i < (int) seg.size() - 20; ++i)
        {
            double avg = 0;
            for (int j = i - 20; j <= i + 20; ++j) avg += seg[(size_t) j];
            x.push_back (seg[(size_t) i] - avg / 41.0);
        }
        if (x.size() < 40) continue;
        const int N = 1024;
        const int L = (int) x.size();
        std::vector<double> mag (N / 2 + 1);
        for (int k = 0; k <= N / 2; ++k)
        {
            std::complex<double> s { 0, 0 };
            for (int i = 0; i < L; ++i)
            {
                double w = 0.5 - 0.5 * std::cos (2 * pi * i / (L - 1));
                s += x[(size_t) i] * w * std::polar (1.0, -2 * pi * k * i / N);
            }
            mag[(size_t) k] = std::abs (s);
        }
        double bandMax = 0; double bandF = 5.5;
        for (int k = 0; k <= N / 2; ++k)
        {
            double f = k * fs / N;
            if (f >= 4 && f <= 8 && mag[(size_t) k] > bandMax) { bandMax = mag[(size_t) k]; bandF = f; }
        }
        std::vector<double> rest (mag.begin() + 1, mag.end());
        if (bandMax > 2 * median (rest) + 1e-9)
        {
            double m = std::accumulate (x.begin(), x.end(), 0.0) / x.size(), v = 0;
            for (auto e : x) v += (e - m) * (e - m);
            depths.push_back (std::sqrt (2.0) * std::sqrt (v / x.size()));
            rates.push_back (bandF);
        }
    }
    if (depths.empty()) { depth = 0.15; rate = 5.5; return; }
    depth = std::clamp (median (depths), 0.05, 0.45);
    rate = std::clamp (median (rates), 4.5, 7.0);
}

// ------------------------------------------------------------------ main

std::shared_ptr<Analysis> analyse (std::vector<double> mono, int sr, const AnalysisOptions& opt,
                                   const std::function<void (float)>& progress, const std::function<bool()>& shouldStop)
{
    auto report = [&] (float v) { if (progress) progress (v); };
    auto stop = [&] { return shouldStop && shouldStop(); };

    auto an = std::make_shared<Analysis>();
    an->sampleRate = sr;
    an->audio = std::move (mono);
    const int len = (int) an->audio.size();
    if (len < sr / 10) return nullptr;
    const double* x = an->audio.data();

    // F0
    HarvestOption ho;
    InitializeHarvestOption (&ho);
    ho.f0_floor = opt.f0Floor;
    ho.f0_ceil = opt.f0Ceil;
    ho.frame_period = frameMs;
    const int nf = GetSamplesForHarvest (sr, len, frameMs);
    std::vector<double> t ((size_t) nf), f0 ((size_t) nf), refined ((size_t) nf);
    report (0.02f);
    Harvest (x, len, sr, &ho, t.data(), f0.data());
    if (stop()) return nullptr;
    report (0.35f);
    StoneMask (x, len, sr, t.data(), f0.data(), nf, refined.data());

    // remove voiced blips shorter than 25 ms
    const int minRun = (int) (25.0 / frameMs);
    for (int i = 0; i < nf;)
    {
        if (refined[(size_t) i] <= 0) { ++i; continue; }
        int j = i;
        while (j < nf && refined[(size_t) j] > 0) ++j;
        if (j - i < minRun) std::fill (refined.begin() + i, refined.begin() + j, 0.0);
        i = j;
    }
    an->f0 = refined;

    // spectral envelope + aperiodicity, in chunks of frames (WORLD frames are independent)
    CheapTrickOption co;
    InitializeCheapTrickOption (sr, &co);
    co.f0_floor = 71.0;
    co.fft_size = GetFFTSizeForCheapTrick (sr, &co);
    D4COption dopt;
    InitializeD4COption (&dopt);
    auto& S = an->spectra;
    S.fftSize = co.fft_size;
    S.bins = co.fft_size / 2 + 1;
    S.sp.assign ((size_t) nf * (size_t) S.bins, 0.0f);
    S.ap.assign ((size_t) nf * (size_t) S.bins, 0.0f);

    const int chunk = 400;   // frames (2 s)
    std::vector<double> rowsSp ((size_t) chunk * (size_t) S.bins), rowsAp ((size_t) chunk * (size_t) S.bins);
    std::vector<double*> ptrSp ((size_t) chunk), ptrAp ((size_t) chunk);
    for (int c0 = 0; c0 < nf; c0 += chunk)
    {
        if (stop()) return nullptr;
        int cn = std::min (chunk, nf - c0);
        for (int i = 0; i < cn; ++i)
        {
            ptrSp[(size_t) i] = rowsSp.data() + (size_t) i * (size_t) S.bins;
            ptrAp[(size_t) i] = rowsAp.data() + (size_t) i * (size_t) S.bins;
        }
        CheapTrick (x, len, sr, t.data() + c0, refined.data() + c0, cn, &co, ptrSp.data());
        D4C (x, len, sr, t.data() + c0, refined.data() + c0, cn, S.fftSize, &dopt, ptrAp.data());
        for (int i = 0; i < cn; ++i)
            for (int b = 0; b < S.bins; ++b)
            {
                S.sp[(size_t) (c0 + i) * (size_t) S.bins + (size_t) b] = (float) ptrSp[(size_t) i][b];
                S.ap[(size_t) (c0 + i) * (size_t) S.bins + (size_t) b] = (float) ptrAp[(size_t) i][b];
            }
        report (0.4f + 0.55f * (float) (c0 + cn) / (float) nf);
    }

    an->midi.resize ((size_t) nf);
    for (int i = 0; i < nf; ++i) an->midi[(size_t) i] = hzToMidi (an->f0[(size_t) i]);
    an->tuning = estimateTuning (an->midi);
    an->notes = segmentNotes (an->midi, an->tuning);
    an->key = opt.forcedKey ? *opt.forcedKey : estimateKey (an->notes);
    estimateChords (an->notes, an->key);
    estimateVibrato (an->midi, an->notes, an->vibratoDepth, an->vibratoRate);
    report (1.0f);
    return an;
}

} // namespace halo
