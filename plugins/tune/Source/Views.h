#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>

namespace tune
{

using lt::ui::Palette;

/** Draws a note name with a thin font and the "#" as a superscript. Returns the bounds used. */
juce::Rectangle<float> drawNoteName (juce::Graphics&, const juce::String& name, juce::Point<float> centre, float height,
                                     juce::Colour, float alpha = 1.0f);

/** Small glass knob: value inside, label below. */
class TuneKnob : public juce::Slider
{
public:
    explicit TuneKnob (const juce::String& label);
    const Palette* palette = nullptr;
    juce::String label;
    std::function<juce::String (double)> formatValue;
    bool bipolar = false;
    bool compact = false;   // knob only (no value, no label)

    juce::Rectangle<float> knobArea() const;
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
};

/**
    RETUNE SPEED: a dark planet with a ring. Left = NATURAL (slow), right = ROBOT (0 ms).
    Dragging up turns it clockwise (towards ROBOT). It flashes on every corrected note.
*/
class RetunePlanet : public juce::Component
{
public:
    explicit RetunePlanet (juce::RangedAudioParameter&);
    const Palette* palette = nullptr;
    void flash()                       { glow = 1.0f; }
    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    float norm = 0.0f, dragStart = 0.0f, glow = 0.0f, spin = 0.0f;
};

/** Giant note + cents meter (-100..+100). The note glows when in tune and trembles when it is off. */
class TunerView : public juce::Component
{
public:
    TunerView();
    const Palette* palette = nullptr;
    void update (bool voiced, int targetNote, float inputMidi);
    void paint (juce::Graphics&) override;

private:
    juce::String note { "A" };
    float cents = 0.0f, shownCents = 0.0f, presence = 0.0f, time = 0.0f;
    bool voiced = false;
};

/** EDIT NOTES: 12 glass orbs (7 naturals below, 5 sharps above). */
class NoteOrbs : public juce::Component
{
public:
    const Palette* palette = nullptr;
    std::function<void (int)> onToggle;
    void update (const std::array<bool, 12>& mask, int sungPitchClass);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** Orb centres/radius in this component's coordinates. */
    juce::Point<float> centreOf (int pitchClass) const;
    float radius() const;

private:
    int hitOrb (juce::Point<float>) const;
    std::array<bool, 12> mask {};
    std::array<float, 12> sparkle {}, lit {};
    int sung = -1, hover = -1;
    float time = 0.0f;
};

/** Horizontal tick meter (IN / OUT). */
class HMeter : public juce::Component
{
public:
    const Palette* palette = nullptr;
    void setLevel (float);
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f, peak = 0.0f;
};

/** Top bar selector: "VOICE   Tenor  v". */
class SelectorPill : public juce::Button
{
public:
    SelectorPill (juce::String captionText) : juce::Button (captionText), caption (std::move (captionText)) {}
    const Palette* palette = nullptr;
    juce::String caption, value;
    void setValue (const juce::String& v) { if (v != value) { value = v; repaint(); } }
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

/** Round glass icon button: undo, redo, settings, power, reset, close. */
class IconButton : public juce::Button
{
public:
    enum class Icon { undo, redo, settings, power, reset, close, prev, next };
    explicit IconButton (Icon i, bool roundShape = true) : juce::Button ({}), icon (i), round (roundShape) {}
    const Palette* palette = nullptr;
    bool active = false;   // power: lit when processing
    void paintButton (juce::Graphics&, bool hover, bool down) override;

    static void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float>, juce::Colour, float thickness);

private:
    Icon icon;
    bool round;
};

/** Small on/off switch (PRESERVE, MATCH REFERENCE rows). */
class GlassSwitch : public juce::Button
{
public:
    GlassSwitch() : juce::Button ({}) { setClickingTogglesState (true); }
    const Palette* palette = nullptr;
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

} // namespace tune
