#include "Wavetables.h"

namespace quasar
{

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr int N = Wavetable::baseSize;
    constexpr int H = Wavetable::maxHarmonics;

    using Spectrum = Wavetable::Spectrum;

    Spectrum emptySpectrum() { Spectrum s; s.re.assign (H, 0.0f); s.im.assign (H, 0.0f); return s; }

    void addSine (Spectrum& s, int n, float amp, float phase = 0.0f)
    {
        if (n <= 0 || n >= H) return;
        // x = amp * sin (2 pi n t + phase)  ->  c_n = amp/2 * (sin(phase) - i cos(phase))
        s.re[(size_t) n] += 0.5f * amp * std::sin (phase);
        s.im[(size_t) n] -= 0.5f * amp * std::cos (phase);
    }

    /** Spectrum of one cycle given in the time domain (N samples). */
    Spectrum spectrumOf (const std::vector<float>& x)
    {
        static juce::dsp::FFT fft (11);   // 2048
        std::vector<juce::dsp::Complex<float>> in ((size_t) N), out ((size_t) N);
        for (int i = 0; i < N; ++i) in[(size_t) i] = { x[(size_t) i], 0.0f };
        fft.perform (in.data(), out.data(), false);
        auto s = emptySpectrum();
        for (int n = 1; n < H; ++n)
        {
            s.re[(size_t) n] = out[(size_t) n].real() / (float) N;
            s.im[(size_t) n] = out[(size_t) n].imag() / (float) N;
        }
        return s;
    }

    template <typename Fn>   // fn (t 0..1) -> sample
    Spectrum timeDomain (Fn fn, int oversample = 8)
    {
        // render oversampled and box-average so naive shapes do not fold harmonics into the frame
        std::vector<float> x ((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            double acc = 0;
            for (int k = 0; k < oversample; ++k)
                acc += fn (((double) i + ((double) k + 0.5) / oversample) / (double) N);
            x[(size_t) i] = (float) (acc / oversample);
        }
        return spectrumOf (x);
    }

    float saw (double t)     { t -= std::floor (t); return (float) (2.0 * t - 1.0); }
    float square (double t, double pw = 0.5) { t -= std::floor (t); return t < pw ? 1.0f : -1.0f; }
    float tri (double t)     { t -= std::floor (t); return (float) (t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t); }
    float sine (double t)    { return (float) std::sin (2.0 * juce::MathConstants<double>::pi * t); }

    /** Morph between key shapes in the time domain. */
    template <typename Fn>
    std::vector<Spectrum> frames (int count, Fn perFrame)
    {
        std::vector<Spectrum> v;
        v.reserve ((size_t) count);
        for (int k = 0; k < count; ++k) v.push_back (perFrame (count > 1 ? (float) k / (float) (count - 1) : 0.0f));
        return v;
    }

    Spectrum additive (std::function<float (int n)> amp, std::function<float (int n)> phase = {})
    {
        auto s = emptySpectrum();
        for (int n = 1; n < H; ++n)
        {
            auto a = amp (n);
            if (std::abs (a) > 1.0e-7f) addSine (s, n, a, phase ? phase (n) : 0.0f);
        }
        return s;
    }

    float formantGain (float freq, float f1, float f2, float f3)
    {
        auto bp = [] (float f, float fc, float bw) { auto d = (f - fc) / bw; return 1.0f / (1.0f + d * d); };
        return 0.08f + bp (freq, f1, 90.0f) + 0.7f * bp (freq, f2, 120.0f) + 0.35f * bp (freq, f3, 180.0f);
    }

    struct Vowel { float f1, f2, f3; };
    const Vowel vowels[] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };

    Vowel vowelAt (float t)
    {
        auto x = juce::jlimit (0.0f, 3.999f, t * 4.0f);
        auto i = (int) x; auto f = x - (float) i;
        auto& a = vowels[i]; auto& b = vowels[i + 1];
        return { a.f1 + f * (b.f1 - a.f1), a.f2 + f * (b.f2 - a.f2), a.f3 + f * (b.f3 - a.f3) };
    }

