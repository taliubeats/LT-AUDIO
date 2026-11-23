#include "AirEngine.h"

namespace air
{

// ================================================================== names

const juce::StringArray& Models::sectionNames()
{
    static const juce::StringArray n { "BODY", "MID AIR", "HIGH AIR" };
    return n;
}

const juce::StringArray& Models::names (int section)
{
    static const juce::StringArray b { "GRAVITY", "CORE", "PROXIMA", "TITAN" };
    static const juce::StringArray m { "LUMEN", "NOVA", "QUASAR", "PHOTON" };
    static const juce::StringArray h { "STRATOS", "ZENITH", "PULSAR", "COMET" };
    return section == body ? b : section == mid ? m : h;
}

juce::String Models::caption (int section, int model)
{
    static const char* c[3][4] = {
        { "Big lows, no mud", "Warm iron body", "Close-mic intimacy", "Harmonic warmth" },
        { "Dynamic presence", "Colored forward mids", "Clean precise presence", "Punchy focused bite" },
        { "Dynamic air", "Silky wide air", "Glossy tube sheen", "Harmonic sparkle" } };
    return c[juce::jlimit (0, 2, section)][juce::jlimit (0, 3, model)];
}

// ================================================================== filters

void Biquad::set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
{
    b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
    a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
}

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    struct W { double cs, sn, alpha; };
    W omega (double fs, double hz, double q)
    {
        auto w0 = twoPi * hz / fs;
        return { std::cos (w0), std::sin (w0), std::sin (w0) / (2.0 * q) };
    }
}

void Biquad::setPeak (double fs, double hz, double q, double db)
{
    if (std::abs (db) < 1.0e-4) { setIdentity(); return; }
    auto A = std::pow (10.0, db / 40.0);
    auto w = omega (fs, hz, q);
    set (1 + w.alpha * A, -2 * w.cs, 1 - w.alpha * A, 1 + w.alpha / A, -2 * w.cs, 1 - w.alpha / A);
}

void Biquad::setLowShelf (double fs, double hz, double q, double db)
{
    if (std::abs (db) < 1.0e-4) { setIdentity(); return; }
    auto A = std::pow (10.0, db / 40.0), sq = 2.0 * std::sqrt (A);
    auto w = omega (fs, hz, q);
    set (A * ((A + 1) - (A - 1) * w.cs + sq * w.alpha), 2 * A * ((A - 1) - (A + 1) * w.cs), A * ((A + 1) - (A - 1) * w.cs - sq * w.alpha),
         (A + 1) + (A - 1) * w.cs + sq * w.alpha, -2 * ((A - 1) + (A + 1) * w.cs), (A + 1) + (A - 1) * w.cs - sq * w.alpha);
}

void Biquad::setHighShelf (double fs, double hz, double q, double db)
{
    if (std::abs (db) < 1.0e-4) { setIdentity(); return; }
    auto A = std::pow (10.0, db / 40.0), sq = 2.0 * std::sqrt (A);
    auto w = omega (fs, hz, q);
    set (A * ((A + 1) + (A - 1) * w.cs + sq * w.alpha), -2 * A * ((A - 1) + (A + 1) * w.cs), A * ((A + 1) + (A - 1) * w.cs - sq * w.alpha),
         (A + 1) - (A - 1) * w.cs + sq * w.alpha, 2 * ((A - 1) - (A + 1) * w.cs), (A + 1) - (A - 1) * w.cs - sq * w.alpha);
}

void Biquad::setHighPass (double fs, double hz, double q)
{
    auto w = omega (fs, hz, q);
    set ((1 + w.cs) / 2, -(1 + w.cs), (1 + w.cs) / 2, 1 + w.alpha, -2 * w.cs, 1 - w.alpha);
}

void Biquad::setLowPass (double fs, double hz, double q)
{
    auto w = omega (fs, hz, q);
    set ((1 - w.cs) / 2, 1 - w.cs, (1 - w.cs) / 2, 1 + w.alpha, -2 * w.cs, 1 - w.alpha);
}

void Biquad::setBandPass (double fs, double hz, double q)
{
    auto w = omega (fs, hz, q);
    set (w.alpha, 0, -w.alpha, 1 + w.alpha, -2 * w.cs, 1 - w.alpha);
}


// ================================================================== dynamics & harmonics

