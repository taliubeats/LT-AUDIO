#pragma once

#include <lt_shared/lt_shared.h>
#include "Curve.h"
#include "DuckEngine.h"

namespace pulsar
{

using lt::ui::Palette;

// ------------------------------------------------------------------ MIX knob

/** The big MIX knob: light behind it and particles that grow with the value and beat with the rhythm. */
class MixKnob : public juce::Slider
{
public:
    MixKnob();

    const Palette* palette = nullptr;

    /** Called ~30 times a second. `beat` = 1 at the start of every ducking cycle. */
    void tick (float beat, float level, float gain);

    juce::Rectangle<float> getKnobArea() const;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool hitTest (int x, int y) override;

private:
    void renderDisc();

    struct Particle { juce::Point<float> pos, vel; float life, size; bool warm; };
    std::vector<Particle> particles;
    juce::Random random;
    juce::Image disc;
    bool discIsLight = false;
    float pulse = 0, level = 0, gainNow = 1;
};

// ------------------------------------------------------------------ visualiser + curve editor

class CurveView : public juce::Component
{
public:
    CurveView (DuckEngine&);

    const Palette* palette = nullptr;
    bool showWave = true;

    std::function<const Curve&()> getCurve;
    std::function<void (const Curve&)> onCurveChanged;

    void tick();

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    enum class Mode { none, point, bend, shift };

    juce::Rectangle<float> plot() const;
    juce::Point<float> pointPosition (const Curve&, int index) const;
    float phaseForX (float x) const;
    int pointAt (juce::Point<float>) const;
    int segmentNear (juce::Point<float>) const;
    void showMenu (int pointIndex);
    void change (const Curve&);

    DuckEngine& engine;
    std::array<float, DuckEngine::displayBins> shownIn {}, shownOut {};
    Mode mode = Mode::none;
    int dragIndex = -1, hoverPoint = -1, hoverSegment = -1;
    float dragStartValue = 0;
    juce::Point<float> dragStart;
    Curve dragStartCurve;
};

// ------------------------------------------------------------------ rate tabs inside the visualiser

/** Small "1/16 1/8 1/4 1/2 1/1" tabs in the top-left corner of the visualiser (selected = underlined). */
class RateTabs : public juce::Component
{
public:
    RateTabs();
    const Palette* palette = nullptr;
    std::function<void (int rate)> onChange;
    void setRate (int);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> tabArea (int) const;
    int rateForTab (int) const noexcept;
    int rate = Rates::quarter;
};

// ------------------------------------------------------------------ shape buttons

class ShapeButton : public juce::Button
{
public:
    explicit ShapeButton (int index);
    const Palette* palette = nullptr;
    void paintButton (juce::Graphics&, bool hover, bool down) override;

private:
    int index;
};

/** Small helper: a path that follows a curve inside a rectangle. */
juce::Path makeCurvePath (const Curve&, juce::Rectangle<float>, int steps = 160);

} // namespace pulsar
