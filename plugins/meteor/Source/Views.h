#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>

namespace meteor
{

using lt::ui::Palette;

/** Glass knob with the label (or the value while touched) underneath. */
class MeteorKnob : public juce::Slider
{
public:
    MeteorKnob (const juce::String& label, float knobSize = 58.0f);
    const Palette* palette = nullptr;
    juce::String label;

    juce::Rectangle<float> knobArea() const;
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;

private:
    float size;
};

/** A row (or grid) of pills bound to a choice parameter. */
class ChoicePills : public juce::Component
{
public:
    ChoicePills (juce::RangedAudioParameter&, const juce::StringArray& labels, int columns = 0);
    void resized() override;

private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int columns;
};

/** A module's on/off title pill: "● REVERSE". */
class PowerPill : public juce::Button
{
public:
    explicit PowerPill (const juce::String& name) : juce::Button (name) { setClickingTogglesState (true); }
    const Palette* palette = nullptr;
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

struct MeteorState
{
    float level = 0.0f, gate = 1.0f;
    bool reverse = false, doubled = false, chop = false, glitch = false;
    int voices = 1, glitches = 0;
};

/**
    A meteor crossing deep space; its tail reacts to the audio. REVERSE flies it backwards,
    DOUBLE adds companion meteors, CHOP cuts the tail, GLITCH makes it jump. Visual only.
*/
class MeteorView : public juce::Component
{
public:
    MeteorView() { setInterceptsMouseClicks (false, false); }
    const Palette* palette = nullptr;
    void tick (const MeteorState&);
    void paint (juce::Graphics&) override;

private:
    struct Spark { juce::Point<float> pos, vel; float life, size; };
    MeteorState st;
    std::vector<Spark> sparks;
    juce::Random random;
    double time = 0;
    float x = 0.15f, dir = 1.0f, jumpFlash = 0, glow = 0;
    int lastGlitches = 0;
    std::vector<float> history;      // recent gate values: drawn as the cut tail
};

/** Draws a min/max waveform of a buffer (cached). */
class Waveform
{
public:
    void draw (juce::Graphics&, juce::Rectangle<float>, const juce::AudioBuffer<float>*, juce::Colour);

private:
    const juce::AudioBuffer<float>* cached = nullptr;
    int cachedSamples = 0, cachedWidth = 0;
    std::vector<std::pair<float, float>> peaks;
};

/** The source box of STROKES: drop zone + waveform. */
class SourceView : public juce::Component
{
public:
    const Palette* palette = nullptr;
    std::shared_ptr<const juce::AudioBuffer<float>> buffer;
    juce::String name;
    bool dragHover = false, capturing = false;
    double captureSeconds = 0;
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override  { if (onClick) onClick(); }

private:
    Waveform wave;
};

/** One stroke: name, waveform, PLAY and DRAG (drag = WAV into the DAW, click = save to the folder). */
class StrokeCard : public juce::Component
{
public:
    explicit StrokeCard (const juce::String& name);
    const Palette* palette = nullptr;
    std::shared_ptr<const juce::AudioBuffer<float>> buffer;
    bool playing = false, waiting = false;
    float progress = 0;
    double sampleRate = 48000.0;
    std::function<void()> onPlay;
    std::function<juce::File()> makeFile;
    std::function<void (const juce::File&)> onSaved;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::Rectangle<float> playArea() const;
    juce::Rectangle<float> dragArea() const;
    juce::String name;
    Waveform wave;
    bool dragging = false, pressedPlay = false;
};

} // namespace meteor