    std::vector<Spectrum> generate (int index)
    {
        juce::Random rnd (1000 + index);
        switch (index)
        {
            case 0:  // BASIC SHAPES: sine -> triangle -> saw -> square -> pulse
                return frames (32, [] (float p)
                {
                    return timeDomain ([p] (double t)
                    {
                        auto x = p * 4.0f;
                        auto seg = juce::jmin (3, (int) x);
                        auto f = x - (float) seg;
                        float a, b;
                        switch (seg)
                        {
                            case 0: a = sine (t); b = tri (t); break;
                            case 1: a = tri (t); b = saw (t); break;
                            case 2: a = saw (t); b = square (t); break;
                            default: a = square (t); b = square (t, 0.2); break;
                        }
                        return a + f * (b - a);
                    });
                });
            case 1:  // ANALOG SAW: bright -> dark, rounded like a real VCO
                return frames (32, [] (float p)
                {
                    auto nc = 2.0f * std::pow (300.0f, 1.0f - p);
                    return additive ([nc] (int n) { return (2.0f / pi) / (float) n / std::sqrt (1.0f + std::pow ((float) n / nc, 2.0f)); },
                                     [] (int n) { return pi - 0.002f * (float) n; });
                });
            case 2:  // PWM
                return frames (32, [] (float p) { auto pw = 0.5 - 0.47 * p; return timeDomain ([pw] (double t) { return square (t, pw); }); });
            case 3:  // WARM SWEEP: resonant low-pass sweep of a saw
                return frames (32, [] (float p)
                {
                    auto fc = 1.5f * std::pow (120.0f, p);
                    return additive ([fc] (int n)
                    {
                        auto r = (float) n / fc;
                        auto hres = 1.0f / std::sqrt ((1.0f - r * r) * (1.0f - r * r) + r * r / 6.0f);
                        return juce::jmin (3.0f, hres) / (float) n * 0.6f;
                    });
                });
            case 4:  // SAW STACK: saw + octave + 12th + 2 octaves
                return frames (32, [] (float p)
                {
                    return timeDomain ([p] (double t)
                    {
                        return saw (t) + juce::jmin (1.0f, p * 3.0f) * 0.6f * saw (2 * t + 0.13)
                             + juce::jlimit (0.0f, 1.0f, p * 3.0f - 1.0f) * 0.45f * saw (3 * t + 0.31)
                             + juce::jlimit (0.0f, 1.0f, p * 3.0f - 2.0f) * 0.35f * saw (4 * t + 0.57);
                    });
                });
            case 5:  // HOLLOW: odd harmonics, triangle -> square
                return frames (32, [] (float p)
                {
                    return additive ([p] (int n)
                    {
                        if (n % 2 == 0) return 0.0f;
                        auto t = ((n / 2) % 2 == 0 ? 1.0f : -1.0f) / (float) (n * n);
                        auto s = 1.0f / (float) n;
                        return t + p * (s - t);
                    });
                });
            case 6:  // ORGAN drawbars
            {
                const float regs[4][9] = { { 1, 0, 0, 0, 0, 0, 0, 0, 0 }, { 1, 0.8f, 0.6f, 0, 0, 0, 0, 0, 0 },
                                           { 1, 0.7f, 0.8f, 0.5f, 0, 0.4f, 0, 0.3f, 0 }, { 1, 1, 1, 0.8f, 0.6f, 0.6f, 0.5f, 0.5f, 0.4f } };
                const int harm[9] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
                return frames (32, [&] (float p)
                {
                    auto x = p * 3.0f; auto i = juce::jmin (2, (int) x); auto f = x - (float) i;
                    auto s = emptySpectrum();
                    for (int k = 0; k < 9; ++k) addSine (s, harm[k], 0.4f * (regs[i][k] + f * (regs[i + 1][k] - regs[i][k])));
                    return s;
                });
            }
            case 7:  // E-PIANO: tine partials fade into a mellow body
                return frames (32, [] (float p)
                {
                    auto s = emptySpectrum();
                    auto bright = 1.0f - p;
                    addSine (s, 1, 1.0f);
                    addSine (s, 2, 0.35f + 0.25f * bright);
                    addSine (s, 3, 0.12f * bright + 0.05f);
                    addSine (s, 4, 0.1f * bright);
                    addSine (s, 7, 0.28f * bright * bright);
                    addSine (s, 14, 0.12f * bright * bright * bright);
                    addSine (s, 20, 0.05f * std::pow (bright, 4.0f));
                    return s;
                });
            case 8:  // GLASS: square-number partials
                return frames (32, [] (float p)
                {
                    auto s = emptySpectrum();
                    for (int k = 1; k <= 12; ++k)
                        addSine (s, k * k, std::pow (0.55f + 0.35f * p, (float) (k - 1)) / std::sqrt ((float) k));
                    return s;
                });
            case 9: case 10: case 11:   // FM 2-op, ratio 1, 2, 3
            {
                auto ratio = (double) (index - 8);
                return frames (32, [ratio] (float p)
                {
                    auto idx = 6.0 * std::pow ((double) p, 1.5);
                    return timeDomain ([=] (double t) { return (float) std::sin (2.0 * juce::MathConstants<double>::pi * t + idx * std::sin (2.0 * juce::MathConstants<double>::pi * ratio * t)); }, 1);
                });
            }
            case 12: case 13:   // VOWELS / CHOIR (formants on a ~130 Hz voice)
            {
                auto choir = index == 13;
                return frames (32, [choir] (float p)
                {
                    auto v = vowelAt (p);
                    auto f0 = choir ? 220.0f : 130.0f;
                    return additive ([=] (int n)
                    {
                        auto f = f0 * (float) n;
                        if (f > 9000.0f) return 0.0f;
                        auto tilt = choir ? 1.0f / std::pow ((float) n, 1.3f) : 1.0f / (float) n;
                        return tilt * formantGain (f, v.f1, v.f2, v.f3);
                    });
                });
            }
            case 14:  // HARMONIC SWEEP
                return frames (64, [] (float p)
                {
                    auto c = 1.0f + 47.0f * p * p;
                    return additive ([c] (int n) { auto d = ((float) n - c) / (1.0f + c * 0.15f); return std::exp (-d * d) + (n == 1 ? 0.3f : 0.0f); });
                });
            case 15: case 16:   // SYNC SAW / SYNC SQUARE
            {
                auto sq = index == 16;
                return frames (32, [sq] (float p)
                {
                    auto r = 1.0 + 5.0 * p;
                    return timeDomain ([=] (double t) { return sq ? square (t * r) : saw (t * r); });
                });
            }
            case 17:  // DIGITAL: sine with fewer and fewer bits
                return frames (32, [] (float p)
                {
                    auto steps = std::pow (2.0f, 7.0f - 6.0f * p);
                    return timeDomain ([steps] (double t) { return std::round (sine (t) * steps) / steps; });
                });
            case 18:  // FOLD
                return frames (32, [] (float p)
                {
                    auto g = 1.0f + 5.0f * p;
                    return timeDomain ([g] (double t) { return std::sin (pi * 0.5f * g * sine (t)); });
                });
            case 19:  // 808 DRIVE
                return frames (32, [] (float p)
                {
                    auto g = 1.0f + 12.0f * p * p;
                    return timeDomain ([g] (double t) { return std::tanh (g * sine (t)) / std::tanh (g); });
                });
            case 20:  // TAPE SINE: warm 2nd / 3rd harmonics
                return frames (32, [] (float p)
                {
                    auto s = emptySpectrum();
                    addSine (s, 1, 1.0f);
                    addSine (s, 2, 0.25f * p, pi * 0.5f);
                    addSine (s, 3, 0.15f * p);
                    addSine (s, 4, 0.06f * p * p, pi * 0.5f);
                    addSine (s, 5, 0.04f * p * p);
                    return s;
                });
            case 21:  // REESE: two saws with a moving phase offset (scan it with an LFO)
                return frames (64, [] (float p)
                {
                    auto off = (double) p;
                    return timeDomain ([off] (double t) { return 0.5f * (saw (t) + saw (t + off)); });
                });
            case 22:  // PLUCK: brightness decays along the table
                return frames (32, [] (float p)
                {
                    auto k = 0.002f + 0.25f * p * p;
                    return additive ([k] (int n) { return std::exp (-k * (float) (n - 1)) / (float) n * 0.8f; });
                });
            case 23:  // STRINGS: saw with a wooden body
                return frames (32, [] (float p)
                {
                    return additive ([p] (int n)
                    {
                        auto f = 196.0f * (float) n;
                        auto body = 1.0f + 0.8f * std::exp (-std::pow ((f - 450.0f) / 200.0f, 2.0f)) + 0.6f * std::exp (-std::pow ((f - 2500.0f) / 700.0f, 2.0f));
                        auto lp = 1.0f / std::sqrt (1.0f + std::pow (f / (1500.0f + 8000.0f * p), 4.0f));
                        return body * lp / (float) n;
                    });
                });
            case 24:  // BRASS
                return frames (32, [] (float p)
                {
                    return additive ([p] (int n)
                    {
                        auto f = 233.0f * (float) n;
                        auto fc = 400.0f + 4500.0f * p;
                        return (1.0f + 1.2f * std::exp (-std::pow ((f - 1200.0f) / 500.0f, 2.0f))) / (float) n / (1.0f + std::pow (f / fc, 3.0f));
                    });
                });
            case 25:  // FLUTE
                return frames (32, [&rnd] (float p)
                {
                    auto s = emptySpectrum();
                    addSine (s, 1, 1.0f);
                    addSine (s, 2, 0.08f + 0.2f * p);
                    addSine (s, 3, 0.05f + 0.12f * p);
                    for (int n = 4; n < 40; ++n) addSine (s, n, 0.012f * p * rnd.nextFloat() / std::sqrt ((float) n), rnd.nextFloat() * twoPi);
                    return s;
                });
            case 26:  // MALLET: 1 : 4 : 10 like a marimba bar
                return frames (32, [] (float p)
                {
                    auto s = emptySpectrum();
                    auto b = 1.0f - p;
                    addSine (s, 1, 1.0f); addSine (s, 4, 0.5f * b + 0.05f); addSine (s, 10, 0.3f * b * b); addSine (s, 3, 0.06f);
                    addSine (s, 17, 0.1f * b * b * b);
                    return s;
                });
            case 27:  // GUITAR: plucked string, pluck position sweeps
                return frames (32, [] (float p)
                {
                    auto pos = 0.08f + 0.4f * p;
                    return additive ([pos] (int n) { return std::abs (std::sin ((float) n * pi * pos)) / (float) (n * n) * 2.0f + 0.0f * (float) n; });
                });
            case 28:  // CHIP: 50 %, 25 %, 12.5 % pulses and the 4-bit triangle
                return frames (32, [] (float p)
                {
                    auto x = p * 3.0f; auto seg = juce::jmin (2, (int) x); auto f = x - (float) seg;
                    auto shape = [] (int k, double t) -> float
                    {
                        switch (k) { case 0: return square (t, 0.5); case 1: return square (t, 0.25); case 2: return square (t, 0.125);
                                     default: return std::round (tri (t) * 7.5f) / 7.5f; }
                    };
                    return timeDomain ([=] (double t) { return shape (seg, t) + f * (shape (seg + 1, t) - shape (seg, t)); });
                });
            case 29:  // NASAL: narrow pulses with a moving comb
                return frames (32, [] (float p) { auto pw = 0.18 - 0.14 * p; return timeDomain ([pw] (double t) { return square (t, pw) + 0.5f * saw (t); }); });
            case 30:  // SINE STACK: octaves fade in
                return frames (32, [] (float p)
                {
                    auto s = emptySpectrum();
                    const int oct[] = { 1, 2, 4, 8, 16 };
                    for (int k = 0; k < 5; ++k) addSine (s, oct[k], juce::jlimit (0.0f, 1.0f, p * 5.0f - (float) k + 1.0f) / (float) (k + 1));
                    return s;
                });
            case 31:  // DIRTY SAW: asymmetric clipping
                return frames (32, [] (float p)
                {
                    auto g = 1.0f + 6.0f * p;
                    return timeDomain ([g] (double t) { auto x = saw (t) * g + 0.3f * (g - 1.0f) / 6.0f; return std::tanh (x) - std::tanh (0.3f * (g - 1.0f) / 6.0f); });
                });
            case 32:  // COMB SWEEP
                return frames (64, [] (float p)
                {
                    auto d = 0.02f + 0.48f * p;
                    return additive ([d] (int n) { return std::abs (std::cos (pi * (float) n * d)) / (float) n; });
                });
            case 33:  // METALLIC: prime partials
                return frames (32, [] (float p)
                {
                    auto s = emptySpectrum();
                    const int primes[] = { 1, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41 };
                    for (int k = 0; k < 14; ++k) addSine (s, primes[k], std::pow (0.45f + 0.45f * p, (float) k) * (k == 0 ? 1.0f : 0.8f));
                    return s;
                });
            case 34:  // DUST: evolving random spectra
            {
                std::vector<std::vector<float>> keys (5);
                for (auto& k : keys) { k.resize (64); for (int n = 1; n < 64; ++n) k[(size_t) n] = rnd.nextFloat() / std::sqrt ((float) n); }
                return frames (32, [&keys] (float p)
                {
                    auto x = p * 4.0f; auto i = juce::jmin (3, (int) x); auto f = x - (float) i;
                    auto s = emptySpectrum();
                    addSine (s, 1, 1.0f);
                    for (int n = 2; n < 64; ++n) addSine (s, n, keys[(size_t) i][(size_t) n] + f * (keys[(size_t) i + 1][(size_t) n] - keys[(size_t) i][(size_t) n]), (float) n * 1.7f);
                    return s;
                });
            }
            case 35:  // GROWL: vowels pushed through a folder
                return frames (32, [] (float p)
                {
                    auto v = vowelAt (p);
                    auto base = additive ([v] (int n) { auto f = 98.0f * (float) n; return f > 8000.0f ? 0.0f : formantGain (f, v.f1, v.f2, v.f3) / (float) n; });
                    // to time domain, fold, back to spectrum
                    std::vector<float> x ((size_t) N, 0.0f);
                    for (int n = 1; n < 90; ++n)
                        for (int i = 0; i < N; ++i)
                        {
                            auto ph = twoPi * (float) n * (float) i / (float) N;
                            x[(size_t) i] += 2.0f * (base.re[(size_t) n] * std::cos (ph) - base.im[(size_t) n] * std::sin (ph));
                        }
                    float pk = 1.0e-6f; for (auto s : x) pk = juce::jmax (pk, std::abs (s));
                    for (auto& s : x) s = std::sin (pi * 0.5f * (1.0f + 2.5f * p) * s / pk);
                    return spectrumOf (x);
                });
            default:  // ANALOG (hidden): sine -> triangle -> saw -> square -> narrow pulse, slightly rounded
                return frames (32, [] (float p)
                {
                    auto sp = timeDomain ([p] (double t)
                    {
                        auto x = p * 4.0f;
                        auto seg = juce::jmin (3, (int) x);
                        auto f = x - (float) seg;
                        float a, b;
                        switch (seg)
                        {
                            case 0: a = sine (t); b = tri (t); break;
                            case 1: a = tri (t); b = saw (t); break;
                            case 2: a = saw (t); b = square (t); break;
                            default: a = square (t); b = square (t, 0.12); break;
                        }
                        return a + f * (b - a);
                    });
                    // analog edges: a gentle roll-off and a little phase lag on the top harmonics
                    for (int n = 1; n < H; ++n)
                    {
                        auto g = 1.0f / std::sqrt (1.0f + std::pow ((float) n / 320.0f, 2.0f));
                        auto ph = -0.0015f * (float) n;
                        auto re = sp.re[(size_t) n], im = sp.im[(size_t) n];
                        sp.re[(size_t) n] = g * (re * std::cos (ph) - im * std::sin (ph));
                        sp.im[(size_t) n] = g * (re * std::sin (ph) + im * std::cos (ph));
                    }
                    return sp;
                });
        }
    }
}

