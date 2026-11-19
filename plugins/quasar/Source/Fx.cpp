#include "Fx.h"
#include "Reverb.h"
#include "ModDefs.h"
#include "NovaEngine.h"
#include "OrbitEngine.h"

namespace quasar
{

using namespace dsp;

namespace
{
    /** Dry / wet for effects that add something on top (delay, reverb): dry stays full up to 50 %. */
    inline void parallelGains (float mix, float& dry, float& wet) noexcept
    {
        dry = juce::jmin (1.0f, 2.0f * (1.0f - mix));
        wet = juce::jmin (1.0f, 2.0f * mix);
    }

    struct Delay
    {
        std::vector<float> buf; int mask = 0, pos = 0;
        void prepare (int maxLen) { int s = 1; while (s < maxLen + 8) s <<= 1; buf.assign ((size_t) s, 0.0f); mask = s - 1; pos = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        inline void push (float v) noexcept { buf[(size_t) pos] = v; pos = (pos + 1) & mask; }
        inline float read (float d) const noexcept   // cubic Hermite, d >= 1
        {
            auto i = (int) d; auto f = d - (float) i;
            auto xm1 = buf[(size_t) ((pos - i + 1) & mask)], x0 = buf[(size_t) ((pos - i) & mask)];
            auto x1 = buf[(size_t) ((pos - i - 1) & mask)], x2 = buf[(size_t) ((pos - i - 2) & mask)];
            auto c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }
    };

    /** Linear SVF (no saturation): used by the effects so a flat setting is an exact identity. */
    struct CleanSvf
    {
        float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1.414f;
        void set (float hz, float q, float sr) noexcept
        {
            auto g = std::tan (pi * juce::jlimit (10.0f, 0.49f * sr, hz) / sr);
            k = 1.0f / juce::jmax (0.05f, q);
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        inline void tick (float v0, float& lo, float& bd, float& hi) noexcept
        {
            auto v3 = v0 - ic2;
            auto v1 = a1 * ic1 + a2 * v3;
            auto v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
            lo = v2; bd = v1; hi = v0 - k * v1 - v2;
        }
        void reset() noexcept { ic1 = ic2 = 0; }
    };

    /** RBJ biquad, transposed direct form II. */
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        inline float tick (float x) noexcept { auto y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void reset() noexcept { z1 = z2 = 0; }
        void set (double c0, double c1, double c2, double d0, double d1, double d2) noexcept
        { b0 = (float) (c0 / d0); b1 = (float) (c1 / d0); b2 = (float) (c2 / d0); a1 = (float) (d1 / d0); a2 = (float) (d2 / d0); }
        void peak (double f, double q, double db, double sr)
        {
            auto A = std::pow (10.0, db / 40.0), w = 2 * juce::MathConstants<double>::pi * f / sr, al = std::sin (w) / (2 * q), c = std::cos (w);
            set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
        void lowShelf (double f, double db, double sr)
        {
            auto A = std::pow (10.0, db / 40.0), w = 2 * juce::MathConstants<double>::pi * f / sr, c = std::cos (w), al = std::sin (w) / 2 * std::sqrt (2.0), sa = 2 * std::sqrt (A) * al;
            set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                 (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
        }
        void highShelf (double f, double db, double sr)
        {
            auto A = std::pow (10.0, db / 40.0), w = 2 * juce::MathConstants<double>::pi * f / sr, c = std::cos (w), al = std::sin (w) / 2 * std::sqrt (2.0), sa = 2 * std::sqrt (A) * al;
            set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                 (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
        }
        void highPass (double f, double sr)
        {
            auto w = 2 * juce::MathConstants<double>::pi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * 0.7071);
            set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        }
    };

    /** Slow RMS-based gain matching (the "auto gain" of drive effects). */
    struct AutoGain
    {
        float inP = 1.0e-6f, outP = 1.0e-6f, g = 1, c = 0.001f;
        void prepare (float sr) { c = 1.0f - std::exp (-1.0f / (0.3f * sr)); }
        inline void track (float in, float out) noexcept { inP += c * (in * in - inP); outP += c * (out * out - outP); }
        inline float gain() noexcept
        {
            auto target = (inP < 1.0e-9f || outP < 1.0e-9f) ? g : juce::jlimit (0.06f, 2.0f, std::sqrt (inP / outP));
            g += 0.002f * (target - g);
            return g;
        }
    };

    // ============================================================== effects

    class Saturation : public Effect
    {
    public:
        void prepare (double s, int block) override
        {
            sr = (float) s;
            over = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
            over->initProcessing ((size_t) block);
            ag.prepare (sr * 2);
            for (auto& d : dc) d.prepare (sr * 2);
            reset();
        }
        void reset() override { if (over) over->reset(); for (auto& z : tilt) z.reset(); for (auto& z : warmL) z.reset(); for (auto& d : dc) d.reset(); }
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) override
        {
            auto type = (nova::SatType) juce::jlimit (0, 7, (int) p[0]);
            auto drive = dbToGain (p[1] * 0.36f);   // 0..+36 dB
            auto tone = p[2] * 0.01f, warmth = p[3] * 0.01f, character = p[4] * 0.01f, outG = dbToGain (p[5]);
            float* ch[2] = { L, R };
            juce::dsp::AudioBlock<float> block (ch, 2, (size_t) n);
            auto up = over->processSamplesUp (block);
            auto osr = sr * 2;
            auto tiltC = OnePole::coeff (700.0f, osr), warmC = OnePole::coeff (180.0f, osr);
            for (int c = 0; c < 2; ++c)
            {
                auto* x = up.getChannelPointer ((size_t) c);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    auto dry = x[i];
                    auto low = warmL[c].lp (dry, warmC);
                    auto in = dry + warmth * 1.5f * low;                 // more bass into the shaper = warmer
                    auto w = nova::NovaEngine::shape (in * drive, type, character) / juce::jmax (1.0f, std::sqrt (drive));
                    w -= warmth * 0.45f * low;
                    auto y = dc[(size_t) c].process (w);
                    auto t = tilt[c].lp (y, tiltC);
                    y = tone >= 0 ? y + tone * (y - t) : t + (1.0f + tone) * (y - t);
                    if (c == 0) ag.track (dry, y);
                    x[i] = dry + mix * (y * outG * agGain - dry);
                }
                if (c == 0) agGain = ag.gain();
            }
            over->processSamplesDown (block);
        }
    private:
        float sr = 48000, agGain = 1;
        std::unique_ptr<juce::dsp::Oversampling<float>> over;
        std::array<OnePole, 2> tilt, warmL;
        std::array<DcBlocker, 2> dc;
        AutoGain ag;
    };

