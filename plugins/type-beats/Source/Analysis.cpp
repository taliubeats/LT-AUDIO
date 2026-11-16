#include "Analysis.h"

namespace typebeats
{

namespace
{
    constexpr double analysisRate = 22050.0;
    constexpr int peakPoints = 1200;

    const char* sharpNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const char* flatNames[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

    // Krumhansl-Kessler key profiles
    const float majorProfile[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    const float minorProfile[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

    using Chroma = std::array<float, 12>;

    float pearson (const float* a, const float* b, int n)
    {
        double ma = 0, mb = 0;
        for (int i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; }
        ma /= n; mb /= n;
        double num = 0, da = 0, db = 0;
        for (int i = 0; i < n; ++i)
        {
            auto x = a[i] - ma, y = b[i] - mb;
            num += x * y; da += x * x; db += y * y;
        }
        return (da > 0 && db > 0) ? (float) (num / std::sqrt (da * db)) : 0.0f;
    }

    std::vector<float> toMono22k (const juce::AudioBuffer<float>& in, double sr)
    {
        auto n = in.getNumSamples();
        std::vector<float> mono ((size_t) n);
        for (int c = 0; c < in.getNumChannels(); ++c)
        {
            auto* d = in.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                mono[(size_t) i] += d[i] / (float) in.getNumChannels();
        }

        if (std::abs (sr - analysisRate) < 1.0)
            return mono;

        auto ratio = sr / analysisRate;
        auto outN = (int) ((double) n / ratio);
        std::vector<float> out ((size_t) outN);

        // simple anti-aliasing low-pass before decimation
        if (ratio > 1.0)
        {
            juce::dsp::IIR::Filter<float> lp1, lp2;
            auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, analysisRate * 0.45);
            lp1.coefficients = coeffs; lp2.coefficients = coeffs;
            for (auto& s : mono) s = lp2.processSample (lp1.processSample (s));
        }

        juce::LagrangeInterpolator interp;
        interp.process (ratio, mono.data(), out.data(), outN);
        return out;
    }

    struct Spectrogram
    {
        int fftOrder, size, hop;
        std::vector<std::vector<float>> frames; // magnitude, size/2 bins
    };

    Spectrogram stft (const std::vector<float>& x, int order, int hop, const std::function<bool()>& shouldExit)
    {
        Spectrogram s { order, 1 << order, hop, {} };
        juce::dsp::FFT fft (order);
        std::vector<float> window ((size_t) s.size), buf ((size_t) s.size * 2);
        for (int i = 0; i < s.size; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (s.size - 1));

        for (size_t start = 0; start + (size_t) s.size <= x.size(); start += (size_t) hop)
        {
            if (shouldExit && (s.frames.size() % 64) == 0 && shouldExit())
                break;

            std::fill (buf.begin(), buf.end(), 0.0f);
            for (int i = 0; i < s.size; ++i)
                buf[(size_t) i] = x[start + (size_t) i] * window[(size_t) i];

            fft.performFrequencyOnlyForwardTransform (buf.data(), true);
            s.frames.emplace_back (buf.begin(), buf.begin() + s.size / 2);
        }
        return s;
    }

    // ---------------------------------------------------------------- tempo

    struct TempoResult { double bpm = 0, firstBeat = 0; };

    TempoResult estimateTempo (const std::vector<float>& x, const std::function<bool()>& shouldExit)
    {
        const int order = 10, hop = 256;
        auto spec = stft (x, order, hop, shouldExit);
        auto fps = analysisRate / hop;
        auto nFrames = (int) spec.frames.size();
        if (nFrames < (int) (fps * 4))
            return {};

        // log-magnitude spectral flux
        std::vector<float> env ((size_t) nFrames, 0.0f), prev (spec.frames[0].size(), 0.0f);
        for (int f = 0; f < nFrames; ++f)
        {
            auto& fr = spec.frames[(size_t) f];
            float flux = 0;
            for (size_t b = 1; b < fr.size(); ++b)
            {
                auto v = std::log1p (100.0f * fr[b]);
                flux += juce::jmax (0.0f, v - prev[b]);
                prev[b] = v;
            }
            env[(size_t) f] = flux;
        }

        // remove local mean, half-wave rectify, normalise
        std::vector<float> onset ((size_t) nFrames);
        auto w = (int) (fps * 0.4);
        double acc = 0;
        std::deque<float> win;
        for (int f = 0; f < nFrames; ++f)
        {
            win.push_back (env[(size_t) f]); acc += env[(size_t) f];
            if ((int) win.size() > w) { acc -= win.front(); win.pop_front(); }
            onset[(size_t) f] = juce::jmax (0.0f, env[(size_t) f] - (float) (acc / (double) win.size()));
        }
        auto mx = *std::max_element (onset.begin(), onset.end());
        if (mx <= 0) return {};
        for (auto& v : onset) v /= mx;

        // light smoothing so tempi that fall between two frame lags are not penalised
        auto smoothed = onset;
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int f = 2; f + 2 < nFrames; ++f)
                smoothed[(size_t) f] = (onset[(size_t) f - 2] + 4 * onset[(size_t) f - 1] + 6 * onset[(size_t) f]
                                         + 4 * onset[(size_t) f + 1] + onset[(size_t) f + 2]) / 16.0f;
            onset.swap (smoothed);
            smoothed = onset;
        }

        // autocorrelation over plausible tempi with a log-normal prior around 120 BPM
        auto minLag = (int) std::floor (fps * 60.0 / 200.0), maxLag = (int) std::ceil (fps * 60.0 / 55.0);
        std::vector<double> ac ((size_t) maxLag * 4 + 2, 0.0);
        for (int lag = 1; lag < (int) ac.size(); ++lag)
        {
            double sum = 0;
            for (int f = lag; f < nFrames; ++f)
                sum += onset[(size_t) f] * onset[(size_t) (f - lag)];
            ac[(size_t) lag] = sum / (double) (nFrames - lag);
        }

        auto score = [&] (double lag)
        {
            auto at = [&] (double l)
            {
                auto i = (int) l; auto fr = l - i;
                if (i + 1 >= (int) ac.size()) return 0.0;
                return ac[(size_t) i] * (1 - fr) + ac[(size_t) i + 1] * fr;
            };
            auto bpm = 60.0 * fps / lag;
            auto prior = std::exp (-0.5 * std::pow (std::log2 (bpm / 120.0) / 0.9, 2.0));
            return (at (lag) + 0.5 * at (lag * 2) + 0.25 * at (lag * 4)) * prior;
        };

        double bestLag = minLag, bestScore = -1;
        for (double lag = minLag; lag <= maxLag; lag += 0.25)
        {
            auto s = score (lag);
            if (s > bestScore) { bestScore = s; bestLag = lag; }
        }

        // refine bpm + phase with a beat-comb over the whole song
        auto coarseBpm = 60.0 * fps / bestLag;
        double bestBpm = coarseBpm, bestPhase = 0, bestComb = -1;
        for (double bpm = coarseBpm * 0.96; bpm <= coarseBpm * 1.04; bpm += 0.05)
        {
            auto period = 60.0 * fps / bpm;
            for (double phase = 0; phase < period; phase += 1.0)
            {
                double sum = 0; int count = 0;
                for (double t = phase; t < nFrames; t += period)
                {
                    auto i = (int) std::lround (t);
                    if (i >= nFrames) break;
                    // tolerate +-1 frame of jitter
                    sum += juce::jmax (onset[(size_t) i], juce::jmax (i > 0 ? onset[(size_t) i - 1] : 0.0f,
                                                                       i + 1 < nFrames ? onset[(size_t) i + 1] : 0.0f));
                    ++count;
                }
                auto comb = count > 0 ? sum / count : 0.0;
                if (comb > bestComb) { bestComb = comb; bestBpm = bpm; bestPhase = phase; }
            }
        }

        // snap to a whole number when the song is clearly on one (most produced music is)
        auto rounded = std::round (bestBpm);
        if (std::abs (rounded - bestBpm) < 0.35)
            bestBpm = rounded;

        return { bestBpm, bestPhase / fps };
    }

