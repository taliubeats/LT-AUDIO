#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace quasar
{

/**
    A band-limited wavetable: up to 64 frames, each stored as 11 mip levels
    (1024 harmonics down to 1). Level L holds 1024 >> L harmonics in 8x that many samples
    (minimum 256), so linear interpolation stays clean. Read-only once built.
*/
class Wavetable
{
public:
    static constexpr int numLevels = 11;
    static constexpr int maxHarmonics = 1024;
    static constexpr int baseSize = 2048;      // size of one source frame (Serum convention)

    struct Spectrum { std::vector<float> re, im; };   // harmonics 0..1023 (index 0 = DC, ignored)

    /** Builds from spectra (one per frame). Each frame is normalised to peak 1. */
    static std::unique_ptr<Wavetable> fromSpectra (const juce::String& name, const std::vector<Spectrum>&);
    /** Builds from single-cycle frames of `frameSize` samples (a user .wav). */
    static std::unique_ptr<Wavetable> fromAudio (const juce::String& name, const float* data, int numSamples, int frameSize);

    int getNumFrames() const noexcept { return numFrames; }
    const juce::String& getName() const noexcept { return name; }

    /** Mip level choice for a phase increment (cycles per sample): level and crossfade weight towards level+1. */
    static inline void levelFor (float inc, int& level, float& w) noexcept
    {
        auto lf = std::log2 (juce::jmax (1.0e-9f, inc * 2048.0f));      // level that is exactly legal
        if (lf <= 0.0f) { level = 0; w = 0.0f; return; }
        auto l = (int) std::ceil (lf);
        w = 1.0f - ((float) l - lf);                                    // 0 right after a level became legal
        level = juce::jmin (numLevels - 1, l);
        if (level == numLevels - 1) w = 0.0f;
    }

    /** One sample: frame position 0..1, phase 0..1. */
    inline float read (float position, float phase, int level, float w) const noexcept
    {
        auto f = juce::jlimit (0.0f, (float) (numFrames - 1), position * (float) (numFrames - 1));
        auto f0 = (int) f;
        auto ff = f - (float) f0;
        auto f1 = juce::jmin (f0 + 1, numFrames - 1);
        auto v = readLevel (level, f0, f1, ff, phase);
        if (w > 0.001f && level + 1 < numLevels)
            v += w * (readLevel (level + 1, f0, f1, ff, phase) - v);
        return v;
    }

    /** Peak of the full-band frame (for drawing). */
    void drawFrame (float position, std::vector<float>& out, int points) const;

private:
    struct Level { int size = 0, mask = 0; std::vector<float> data; };

    inline float readLevel (int L, int f0, int f1, float ff, float phase) const noexcept
    {
        auto& lv = levels[(size_t) L];
        auto x = phase * (float) lv.size;
        auto i0 = (int) x;
        auto fr = x - (float) i0;
        i0 &= lv.mask;
        auto i1 = (i0 + 1) & lv.mask;
        auto* a = lv.data.data() + (size_t) f0 * (size_t) lv.size;
        auto va = a[i0] + fr * (a[i1] - a[i0]);
        if (f1 == f0 || ff < 1.0e-4f) return va;
        auto* b = lv.data.data() + (size_t) f1 * (size_t) lv.size;
        auto vb = b[i0] + fr * (b[i1] - b[i0]);
        return va + ff * (vb - va);
    }

    juce::String name;
    int numFrames = 1;
    std::array<Level, numLevels> levels;
};

/**
    The factory wavetables, generated on demand and shared by every instance of the plugin.
    get() builds synchronously (never call it from the audio thread); tryGet() is lock-free.
*/
class WavetableLibrary
{
public:
    WavetableLibrary();
    ~WavetableLibrary();

    static const juce::StringArray& names();   // factory table names (the "table" choice), last = "USER"
    static int analogIndex() { return names().size(); }   // hidden table used by ANALOG mode
    static int userIndex()   { return names().size() - 1; }

    const Wavetable* get (int index);
    const Wavetable* tryGet (int index) const noexcept;
    void request (int index);                    // build in the background (audio-thread safe)

private:
    class Builder;
    std::unique_ptr<Builder> builder;
    juce::CriticalSection lock;
    std::vector<std::unique_ptr<Wavetable>> owned;
    std::array<std::atomic<const Wavetable*>, 64> tables {};
    std::array<std::atomic<bool>, 64> requested {};
};

} // namespace quasar