namespace
{
    constexpr int controlEvery = 16;   // frames between two control() calls
    inline float smoothstep (float t) noexcept { t = juce::jlimit (0.0f, 1.0f, t); return t * t * (3.0f - 2.0f * t); }
    inline float coef (double rate, double ms) { return (float) (1.0 - std::exp (-1.0 / (rate * ms * 0.001))); }
}

void Dynamics::prepare (double fs, Filter f, double hz, float thresholdDb, float widthDb, float guardSlope, float guardMaxDb, float transientDb)
{
    if (f == Filter::highPass) det.setHighPass (fs, hz, 0.707);
    else                       det.setLowPass (fs, hz, 0.707);
    refHp.setCutoff (fs, 120.0);
    auto lowBand = f == Filter::lowPass;
    band.set (fs, lowBand ? 6.0 : 3.0, lowBand ? 90.0 : 60.0);
    fast.set (fs, 0.15, lowBand ? 15.0 : 10.0);
    ref.set (fs, 25.0, 350.0);
    auto ctrlRate = fs / controlEvery;                     // the typical level is updated at control rate
    kRise = coef (ctrlRate, 1200.0);
    kFall = coef (ctrlRate, 1200.0);
    kLearn = coef (ctrlRate, 120.0);
    warmTicks = (int) (ctrlRate * 0.05);                   // learn only after 50 ms of voice...
    learnTicks = (int) (ctrlRate * 0.6);                   // ...and quickly during the first 0.6 s
    threshold = thresholdDb; width = widthDb; slope = guardSlope; guardMax = guardMaxDb; transient = transientDb;
    auto ctrl = fs / controlEvery;
    kDown = coef (ctrl, 1.5);
    kUp = coef (ctrl, 70.0);
    reset();
}

void Dynamics::reset()
{
    det.reset(); refHp.reset(); band.reset(); fast.reset(); ref.reset();
    typicalDb = -120.0f;
    smoothed = 0.0f;
    voiced = 0;
}

float Dynamics::gainDb (float boostDb, float amount) noexcept
{
    auto refDb = ref.db();
    auto bandDb = band.db();
    auto voice = juce::jlimit (0.0f, 1.0f, (refDb + 62.0f) / 12.0f);
    if (voice > 0.5f)          // learn how this voice usually sounds in this band (held during silence)
    {
        ++voiced;
        if (voiced == warmTicks) typicalDb = bandDb;
        else if (voiced > warmTicks)
            typicalDb += (voiced < learnTicks ? kLearn : bandDb > typicalDb ? kRise : kFall) * (bandDb - typicalDb);
    }
    auto r = voiced > warmTicks ? bandDb - typicalDb : 0.0f;
    auto g = width > 0.01f ? boostDb * smoothstep ((threshold - r) / width) : boostDb;
    if (r > threshold) g -= juce::jmin (guardMax * amount, (r - threshold) * slope * amount);     // de-ess / de-harsh / de-boom
    auto jump = fast.db() - bandDb;
    if (jump > transient) g -= juce::jmin (6.0f * amount, (jump - transient) * 0.8f * amount);   // sharp consonants / plosives
    g *= voice;                                                                                     // silence: lift nothing
    smoothed += (g - smoothed) * (g < smoothed ? kDown : kUp);
    if (amount <= 0.0f && std::abs (smoothed) < 1.0e-3f) smoothed = 0.0f;
    return smoothed;
}

void Harmonics::prepare (double fs, double postHighPassHz)
{
    level.set (fs, 0.5, 40.0);
    post.setHighPass (fs, postHighPassHz, 0.707);
    reset();
}

void Harmonics::reset() { level.reset(); post.reset(); }

void Harmonics::process (const float* b, float* out, int ch, float even, float odd) noexcept
{
    float pw = 0;
    for (int c = 0; c < ch; ++c) pw += b[c] * b[c];
    level.push (pw / (float) ch);
    auto s = std::sqrt (level.p + 1.0e-10f);
    auto inv = 1.0f / s;
    for (int c = 0; c < ch; ++c)
    {
        auto n = juce::jlimit (-4.0f, 4.0f, b[c] * inv);           // ~unit RMS
        auto n2 = n * n;
        auto h = even * (n2 - 1.0f) + odd * (n2 * n - 1.5f * n);   // pure 2nd / 3rd harmonic of a sine (no fundamental)
        out[c] = post.process (h * s, c);
    }
}

