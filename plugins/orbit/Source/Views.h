#pragma once

#include <lt_shared/lt_shared.h>

namespace orbit
{

using lt::ui::Palette;

/** Big or small glass knob; the big ones show their value inside. */
class OrbitKnob : public juce::Slider
{
public:
    OrbitKnob (const juce::String& label, bool big);
    const Palette* palette = nullptr;
    juce::String label;
    bool labelAbove = false;
    std::function<juce::String (double)> formatValue;

    juce::Rectangle<float> knobArea() const;
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;

private:
    bool big;
};

struct OrbitState
{
    double delaySeconds = 0.5;
    float feedback = 0.45f, level = 0.0f, width = 1.0f;
    int mode = 1, hits = 0;
    bool frozen = false;
};

/**
    The planet is the original sound; every echo is a satellite on rings further and further out
    and dimmer. FEEDBACK = more rings, TIME = speed, PING-PONG = bounce sides, REVERSE = inward,
    FREEZE = endless orbits. Visual only.
*/
class OrbitView : public juce::Component
{
public:
    OrbitView();
    const Palette* palette = nullptr;
    void tick (const OrbitState&);
    void paint (juce::Graphics&) override;

private:
    struct Echo { double born; float angle; bool ambient; int ring; };
    juce::Point<float> onRing (float ringIndex, float angle) const;
    float ringRadius (float ringIndex) const;
    int ringCount() const;

    std::vector<Echo> echoes;
    OrbitState st;
    juce::Random random;
    double time = 0;
    int lastHits = 0;
    float pulse = 0;
};

/** Two thin bars (L, R). */
class StereoMeter : public juce::Component
{
public:
    const Palette* palette = nullptr;
    void setLevels (float l, float r);
    void paint (juce::Graphics&) override;

private:
    float lv[2] {}, pk[2] {};
};

class FreezePill : public juce::Button
{
public:
    FreezePill() : juce::Button ("FREEZE") { setClickingTogglesState (true); }
    const Palette* palette = nullptr;
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

} // namespace orbit