// ================================================================== Wavetable

std::unique_ptr<Wavetable> Wavetable::fromSpectra (const juce::String& name, const std::vector<Spectrum>& spectra)
{
    auto wt = std::make_unique<Wavetable>();
    wt->name = name;
    wt->numFrames = juce::jlimit (1, 256, (int) spectra.size());

    std::vector<std::unique_ptr<juce::dsp::FFT>> ffts;
    for (int L = 0; L < numLevels; ++L)
    {
        auto harm = maxHarmonics >> L;
        auto size = juce::jmax (256, 8 * harm);   // 8x oversampled table: linear interpolation images stay far below -80 dB
        auto& lv = wt->levels[(size_t) L];
        lv.size = size; lv.mask = size - 1;
        lv.data.assign ((size_t) size * (size_t) wt->numFrames, 0.0f);
    }

    std::vector<juce::dsp::Complex<float>> in, out;
    for (int f = 0; f < wt->numFrames; ++f)
    {
        auto& sp = spectra[(size_t) f];
        float peak = 0.0f;
        for (int L = 0; L < numLevels; ++L)
        {
            auto& lv = wt->levels[(size_t) L];
            auto harm = juce::jmin (maxHarmonics >> L, (int) sp.re.size());
            auto order = (int) std::round (std::log2 ((double) lv.size));
            juce::dsp::FFT fft (order);
            in.assign ((size_t) lv.size, { 0.0f, 0.0f });
            out.assign ((size_t) lv.size, { 0.0f, 0.0f });
            for (int n = 1; n < harm; ++n)
            {
                in[(size_t) n] = { sp.re[(size_t) n], sp.im[(size_t) n] };
                in[(size_t) (lv.size - n)] = { sp.re[(size_t) n], -sp.im[(size_t) n] };
            }
            fft.perform (in.data(), out.data(), true);   // inverse, JUCE scales by 1/size
            auto* dst = lv.data.data() + (size_t) f * (size_t) lv.size;
            for (int i = 0; i < lv.size; ++i) dst[i] = out[(size_t) i].real() * (float) lv.size;
            if (L == 0)
                for (int i = 0; i < lv.size; ++i) peak = juce::jmax (peak, std::abs (dst[i]));
        }
        auto g = peak > 1.0e-6f ? 1.0f / peak : 0.0f;
        for (auto& lv : wt->levels)
            juce::FloatVectorOperations::multiply (lv.data.data() + (size_t) f * (size_t) lv.size, g, lv.size);
    }
    return wt;
}