// ================================================================== the 12 models
// Each model imitates the *behaviour* of a classic circuit. The references are for the developers only;
// the UI and the user manual only use the LT names. All of them sit on a program-relative Dynamics block:
// the "smart" ones (LUMEN, STRATOS) fade their boost with the brightness of the voice (Dolby-A trick style),
// the others are classic static curves protected against boom / harshness / sibilance peaks.

namespace
{
    using F = Dynamics::Filter;

    /** Bounded colour curves: pure 2nd / 3rd harmonic at normal levels (alias-free at 2x), gentle and finite when hot. */
    inline float evenCurve (float x) noexcept { auto x2 = x * x; return x2 / (1.0f + x2); }
    inline float oddCurve (float x) noexcept                  // cubic soft clip residual: -x^3/3 inside +-1, bounded outside
    {
        auto c = juce::jlimit (-1.0f, 1.0f, x);
        return (c - c * c * c / 3.0f) * 1.5f - x * 1.5f + 0.5f * (x - c);
    }

    inline float clampFrac (float g, float full) noexcept { return full > 1.0e-4f ? juce::jlimit (-1.0f, 1.0f, g / full) : 0.0f; }

    /** Second harmonic "tube" colour on the whole signal (DC removed). Tiny at normal levels, more when driven (TRIM). */
    struct TubeColour
    {
        OnePole dc;
        void prepare (double fs) { dc.setCutoff (fs, 10.0); }
        void reset() { dc.reset(); }
        inline float process (float y, int ch, float amount) noexcept { return y + dc.hp (amount * evenCurve (y), ch); }
    };

    // ---------------------------------------------------------------- BODY (100-500 Hz)

    /** GRAVITY - Pultec EQP-1A "low trick": big low shelf + a scoop just above it (weight without mud), plosive-safe. */
    struct Gravity : Model
    {
        Dynamics dyn; Biquad shelf, scoop, subsonic; TubeColour tube;
        void prepareModel() override { dyn.prepare (fs, F::lowPass, 150.0, 9.0f, 6.0f, 0.8f, 8.0f, 6.0f); tube.prepare (fs); subsonic.setHighPass (fs, 38.0, 0.7); }
        void reset() override { dyn.reset(); shelf.reset(); scoop.reset(); subsonic.reset(); tube.reset(); }
        void control() override
        {
            auto full = 12.0f * a;
            auto g = dyn.gainDb (full, a);
            shelf.setLowShelf (fs, 165.0, 0.8, g);
            scoop.setPeak (fs, 560.0, 1.0, -2.5f * a);         // the attenuator: big lows, no mud
        }
        void process (float* f, int ch) noexcept override
        {
            dyn.push (f, ch);
            auto hpMix = juce::jmin (1.0f, a * 8.0f);     // the boost would lift rumble below the voice: cut it
            for (int c = 0; c < ch; ++c)
            {
                auto y = tube.process (scoop.process (shelf.process (f[c], c), c), c, 0.05f * a);
                f[c] = y + hpMix * (subsonic.process (y, c) - y);
            }
        }
    };

    /** CORE - Neve 1073 220 Hz low shelf + input/output transformer (iron) harmonics on the lows. */
    struct Core : Model
    {
        Dynamics dyn; Biquad shelf, soft, subsonic; OnePole split, dc;
        void prepareModel() override
        {
            dyn.prepare (fs, F::lowPass, 150.0, 9.0f, 6.0f, 0.8f, 7.0f, 7.0f);
            split.setCutoff (fs, 260.0); dc.setCutoff (fs, 10.0); subsonic.setHighPass (fs, 50.0, 0.7);
        }
        void reset() override { dyn.reset(); shelf.reset(); soft.reset(); subsonic.reset(); split.reset(); dc.reset(); }
        void control() override
        {
            auto g = dyn.gainDb (10.0f * a, a);
            shelf.setLowShelf (fs, 280.0, 0.8, g);
            soft.setHighShelf (fs, nyq (13000.0), 0.7, -1.0 * a);
        }
        void process (float* f, int ch) noexcept override
        {
            dyn.push (f, ch);
            for (int c = 0; c < ch; ++c)
            {
                auto x = f[c];
                auto l = split.lp (x, c);
                x += dc.hp (0.2f * a * evenCurve (l), c) + 0.3f * a * oddCurve (l);    // input iron: 2nd + 3rd harmonics of the lows
                auto y = soft.process (shelf.process (x, c), c);
                f[c] = y + juce::jmin (1.0f, a * 8.0f) * (subsonic.process (y, c) - y);
            }
        }
    };

