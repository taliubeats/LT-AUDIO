#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include "Session.h"

namespace halo
{

using lt::ui::Palette;

juce::Colour glowColour (const Palette&);

/** Round / bare icon button. */
class IconButton : public juce::Button
{
public:
    enum class Icon { undo, redo, settings, power, prev, next, play, stop, regen, drag, close, mic };
    explicit IconButton (Icon i, bool roundShape = true) : juce::Button ({}), icon (i), round (roundShape) {}
    const Palette* palette = nullptr;
    bool active = false;
    void paintButton (juce::Graphics&, bool hover, bool down) override;
    static void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float>, juce::Colour, float thickness);

private:
    Icon icon;
    bool round;
};

/** Planet knob with a glowing ring (DRY, WET, WIDTH, TIGHTNESS, choir knobs). */
class PlanetKnob : public juce::Slider
{
public:
    explicit PlanetKnob (juce::String labelText, int seed = 0);
    const Palette* palette = nullptr;
    juce::String label;
    std::function<juce::String (double)> formatValue;
    void paint (juce::Graphics&) override;

private:
    int seed;
};

/** Tiny glass knob with label below (strip LEVEL / PAN / DELAY / HUMANIZE). */
class SmallKnob : public juce::Slider
{
public:
    explicit SmallKnob (juce::String labelText);
    const Palette* palette = nullptr;
    juce::String label;
    bool bipolar = false;
    std::function<juce::String (double)> formatValue;
    void paint (juce::Graphics&) override;
};

/** Two-state segmented switch: "FAST | BEST", "SOLO HARMONIES | WITH LEAD", "HARMONY | CHOIR". */
class SegmentSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    SegmentSwitch (juce::String a, juce::String b) : left (std::move (a)), right (std::move (b)) {}
    const Palette* palette = nullptr;
    int selected = 0;
    bool pillStyle = true;        // false: just text with the active side lit
    std::function<void (int)> onChange;
    std::function<bool (int)> isEnabledSide;
    void setSelected (int s) { if (s != selected) { selected = s; repaint(); } }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::String left, right;
};

/** Small toggle (SMART / FIXED, MIDI-like switches). */
class GlassSwitch : public juce::Button
{
public:
    GlassSwitch() : juce::Button ({}) { setClickingTogglesState (true); }
    const Palette* palette = nullptr;
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

/** Horizontal tick meter. */
class HMeter : public juce::Component
{
public:
    const Palette* palette = nullptr;
    void setLevel (float);
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f, peak = 0.0f;
};

/** Caption + value box with an optional inner button ("KEY & SCALE  A Minor  [DETECT]"). */
class ValueBox : public juce::Button
{
public:
    explicit ValueBox (juce::String captionText) : juce::Button (captionText), caption (std::move (captionText)) {}
    const Palette* palette = nullptr;
    juce::String caption, value;
    void setValue (const juce::String& v) { if (v != value) { value = v; repaint(); } }
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

/** Glowing GENERATE pill with progress. */
class GenerateButton : public juce::Button
{
public:
    GenerateButton() : juce::Button ("GENERATE") {}
    const Palette* palette = nullptr;
    bool busy = false, attention = false;
    float progress = 0.0f, time = 0.0f;
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

/** "DRAG" handle: dragging it out of the plugin drops a WAV into the DAW. */
class DragHandle : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DragHandle (juce::String text = "DRAG") : label (std::move (text)) {}
    const Palette* palette = nullptr;
    juce::String label;
    bool compact = false;
    std::function<juce::File()> makeFile;      // writes the WAV, returns it (or empty)
    std::function<void()> onClickNoDrag;       // click = export to the default folder
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

private:
    bool dragging = false;
};

/**
    Stardust waveform of the vocal with the melody curve, note dots, playhead and (when available) the lyrics:
    words marked for refuerzo glow; clicking a word toggles it. Click elsewhere = move the playhead.
*/
class WaveView : public juce::Component
{
public:
    const Palette* palette = nullptr;
    bool vertical = false;                     // CHOIR tab: tall layout
    std::function<void (int)> onWordClicked;
    std::function<void (double)> onSeek;
    void setData (std::shared_ptr<const Analysis>, std::vector<Word>, std::vector<Region> refuerzo);
    void setPlayhead (double seconds, bool playing);
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override { image = {}; }
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverWord = -1; repaint(); }

private:
    juce::Rectangle<float> waveArea() const;
    juce::Rectangle<float> wordsArea() const;
    int wordAt (juce::Point<float>) const;
    void renderImage();

    std::shared_ptr<const Analysis> an;
    std::vector<Word> words;
    std::vector<Region> regions;
    std::vector<float> env;     // envelope columns
    juce::Image image;
    double playhead = 0.0;
    bool playing = false;
    int hoverWord = -1;
    float time = 0.0f;
};

/** Planet (lead) with 8 moons (voices). Click a moon = listen to that voice alone. */
class OrbitView : public juce::Component
{
public:
    struct Moon { bool on = false, valid = false, dirty = false, muted = false; juce::String label; };
    const Palette* palette = nullptr;
    std::array<Moon, numStrips> moons;
    juce::String centreNote { "A" };
    int soloMoon = -1;
    std::function<void (int)> onMoonClicked;
    void tick (float energy);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; }

private:
    juce::Point<float> moonCentre (int) const;
    int hitMoon (juce::Point<float>) const;
    float time = 0.0f, energy = 0.0f;
    int hover = -1;
};

/** One of the 8 voice strips. */
class StripView : public juce::Component
{
public:
    StripView();
    const Palette* palette = nullptr;
    int index = 0;
    juce::ComboBox part, style, voice;
    GlassSwitch smart;
    SmallKnob level { "LEVEL" }, pan { "PAN" }, delay { "DELAY" }, humanize { "HUMANIZE" };
    juce::TextButton mute { "M" }, solo { "S" };
    IconButton regen { IconButton::Icon::regen, false }, power { IconButton::Icon::power, false };
    DragHandle drag;
    bool enabledStrip = true, dirty = false, valid = false;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
};

/** CHOIR: a star cluster, one star per singer. */
class ChoirCluster : public juce::Component
{
public:
    const Palette* palette = nullptr;
    void setVoices (int v)        { voices = v; }
    void tick (float energy);
    void paint (juce::Graphics&) override;

private:
    int voices = 32;
    float shown = 32.0f, time = 0.0f, energy = 0.0f;
};

/** CHOIR TYPE cards: DUO, CHURCH, GOSPEL, STADIUM, ANGELS. */
class ChoirTypeCards : public juce::Component
{
public:
    const Palette* palette = nullptr;
    int selected = 3;
    std::function<void (int)> onSelect;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

private:
    juce::Rectangle<float> card (int) const;
    int hover = -1;
};

/** Vertical glass slider with % above and name below (SECTIONS). */
class SectionSlider : public juce::Slider
{
public:
    explicit SectionSlider (juce::String n);
    const Palette* palette = nullptr;
    juce::String name;
    void paint (juce::Graphics&) override;
};

} // namespace halo
