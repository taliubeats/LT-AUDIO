#pragma once

#include <lt_shared/lt_shared.h>

namespace lt_void
{

using lt::ui::Palette;

/** What the black hole reacts to (filled by the editor every frame). */
struct HoleState
{
    float size = 0.9f, decay = 8.0f, modRate = 0.5f, modDepth = 0.5f, brightness = 0.5f;
    float level = 0.0f;
    bool frozen = false;
    int hits = 0;
};

/**
    The centre of LT VOID: a nebula spiralling into a black hole. The arc around it is the DECAY knob
    (drag up / down anywhere on it). Purely visual - the audio is never affected by the animation.
*/
class BlackHoleView : public juce::Slider
{
public:
    BlackHoleView();

    const Palette* palette = nullptr;
    void tick (const HoleState&);

    juce::Rectangle<float> getRingArea() const;
    void paint (juce::Graphics&) override;
    void resized() override {}
    bool hitTest (int x, int y) override;

private:
    static void buildMaps();

    struct Grain { float angle, radius, speed, size, life; };
    std::vector<Grain> grains;
    juce::Random random;
    HoleState st;
    double time = 0;
    float rotA = 0, rotB = 0, rotC = 0, flash = 0, freezeGlow = 0;
    int lastHits = 0;
};

/** Glass knob. Big = value inside + label under (MIX / PRE-DELAY / SIZE); small = label under, value on hover. */
class VoidKnob : public juce::Slider
{
public:
    VoidKnob (const juce::String& label, bool big);
    const Palette* palette = nullptr;
    juce::String label;
    std::function<juce::String (double)> formatValue;

    void paint (juce::Graphics&) override;
    juce::Rectangle<float> knobArea() const;
    bool hitTest (int x, int y) override;

private:
    bool big;
};

/** Thin vertical level meter with a falling peak marker. */
class LevelMeter : public juce::Component
{
public:
    const Palette* palette = nullptr;
    void setLevel (float linear);
    void paint (juce::Graphics&) override;

private:
    float level = 0, peak = 0;
    int peakHold = 0;
};

/** The FREEZE button with the infinity sign. */
class FreezeButton : public juce::Button
{
public:
    FreezeButton() : juce::Button ("FREEZE") { setClickingTogglesState (true); }
    const Palette* palette = nullptr;
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

juce::Path infinityPath (juce::Rectangle<float>);

} // namespace lt_void