    /** PROXIMA - close-mic proximity effect: 6 dB/oct-like rise below ~300 Hz, chest, a touch softer presence; pop-safe. */
    struct Proxima : Model
    {
        Dynamics dyn; Biquad shelf, chest, presence, rumble;
        void prepareModel() override { dyn.prepare (fs, F::lowPass, 150.0, 9.0f, 6.0f, 1.0f, 9.0f, 5.0f); rumble.setHighPass (fs, 55.0, 0.7); }
        void reset() override { dyn.reset(); shelf.reset(); chest.reset(); presence.reset(); rumble.reset(); }
        void control() override
        {
            auto full = 11.0f * a;
            auto g = dyn.gainDb (full, a);
            shelf.setLowShelf (fs, 290.0, 0.5, g);
            chest.setPeak (fs, 150.0, 0.9, 2.0f * a * juce::jmax (0.0f, clampFrac (g, full)));
            presence.setPeak (fs, 3500.0, 0.7, -1.5f * a);
        }
        void process (float* f, int ch) noexcept override
        {
            dyn.push (f, ch);
            auto hpMix = juce::jmin (1.0f, a * 8.0f);
            for (int c = 0; c < ch; ++c)
            {
                auto y = presence.process (chest.process (shelf.process (f[c], c), c), c);
                auto hp = rumble.process (y, c);
                f[c] = y + hpMix * (hp - y);
            }
        }
    };

    /** TITAN - tube / tape warmth: low-mid bump + clean, level-independent 2nd/3rd harmonics of the 150-450 Hz band. */
    struct Titan : Model
    {
        Dynamics dyn; Biquad bump, band; Harmonics harm;
        void prepareModel() override
        {
            dyn.prepare (fs, F::lowPass, 150.0, 9.0f, 6.0f, 0.6f, 5.0f, 7.0f);
            band.setBandPass (fs, 260.0, 0.7);
            harm.prepare (fs, 330.0);
        }
        void reset() override { dyn.reset(); bump.reset(); band.reset(); harm.reset(); }
        float mixH = 0.0f;
        void control() override
        {
            auto full = 4.0f * a;
            auto g = dyn.gainDb (full, a);
            bump.setPeak (fs, 230.0, 0.8, g);
            mixH = 0.45f * a;
        }
        void process (float* f, int ch) noexcept override
        {
            dyn.push (f, ch);
            float b[2], h[2];
            for (int c = 0; c < ch; ++c) b[c] = band.process (f[c], c);
            harm.process (b, h, ch, 0.65f, 0.35f);
            for (int c = 0; c < ch; ++c) f[c] = bump.process (f[c], c) + mixH * h[c];
        }
    };

    // ---------------------------------------------------------------- MID AIR (1.5-6 kHz)

    /** LUMEN - dynamic presence (Fresh Air "Mid Air" / Dolby-A band 3 trick): a broad lift that is full on soft or dull
        passages and fades away on bright, harsh peaks. */
    struct Lumen : Model
    {
        Dynamics dyn; Biquad bell;
        void prepareModel() override { dyn.prepare (fs, F::highPass, 4500.0, 8.0f, 10.0f, 0.6f, 6.0f, 7.0f); }
        void reset() override { dyn.reset(); bell.reset(); }
        void control() override { bell.setPeak (fs, 4000.0, 0.5, dyn.gainDb (13.0f * a, a)); }
        void process (float* f, int ch) noexcept override
        {
            dyn.push (f, ch);
            for (int c = 0; c < ch; ++c) f[c] = bell.process (f[c], c);
        }
    };

    /** NOVA - Neve 1073 mid bells (3.2 + 4.8 kHz) with inductor / class-A colour; harshness-guarded. */
    struct Nova : Model
    {
        Dynamics dyn; Biquad bell1, bell2; TubeColour tube;
        void prepareModel() override { dyn.prepare (fs, F::highPass, 3000.0, 7.0f, 6.0f, 0.6f, 5.0f, 8.0f); tube.prepare (fs); }
        void reset() override { dyn.reset(); bell1.reset(); bell2.reset(); tube.reset(); }
        void control() override
        {
            auto full = 7.0f * a;
            auto g = dyn.gainDb (full, a);
            bell1.setPeak (fs, 3200.0, 0.9, g);
            bell2.setPeak (fs, 4800.0, 1.1, g * 4.0f / 7.0f);
        }
        void process (float* f, int ch) noexcept override
        {
            dyn.push (f, ch);
            for (int c = 0; c < ch; ++c) f[c] = tube.process (bell2.process (bell1.process (f[c], c), c), c, 0.08f * a);
        }
    };