    class Compressor : public Effect
    {
    public:
        void prepare (double s, int) override { sr = (float) s; reset(); }
        void reset() override { env = 0; rms = 0; gr = 0; }
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) override
        {
            auto thr = p[0], ratio = p[1], knee = juce::jmax (0.01f, p[4]), makeup = dbToGain (p[5]);
            auto mode = (int) p[6];
            auto att = std::exp (-1.0f / (juce::jmax (0.05f, p[2] * (mode == 2 ? 1.8f : 1.0f)) * 0.001f * sr));
            auto rel = std::exp (-1.0f / (p[3] * 0.001f * sr));
            auto rmsC = std::exp (-1.0f / (0.01f * sr));
            for (int i = 0; i < n; ++i)
            {
                auto peak = juce::jmax (std::abs (L[i]), std::abs (R[i]));
                float det;
                if (mode == 1) { rms = rmsC * rms + (1 - rmsC) * peak * peak; det = std::sqrt (rms) * 1.41f; }
                else det = peak;
                auto lvl = gainToDb (det);
                // soft knee gain computer
                float over = lvl - thr, g;
                if (2 * over < -knee) g = 0;
                else if (2 * std::abs (over) <= knee) g = (1.0f / ratio - 1.0f) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2 * knee);
                else g = (1.0f / ratio - 1.0f) * over;
                auto target = -g;   // positive dB of reduction
                env = target > env ? att * env + (1 - att) * target : rel * env + (1 - rel) * target;
                gr = env;
                auto gain = dbToGain (-env) * makeup;
                L[i] += mix * (L[i] * gain - L[i]);
                R[i] += mix * (R[i] * gain - R[i]);
            }
        }
    private:
        float sr = 48000, env = 0, rms = 0, gr = 0;
    };

    class Chorus : public Effect
    {
    public:
        void prepare (double s, int) override { sr = (float) s; for (auto& d : dl) d.prepare ((int) (0.07 * sr)); reset(); }
        void reset() override { for (auto& d : dl) d.clear(); phase = 0; fbL = fbR = 0; lpL.reset(); lpR.reset(); }
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) override
        {
            auto rate = p[0], depth = p[1] * 0.01f, base = p[2] * 0.001f * sr, fb = p[3] * 0.01f, width = p[4] * 0.01f;
            auto voices = 2 + (int) p[5];
            auto lpC = OnePole::coeff (p[6], sr);
            auto inc = rate / sr;
            for (int i = 0; i < n; ++i)
            {
                phase += inc; if (phase >= 1.0f) phase -= 1.0f;
                dl[0].push (L[i] + fbL * fb); dl[1].push (R[i] + fbR * fb);
                float wl = 0, wr = 0;
                for (int v = 0; v < voices; ++v)
                {
                    auto ph = phase + (float) v / (float) voices;
                    auto mL = 0.5f + 0.5f * std::sin (twoPi * ph);
                    auto mR = 0.5f + 0.5f * std::sin (twoPi * (ph + 0.25f * width));
                    wl += dl[0].read (2.0f + base * (0.3f + 0.7f * (1.0f - depth * 0.5f)) + base * depth * mL);
                    wr += dl[1].read (2.0f + base * (0.3f + 0.7f * (1.0f - depth * 0.5f)) + base * depth * mR);
                }
                wl /= std::sqrt ((float) voices); wr /= std::sqrt ((float) voices);
                wl = lpL.lp (wl, lpC); wr = lpR.lp (wr, lpC);
                fbL = wl; fbR = wr;
                // classic chorus: 100 % mix = dry + chorus, equal level
                L[i] = L[i] * (1.0f - 0.5f * mix) + wl * mix * 0.7f;
                R[i] = R[i] * (1.0f - 0.5f * mix) + wr * mix * 0.7f;
            }
        }
    private:
        float sr = 48000, phase = 0, fbL = 0, fbR = 0;
        std::array<Delay, 2> dl; OnePole lpL, lpR;
    };