    // ---------------------------------------------------------------- chroma

    struct ChromaFrames { std::vector<Chroma> treble, bass; double fps = 0; };

    ChromaFrames computeChroma (const std::vector<float>& x, const std::function<bool()>& shouldExit)
    {
        const int order = 13, hop = 2048;
        auto spec = stft (x, order, hop, shouldExit);
        ChromaFrames out;
        out.fps = analysisRate / hop;
        auto size = spec.size;

        // precompute bin -> pitch class / weight
        struct BinMap { int pc; float weight; bool bass; };
        std::vector<BinMap> map ((size_t) size / 2, { -1, 0.0f, false });
        for (int b = 1; b < size / 2; ++b)
        {
            auto f = b * analysisRate / size;
            if (f < 50.0 || f > 2000.0) continue;
            auto midi = 69.0 + 12.0 * std::log2 (f / 440.0);
            auto nearest = std::round (midi);
            auto dev = std::abs (midi - nearest);                       // 0..0.5 semitones
            auto weight = (float) std::pow (std::cos (dev * juce::MathConstants<double>::pi), 2.0); // favour bins near pitch centres
            map[(size_t) b] = { ((int) nearest % 12 + 12) % 12, weight, f < 260.0 };
        }

        for (auto& fr : spec.frames)
        {
            Chroma tr {}, ba {};
            for (int b = 1; b < size / 2; ++b)
            {
                auto& m = map[(size_t) b];
                if (m.pc < 0) continue;
                auto v = std::sqrt (fr[(size_t) b]) * m.weight;
                if (m.bass) ba[(size_t) m.pc] += v;
                else        tr[(size_t) m.pc] += v;
            }
            out.treble.push_back (tr);
            out.bass.push_back (ba);
        }
        return out;
    }