    /** QUASAR - SSL E-series high-mid band: clean and precise; harshness-guarded. */
    struct Quasar : Model
    {
        Dynamics dyn; Biquad bell;
        void prepareModel() override { dyn.prepare (fs, F::highPass, 3000.0, 7.0f, 6.0f, 0.6f, 5.0f, 8.0f); }
        void reset() override { dyn.reset(); bell.reset(); }
        void control() override { bell.setPeak (fs, 4000.0, 1.0, dyn.gainDb (9.0f * a, a)); }
        void process (float* f, int ch) noexcept override
        {
            dyn.push (f, ch);
            for (int c = 0; c < ch; ++c) f[c] = bell.process (f[c], c);
        }
    };

    /** PHOTON - API 550A: proportional Q (wide when subtle, focused when pushed) and op-amp punch. */
    struct Photon : Model
    {
        Dynamics dyn; Biquad bell;
        void prepareModel() override { dyn.prepare (fs, F::highPass, 3000.0, 7.0f, 6.0f, 0.6f, 5.0f, 8.0f); }
        void reset() override { dyn.reset(); bell.reset(); }
        void control() override
        {
            auto g = dyn.gainDb (11.0f * a, a);
            bell.setPeak (fs, 3000.0, 0.5 + 1.2 * juce::jmax (0.0f, g) / 11.0, g);
        }
        void process (float* f, int ch) noexcept override
        {
            dyn.push (f, ch);
            auto k = 0.09f * a;
            for (int c = 0; c < ch; ++c) { auto y = bell.process (f[c], c); f[c] = y + 2.0f * k * oddCurve (y); }
        }
    };

    // ---------------------------------------------------------------- HIGH AIR (8-20 kHz)

    /** STRATOS - dynamic air (Fresh Air "High Air" / Dolby-A band 4 trick): a big, smooth top shelf that lifts the
        breathy detail and backs off on sibilance and sharp transients by itself. */
    struct Stratos : Model
    {
        Dynamics dyn; Biquad shelf, ultra;
        void prepareModel() override { dyn.prepare (fs, F::highPass, 9000.0, 8.0f, 10.0f, 0.8f, 8.0f, 7.0f); }
        void reset() override { dyn.reset(); shelf.reset(); ultra.reset(); }
        void control() override
        {
            auto g = dyn.gainDb (17.0f * a, a);
            shelf.setHighShelf (fs, nyq (9500.0), 0.55, g);
            ultra.setHighShelf (fs, nyq (18000.0), 0.6, 0.25f * g);
        }
        void process (float* f, int ch) noexcept override
        {
            dyn.push (f, ch);
            for (int c = 0; c < ch; ++c) f[c] = ultra.process (shelf.process (f[c], c), c);
        }
    };

    /** ZENITH - Maag EQ4 Air Band (20 kHz): a very wide, silky shelf whose slope starts low; sibilance-guarded. */
    struct Zenith : Model
    {
        Dynamics dyn; Biquad shelf, ultra;
        void prepareModel() override { dyn.prepare (fs, F::highPass, 6000.0, 7.0f, 5.0f, 0.8f, 6.0f, 7.0f); }
        void reset() override { dyn.reset(); shelf.reset(); ultra.reset(); }
        void control() override
        {
            auto g = dyn.gainDb (13.0f * a, a);
            shelf.setHighShelf (fs, nyq (12000.0), 0.4, g);
            ultra.setHighShelf (fs, nyq (24000.0), 0.5, 0.4f * g);
        }
        void process (float* f, int ch) noexcept override
        {
            dyn.push (f, ch);
            for (int c = 0; c < ch; ++c) f[c] = ultra.process (shelf.process (f[c], c), c);
        }
    };