    class Flanger : public Effect
    {
    public:
        void prepare (double s, int) override { sr = (float) s; for (auto& d : dl) d.prepare ((int) (0.03 * sr)); reset(); }
        void reset() override { for (auto& d : dl) d.clear(); phase = 0; fb = { 0, 0 }; }
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) override
        {
            auto inc = p[0] / sr, depth = p[1] * 0.01f, base = p[2] * 0.001f * sr, feedback = p[3] * 0.01f * 0.95f, width = p[4] * 0.01f;
            float* ch[2] = { L, R };
            for (int i = 0; i < n; ++i)
            {
                phase += inc; if (phase >= 1.0f) phase -= 1.0f;
                for (int c = 0; c < 2; ++c)
                {
                    auto m = 0.5f + 0.5f * std::sin (twoPi * (phase + (float) c * 0.5f * width));
                    auto d = 1.5f + base * (0.15f + 0.85f * depth * m + (1.0f - depth) * 0.5f);
                    auto x = ch[c][i];
                    dl[(size_t) c].push (softClip (x + fb[(size_t) c] * feedback));
                    auto w = dl[(size_t) c].read (d);
                    fb[(size_t) c] = w;
                    ch[c][i] = x + mix * ((x + w) * 0.5f - x);
                }
            }
        }
    private:
        float sr = 48000, phase = 0; std::array<float, 2> fb {}; std::array<Delay, 2> dl;
    };

    class Phaser : public Effect
    {
    public:
        void prepare (double s, int) override { sr = (float) s; reset(); }
        void reset() override { for (auto& c : st) c.fill (0.0f); phase = 0; fb = { 0, 0 }; }
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) override
        {
            static constexpr int stageCounts[] = { 2, 4, 6, 8, 12 };
            auto inc = p[0] / sr, depth = p[1] * 0.01f, centre = p[2], feedback = p[3] * 0.01f * 0.9f, width = p[5] * 0.01f;
            auto stages = stageCounts[juce::jlimit (0, 4, (int) p[4])];
            float* ch[2] = { L, R };
            for (int i = 0; i < n; ++i)
            {
                phase += inc; if (phase >= 1.0f) phase -= 1.0f;
                for (int c = 0; c < 2; ++c)
                {
                    auto m = std::sin (twoPi * (phase + (float) c * 0.25f * width));
                    auto hz = juce::jlimit (30.0f, 0.45f * sr, centre * std::exp2 (m * depth * 2.5f));
                    auto g = std::tan (pi * hz / sr);
                    auto a = (1.0f - g) / (1.0f + g);
                    auto x = ch[c][i];
                    auto y = x + softClip (fb[(size_t) c] * feedback);
                    for (int k = 0; k < stages; ++k)
                    {
                        // first-order all-pass: y = -a x + z ; z = x + a y
                        auto& z = st[(size_t) c][(size_t) k];
                        auto out = z - a * y;
                        z = y + a * out;
                        y = out;
                    }
                    fb[(size_t) c] = y;
                    ch[c][i] = x + mix * ((x + y) * 0.5f - x);
                }
            }
        }
    private:
        float sr = 48000, phase = 0; std::array<float, 2> fb {};
        std::array<std::array<float, 12>, 2> st {};
    };

    class DelayFx : public Effect
    {
    public:
        void prepare (double s, int block) override { engine.prepare (s, block); tmp.setSize (2, block); dry.setSize (2, block); }
        void reset() override { engine.reset(); }
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext& ctx) override
        {
            static constexpr double beats[] = { 0.0625, 0.125, 1.0 / 6, 0.25, 0.375, 1.0 / 3, 0.5, 0.75, 2.0 / 3, 1, 1.5, 2, 4 };
            orbit::OrbitSettings s;
            static constexpr int modeMap[] = { orbit::Modes::digital, orbit::Modes::tape, orbit::Modes::analog, orbit::Modes::pingPong,
                                               orbit::Modes::reverse, orbit::Modes::shimmer, orbit::Modes::lofi, orbit::Modes::dub };
            s.mode = modeMap[juce::jlimit (0, 7, (int) p[0])];
            s.delaySeconds = p[1] > 0.5f ? beats[juce::jlimit (0, 12, (int) p[2])] * 60.0 / juce::jmax (20.0, ctx.bpm) : p[3] * 0.001;
            s.delaySeconds = juce::jlimit (0.005, 5.5, s.delaySeconds);
            s.feedback = p[4] * 0.01f * 0.95f;
            s.lowCut = p[5]; s.highCut = p[6]; s.width = p[7] * 0.01f;
            s.mix = 1.0f; s.wobble = s.mode == orbit::Modes::tape ? 0.25f : 0.05f; s.drive = 0.15f;
            tmp.setSize (2, n, false, false, true);
            tmp.copyFrom (0, 0, L, n); tmp.copyFrom (1, 0, R, n);
            engine.process (tmp, s);
            float d, w; parallelGains (mix, d, w);
            for (int i = 0; i < n; ++i) { L[i] = L[i] * d + tmp.getSample (0, i) * w; R[i] = R[i] * d + tmp.getSample (1, i) * w; }
        }
    private:
        orbit::OrbitEngine engine;
        juce::AudioBuffer<float> tmp, dry;
    };

    class ReverbFx : public Effect
    {
    public:
        void prepare (double s, int block) override { verb.prepare (s); wl.assign ((size_t) block, 0.0f); wr.assign ((size_t) block, 0.0f); }
        void reset() override { verb.reset(); }
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) override
        {
            QuasarVerb::Settings s;
            s.type = (int) p[0]; s.size = p[1] * 0.01f; s.preDelayMs = p[2]; s.decay = p[3]; s.damp = p[4];
            s.width = p[5] * 0.01f; s.lowCut = p[6]; s.mod = p[7] * 0.01f;
            if ((int) wl.size() < n) return;
            verb.process (L, R, wl.data(), wr.data(), n, s);
            float d, w; parallelGains (mix, d, w);
            for (int i = 0; i < n; ++i) { L[i] = L[i] * d + wl[(size_t) i] * w; R[i] = R[i] * d + wr[(size_t) i] * w; }
        }
    private:
        QuasarVerb verb; std::vector<float> wl, wr;
    };

    class Equalizer : public Effect
    {
    public:
        void prepare (double s, int) override { sr = s; reset(); }
        void reset() override { for (auto& c : bq) for (auto& b : c) b.reset(); }
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) override
        {
            // smoothed control values -> coefficients per block (blocks are 32 samples)
            auto sm = [] (float& v, float t) { v = v < -1.0e8f ? t : v + 0.25f * (t - v); return v; };
            auto lf = sm (s[0], p[0]), lg = sm (s[1], p[1]), mf = sm (s[2], p[2]), mg = sm (s[3], p[3]), mq = sm (s[4], p[4]),
                 hf = sm (s[5], p[5]), hg = sm (s[6], p[6]), lc = sm (s[7], p[7]);
            bool useLow = std::abs (lg) > 0.01f, useMid = std::abs (mg) > 0.01f, useHigh = std::abs (hg) > 0.01f, useCut = lc > 20.5f;
            for (auto& c : bq)
            {
                if (useLow) c[0].lowShelf (lf, lg, sr);
                if (useMid) c[1].peak (mf, mq, mg, sr);
                if (useHigh) c[2].highShelf (hf, hg, sr);
                if (useCut) c[3].highPass (lc, sr);
            }
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    auto x = ch[c][i], y = x;
                    if (useCut) y = bq[(size_t) c][3].tick (y);
                    if (useLow) y = bq[(size_t) c][0].tick (y);
                    if (useMid) y = bq[(size_t) c][1].tick (y);
                    if (useHigh) y = bq[(size_t) c][2].tick (y);
                    ch[c][i] = x + mix * (y - x);
                }
        }
    private:
        double sr = 48000; std::array<std::array<Biquad, 4>, 2> bq;
        std::array<float, 8> s { -1.0e9f, -1.0e9f, -1.0e9f, -1.0e9f, -1.0e9f, -1.0e9f, -1.0e9f, -1.0e9f };
    };

    class FilterFx : public Effect
    {
    public:
        void prepare (double s, int) override { sr = (float) s; reset(); }
        void reset() override { for (auto& a : f) for (auto& b : a) b.reset(); cut = -1; }
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) override
        {
            auto type = (int) p[0];
            cut = cut < 0 ? p[1] : cut * std::pow (p[1] / cut, 0.3f);
            auto q = 0.5f + p[2] * 0.01f * 12.0f, drive = 1.0f + p[3] * 0.01f * 6.0f, gainDb = p[4];
            auto two = type == 1 || type == 3;
            for (auto& a : f) { a[0].set (cut, two ? 0.54f : q, sr); a[1].set (cut, two ? q : q, sr); }
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    auto x = ch[c][i];
                    auto v = drive > 1.01f ? softClip (x * drive) / std::sqrt (drive) : x;
                    float lo, bd, hi, y;
                    f[(size_t) c][0].tick (v, lo, bd, hi);
                    switch (type)
                    {
                        case 0: y = lo; break;
                        case 1: f[(size_t) c][1].tick (lo, lo, bd, hi); y = lo; break;
                        case 2: y = hi; break;
                        case 3: f[(size_t) c][1].tick (hi, lo, bd, hi); y = hi; break;
                        case 4: y = bd; break;
                        case 5: y = lo + hi; break;
                        default: y = v + (dbToGain (gainDb) - 1.0f) * bd * (1.0f / q); break;
                    }
                    ch[c][i] = x + mix * (y - x);
                }
        }
    private:
        float sr = 48000, cut = -1; std::array<std::array<CleanSvf, 2>, 2> f;
    };

    class Distortion : public Effect
    {
    public:
        void prepare (double s, int block) override
        {
            sr = (float) s;
            over = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
            over->initProcessing ((size_t) block);
            reset();
        }
        void reset() override { if (over) over->reset(); for (auto& z : lp) z.reset(); for (auto& d : dc) d.reset(); }
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) override
        {
            auto mode = (int) p[0];
            auto drive = dbToGain (p[1] * 0.36f), bias = p[3] * 0.01f * 0.5f, outG = dbToGain (p[4]) / juce::jmax (1.0f, std::pow (drive, 0.45f));
            auto lpC = OnePole::coeff (p[2], sr * 2);
            float* ch[2] = { L, R };
            juce::dsp::AudioBlock<float> block (ch, 2, (size_t) n);
            auto up = over->processSamplesUp (block);
            for (int c = 0; c < 2; ++c)
            {
                auto* x = up.getChannelPointer ((size_t) c);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    auto dry = x[i];
                    auto v = dry * drive + bias;
                    float y;
                    switch (mode)
                    {
                        case 1: y = juce::jlimit (-1.0f, 1.0f, v); break;
                        case 2: { y = v; for (int k = 0; k < 4 && std::abs (y) > 1.0f; ++k) y = y > 0 ? 2.0f - y : -2.0f - y; break; }
                        case 3: y = softClip (v * 3.0f) * 0.8f + 0.2f * juce::jlimit (-1.0f, 1.0f, v * 8.0f); break;
                        case 4: y = v >= 0 ? softClip (v) : softClip (v * 0.6f) * 0.85f; break;
                        case 5: y = std::abs (softClip (v)) * 2.0f - 0.6f; break;
                        case 6: y = std::sin (v * 1.5f); break;
                        default: y = softClip (v); break;
                    }
                    y = dc[(size_t) c].process (y);
                    y = lp[(size_t) c].lp (y, lpC);
                    x[i] = dry + mix * (y * outG - dry);
                }
            }
            over->processSamplesDown (block);
        }
    private:
        float sr = 48000;
        std::unique_ptr<juce::dsp::Oversampling<float>> over;
        std::array<OnePole, 2> lp; std::array<DcBlocker, 2> dc;
    };

    class Bitcrush : public Effect
    {
    public:
        void prepare (double s, int) override { sr = (float) s; reset(); }
        void reset() override { cnt = 0; hold = { 0, 0 }; lp[0].reset(); lp[1].reset(); }
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) override
        {
            auto step = juce::jmax (1.0f, sr / juce::jmax (100.0f, p[0]));
            auto q = std::exp2 (juce::jlimit (1.0f, 16.0f, p[1]) - 1.0f), jitter = p[2] * 0.01f;
            auto lpC = OnePole::coeff (p[3], sr);
            for (int i = 0; i < n; ++i)
            {
                cnt -= 1.0f;
                if (cnt <= 0.0f)
                {
                    cnt += step * (1.0f + jitter * (rnd.white() * 0.5f));
                    hold[0] = std::round (L[i] * q) / q; hold[1] = std::round (R[i] * q) / q;
                }
                auto yl = lp[0].lp (hold[0], lpC), yr = lp[1].lp (hold[1], lpC);
                L[i] += mix * (yl - L[i]); R[i] += mix * (yr - R[i]);
            }
        }
    private:
        float sr = 48000, cnt = 0; std::array<float, 2> hold {}; std::array<OnePole, 2> lp; Noise rnd;
    };

    class Tape : public Effect
    {
    public:
        void prepare (double s, int) override { sr = (float) s; for (auto& d : dl) d.prepare ((int) (0.05 * sr)); reset(); }
        void reset() override { for (auto& d : dl) d.clear(); wow = flut = 0; for (auto& z : lp) z.reset(); for (auto& z : hp) z.reset(); env = 0; }
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) override
        {
            auto wowD = p[0] * 0.01f * 0.0018f * sr, flD = p[1] * 0.01f * 0.00025f * sr, sat = p[2] * 0.01f, hiss = p[3] * 0.01f, age = p[4] * 0.01f;
            auto lpC = OnePole::coeff (20000.0f * std::pow (0.2f, age), sr), hpC = OnePole::coeff (20.0f + 100.0f * age, sr);
            auto drive = 1.0f + sat * 3.0f;
            auto envC = OnePole::coeff (3.0f, sr);
            float* ch[2] = { L, R };
            for (int i = 0; i < n; ++i)
            {
                wow += 0.55f / sr; if (wow >= 1) wow -= 1;
                flut += 6.8f / sr; if (flut >= 1) flut -= 1;
                wander += 0.0005f * (rnd.white() - wander * 0.5f);
                auto d = 2.0f + wowD * (1.0f + std::sin (twoPi * wow) + 0.3f * wander) + flD * (1.0f + std::sin (twoPi * flut));
                env += envC * (juce::jmax (std::abs (L[i]), std::abs (R[i])) - env);
                for (int c = 0; c < 2; ++c)
                {
                    auto x = ch[c][i];
                    dl[(size_t) c].push (x);
                    auto y = dl[(size_t) c].read (d);
                    y = softClip (y * drive) / softClip (drive);
                    y = lp[(size_t) c].lp (y, lpC);
                    y -= hp[(size_t) c].lp (y, hpC);
                    y += hiss * 0.02f * rnd.pink() * juce::jmin (1.0f, env * 4.0f);   // hiss follows the signal: no endless noise
                    ch[c][i] = x + mix * (y - x);
                }
            }
        }
    private:
        float sr = 48000, wow = 0, flut = 0, wander = 0, env = 0;
        std::array<Delay, 2> dl; std::array<OnePole, 2> lp, hp; Noise rnd;
    };

    class StereoWidth : public Effect
    {
    public:
        void prepare (double s, int) override { sr = (float) s; haas.prepare ((int) (0.035 * sr)); reset(); }
        void reset() override { haas.clear(); sideLp.reset(); }
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) override
        {
            auto w = p[0] * 0.01f, monoHz = p[1], haasS = p[2] * 0.001f * sr;
            auto c = OnePole::coeff (monoHz, sr);
            for (int i = 0; i < n; ++i)
            {
                auto r = R[i];
                if (haasS > 1.0f) { haas.push (r); r = haas.read (haasS); }
                auto mid = 0.5f * (L[i] + r), side = 0.5f * (L[i] - r);
                if (monoHz > 20.5f) side -= sideLp.lp (side, c);
                side *= w;
                auto yl = mid + side, yr = mid - side;
                L[i] += mix * (yl - L[i]); R[i] += mix * (yr - R[i]);
            }
        }
    private:
        float sr = 48000; Delay haas; OnePole sideLp;
    };

    class Utility : public Effect
    {
    public:
        void prepare (double, int) override { g.setLength (256); g.snap (1); }
        void reset() override {}
        void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) override
        {
            g.set (dbToGain (p[0]));
            auto pan = p[1] * 0.01f;
            auto gl = pan > 0 ? 1.0f - pan : 1.0f, gr = pan < 0 ? 1.0f + pan : 1.0f;
            auto mono = p[2] > 0.5f; auto il = p[3] > 0.5f ? -1.0f : 1.0f, ir = p[4] > 0.5f ? -1.0f : 1.0f;
            for (int i = 0; i < n; ++i)
            {
                auto gg = g.next();
                auto l = L[i] * il, r = R[i] * ir;
                if (mono) l = r = 0.5f * (l + r);
                l *= gg * gl; r *= gg * gr;
                L[i] += mix * (l - L[i]); R[i] += mix * (r - R[i]);
            }
        }
    private:
        Ramp g;
    };
}

