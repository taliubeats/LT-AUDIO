#pragma once

#include <lt_shared/lt_shared.h>

namespace vocal
{

using lt::ui::Palette;

class SuiteKnob : public juce::Slider
{
public:
    SuiteKnob (const juce::String& label, float diameter);
    const Palette* palette = nullptr;
    juce::String label;
    juce::Rectangle<float> knobArea() const;
    float getDiameter() const { return diameter; }
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;

private:
    float diameter;
};

/** The floating astronaut. Bobs gently; while the assistant works it glows and shows "thinking" dots. */
class Mascot : public juce::Component, public juce::SettableTooltipClient
{
public:
    Mascot();
    const Palette* palette = nullptr;
    bool thinking = false;
    std::function<void()> onClick;
    void tick();
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    juce::Image image, lightImage;
    double time = 0;
};

/** Tab pill with a line icon (TUNE, CLEANUP, TONE, DYNAMICS, SPACE, FX). */
class ModuleTab : public juce::Button
{
public:
    ModuleTab (const juce::String& name, int icon) : juce::Button (name), icon (icon) {}
    const Palette* palette = nullptr;
    void paintButton (juce::Graphics&, bool hover, bool down) override;

private:
    int icon;
};

/** Big note + cents meter for the TUNE module. */
class TuneDisplay : public juce::Component
{
public:
    const Palette* palette = nullptr;
    void set (bool voiced, float inputMidi, int target);
    void paint (juce::Graphics&) override;

private:
    bool voiced = false;
    float midi = 0, shownCents = 0;
    int target = -1;
};

/** Glass pill that opens a popup of choices. */
class ChoicePill : public juce::Button
{
public:
    ChoicePill() : juce::Button ("choice") {}
    const Palette* palette = nullptr;
    juce::String text;
    bool large = false;
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

/** Small on/off switch with a label. */
class ModuleSwitch : public juce::Button
{
public:
    explicit ModuleSwitch (const juce::String& label) : juce::Button (label) { setClickingTogglesState (true); }
    const Palette* palette = nullptr;
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

/** Vertical meter made of small dashes, like the design's INPUT / OUTPUT strips. */
class DashMeter : public juce::Component
{
public:
    const Palette* palette = nullptr;
    juce::String label;
    void setLevel (float);
    void paint (juce::Graphics&) override;

private:
    float level = 0, peak = 0;
};

/** Live curve of the TONE EQ (body, mud, presence, air). */
class EqCurve : public juce::Component
{
public:
    const Palette* palette = nullptr;
    float lowCut = 90, body = 0, mud = 2, presence = 2, air = 3;
    void paint (juce::Graphics&) override;
};

/** Horizontal readout bar (gain reduction, de-essing). */
class ReductionBar : public juce::Component
{
public:
    const Palette* palette = nullptr;
    juce::String label;
    float maxDb = 12.0f;
    void setValue (float db);
    void paint (juce::Graphics&) override;

private:
    float value = 0;
};

} // namespace vocal