    /** PULSAR - Pultec high boost (broad 12 kHz) with the high attenuator taming the very top, plus tube colour. */
    struct PulsarTop : Model
    {
        Dynamics dyn; Biquad bell, atten; TubeColour tube;
        void prepareModel() override { dyn.prepare (fs, F::highPass, 6000.0, 7.0f, 5.0f, 0.8f, 6.0f, 7.0f); tube.prepare (fs); }
        void reset() override { dyn.reset(); bell.reset(); atten.reset(); tube.reset(); }
        void control() override
        {
            auto g = dyn.gainDb (12.0f * a, a);
            bell.setPeak (fs, nyq (12000.0), 0.45, g);
            atten.setHighShelf (fs, nyq (20000.0), 0.7, -3.0 * a);
        }
        void process (float* f, int ch) noexcept override
        {
            dyn.push (f, ch);
            for (int c = 0; c < ch; ++c) f[c] = tube.process (atten.process (bell.process (f[c], c), c), c, 0.08f * a);
        }
    };

    /** COMET - Aural Exciter style: new upper harmonics made from the 2.5 kHz+ band (clean, level-independent),
        mixed in under the voice; sibilance-guarded so it sparkles instead of fizzing. */
    struct Comet : Model
    {
        Dynamics dyn; Biquad side, lift; Harmonics harm;
        float mixH = 0.0f;
        void prepareModel() override
        {
            dyn.prepare (fs, F::highPass, 6000.0, 7.0f, 5.0f, 0.8f, 6.0f, 7.0f);
            side.setHighPass (fs, 2000.0, 0.707);
            harm.prepare (fs, 5000.0);
        }
        void reset() override { dyn.reset(); side.reset(); lift.reset(); harm.reset(); }
        void control() override
        {
            auto full = 3.0f * a;
            auto g = dyn.gainDb (full, a);
            lift.setHighShelf (fs, nyq (8000.0), 0.7, g);
            mixH = 0.7f * a * juce::jmax (0.0f, 0.5f + 0.5f * clampFrac (g, full));
        }
        void process (float* f, int ch) noexcept override
        {
            dyn.push (f, ch);
            float b[2], h[2];
            for (int c = 0; c < ch; ++c) b[c] = side.process (f[c], c);
            harm.process (b, h, ch, 1.0f, 0.25f);
            for (int c = 0; c < ch; ++c) f[c] = lift.process (f[c], c) + mixH * h[c];
        }
    };
}

std::unique_ptr<Model> AirEngine::createModel (int section, int index)
{
    switch (section * 4 + index)
    {
        case 0:  return std::make_unique<Gravity>();
        case 1:  return std::make_unique<Core>();
        case 2:  return std::make_unique<Proxima>();
        case 3:  return std::make_unique<Titan>();
        case 4:  return std::make_unique<Lumen>();
        case 5:  return std::make_unique<Nova>();
        case 6:  return std::make_unique<Quasar>();
        case 7:  return std::make_unique<Photon>();
        case 8:  return std::make_unique<Stratos>();
        case 9:  return std::make_unique<Zenith>();
        case 10: return std::make_unique<PulsarTop>();
        default: return std::make_unique<Comet>();
    }
}

// ================================================================== section (4 models, crossfade on change)

class AirEngine::Section
{
public:
    explicit Section (int sectionIndex)
    {
        for (int i = 0; i < 4; ++i) models[(size_t) i] = createModel (sectionIndex, i);
    }

    void prepare (double fs)
    {
        for (auto& m : models) m->prepare (fs);
        amount.reset (fs, 0.05);
        fadeStep = (float) (1.0 / (0.03 * fs));    // 30 ms crossfade between models
        primed = false;
    }

    void reset()
    {
        for (auto& m : models) m->reset();
        primed = false;
    }

