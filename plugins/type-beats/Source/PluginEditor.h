#pragma once

#include "PluginProcessor.h"
#include "Views.h"
#include "Downloader.h"

namespace typebeats
{

class Editor : public juce::AudioProcessorEditor,
               public juce::FileDragAndDropTarget,
               private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

    static constexpr int baseWidth = 1100, baseHeight = 640;

private:
    /** Everything is laid out at base size inside this, then scaled to the window. */
    struct Content : public juce::Component {};

    /** Title, link bar and dividers: above the nebula, below the controls. */
    struct Decor : public juce::Component
    {
        explicit Decor (Editor& e) : owner (e) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        Editor& owner;
    };

    void timerCallback() override;
    void layout();
    void applyTheme();
    void refreshFromResult();
    void browse();
    void startDownload();
    void exportMidi();
    juce::File createTempMidi();
    void showSettingsMenu();
    void showActivation (bool show);
    void updateSelectionNotes();

    Processor& proc;
    Palette palette;
    lt::ui::GlassLookAndFeel lnf;
    Content content;
    lt::ui::NebulaBackground background { 23 };
    Decor decor { *this };

    juce::TextButton themeButton { "THEME" }, settingsButton { "SETTINGS" }, licenseButton,
                     getButton { "GET" }, exportButton { "EXPORT MIDI" };
    juce::TextEditor linkEditor;
    WaveformView waveform;
    BpmView bpmView;
    KeyView keyView;
    PianoView piano;
    ChordView chordView;
    DragHandle dragHandle;

    Downloader downloader;
    juce::String downloadStatus;
    float downloadProgress = 0;
    bool downloading = false;

    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> linkBar, bpmArea, keyArea, chordArea;
    juce::String transientMessage;
    juce::uint32 transientUntil = 0;

    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace typebeats