std::unique_ptr<Wavetable> Wavetable::fromAudio (const juce::String& name, const float* data, int numSamples, int frameSize)
{
    frameSize = juce::jmax (16, frameSize);
    auto count = juce::jlimit (1, 256, numSamples / frameSize);
    std::vector<Spectrum> spectra;
    std::vector<float> cycle ((size_t) N);
    for (int f = 0; f < count; ++f)
    {
        // resample the cycle to 2048 points (linear is fine: we only keep the band-limited spectrum)
        for (int i = 0; i < N; ++i)
        {
            auto x = (double) i * frameSize / N;
            auto i0 = (int) x; auto fr = (float) (x - i0);
            auto a = data[f * frameSize + (i0 % frameSize)], b = data[f * frameSize + ((i0 + 1) % frameSize)];
            cycle[(size_t) i] = a + fr * (b - a);
        }
        spectra.push_back (spectrumOf (cycle));
    }
    return fromSpectra (name, spectra);
}

void Wavetable::drawFrame (float position, std::vector<float>& out, int points) const
{
    out.resize ((size_t) points);
    for (int i = 0; i < points; ++i)
        out[(size_t) i] = read (position, (float) i / (float) points, 2, 0.0f);
}

// ================================================================== library

const juce::StringArray& WavetableLibrary::names()
{
    static const juce::StringArray n { "BASIC SHAPES", "ANALOG SAW", "PWM", "WARM SWEEP", "SAW STACK", "HOLLOW", "ORGAN", "E-PIANO",
                                       "GLASS", "FM 1:1", "FM 2:1", "FM 3:1", "VOWELS", "CHOIR", "HARMONIC SWEEP", "SYNC SAW",
                                       "SYNC SQUARE", "DIGITAL", "FOLD", "808 DRIVE", "TAPE SINE", "REESE", "PLUCK", "STRINGS",
                                       "BRASS", "FLUTE", "MALLET", "GUITAR", "CHIP", "NASAL", "SINE STACK", "DIRTY SAW",
                                       "COMB SWEEP", "METALLIC", "DUST", "GROWL", "USER" };
    return n;
}