    Chroma normalised (Chroma c)
    {
        auto mx = *std::max_element (c.begin(), c.end());
        if (mx > 0) for (auto& v : c) v /= mx;
        return c;
    }

    float energyOf (const Chroma& c) { float s = 0; for (auto v : c) s += v; return s; }

    // ---------------------------------------------------------------- key

    std::vector<KeyCandidate> estimateKey (const ChromaFrames& ch)
    {
        Chroma total {};
        for (size_t i = 0; i < ch.treble.size(); ++i)
        {
            auto t = normalised (ch.treble[i]);
            auto b = normalised (ch.bass[i]);
            auto e = energyOf (ch.treble[i]) + energyOf (ch.bass[i]);
            if (e <= 1e-6f) continue;
            for (int p = 0; p < 12; ++p)
                total[(size_t) p] += t[(size_t) p] + 0.6f * b[(size_t) p];
        }

        struct Scored { KeyCandidate k; float r; };
        std::vector<Scored> all;
        for (int tonic = 0; tonic < 12; ++tonic)
        {
            for (int minor = 0; minor < 2; ++minor)
            {
                float rotated[12];
                for (int p = 0; p < 12; ++p)
                    rotated[p] = total[(size_t) ((p + tonic) % 12)];
                auto r = pearson (rotated, minor ? minorProfile : majorProfile, 12);
                all.push_back ({ { tonic, minor == 1, 0 }, r });
            }
        }

        std::sort (all.begin(), all.end(), [] (const Scored& a, const Scored& b) { return a.r > b.r; });

        std::vector<KeyCandidate> keys;
        auto r1 = all[0].r, r2 = all[1].r;
        auto top = juce::jlimit (35, 99, (int) std::lround (100.0 * (0.62 + (r1 - r2) * 2.5 + (r1 - 0.7) * 0.8)));
        for (int i = 0; i < 4; ++i)
        {
            auto k = all[(size_t) i].k;
            k.percent = i == 0 ? top : juce::jlimit (5, top - 1, (int) std::lround (top * std::exp (-(r1 - all[(size_t) i].r) * 5.0)));
            keys.push_back (k);
        }
        return keys;
    }

