#pragma once

#include "Analysis.h"
#include "Skins.h"

namespace cosmos
{

/** Loudness readings copied from the audio thread once per frame. */
struct Readings
{
    float momentary = -200, shortTerm = -200, integrated = -200, range = 0, maxMomentary = -200, maxShortTerm = -200;
    std::array<float, 2> truePeakMax { -200, -200 }, truePeakNow { -200, -200 };
};

struct VisualContext
{
    const Analyzer& a;
    const Readings& loud;
    const Skin& skin;
    float target;          // LUFS target, or 0 for "off"
    double time;           // seconds, for animation
};

enum class VisualType
{
    spectrum, spectrogram, waveform, loudness, levels, vu, vector, scope, tuner,
    starmap, rosette, ridges, ripples, nucleus, attractor,
    plate, barClock, steps, lowEnd, width, fifths,
    count
};

/** One visual. Some keep their own state (images, particles), so every panel owns its instance. */
class Visual
{
public:
    virtual ~Visual() = default;
    virtual void paint (juce::Graphics&, juce::Rectangle<float>, const VisualContext&) = 0;
    /** Double-click inside the panel (loudness / levels use it to reset). Returns true if handled. */
    virtual bool doubleClicked() { return false; }

    static std::unique_ptr<Visual> create (VisualType);
    static juce::String nameOf (VisualType);
    static juce::String groupOf (VisualType);      // METER / SPECTRAL / GENERATIVE / MUSICAL
    static float preferredWeight (VisualType);     // relative width in a strip

    std::function<void()> onResetLoudness;
};

/** Shared drawing helpers. */
namespace draw
{
    void label (juce::Graphics&, const juce::String&, float x, float y, juce::Colour, float size = 9.5f,
                juce::Justification = juce::Justification::topLeft, float width = 200.0f);
    float freqToX (float hz, juce::Rectangle<float> r, float lo = 20.0f, float hi = 20000.0f);
    juce::String hzText (float hz);
    juce::String lufsText (float v);
}

} // namespace cosmos