class WavetableLibrary::Builder : public juce::Thread
{
public:
    explicit Builder (WavetableLibrary& l) : juce::Thread ("QUASAR wavetables"), lib (l) { startThread (juce::Thread::Priority::low); }
    ~Builder() override { stopThread (4000); }
    void run() override
    {
        while (! threadShouldExit())
        {
            for (int i = 0; i < (int) lib.requested.size() && ! threadShouldExit(); ++i)
                if (lib.requested[(size_t) i].exchange (false)) lib.get (i);
            wait (100);
        }
    }
    WavetableLibrary& lib;
};

WavetableLibrary::WavetableLibrary() { builder = std::make_unique<Builder> (*this); }
WavetableLibrary::~WavetableLibrary() { builder.reset(); }

const Wavetable* WavetableLibrary::tryGet (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, (int) tables.size()) ? tables[(size_t) index].load (std::memory_order_acquire) : nullptr;
}

void WavetableLibrary::request (int index)
{
    if (juce::isPositiveAndBelow (index, (int) tables.size()) && tables[(size_t) index].load() == nullptr)
    {
        requested[(size_t) index].store (true);
        if (builder != nullptr) builder->notify();
    }
}

const Wavetable* WavetableLibrary::get (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) tables.size()) || index == userIndex()) return nullptr;
    if (auto* t = tryGet (index)) return t;
    const juce::ScopedLock sl (lock);
    if (auto* t = tryGet (index)) return t;
    auto name = index == analogIndex() ? juce::String ("ANALOG") : names()[index];
    owned.push_back (Wavetable::fromSpectra (name, generate (index)));
    tables[(size_t) index].store (owned.back().get(), std::memory_order_release);
    return owned.back().get();
}

} // namespace quasar