std::unique_ptr<Effect> Effect::create (FxType t)
{
    std::unique_ptr<Effect> e;
    switch (t)
    {
        case FxType::saturation:  e = std::make_unique<Saturation>(); break;
        case FxType::compressor:  e = std::make_unique<Compressor>(); break;
        case FxType::chorus:      e = std::make_unique<Chorus>(); break;
        case FxType::flanger:     e = std::make_unique<Flanger>(); break;
        case FxType::phaser:      e = std::make_unique<Phaser>(); break;
        case FxType::delay:       e = std::make_unique<DelayFx>(); break;
        case FxType::reverb:      e = std::make_unique<ReverbFx>(); break;
        case FxType::equalizer:   e = std::make_unique<Equalizer>(); break;
        case FxType::filter:      e = std::make_unique<FilterFx>(); break;
        case FxType::distortion:  e = std::make_unique<Distortion>(); break;
        case FxType::bitcrush:    e = std::make_unique<Bitcrush>(); break;
        case FxType::tape:        e = std::make_unique<Tape>(); break;
        case FxType::stereoWidth: e = std::make_unique<StereoWidth>(); break;
        case FxType::utility:     e = std::make_unique<Utility>(); break;
        case FxType::empty: case FxType::count: default: return {};
    }
    e->type = t;
    return e;
}

