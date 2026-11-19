#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace quasar
{

/**
    A sample for the SAMPLE oscillator mode, stored as mono with 5 octave-down copies
    (half-band filtered) so it can be pitched up without aliasing.
*/
class SampleData
{
public:
    static constexpr int numLevels = 5;
    static constexpr double maxSeconds = 30.0;

    static std::unique_ptr<SampleData> load (const juce::File&, juce::String& error);
    static std::unique_ptr<SampleData> fromBuffer (const juce::AudioBuffer<float>&, double sampleRate, const juce::String& name, int rootNote);

    /** Guesses the root note from names like "Pluck_C.wav", "bass_F#2.wav". Returns 60 (C4) if unknown. */
    static int rootFromName (const juce::String&);

    const juce::String& getName() const noexcept { return name; }
    juce::File file;
    int rootNote = 60;
    double sourceRate = 44100.0;
    int length() const noexcept { return (int) levels[0].size(); }

    /** Reads at a position measured in level-0 samples, with `ratio` = playback speed (chooses the level). */
    inline float read (double pos, float ratio) const noexcept
    {
        int L = 0;
        while (ratio > 1.5f && L < numLevels - 1) { ratio *= 0.5f; ++L; }
        auto& d = levels[(size_t) L];
        auto p = pos / (double) (1 << L);
        auto i = (int) p;
        if (i < 0 || i + 2 >= (int) d.size()) return 0.0f;
        auto f = (float) (p - i);
        // 4-point Hermite
        auto xm1 = d[(size_t) juce::jmax (0, i - 1)], x0 = d[(size_t) i], x1 = d[(size_t) i + 1], x2 = d[(size_t) i + 2];
        auto c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    /** Peak envelope for drawing (points between 0 and 1). */
    std::vector<float> overview (int points) const;

private:
    juce::String name;
    std::array<std::vector<float>, numLevels> levels;
};

} // namespace quasar