    /** In-place over `n` frames of `ch` channels. Returns the added energy ratio (for the visuals). */
    float process (float* const* data, int ch, int n, int model, float target)
    {
        model = juce::jlimit (0, 3, model);
        if (! primed)
        {
            current = model; previous = -1; fade = 1.0f;
            amount.setCurrentAndTargetValue (target);
            models[(size_t) current]->setAmount (taper (target));
            primed = true;
        }
        amount.setTargetValue (target);
        if (model != current)
        {
            previous = current;
            current = model;
            models[(size_t) current]->reset();
            models[(size_t) current]->setAmount (taper (amount.getCurrentValue()));
            fade = 0.0f;
        }

        double inSq = 0, diffSq = 0;
        for (int start = 0; start < n; start += controlEvery)
        {
            auto len = juce::jmin (controlEvery, n - start);
            auto av = taper (amount.isSmoothing() ? amount.skip (len) : amount.getCurrentValue());
            auto* cur = models[(size_t) current].get();
            auto* prev = previous >= 0 ? models[(size_t) previous].get() : nullptr;
            cur->setAmount (av);
            cur->control();
            if (prev != nullptr) { prev->setAmount (av); prev->control(); }

            for (int i = start; i < start + len; ++i)
            {
                float in[2], a[2], b[2];
                for (int c = 0; c < ch; ++c) in[c] = a[c] = b[c] = data[c][i];
                cur->process (a, ch);
                if (prev != nullptr)
                {
                    fade = juce::jmin (1.0f, fade + fadeStep);
                    prev->process (b, ch);
                    for (int c = 0; c < ch; ++c) a[c] = b[c] + fade * (a[c] - b[c]);
                }
                for (int c = 0; c < ch; ++c)
                {
                    inSq += (double) in[c] * in[c];
                    diffSq += (double) (a[c] - in[c]) * (a[c] - in[c]);
                    data[c][i] = a[c];
                }
            }
            if (prev != nullptr && fade >= 1.0f) previous = -1;
        }
        return inSq > 1.0e-9 ? (float) std::sqrt (diffSq / inSq) : 0.0f;
    }

private:
    /** Musical knob taper: the first half of the knob already does a lot, the top end goes big. */
    static float taper (float a) noexcept { return a <= 0.0f ? 0.0f : std::pow (a, 0.7f); }

    std::array<std::unique_ptr<Model>, 4> models;
    juce::SmoothedValue<float> amount;
    int current = 0, previous = -1;
    float fade = 1.0f, fadeStep = 0.001f;
    bool primed = false;
};

// ================================================================== engine

AirEngine::AirEngine()
{
    for (int s = 0; s < 3; ++s) sections[(size_t) s] = std::make_unique<Section> (s);
}

AirEngine::~AirEngine() = default;

void AirEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    sr = sampleRate;
    maxBlock = juce::jmax (16, maxBlockSize);
    channels = juce::jlimit (1, 2, numChannels);
    os = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    os->initProcessing ((size_t) maxBlock);
    osRate = sr * 2.0;
    latency = (int) std::round (os->getLatencyInSamples());
    dryOs.setSize (2, maxBlock * 2);
    for (auto& s : sections) s->prepare (osRate);
    for (auto* v : { &trimS, &outS }) v->reset (sr, 0.05);
    agS.reset (sr, 0.3);
    mixS.reset (osRate, 0.05);

    for (auto* k : { &kShelf, &kShelfOut }) k->setHighShelf (sr, 1681.97, 0.7072, 3.9998);
    for (auto* k : { &kHp, &kHpOut }) k->setHighPass (sr, 38.135, 0.5003);
    kBlockLen = juce::jmax (1, (int) (sr * 0.1));
    reset();
}

void AirEngine::reset()
{
    if (os) os->reset();
    for (auto& s : sections) s->reset();
    for (auto* k : { &kShelf, &kHp, &kShelfOut, &kHpOut }) k->reset();
    kSum = 0; kCount = 0; kIndex = 0;
    agIn = agOut = 0;
    kBlocks.fill (0.0);
    inLufs = -100.0f;
    autoGainDb = 0.0f;
    primed = false;
}

void AirEngine::process (juce::AudioBuffer<float>& buffer, const AirSettings& s)
{
    if (os == nullptr) return;
    auto n = buffer.getNumSamples();
    for (int start = 0; start < n; start += maxBlock)
        processChunk (buffer, start, juce::jmin (maxBlock, n - start), s);
}