// ================================================================== chain

FxChain::FxChain (juce::AudioProcessorValueTreeState& s, int b) : state (s), bus (b)
{
    for (int i = 0; i < numFxSlots; ++i)
    {
        auto& sl = slots[(size_t) i];
        sl.type = s.getRawParameterValue (IDs::fx (b, i, "type"));
        sl.enabled = s.getRawParameterValue (IDs::fx (b, i, "on"));
        sl.mix = s.getRawParameterValue (IDs::fx (b, i, "mix"));
        sl.mixDest = ModDefs::destIndex (IDs::fx (b, i, "mix"));
        for (int p = 0; p < numFxParams; ++p)
        {
            sl.p[(size_t) p] = s.getRawParameterValue (IDs::fxParam (b, i, p));
            sl.pDest[(size_t) p] = ModDefs::destIndex (IDs::fxParam (b, i, p));
        }
        sl.on.setLength (fadeLength); sl.mixR.setLength (128);
    }
}

FxChain::~FxChain()
{
    for (auto& s : slots) s.pending.store (nullptr);
    alive.clear();
}

void FxChain::prepare (double sampleRate, int block)
{
    sr = sampleRate; maxBlock = juce::jmax (32, block);
    scratch.setSize (4, maxBlock);
    const juce::ScopedLock sl (serviceLock);
    // non-realtime: rebuild everything synchronously
    for (auto& s : slots) { s.pending.store (nullptr); s.current = nullptr; s.fading = nullptr; s.requested = -1; }
    alive.clear();
    int dummyA, dummyB, ra, rb;
    retireFifo.prepareToRead (retireFifo.getNumReady(), dummyA, ra, dummyB, rb);
    retireFifo.finishedRead (ra + rb);
    service (true);
}

