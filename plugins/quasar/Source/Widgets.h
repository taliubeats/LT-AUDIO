#pragma once

#include <lt_shared/lt_shared.h>
#include "PluginProcessor.h"

namespace quasar::ui
{

using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Palette;

/** QUASAR colours (dark only): violet glass on deep space. */
struct Colours
{
    static inline const juce::Colour accent     { 0xff7b2cff };
    static inline const juce::Colour arc        { 0xffa46bff };
    static inline const juce::Colour arcGlow    { 0xff8a4dff };
    static inline const juce::Colour text       { 0xffeae6f5 };
    static inline const juce::Colour dim        { 0x99d6cfee };
    static inline const juce::Colour faint      { 0x40d6cfee };
    static inline const juce::Colour panel      { 0x99080512 };
    static inline const juce::Colour edge       { 0x33c9b8ff };
    static inline const juce::Colour display    { 0xcc06030d };
};

void drawPanel (juce::Graphics&, juce::Rectangle<float>, float corner = 10.0f, bool highlight = false);
void drawDisplay (juce::Graphics&, juce::Rectangle<float>, float corner = 6.0f);
void drawTitle (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size = 17.0f, juce::Justification = juce::Justification::centredLeft);
void drawKnob (juce::Graphics&, juce::Rectangle<float>, float proportion, bool bipolar, float glow = 1.0f, bool enabled = true);

/** Rounded-rectangle buttons and dark menus. */
class LookAndFeel : public lt::ui::GlassLookAndFeel
{
public:
    LookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hover, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool hover, bool down) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override { return Fonts::regular (15.0f); }
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};

/** Keeps parameter attachments; clear() before the controls are destroyed. */
struct Bindings
{
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliders;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> buttons;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> others;
    void clear() { sliders.clear(); buttons.clear(); others.clear(); }
};

// ================================================================== knob

/**
    Glass knob with a glowing value arc. Modulation sources can be dropped on it; every route
    shows as a coloured ring, and a white dot shows where the value really is now.
    ALT + drag (or right-click) edits the depth of the newest route.
*/
class Knob : public juce::Slider, public juce::DragAndDropTarget
{
public:
    Knob (Processor&, const juce::String& paramId, const juce::String& label, float diameter = 42.0f);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::String getTextFromValue (double) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
    juce::Rectangle<float> knobArea() const;

    juce::String label;
    bool showLabel = true, bipolar = false, valueInside = false;
    float labelSize = 13.0f;
    juce::Colour labelColour = Colours::text;
    void bind (Bindings&);
    const juce::String paramId;
    int dest = 0;

private:
    void showModMenu();
    Processor& proc;
    juce::RangedAudioParameter* param = nullptr;
    float diameter;
    bool dropHover = false, editingMod = false;
    int editSlot = -1; float editStart = 0;
};

/** A label / tab that is also a drag source for a modulation source ("mod:<index>"). */
class ModSourceTab : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModSourceTab (const juce::String& text, int source);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void()> onClick, onDoubleClick;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }
    bool selected = false, underline = true;
    float fontSize = 15.0f;
    juce::String text;
    int source;
private:
    bool dragging = false;
};

/** Number box (OCT, SEM, FIN...): drag up / down, double-click = default, label under it. */
class ValueBox : public juce::Component
{
public:
    ValueBox (juce::RangedAudioParameter&, const juce::String& label, float stepValue = 1.0f);
    ~ValueBox() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    std::function<juce::String (float)> format;
    bool labelBelow = true;
private:
    juce::RangedAudioParameter& param;
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::String label; float value = 0, startValue = 0, stepValue;
};

/** "WAVETABLE / ANALOG / SAMPLE" style text tabs bound to a choice parameter (or free). */
class TextTabs : public juce::Component
{
public:
    TextTabs (juce::StringArray items, juce::RangedAudioParameter* param = nullptr);
    ~TextTabs() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setSelected (int i) { selected = i; repaint(); }
    int getSelected() const noexcept { return selected; }
    std::function<void (int)> onChange;
    bool separators = true, boxed = false;
    float fontSize = 13.0f;
    juce::StringArray items;
private:
    juce::Rectangle<float> itemArea (int) const;
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::ParameterAttachment> att;
    int selected = 0;
};

