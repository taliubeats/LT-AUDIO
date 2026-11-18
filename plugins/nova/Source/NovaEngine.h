#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace nova
{

enum class SatType { tape = 0, tube, console, transformer, transistor, diode, vinyl, exciter, count };

struct SatTypes
{
    static const juce::StringArray& names();
    static juce::String description (int type);
};

/** Everything the engine needs for one block (already in engine units). */
struct NovaSettings
{
    int type = (int) SatType::tube;
    float drive = 0.5f;        // 0..1
    float tone = 0.0f;         // -1 dark .. +1 bright
    float warmth = 0.0f;       // 0..1
    float air = 0.0f;          // 0..1
    float character = 0.5f;    // 0 = even (warm, asymmetric) .. 1 = odd (hard, symmetric)
    float inputDb = 0.0f, outputDb = 0.0f;
    float mix = 1.0f;          // 0..1
    bool autoGain = true;
};

/**
    LT NOVA saturation engine: pre-emphasis (warmth) -> 4x oversampled waveshaper (8 characters)
    -> post EQ (tone tilt + air) -> auto gain -> dry/wet. Latency is fixed and reported.
*/
class NovaEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    /** Processes `buffer` in place. `dry` (optional) is the latency-aligned dry signal used for MIX. */
    void process (juce::AudioBuffer<float>& buffer, const NovaSettings&);

    int getLatencySamples() const noexcept       { return latency; }
    float getDriveEnergy() const noexcept        { return energy.load(); }   // 0..1, for the star animation
    float getAutoGainDb() const noexcept         { return autoGainDb.load(); }

    /** The static curve (input -> output) for a sample value, used by tests and the UI. */
    static float shape (float x, SatType, float character, float sag = 0.0f) noexcept;

private:
    void updateFilters (const NovaSettings&);
    void updateEmphasis (SatType, float drive01);

    double sr = 44100.0;
    int channels = 2, latency = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    using IIR = juce::dsp::IIR::Filter<float>;
    std::vector<IIR> emphPre, emphPost, emphPre2, emphPost2, warmPre, warmPost, toneLow, toneHigh, airShelf, tapeLp, vinylLp, vinylHp, exciterHp;
    std::vector<float> dcX, dcY;                       // DC blocker state (asymmetric curves add DC)
    juce::AudioBuffer<float> dryBuffer, exciterBand;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 14 };

    juce::SmoothedValue<float> driveGain, outGain, mixSmooth, autoGain;
    float inPow = 0.0f, outPow = 0.0f, sagEnv = 0.0f;
    int emphType = -1; float emphDrive = -1.0f;
    juce::Random noise;
    NovaSettings last;
    bool filtersReady = false, firstBlock = true;

    std::atomic<float> energy { 0.0f }, autoGainDb { 0.0f };
};

} // namespace nova