void FxChain::retire (Effect* e) noexcept
{
    if (e == nullptr) return;
    int s1, n1, s2, n2;
    retireFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) retireQueue[(size_t) s1] = e;
    else if (n2 > 0) retireQueue[(size_t) s2] = e;
    retireFifo.finishedWrite (n1 + n2);   // if full, the effect simply stays alive until prepare()
}

void FxChain::service (bool immediate)
{
    const juce::ScopedLock sl (serviceLock);
    // free what the audio thread no longer uses
    int s1, n1, s2, n2;
    retireFifo.prepareToRead (retireFifo.getNumReady(), s1, n1, s2, n2);
    auto drop = [this] (Effect* e) { alive.erase (std::remove_if (alive.begin(), alive.end(), [e] (auto& p) { return p.get() == e; }), alive.end()); };
    for (int i = 0; i < n1; ++i) drop (retireQueue[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) drop (retireQueue[(size_t) (s2 + i)]);
    retireFifo.finishedRead (n1 + n2);

    for (auto& s : slots)
    {
        auto t = juce::roundToInt (s.type->load());
        if (t == s.requested) continue;
        s.requested = t;
        auto e = Effect::create ((FxType) t);
        Effect* raw = e.get();
        if (e) { e->prepare (sr, maxBlock); alive.push_back (std::move (e)); }
        if (immediate) { s.current = raw; s.fading = nullptr; s.on.snap (s.enabled->load() > 0.5f ? 1.0f : 0.0f); continue; }
        // a marker for "empty": an effect-less pending is represented by a sentinel
        auto* old = s.pending.exchange (raw != nullptr ? raw : reinterpret_cast<Effect*> (&s));
        if (old != nullptr && old != reinterpret_cast<Effect*> (&s)) drop (old);
    }
}

void FxChain::reset()
{
    for (auto& s : slots) { if (s.current) s.current->reset(); }
}

int FxChain::getType (int slot) const { return juce::roundToInt (slots[(size_t) slot].type->load()); }

void FxChain::setType (int slot, int type)
{
    auto set = [this] (const juce::String& id, float realValue)
    {
        if (auto* p = state.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (realValue)); p->endChangeGesture(); }
    };
    set (IDs::fx (bus, slot, "type"), (float) type);
    set (IDs::fx (bus, slot, "on"), 1.0f);
    set (IDs::fx (bus, slot, "mix"), (type == (int) FxType::reverb || type == (int) FxType::delay) ? 30.0f : 100.0f);
    auto& info = fxInfo (type);
    for (int p = 0; p < numFxParams; ++p) set (IDs::fxParam (bus, slot, p), info.params[(size_t) p].def);
    service();
}

