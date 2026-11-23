#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>

namespace air
{

using lt::ui::Palette;

/** What the atmosphere display needs each frame. */
struct AtmosphereState
{
    float amount[3] {};          // knobs 0..1
    float activity[3] {};        // engine: how much each section adds right now
    float lufs = -100.0f;        // input short-term loudness
    bool bypassed = false;
    std::function<float (float hz)> level;   // spectrum level 0..1 at a frequency (UI thread)
};

/**
    The curved horizon of a dark planet seen from orbit with three glowing atmosphere layers
    (BODY deep violet, MID AIR lavender, HIGH AIR white-violet rim) that grow with their knobs,
    and the live vocal spectrum drawn as purple stardust.
*/
class AtmosphereView : public juce::Component
{
public:
    AtmosphereView();
    void tick (const AtmosphereState&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Geometry { juce::Point<float> centre; float rim, surface; };
    Geometry geometry() const;
    float surfaceY (float x) const;
    void renderPlanet();

    static constexpr int points = 96;
    std::array<float, points> spectrum {};
    float shown[3] {}, glow[3] {};
    float lufs = -100.0f, time = 0.0f;
    bool bypassed = false;
    juce::Image planet;
};

/** Big knob: a dark planet inside a thin track with a glowing purple arc, the value in the middle, 0 and 100 below. */
class PlanetKnob : public juce::Slider
{
public:
    explicit PlanetKnob (int seed);
    void paint (juce::Graphics&) override;
    juce::Point<float> centre() const;
    float radius() const;

private:
    int seed;
    juce::Image texture;
    void renderTexture (float r);
};

/** The 4 model pills of a module (one is lit). */
class ModelPills : public juce::Component
{
public:
    explicit ModelPills (const juce::StringArray& names);
    std::function<void (int)> onSelect;
    void setSelected (int i)          { if (i != selected) { selected = i; repaint(); } }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::StringArray names;
    std::vector<juce::Rectangle<float>> boxes;
    int selected = 0, hover = -1;
    int hit (juce::Point<float>) const;
};

/** Small round knob of the bottom bar (TRIM / MIX / OUTPUT). `inner` draws the label inside (MIX). */
class SmallKnob : public juce::Slider
{
public:
    explicit SmallKnob (juce::String innerLabel = {}) : inner (std::move (innerLabel))
    {
        setSliderStyle (juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setMouseDragSensitivity (200);
    }
    bool bipolar = false;
    void paint (juce::Graphics&) override;

private:
    juce::String inner;
};

/** IN / OUT horizontal bar meter with peak hold and a clip dot. */
class BarMeter : public juce::Component
{
public:
    void setLevel (float peak, float dt);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { clip = 0.0f; repaint(); }

private:
    float level = 0.0f, hold = 0.0f, holdTime = 0.0f, clip = 0.0f;
};

/** Plain line icon button (undo, redo, settings, power, prev, next). */
class IconButton : public juce::Button
{
public:
    enum class Icon { undo, redo, settings, power, prev, next };
    explicit IconButton (Icon i, bool roundShape = false) : juce::Button ({}), icon (i), round (roundShape) {}
    bool lit = false;
    void paintButton (juce::Graphics&, bool hover, bool down) override;
    static void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float>, juce::Colour, float thickness);

private:
    Icon icon;
    bool round;
};

/** "A | B": the lit letter is the setting you hear. Click a letter to switch. */
class ABButton : public juce::Component
{
public:
    std::function<void (int)> onSelect;
    void setSlot (int s)              { if (s != slot) { slot = s; repaint(); } }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    int slot = 0;
};

/** Small glass pill with text (SAVE, LINK). Toggle buttons light up purple when on. */
class PillButton : public juce::Button
{
public:
    explicit PillButton (const juce::String& text) : juce::Button (text) {}
    float fontSize = 11.5f;
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

/** "< Silky Pop Vocal >": the preset name (click = menu). */
class PresetName : public juce::Button
{
public:
    PresetName() : juce::Button ({}) {}
    juce::String text;
    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

/** Shared drawing: the logotype "LT AIR" with its stream of stellar wind (also used by the icon). */
void drawLogo (juce::Graphics&, juce::Rectangle<float> area);

} // namespace air