void AirEngine::processChunk (juce::AudioBuffer<float>& buffer, int start, int n, const AirSettings& s)
{
    auto ch = juce::jmin (channels, buffer.getNumChannels());
    if (ch <= 0 || n <= 0) return;

    auto trimT = s.bypass ? 1.0f : juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 12.0f, s.trimDb));
    auto outT = s.bypass ? 1.0f : juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 12.0f, s.outputDb));
    auto mixT = s.bypass ? 0.0f : juce::jlimit (0.0f, 1.0f, s.mix);
    if (! primed)
    {
        trimS.setCurrentAndTargetValue (trimT);
        outS.setCurrentAndTargetValue (outT);
        mixS.setCurrentAndTargetValue (mixT);
        agS.setCurrentAndTargetValue (1.0f);
        primed = true;
    }
    trimS.setTargetValue (trimT);
    outS.setTargetValue (outT);
    mixS.setTargetValue (mixT);

    // input meter + loudness (before TRIM: what comes from the track)
    float pk = 0;
    double blockIn = 0;
    for (int i = 0; i < n; ++i)
    {
        double sq = 0;
        for (int c = 0; c < ch; ++c)
        {
            auto x = buffer.getSample (c, start + i);
            pk = juce::jmax (pk, std::abs (x));
            auto k = kHp.process (kShelf.process (x, c), c);
            sq += (double) k * k;
        }
        if (ch == 1) sq *= 2.0;   // mono counts as both speakers
        blockIn += sq;
        kSum += sq;
        if (++kCount >= kBlockLen)
        {
            kBlocks[(size_t) kIndex] = kSum / kCount;
            kIndex = (kIndex + 1) % (int) kBlocks.size();
            kSum = 0; kCount = 0;
            double mean = 0;
            for (auto b : kBlocks) mean += b;
            mean /= (double) kBlocks.size();
            inLufs = mean > 1.0e-10 ? (float) (-0.691 + 10.0 * std::log10 (mean)) : -100.0f;
        }
    }
    inPeak = juce::jmax (inPeak.load(), pk);

    // TRIM
    for (int i = 0; i < n; ++i)
    {
        auto g = trimS.getNextValue();
        for (int c = 0; c < ch; ++c) buffer.getWritePointer (c)[start + i] *= g;
    }

    // up -> sections -> mix with the (also oversampled) dry -> down
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) ch, (size_t) start, (size_t) n);
    auto up = os->processSamplesUp (block);
    auto nOs = (int) up.getNumSamples();
    for (int c = 0; c < ch; ++c) dryOs.copyFrom (c, 0, up.getChannelPointer ((size_t) c), nOs);

    float* ptrs[2] = { up.getChannelPointer (0), ch > 1 ? up.getChannelPointer (1) : nullptr };
    for (int sec = 0; sec < 3; ++sec)
    {
        auto act = sections[(size_t) sec]->process (ptrs, ch, nOs, s.model[sec], juce::jlimit (0.0f, 1.0f, s.amount[sec]));
        auto old = activity[(size_t) sec].load();
        activity[(size_t) sec] = old + 0.25f * (act - old);
    }

    for (int i = 0; i < nOs; ++i)
    {
        auto m = mixS.getNextValue();
        for (int c = 0; c < ch; ++c)
        {
            auto d = dryOs.getSample (c, i);
            ptrs[c][i] = d + m * (ptrs[c][i] - d);
        }
    }
    os->processSamplesDown (block);

    // AUTO GAIN: slow K-weighted loudness match of output to input (held during silence)
    double blockOut = 0;
    for (int i = 0; i < n; ++i)
    {
        double sq = 0;
        for (int c = 0; c < ch; ++c)
        {
            auto k = kHpOut.process (kShelfOut.process (buffer.getSample (c, start + i), c), c);
            sq += (double) k * k;
        }
        blockOut += ch == 1 ? sq * 2.0 : sq;
    }
    {
        auto k = 1.0 - std::exp (-(double) n / (sr * 1.5));
        if (blockIn / n > 1.0e-7)
        {
            agIn += k * (blockIn / n - agIn);
            agOut += k * (blockOut / n - agOut);
        }
        auto target = 1.0f;
        if (s.autoGain && ! s.bypass && agIn > 1.0e-9 && agOut > 1.0e-9)
            target = juce::jlimit (0.25f, 4.0f, (float) std::sqrt (agIn / agOut));
        agS.setTargetValue (target);
        autoGainDb = juce::Decibels::gainToDecibels (agS.getTargetValue());
    }

    // AUTO GAIN + OUTPUT + meter
    float opk = 0;
    for (int i = 0; i < n; ++i)
    {
        auto g = outS.getNextValue() * agS.getNextValue();
        for (int c = 0; c < ch; ++c)
        {
            auto& x = buffer.getWritePointer (c)[start + i];
            x *= g;
            if (! std::isfinite (x)) x = 0.0f;
            opk = juce::jmax (opk, std::abs (x));
        }
    }
    outPeak = juce::jmax (outPeak.load(), opk);
}

} // namespace air
