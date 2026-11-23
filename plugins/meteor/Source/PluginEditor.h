#pragma once

#include "PluginProcessor.h"
#include "Views.h"

namespace meteor
{

class Editor : public juce::AudioProcessorEditor,
               public juce::FileDragAndDropTarget,
               public juce::DragAndDropContainer,
               private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { sourceView.dragHover = true; sourceView.repaint(); }
    void fileDragExit (const juce::StringArray&) override            { sourceView.dragHover = false; sourceView.repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

    static constexpr int baseWidth = 1100, baseHeight = 615;

private:
    struct Content : public juce::Component {};
    struct Decor : public juce::Component
    {
        explicit Decor (Editor& e) : owner (e) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        Editor& owner;
    };

    struct Module
    {
        juce::Rectangle<int> card;
        std::unique_ptr<PowerPill> power;
        std::unique_ptr<juce::ButtonParameterAttachment> powerAttachment;
        std::vector<std::pair<juce::String, juce::Rectangle<int>>> labels;   // small captions inside the card
    };

    void timerCallback() override;
    void layout();
    void applyTheme();
    void showPage (int);
    void showPresetMenu();
    void savePreset();
    void showActivation (bool);
    void chooseFile();
    void toggleCapture();
    void setStatus (const juce::String&, bool error = false);
    MeteorKnob& addKnob (const char* id, const juce::String& label, float size = 56.0f);
    ChoicePills& addPills (const char* id, const juce::StringArray& labels, int columns = 0);

    Processor& proc;
    Palette palette;
    lt::ui::GlassLookAndFeel lnf;
    Content content;
    lt::ui::NebulaBackground background { 47 };
    Decor decor { *this };

    juce::TextButton presetButton, saveButton { "SAVE" }, themeButton { "THEME" }, licenseButton,
                     liveTab { "LIVE" }, strokesTab { "STROKES" };

    // LIVE
    MeteorView meteorView;
    Module modules[4];
    juce::OwnedArray<MeteorKnob> knobs;
    juce::OwnedArray<ChoicePills> pills;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;
    MeteorKnob* mixKnob = nullptr;
    MeteorKnob* outKnob = nullptr;
    std::vector<juce::Component*> livePage, strokesPage;

    // STROKES
    SourceView sourceView;
    juce::TextButton loadButton { "LOAD FILE" }, captureButton { "USE TRACK" }, folderButton { "OPEN FOLDER" };
    juce::OwnedArray<StrokeCard> cards;
    juce::String status;
    bool statusError = false;
    juce::uint32 statusTime = 0;
    int lastGeneration = -1;
    std::unique_ptr<juce::FileChooser> chooser;

    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace meteor