void FxChain::remove (int slot)
{
    // close the gap: move every following slot one up, then clear the last one
    for (int s = slot; s < numFxSlots - 1; ++s) move (s + 1, s);
    auto set = [this] (const juce::String& id, float realValue)
    { if (auto* p = state.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (realValue)); p->endChangeGesture(); } };
    set (IDs::fx (bus, numFxSlots - 1, "type"), 0.0f);
    service();
}

void FxChain::move (int from, int to)
{
    if (from == to || ! juce::isPositiveAndBelow (from, numFxSlots) || ! juce::isPositiveAndBelow (to, numFxSlots)) return;
    // rotate the parameters of [from .. to]
    auto read = [this] (int slot)
    {
        std::vector<float> v;
        for (auto* n : { "type", "on", "mix" }) v.push_back (state.getParameter (IDs::fx (bus, slot, n))->getValue());
        for (int p = 0; p < numFxParams; ++p) v.push_back (state.getParameter (IDs::fxParam (bus, slot, p))->getValue());
        return v;
    };
    auto write = [this] (int slot, const std::vector<float>& v)
    {
        int k = 0;
        for (auto* n : { "type", "on", "mix" }) { auto* p = state.getParameter (IDs::fx (bus, slot, n)); p->beginChangeGesture(); p->setValueNotifyingHost (v[(size_t) k++]); p->endChangeGesture(); }
        for (int p = 0; p < numFxParams; ++p) { auto* pp = state.getParameter (IDs::fxParam (bus, slot, p)); pp->beginChangeGesture(); pp->setValueNotifyingHost (v[(size_t) k++]); pp->endChangeGesture(); }
    };
    std::vector<std::vector<float>> vals;
    auto lo = juce::jmin (from, to), hi = juce::jmax (from, to);
    for (int s = lo; s <= hi; ++s) vals.push_back (read (s));
    if (from < to) std::rotate (vals.begin(), vals.begin() + 1, vals.end());
    else std::rotate (vals.rbegin(), vals.rbegin() + 1, vals.rend());
    for (int s = lo; s <= hi; ++s) write (s, vals[(size_t) (s - lo)]);
    // same type in the same place keeps its instance; others get a fresh one (with a crossfade)
    for (int s = lo; s <= hi; ++s) slots[(size_t) s].requested = -2;
    service();
}