    // ---------------------------------------------------------------- chords

    struct Template { Chord chord; Chroma vec; float norm; };

    std::vector<Template> makeTemplates()
    {
        std::vector<Template> t;
        auto& qs = Chord::qualities();
        for (int root = 0; root < 12; ++root)
        {
            for (int q = 0; q < (int) qs.size(); ++q)
            {
                Chroma v {};
                for (auto iv : qs[(size_t) q].intervals)
                    v[(size_t) ((root + iv) % 12)] = iv == 0 ? 1.15f : 1.0f;
                float n = 0;
                for (auto x : v) n += x * x;
                t.push_back ({ { root, q }, v, std::sqrt (n) });
            }
        }
        return t;
    }

    float cosine (const Chroma& a, const Template& t)
    {
        float dot = 0, na = 0;
        for (int i = 0; i < 12; ++i) { dot += a[(size_t) i] * t.vec[(size_t) i]; na += a[(size_t) i] * a[(size_t) i]; }
        return na > 0 ? dot / (std::sqrt (na) * t.norm) : 0.0f;
    }

    bool isDiatonic (const Chord& c, const KeyCandidate& key)
    {
        static const int majorScale[] = { 0, 2, 4, 5, 7, 9, 11 };
        auto tonic = key.minor ? (key.tonic + 3) % 12 : key.tonic; // relative major
        for (auto pc : c.pitchClasses())
        {
            auto rel = ((pc - tonic) % 12 + 12) % 12;
            if (std::find (std::begin (majorScale), std::end (majorScale), rel) == std::end (majorScale))
                return false;
        }
        return true;
    }

    float chordScore (const Chroma& treble, const Chroma& bass, const Template& t, const KeyCandidate& key)
    {
        auto& q = Chord::qualities()[(size_t) t.chord.quality];
        auto s = cosine (treble, t) + q.bias;
        s += 0.18f * bass[(size_t) t.chord.root];          // bass note on the root
        if (isDiatonic (t.chord, key)) s += 0.05f;
        return s;
    }