/** iOS-like glass switch bound to a bool parameter. */
class Switch : public juce::Component
{
public:
    explicit Switch (juce::RangedAudioParameter&);
    ~Switch() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    juce::RangedAudioParameter& param;
    std::unique_ptr<juce::ParameterAttachment> att;
    bool on = false;
};

/** Small dropdown ("LADDER 24 v", "MAIN v") bound to a choice / int parameter. */
class Dropdown : public juce::Component
{
public:
    Dropdown (juce::RangedAudioParameter&, juce::StringArray names);
    ~Dropdown() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void (juce::PopupMenu&)> buildMenu;   // optional custom menu (item id = value + 1)
    std::function<int (float)> toIndex;                 // optional mapping parameter value -> item
    std::function<float (int)> fromIndex;               // and back
    float fontSize = 13.0f;
    bool centred = true;
    juce::StringArray names;
    void sendUpdate() { if (att) att->sendInitialUpdate(); }
private:
    juce::RangedAudioParameter& param;
    std::unique_ptr<juce::ParameterAttachment> att;
    int value = 0;
};

/** Vertical glass fader (mixer and effect output). */
class Fader : public juce::Slider
{
public:
    Fader();
    void paint (juce::Graphics&) override;
};

/** Segmented level meter. */
class Meter : public juce::Component, private juce::Timer
{
public:
    explicit Meter (std::function<float (int ch)> level, int channels = 2, bool rainbow = false);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    std::function<float (int)> level;
    int channels; bool rainbow;
    std::array<float, 2> shown {}, peak {};
};

// ================================================================== displays

/** The oscillator display: 3D wavetable frames, the analog shape or the sample. Drop a .wav here. */
class WaveView : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    WaveView (Processor&, int osc);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override {}
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void mouseDown (const juce::MouseEvent&) override;
    int osc;   // 0..2, 3 = sub, 4 = noise
private:
    void timerCallback() override { repaint(); }
    Processor& proc;
    std::vector<float> scratch;
};

class FilterView : public juce::Component, private juce::Timer
{
public:
    FilterView (Processor&, int filter);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    Processor& proc; int filter;
};

/** ADSR with handles: drag the attack peak, the decay / sustain corner and the release end. */
class EnvView : public juce::Component, private juce::Timer
{
public:
    explicit EnvView (Processor&);
    void setEnv (int e) { env = e; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
private:
    void timerCallback() override { repaint(); }
    struct Geo { float total, att, hold, dec, rel; juce::Rectangle<float> area; };
    Geo geometry() const;
    juce::Point<float> pointFor (int handle, const Geo&) const;
    float get (const char* what) const;
    void set (const char* what, float realValue);
    Processor& proc; int env = 0, dragging = -1, hover = -1;
};

/** Editable LFO shape: drag points, double-click adds / removes, ALT + drag bends a segment. */
class LfoView : public juce::Component, private juce::Timer
{
public:
    explicit LfoView (Processor&);
    void setLfo (int l) { lfo = l; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (12.0f, 10.0f); }
    juce::Point<float> toScreen (float x, float y) const;
    Processor& proc; int lfo = 0, dragging = -1, bending = -1;
    LfoShape editing; float bendStart = 0;
};

/** Glass piano: click to play, shows the notes that sound. */
class Keyboard : public juce::Component, private juce::Timer
{
public:
    explicit Keyboard (Processor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int lowest = 24, octaves = 6;
private:
    void timerCallback() override { repaint(); }
    int noteAt (juce::Point<float>, float& velocity) const;
    juce::Rectangle<float> keyRect (int note) const;
    static bool isBlack (int n) { auto k = n % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }
    Processor& proc; int down = -1;
};

/** PITCH / MOD wheel. Pitch springs back to the centre. */
class Wheel : public juce::Component
{
public:
    Wheel (Processor&, bool pitch);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void send();
    Processor& proc; bool pitch; float value, startValue = 0;
};

} // namespace quasar::ui
