#pragma once

#include "PluginProcessor.h"

namespace horizon
{

/**
    The rolling waveform. "Now" is the glowing line on the right (the horizon); older audio
    drifts to the left until it falls out of the buffer.

      drag                 select
      drag the selection   drag it out as a WAV (to the DAW, Finder, anywhere)
      alt + click          preview (the selection, or from that point)
      cmd/ctrl + click     pause / resume recording
      cmd/ctrl+shift+drag  zoom (the wheel zooms too)
      middle drag          move through time (back to the right edge = follow live again)
      double click         clear the selection
*/
class WaveView : public juce::Component,
                 public juce::DragAndDropContainer
{
public:
    explicit WaveView (Processor&);

    void setPalette (const lt::ui::Palette& p) { palette = p; repaint(); }
    void tick();                                       // 30 Hz from the editor

    std::function<void (const juce::String&)> onMessage;
    std::function<void()> onPauseToggled, onRightClick;

    bool hasSelection() const noexcept          { return ! selection.isEmpty(); }
    juce::Range<juce::int64> getSelection() const { return selection; }
    void clearSelection()                       { selection = {}; repaint(); }
    void setSelection (juce::Range<juce::int64> r) { selection = r; repaint(); }
    void exportSelection (bool reveal);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum class Drag { none, select, moveStart, moveEnd, exportOut, zoom, pan };

    juce::Rectangle<float> waveArea() const;
    juce::int64 viewEnd() const;
    juce::int64 viewSpan() const;
    double xToPos (float x) const;
    float posToX (juce::int64 pos) const;
    void setSpan (double newSpan, float anchorX);
    void updateCursor (const juce::MouseEvent&);
    void startExportDrag();

    Processor& proc;
    lt::ui::Palette palette = lt::ui::Palette::dark();

    juce::Range<juce::int64> selection;
    double span = 0.0;                    // samples visible (0 = the whole buffer)
    bool following = true;                // right edge = now
    juce::int64 frozenEnd = 0;            // right edge when not following
    Drag drag = Drag::none;
    juce::int64 dragAnchor = 0;
    double dragStartSpan = 0.0;
    juce::int64 dragStartEnd = 0;
    bool exportStarted = false;
    float hoverX = -1.0f;
};

class Editor : public juce::AudioProcessorEditor,
               private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int baseWidth = 1040, baseHeight = 400;

private:
    struct Content : public juce::Component
    {
        explicit Content (Editor& e) : owner (e) {}
        void mouseDown (const juce::MouseEvent& e) override { owner.mouseDown (e.getEventRelativeTo (&owner)); }
        Editor& owner;
    };
    struct Decor : public juce::Component          // logo + hints, above the animated background
    {
        explicit Decor (Editor& e) : owner (e) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        Editor& owner;
    };

    void paintContent (juce::Graphics&);
    void timerCallback() override;
    void showSettingsMenu();
    void showLengthMenu();
    void chooseFolder();
    void showActivation (bool);
    void refreshButtons();
    void flash (const juce::String&);

    Processor& proc;
    lt::ui::Palette palette = lt::ui::Palette::tinted (juce::Colour (0xffff8a3d), juce::Colour (0xff6a1fb0),
                                                       juce::Colour (0xff2a0b55), juce::Colour (0xff06040c));
    lt::ui::GlassLookAndFeel lnf;
    Content content { *this };
    lt::ui::NebulaBackground background { 41 };
    Decor decor { *this };
    WaveView wave;

    juce::TextButton recButton, lengthButton, bpmButton { "BPM IN NAME" }, folderButton { "FOLDER" },
                     saveButton { "SAVE SELECTION" }, settingsButton { "..." }, licenseButton;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::String message;
    double messageUntil = 0.0;
    float level = 0.0f;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace horizon