    std::vector<ChordSegment> estimateChords (const ChromaFrames& ch, double bpm, double firstBeat,
                                              double duration, const KeyCandidate& key)
    {
        auto templates = makeTemplates();
        if (ch.treble.empty())
            return {};

        // segment on beats (fallback: every 0.5 s)
        auto beat = bpm > 0 ? 60.0 / bpm : 0.5;
        std::vector<double> bounds;
        for (double t = std::fmod (firstBeat, beat); t < duration; t += beat)
            bounds.push_back (t);
        if (bounds.empty() || bounds.front() > 0.01) bounds.insert (bounds.begin(), 0.0);
        bounds.push_back (duration);

        struct Seg { double s, e; Chroma tr, ba; float energy; };
        std::vector<Seg> segs;
        float maxEnergy = 0;
        for (size_t i = 0; i + 1 < bounds.size(); ++i)
        {
            Seg seg { bounds[i], bounds[i + 1], {}, {}, 0 };
            auto f0 = (int) std::floor (seg.s * ch.fps), f1 = juce::jmax (f0 + 1, (int) std::ceil (seg.e * ch.fps));
            for (int f = f0; f < f1 && f < (int) ch.treble.size(); ++f)
                for (int p = 0; p < 12; ++p)
                {
                    seg.tr[(size_t) p] += ch.treble[(size_t) f][(size_t) p];
                    seg.ba[(size_t) p] += ch.bass[(size_t) f][(size_t) p];
                }
            seg.energy = energyOf (seg.tr) + energyOf (seg.ba);
            maxEnergy = juce::jmax (maxEnergy, seg.energy);
            seg.tr = normalised (seg.tr);
            seg.ba = normalised (seg.ba);
            segs.push_back (seg);
        }

        auto nS = (int) templates.size() + 1;   // last state = no chord
        auto T = (int) segs.size();
        const float switchPenalty = 0.22f;

        std::vector<std::vector<float>> emis ((size_t) T, std::vector<float> ((size_t) nS));
        for (int t = 0; t < T; ++t)
        {
            auto quiet = segs[(size_t) t].energy < maxEnergy * 0.04f;
            for (int s = 0; s < nS - 1; ++s)
                emis[(size_t) t][(size_t) s] = quiet ? 0.0f : chordScore (segs[(size_t) t].tr, segs[(size_t) t].ba, templates[(size_t) s], key);
            emis[(size_t) t][(size_t) nS - 1] = quiet ? 1.0f : 0.35f;
        }

        // Viterbi with a constant chord-change penalty (smooths flickering detections)
        std::vector<float> dp (emis[0]);
        std::vector<std::vector<int>> back ((size_t) T, std::vector<int> ((size_t) nS, 0));
        for (int t = 1; t < T; ++t)
        {
            auto bestPrev = (int) (std::max_element (dp.begin(), dp.end()) - dp.begin());
            std::vector<float> nd ((size_t) nS);
            for (int s = 0; s < nS; ++s)
            {
                auto stay = dp[(size_t) s], jump = dp[(size_t) bestPrev] - switchPenalty;
                if (stay >= jump) { nd[(size_t) s] = stay + emis[(size_t) t][(size_t) s]; back[(size_t) t][(size_t) s] = s; }
                else              { nd[(size_t) s] = jump + emis[(size_t) t][(size_t) s]; back[(size_t) t][(size_t) s] = bestPrev; }
            }
            dp.swap (nd);
        }

        std::vector<int> path ((size_t) T);
        path[(size_t) T - 1] = (int) (std::max_element (dp.begin(), dp.end()) - dp.begin());
        for (int t = T - 1; t > 0; --t)
            path[(size_t) t - 1] = back[(size_t) t][(size_t) path[(size_t) t]];

        // merge runs + alternatives from the run's average chroma
        std::vector<ChordSegment> out;
        for (int t = 0; t < T;)
        {
            auto s = path[(size_t) t];
            auto u = t;
            Chroma tr {}, ba {};
            while (u < T && path[(size_t) u] == s)
            {
                for (int p = 0; p < 12; ++p) { tr[(size_t) p] += segs[(size_t) u].tr[(size_t) p]; ba[(size_t) p] += segs[(size_t) u].ba[(size_t) p]; }
                ++u;
            }

            if (s < nS - 1)
            {
                ChordSegment cs { segs[(size_t) t].s, segs[(size_t) u - 1].e, templates[(size_t) s].chord, {} };
                tr = normalised (tr); ba = normalised (ba);

                std::vector<std::pair<float, Chord>> ranked;
                for (auto& tp : templates)
                    if (tp.chord != cs.chord)
                        ranked.push_back ({ chordScore (tr, ba, tp, key), tp.chord });
                std::sort (ranked.begin(), ranked.end(), [] (auto& a, auto& b) { return a.first > b.first; });
                for (size_t i = 0; i < ranked.size() && cs.alternatives.size() < 3; ++i)
                    cs.alternatives.push_back (ranked[i].second);

                if (! out.empty() && out.back().chord == cs.chord)
                    out.back().end = cs.end;
                else
                    out.push_back (cs);
            }
            t = u;
        }
        return out;
    }

