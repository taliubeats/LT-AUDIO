#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace air
{

/** The three sections and their four character models (names shown in the UI). */
struct Models
{
    enum Section { body = 0, mid, high, numSections };
    enum { perSection = 4 };

    static const juce::StringArray& sectionNames();               // BODY, MID AIR, HIGH AIR
    static const juce::StringArray& names (int section);          // GRAVITY, CORE, ...
    static juce::String caption (int section, int model);         // "Warm iron body" ...
};

struct AirSettings
{
    float amount[3] { 0.45f, 0.20f, 0.30f };   // 0..1 per section (knob 0..100)
    int model[3] { 1, 0, 1 };                   // CORE, LUMEN, ZENITH
    float trimDb = 0.0f;                        // -24..+12, before the processing (drives the colour stages)
    float mix = 1.0f;                           // 0..1
    float outputDb = 0.0f;                      // -24..+12
    bool autoGain = false;                      // match the output loudness to the input (for honest A/B)
    bool bypass = false;                        // smooth: everything goes to unity (still latency-aligned)
};

/** A small TDF-II biquad with RBJ cookbook designs; state for up to 2 channels. Real-time safe (no allocation). */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};

    inline float process (float x, int ch) noexcept
    {
        auto y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }

    void setPeak (double fs, double hz, double q, double db);
    void setLowShelf (double fs, double hz, double q, double db);
    void setHighShelf (double fs, double hz, double q, double db);
    void setHighPass (double fs, double hz, double q);
    void setLowPass (double fs, double hz, double q);
    void setBandPass (double fs, double hz, double q);   // 0 dB at the centre
    void setIdentity() noexcept { b0 = 1; b1 = b2 = a1 = a2 = 0; }

private:
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept;
};

/** One-pole low-pass / high-pass (for band splits and DC blocking). */
struct OnePole
{
    float a = 1.0f, z[2] {};
    void setCutoff (double fs, double hz) { a = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * hz / fs)); }
    inline float lp (float x, int ch) noexcept { z[ch] += a * (x - z[ch]); return z[ch]; }
    inline float hp (float x, int ch) noexcept { return x - lp (x, ch); }
    void reset() noexcept { z[0] = z[1] = 0.0f; }
};

/** Power envelope (attack / release in ms), fed with the mean square of the channels (stereo-linked). */
struct PowerEnv
{
    float att = 0, rel = 0, p = 0;
    void set (double fs, double attackMs, double releaseMs)
    {
        att = (float) std::exp (-1.0 / (fs * attackMs * 0.001));
        rel = (float) std::exp (-1.0 / (fs * releaseMs * 0.001));
    }
    inline void push (float power) noexcept { auto c = power > p ? att : rel; p = power + c * (p - power); }
    float db() const noexcept { return 10.0f * std::log10 (p + 1.0e-12f); }
    void reset() noexcept { p = 0.0f; }
};

/**
    The heart of every model: a stereo-linked, *program-relative* gain computer (Dolby-A "trick" style).
    It compares the short-term level of a detection band with that band's own long-term level in this voice:
      - below `threshold - width` (soft or ordinary passages) the model gives its full boost, between them it fades out
        (upward-compressor feel: soft detail is lifted most, peaks that stick out get nothing);
      - above the threshold (esses, harsh notes, plosives, boom) it pulls the band down (built-in de-esser / de-booster);
      - sharp transients in the band duck the boost for a moment;
      - nothing is lifted while the voice is silent (no noise pumping), and the long-term level is held meanwhile.
    Because it is relative to the voice itself, it works the same on a quiet or loud, dark or bright vocal.
*/
struct Dynamics
{
    enum class Filter { highPass, lowPass };
    void prepare (double fs, Filter, double hz, float thresholdDb, float widthDb, float guardSlope, float guardMaxDb, float transientDb);
    void reset();
    inline void push (const float* frame, int ch) noexcept
    {
        float pb = 0, pr = 0;
        for (int c = 0; c < ch; ++c)
        {
            auto d = det.process (frame[c], c);
            auto r = refHp.hp (frame[c], c);
            pb += d * d; pr += r * r;
        }
        auto inv = 1.0f / (float) ch;
        band.push (pb * inv); fast.push (pb * inv); ref.push (pr * inv);
    }
    /** Control rate: the gain (dB) to use now, for a model whose full boost is `boostDb` at this amount. */
    float gainDb (float boostDb, float amount) noexcept;
    float relativeDb() const noexcept { return band.db() - typicalDb; }

private:
    Biquad det;
    OnePole refHp;
    PowerEnv band, fast, ref;
    float typicalDb = -120.0f, kRise = 0.001f, kFall = 0.001f, kLearn = 0.01f;   // log-domain: the esses don't inflate it
    int voiced = 0, warmTicks = 1, learnTicks = 1;
    float threshold = -10, width = 10, slope = 0.5f, guardMax = 6, transient = 6;
    float smoothed = 0, kDown = 1, kUp = 1;
};

