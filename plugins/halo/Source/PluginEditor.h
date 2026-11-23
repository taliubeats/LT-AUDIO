#pragma once

#include "PluginProcessor.h"
#include "Views.h"

namespace halo
{

class Editor : public juce::AudioProcessorEditor,
               public juce::FileDragAndDropTarget,
               public juce::DragAndDropContainer,
               private juce::ChangeListener,
               private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHover = true; decor.repaint(); }
    void fileDragExit (const juce::StringArray&) override            { dropHover = false; decor.repaint(); }

    static constexpr int baseWidth = 1200, baseHeight = 900;

private:
    struct Content : public juce::Component {};
    struct Decor : public juce::Component
    {
        explicit Decor (Editor& e) : owner (e) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        Editor& owner;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void layout();
    void setTab (int);
    void refreshSessionViews();
    void syncStripCombos();
    void onGenerate();
    void browse();
    void toggleCapture();
    void showKeyMenu();
    void showPresetMenu();
    void showSettingsMenu();
    void stepPreset (int delta);
    void savePreset();
    void exportAll();
    void showActivation (bool);
    void downloadLyricsModel();
    juce::String keyText() const;
    bool anyDirty() const;

    Processor& proc;
    Palette palette = Palette::dark();
    lt::ui::GlassLookAndFeel lnf;
    Content content;
    lt::ui::NebulaBackground background { 83 };
    Decor decor { *this };
    juce::TooltipWindow tooltips { this, 600 };

    // top bar
    SegmentSwitch tabs { "HARMONY", "CHOIR" };
    ValueBox keyBox { "KEY & SCALE:" }, presetBox { "" };
    juce::TextButton detectButton { "DETECT" }, saveButton { "SAVE" }, licenseButton;
    IconButton prevPreset { IconButton::Icon::prev, false }, nextPreset { IconButton::Icon::next, false };
    IconButton undoButton { IconButton::Icon::undo }, redoButton { IconButton::Icon::redo },
               settingsButton { IconButton::Icon::settings }, powerButton { IconButton::Icon::power };

    // vocal (shared by both tabs)
    juce::TextButton browseButton { "BROWSE" }, useTrackButton { "USE TRACK" };
    juce::TextButton browseButton2 { "BROWSE" }, useTrackButton2 { "USE TRACK" };
    WaveView wave, choirWave;

    // HARMONY
    OrbitView orbits;
    std::array<StripView, numStrips> strips;
    PlanetKnob dry { "DRY", 1 }, wet { "WET", 2 }, width { "WIDTH", 3 }, tightness { "TIGHTNESS", 4 };
    SegmentSwitch quality { "FAST", "BEST" };
    HMeter inMeter, outMeter;
    IconButton playButton { IconButton::Icon::play, false }, stopButton { IconButton::Icon::stop, false };
    GlassSwitch leadSwitch;
    SegmentSwitch leadLabel { "SOLO HARMONIES", "WITH LEAD" };
    GenerateButton generate;
    DragHandle dragAll { "DRAG ALL" };
    juce::TextButton exportButton { "EXPORT" };

    // CHOIR
    juce::Slider voicesSlider;
    ChoirCluster cluster;
    ChoirTypeCards typeCards;
    juce::ComboBox choirHarmony;
    SectionSlider sop { "SOPRANO" }, alto { "ALTO" }, tenor { "TENOR" }, bass { "BASS" };
    PlanetKnob spread { "SPREAD", 5 }, loose { "LOOSE", 6 }, cWidth { "WIDTH", 7 }, room { "ROOM", 8 }, breath { "BREATH", 9 };
    GenerateButton generate2;
    IconButton playButton2 { IconButton::Icon::play, false }, stopButton2 { IconButton::Icon::stop, false };
    GlassSwitch leadSwitch2;
    SegmentSwitch leadLabel2 { "SOLO CHOIR", "WITH LEAD" };
    DragHandle dragChoir { "DRAG ALL" };
    juce::TextButton exportButton2 { "EXPORT" };
    juce::ToggleButton separateToggle { "Separate sections" };

    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> buttonAttachments;
    std::unique_ptr<juce::ParameterAttachment> qualityAttachment, cTypeAttachment;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::URL::DownloadTask> download;

    std::vector<juce::Component*> harmonyOnly, choirOnly;
    bool dropHover = false, syncingCombos = false;
    juce::String lastDecorSignature;
    float downloadProgress = -1.0f;
    juce::String notice;              // short message shown in the status line (e.g. why a drag did not start)
    juce::uint32 noticeUntil = 0;
    void showNotice (const juce::String&);
    juce::File dragStrip (int index);
    juce::File dragMix();
    juce::File dragChoirFile();
    static void logDrag (const juce::String&);
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace halo