    /** Major vs relative minor share the same notes; the chords decide which one is "home". */
    void resolveRelativeKey (std::vector<KeyCandidate>& keys, const std::vector<ChordSegment>& chords)
    {
        if (keys.size() < 2 || chords.empty())
            return;

        auto& top = keys[0];
        auto relTonic = top.minor ? (top.tonic + 3) % 12 : (top.tonic + 9) % 12;
        auto relIt = std::find_if (keys.begin() + 1, keys.begin() + (long) juce::jmin ((size_t) 3, keys.size()),
                                   [&] (const KeyCandidate& k) { return k.tonic == relTonic && k.minor != top.minor; });
        if (relIt == keys.begin() + (long) juce::jmin ((size_t) 3, keys.size()))
            return;

        double total = 0;
        for (auto& c : chords) total += c.end - c.start;

        auto evidence = [&] (int tonic, bool minor)
        {
            auto matches = [&] (const Chord& c)
            {
                auto q = c.quality;
                auto isMinor = q == 1 || q == 4;
                auto isMajor = q == 0 || q == 2 || q == 3;
                return c.root == tonic && (minor ? isMinor : isMajor);
            };
            double d = 0;
            for (auto& c : chords) if (matches (c.chord)) d += c.end - c.start;
            auto e = total > 0 ? d / total : 0.0;
            if (matches (chords.front().chord)) e += 0.15;
            if (matches (chords.back().chord))  e += 0.10;
            return e;
        };

        if (evidence (relIt->tonic, relIt->minor) > evidence (top.tonic, top.minor) + 0.05)
        {
            std::swap (relIt->tonic, top.tonic);
            std::swap (relIt->minor, top.minor);
        }
    }

    std::vector<float> makePeaks (const juce::AudioBuffer<float>& a)
    {
        std::vector<float> peaks (peakPoints, 0.0f);
        auto n = a.getNumSamples();
        if (n == 0) return peaks;
        float mx = 0;
        for (int p = 0; p < peakPoints; ++p)
        {
            auto s0 = (int) ((juce::int64) p * n / peakPoints), s1 = (int) ((juce::int64) (p + 1) * n / peakPoints);
            float v = 0;
            for (int c = 0; c < a.getNumChannels(); ++c)
                v = juce::jmax (v, a.getMagnitude (c, s0, juce::jmax (1, s1 - s0)));
            peaks[(size_t) p] = v;
            mx = juce::jmax (mx, v);
        }
        if (mx > 0) for (auto& v : peaks) v /= mx;
        return peaks;
    }
}

// ---------------------------------------------------------------- Chord

const std::vector<Chord::Quality>& Chord::qualities()
{
    static const std::vector<Quality> q = {
        { "",     { 0, 4, 7 },     0.00f },
        { "m",    { 0, 3, 7 },     0.00f },
        { "7",    { 0, 4, 7, 10 }, -0.05f },
        { "maj7", { 0, 4, 7, 11 }, -0.05f },
        { "m7",   { 0, 3, 7, 10 }, -0.04f },
        { "dim",  { 0, 3, 6 },     -0.08f },
        { "sus4", { 0, 5, 7 },     -0.07f },
        { "sus2", { 0, 2, 7 },     -0.07f },
    };
    return q;
}

std::vector<int> Chord::pitchClasses() const
{
    std::vector<int> pcs;
    if (! isValid()) return pcs;
    for (auto iv : qualities()[(size_t) quality].intervals)
        pcs.push_back ((root + iv) % 12);
    return pcs;
}

juce::String Chord::name (bool flats) const
{
    if (! isValid()) return "N.C.";
    return Analyzer::pitchName (root, flats) + qualities()[(size_t) quality].suffix;
}

juce::String Analyzer::pitchName (int pc, bool flats)
{
    pc = ((pc % 12) + 12) % 12;
    return flats ? flatNames[pc] : sharpNames[pc];
}

// ---------------------------------------------------------------- Key

bool KeyCandidate::prefersFlats() const
{
    // F, Bb, Eb, Ab, Db, Gb major and their relative minors
    auto majorTonic = minor ? (tonic + 3) % 12 : tonic;
    return majorTonic == 5 || majorTonic == 10 || majorTonic == 3 || majorTonic == 8 || majorTonic == 1 || majorTonic == 6;
}

juce::String KeyCandidate::name (bool shortForm) const
{
    auto n = Analyzer::pitchName (tonic, prefersFlats());
    if (shortForm) return n + (minor ? "m" : "");
    return n.substring (0, 1).toUpperCase() + n.substring (1) + (minor ? " MINOR" : " MAJOR");
}

// ---------------------------------------------------------------- serialisation

juce::var AnalysisResult::toVar() const
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("duration", duration);
    obj->setProperty ("bpm", bpm);
    obj->setProperty ("firstBeat", firstBeat);