/** Clean harmonics (2nd + 3rd) of a band, normalised by its level so the colour is the same loud or soft. */
struct Harmonics
{
    void prepare (double fs, double postHighPassHz);
    void reset();
    /** frame -> band already filtered by the caller; returns harmonics in `out` (same scale as the band). */
    void process (const float* bandFrame, float* out, int ch, float even, float odd) noexcept;

private:
    PowerEnv level;
    Biquad post;
};

/** One character model. amount 0 must be an exact pass-through. Runs at the oversampled rate on stereo frames. */
struct Model
{
    virtual ~Model() = default;
    void prepare (double sampleRate) { fs = sampleRate; prepareModel(); reset(); setAmount (0.0f); control(); }
    virtual void reset() = 0;
    void setAmount (float amount) { a = amount; }
    /** Called every few samples: dynamics -> filter coefficients. */
    virtual void control() = 0;
    virtual void process (float* frame, int ch) noexcept = 0;

protected:
    virtual void prepareModel() {}
    double fs = 88200.0;
    float a = 0.0f;
    double nyq (double hz) const { return juce::jmin (hz, fs * 0.45); }
};

/**
    LT AIR: vocal enhancer. Input TRIM -> BODY -> MID AIR -> HIGH AIR -> MIX -> (AUTO GAIN) -> OUTPUT.
    The whole chain (and the dry signal for MIX) runs 2x oversampled with linear-phase filters, so the colour
    stages don't alias and dry/wet stay perfectly aligned. Latency = getLatencySamples() (constant).
*/
class AirEngine
{
public:
    AirEngine();
    ~AirEngine();

    void prepare (double sampleRate, int maxBlockSize, int numChannels = 2);
    void reset();
    void process (juce::AudioBuffer<float>&, const AirSettings&);

    int getLatencySamples() const noexcept           { return latency; }

    // UI (thread-safe)
    float takeInputPeak() noexcept                   { return inPeak.exchange (0.0f); }
    float takeOutputPeak() noexcept                  { return outPeak.exchange (0.0f); }
    /** Short-term loudness of the input (3 s, K-weighted), or -100 when silent. */
    float getInputLufs() const noexcept              { return inLufs.load(); }
    /** How much each section is adding right now (0 = nothing, ~1 = a lot). For the atmosphere glow. */
    float getSectionActivity (int s) const noexcept  { return activity[(size_t) s].load(); }
    float getAutoGainDb() const noexcept             { return autoGainDb.load(); }

    /** Model factory (also used by the tests). section, index -> new model. */
    static std::unique_ptr<Model> createModel (int section, int index);

private:
    class Section;
    std::array<std::unique_ptr<Section>, 3> sections;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::AudioBuffer<float> dryOs;
    juce::SmoothedValue<float> trimS, mixS, outS, agS;
    double sr = 44100.0, osRate = 88200.0;
    int maxBlock = 512, channels = 2, latency = 0;
    bool primed = false;

    // loudness (BS.1770 short-term) of the input + auto gain
    Biquad kShelf, kHp, kShelfOut, kHpOut;
    double kSum = 0.0, agIn = 0.0, agOut = 0.0;
    int kCount = 0, kBlockLen = 4410, kIndex = 0;
    std::array<double, 30> kBlocks {};

    std::atomic<float> inPeak { 0 }, outPeak { 0 }, inLufs { -100.0f }, autoGainDb { 0.0f };
    std::array<std::atomic<float>, 3> activity {};

    void processChunk (juce::AudioBuffer<float>&, int start, int n, const AirSettings&);
};

} // namespace air
