#pragma once

#include "PluginProcessor.h"
#include "Look.h"

namespace eclipse
{

namespace ui
{
constexpr int W = 1376, H = 1000;
const juce::Rectangle<float> sourceArea { 18, 74, 1340, 232 };
const juce::Rectangle<float> padArea    { 18, 318, 700, 542 };
const juce::Rectangle<float> editArea   { 732, 318, 626, 542 };
const juce::Rectangle<float> seqArea    { 18, 872, 1340, 110 };
}

/** A hardware key. Draws itself with the skin. */
class KeyButton : public juce::Button
{
public:
    KeyButton (Processor& p, const juce::String& text = {}, bool led = false) : juce::Button (text), proc (p), led (led) {}
    float fontSize = 11.5f;
    juce::Colour onColour;
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> icon;   // instead of text
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    Processor& proc;
    bool led;
};

/** Rotary knob with value and label under it. */
class Knob : public juce::Slider
{
public:
    Knob (Processor& p, juce::String name, bool big = false);
    void paint (juce::Graphics&) override;
    juce::String caption;      // big knob: line under the label
private:
    Processor& proc;
    juce::String name;
    bool big;
};

/** Horizontal fader (SENSITIVITY, OUT, Subtle-Bold). */
class Fader : public juce::Slider
{
public:
    explicit Fader (Processor& p) : proc (p) { setSliderStyle (LinearHorizontal); setTextBoxStyle (NoTextBox, false, 0, 0); }
    void paint (juce::Graphics& g) override { look::fader (g, proc.skin(), getLocalBounds().toFloat().reduced (7, 0), (float) valueToProportionOfLength (getValue())); }
private:
    Processor& proc;
};

// ==================================================================== SOURCE
class SourceView : public juce::Component,
                   public juce::FileDragAndDropTarget
{
public:
    explicit SourceView (Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;
    void kitChanged();
    void zoom (float factor, float aroundX01);
    void jumpSection (int dir);
    void browse();

    juce::Rectangle<float> wave;   // the LCD area (local coords)
    KeyButton aiKey, recloneKey, browseKey, prevKey, nextKey, outKey, inKey;
    Fader sensitivity;
private:
    Processor& proc;
    std::unique_ptr<juce::SliderParameterAttachment> sensAttach;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<float> peaks;      // min,max per 256 samples
    const void* peaksOf = nullptr;
    double viewStart = 0, viewLen = 1;   // in samples
    float dragX = 0; double dragStart = 0;
    bool dragOver = false;
    void buildPeaks();
};

// ==================================================================== PADS
class PadGrid;

class PadComp : public juce::Component,
                public juce::DragAndDropTarget,
                public juce::FileDragAndDropTarget
{
public:
    PadComp (Processor&, PadGrid&);
    int index = 0;
    float lit = 0;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { hover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { hover = false; repaint(); }
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { hover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { hover = false; repaint(); }
private:
    Processor& proc;
    PadGrid& grid;
    bool hover = false, dragging = false;
    juce::Image wave;
    const void* waveOf = nullptr;
    juce::String waveSkin;
};

class PadGrid : public juce::Component
{
public:
    explicit PadGrid (Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void tick (float dt);                         // lights
    void refresh();
    int bank = 0;
    std::function<void()> onSelect;
private:
    Processor& proc;
    std::array<std::unique_ptr<PadComp>, padsPerBank> pads;
    std::array<uint32_t, numPads> lastHits {};
    std::array<float, numPads> lights {};
    juce::Rectangle<float> bankRect (int b) const;
};

// ==================================================================== PAD EDITOR
class PadEditor : public juce::Component
{
public:
    explicit PadEditor (Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void padChanged();                            // selection or kit changed
    void tick();
    std::function<void()> onFindSimilar;
private:
    Processor& proc;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::unique_ptr<Knob> vibe;
    KeyButton typeKey, muteKey, soloKey, prevHit, nextHit, loadKey, outKey, similarKey;
    std::array<std::unique_ptr<KeyButton>, 5> choke;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> buttonAttachments;
    std::unique_ptr<juce::ParameterAttachment> chokeAttach, outAttach, startAttach, endAttach;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::TextEditor> rename;
    juce::Rectangle<float> wave, titleArea;
    int attachedPad = -1, dragHandle = 0;
    float startV = 0, endV = 1;
    void attach();
    void showTypeMenu();
    void showOutMenu();
};

// ==================================================================== MIDI STRIP
class SeqStrip : public juce::Component
{
public:
    explicit SeqStrip (Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void refresh();
    KeyButton editKey, playKey, prevKey, nextKey;
    std::function<void (bool)> onEdit;
    std::function<void()> onPatternChanged;
private:
    Processor& proc;
    juce::Rectangle<float> bpmBox, patBox, roll;
    double dragBpm = 0;
    bool dragging = false;
};

} // namespace eclipse
