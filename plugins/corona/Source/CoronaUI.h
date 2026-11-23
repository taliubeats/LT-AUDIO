#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace corona::ui
{

// Everything is laid out in the 1376 x 768 frame of the approved design (design/corona/main-dark.webp).
inline constexpr int W = 1376, H = 768, FACE_Y = 405;

/** Images built once from the approved picture: the photo with the painted needle removed (hot tubes),
    the same photo with the red tube light removed (cold tubes), and the faceplate drawn in code. */
struct Art
{
    juce::Image hot, cold, face;
    static const Art& get();
private:
    Art();
};

/** One knob: geometry and the angle of every printed position. */
struct KnobSpec
{
    const char* id; int x, y; float r;
    float a0, a1;                    // continuous: angle at min / max (degrees, 0 = up)
    std::vector<float> steps;        // stepped: one angle per position
    bool big;
};
const std::array<KnobSpec, 8>& knobSpecs();
const KnobSpec& spec (const juce::String& id);

/** Black bakelite knob drawn in code (lobed when big, knurled when small), worn and scratched. */
class Knob : public juce::Slider
{
public:
    explicit Knob (const KnobSpec&);
    void paint (juce::Graphics&) override;
    const KnobSpec& s;
private:
    juce::Image wear;
    std::vector<std::array<float, 3>> chips;
};

/** Chrome bat-handle toggle. Vertical (AUTO GAIN: up = OUT) or tilting (POWER: left = OFF). */
class Lever : public juce::Button
{
public:
    Lever (bool tilting, bool withPlate);
    void paintButton (juce::Graphics&, bool, bool) override;
private:
    bool tilting, plate;
};

/** The red pilot lamp; clicking it also switches POWER. */
class Jewel : public juce::Button
{
public:
    Jewel();
    void paintButton (juce::Graphics&, bool, bool) override;
};

/** Upper half: tubes that heat with the gain reduction, and the GR meter. */
class Chassis : public juce::Component
{
public:
    Chassis();
    void update (float grDb, bool power, double dt);
    void paint (juce::Graphics&) override;
    float heatPercent() const noexcept { return heat * 100.0f; }
private:
    float needle = 0, heat = 0.2f, glow = 0.35f, lamp = 1, t = 0;
    juce::Random rng;
};

juce::Font labelFont (float size);

} // namespace corona::ui