void FxChain::process (juce::AudioBuffer<float>& buf, int n, const FxContext& ctx, const std::function<float (int)>& globalOffset)
{
    auto* L = buf.getWritePointer (0);
    auto* R = buf.getWritePointer (1);
    auto* oldL = scratch.getWritePointer (0); auto* oldR = scratch.getWritePointer (1);
    auto* inL = scratch.getWritePointer (2);  auto* inR = scratch.getWritePointer (3);

    for (int si = 0; si < numFxSlots; ++si)
    {
        auto& s = slots[(size_t) si];
        if (auto* p = s.pending.exchange (nullptr))
        {
            auto* incoming = p == reinterpret_cast<Effect*> (&s) ? nullptr : p;
            if (s.fading != nullptr) retire (s.fading);
            s.fading = s.current;
            s.fadePos = 0;
            s.current = incoming;
            if (s.fading == nullptr && s.current != nullptr) { s.fadePos = fadeLength; s.on.snap (0.0f); }
        }
        if (s.current == nullptr && s.fading == nullptr) { activity[(size_t) si].store (0.0f); continue; }

        s.on.set (s.enabled->load() > 0.5f ? 1.0f : 0.0f);
        if (! s.on.isSmoothing() && s.on.value <= 0.0f && s.fading == nullptr) { activity[(size_t) si].store (0.0f); continue; }

        // effective parameters (with modulation)
        auto realParams = [&] (Effect* e)
        {
            std::array<float, 8> v {};
            auto& info = fxInfo ((int) e->type);
            for (int p = 0; p < numFxParams; ++p)
                v[(size_t) p] = info.params[(size_t) p].toReal (s.p[(size_t) p]->load() + globalOffset (s.pDest[(size_t) p]));
            return v;
        };
        auto mixTarget = juce::jlimit (0.0f, 1.0f, s.mix->load() * 0.01f + globalOffset (s.mixDest));
        s.mixR.set (mixTarget);
        auto mixNow = s.mixR.next();
        for (int k = 1; k < n; ++k) s.mixR.next();
        auto onNow = s.on.value;
        for (int k = 0; k < n; ++k) s.on.next();
        auto mixEff = mixNow * juce::jlimit (0.0f, 1.0f, onNow);

        if (s.fading != nullptr)
        {
            // old instance on a copy of the input, then crossfade
            juce::FloatVectorOperations::copy (inL, L, n); juce::FloatVectorOperations::copy (inR, R, n);
            juce::FloatVectorOperations::copy (oldL, L, n); juce::FloatVectorOperations::copy (oldR, R, n);
            s.fading->process (oldL, oldR, n, realParams (s.fading), mixEff, ctx);
            if (s.current != nullptr) s.current->process (L, R, n, realParams (s.current), mixEff, ctx);
            for (int i = 0; i < n; ++i)
            {
                auto t = juce::jmin (1.0f, (float) (s.fadePos + i) / (float) fadeLength);
                auto nl = s.current != nullptr ? L[i] : inL[i], nr = s.current != nullptr ? R[i] : inR[i];
                L[i] = oldL[i] + t * (nl - oldL[i]);
                R[i] = oldR[i] + t * (nr - oldR[i]);
            }
            s.fadePos += n;
            if (s.fadePos >= fadeLength) { retire (s.fading); s.fading = nullptr; }
        }
        else if (s.current != nullptr)
        {
            s.current->process (L, R, n, realParams (s.current), mixEff, ctx);
        }
        activity[(size_t) si].store (onNow);
    }
}

} // namespace quasar