    juce::Array<juce::var> k;
    for (auto& key : keys)
        k.add (juce::Array<juce::var> { key.tonic, key.minor, key.percent });
    obj->setProperty ("keys", k);

    juce::Array<juce::var> c;
    for (auto& seg : chords)
    {
        juce::Array<juce::var> alts;
        for (auto& a : seg.alternatives) alts.add (juce::Array<juce::var> { a.root, a.quality });
        c.add (juce::Array<juce::var> { seg.start, seg.end, seg.chord.root, seg.chord.quality, alts });
    }
    obj->setProperty ("chords", c);

    juce::MemoryBlock mb (peaks.data(), peaks.size() * sizeof (float));
    obj->setProperty ("peaks", mb.toBase64Encoding());
    return juce::var (obj.release());
}

AnalysisResult AnalysisResult::fromVar (const juce::var& v)
{
    AnalysisResult r;
    if (! v.isObject()) return r;
    r.duration = v["duration"];
    r.bpm = v["bpm"];
    r.firstBeat = v["firstBeat"];

    if (auto* k = v["keys"].getArray())
        for (auto& key : *k)
            r.keys.push_back ({ (int) key[0], (bool) key[1], (int) key[2] });

    if (auto* c = v["chords"].getArray())
        for (auto& seg : *c)
        {
            ChordSegment s { seg[0], seg[1], { (int) seg[2], (int) seg[3] }, {} };
            if (auto* alts = seg[4].getArray())
                for (auto& a : *alts) s.alternatives.push_back ({ (int) a[0], (int) a[1] });
            r.chords.push_back (s);
        }

    juce::MemoryBlock mb;
    if (mb.fromBase64Encoding (v["peaks"].toString()))
    {
        r.peaks.resize (mb.getSize() / sizeof (float));
        std::memcpy (r.peaks.data(), mb.getData(), r.peaks.size() * sizeof (float));
    }
    return r;
}

// ---------------------------------------------------------------- Analyzer

AnalysisResult Analyzer::analyse (const juce::AudioBuffer<float>& audio, double sampleRate,
                                  std::function<bool()> shouldExit, std::function<void (float)> progress)
{
    AnalysisResult r;
    if (audio.getNumSamples() == 0 || sampleRate <= 0)
        return r;

    auto report = [&] (float p) { if (progress) progress (p); };
    auto aborted = [&] { return shouldExit && shouldExit(); };

    r.duration = audio.getNumSamples() / sampleRate;
    r.peaks = makePeaks (audio);
    report (0.1f);

    auto mono = toMono22k (audio, sampleRate);
    if (aborted()) return {};
    report (0.25f);

    auto tempo = estimateTempo (mono, shouldExit);
    r.bpm = tempo.bpm;
    r.firstBeat = tempo.firstBeat;
    if (aborted()) return {};
    report (0.55f);

    auto chroma = computeChroma (mono, shouldExit);
    if (aborted()) return {};
    report (0.8f);

    r.keys = estimateKey (chroma);
    if (! r.keys.empty())
    {
        r.chords = estimateChords (chroma, r.bpm, r.firstBeat, r.duration, r.keys.front());
        resolveRelativeKey (r.keys, r.chords);
    }

    report (1.0f);
    return r;
}

} // namespace typebeats
