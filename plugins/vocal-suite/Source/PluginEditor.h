#pragma once

#include "PluginProcessor.h"
#include "Views.h"

namespace vocal
{

class Editor : public juce::AudioProcessorEditor,
               private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int baseWidth = 1200, baseHeight = 896;

private:
    struct Content : public juce::Component {};
    struct Decor : public juce::Component
    {
        explicit Decor (Editor& e) : owner (e) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        static float bubbleTextSize (const juce::String&);
        Editor& owner;
    };

    /** BYPASS pill: glows pink when the chain is bypassed. */
    struct BypassButton : public juce::Button
    {
        BypassButton() : juce::Button ("BYPASS") { setClickingTogglesState (true); }
        const Palette* palette = nullptr;
        void paintButton (juce::Graphics&, bool hover, bool down) override;
    };

    enum Module { tuneM = 0, cleanupM, toneM, dynamicsM, spaceM, fxM, numModules };

    void timerCallback() override;
    void layout();
    void applyTheme();
    void showModule (int);
    void send();
    void showPresetMenu();
    void savePreset();
    void showAssistantMenu();
    void askForApiKey();
    void showActivation (bool);
    void setBubble (const juce::String&, bool highlightFirstWord);

    SuiteKnob& knob (const char* id, const juce::String& label, float d, int module);
    ChoicePill& choice (const char* id, int module, bool large = false);
    ModuleSwitch& toggle (const char* id, const juce::String& label, int module);

    Processor& proc;
    Palette palette;
    lt::ui::GlassLookAndFeel lnf;
    Content content;
    lt::ui::NebulaBackground background { 61 };
    Decor decor { *this };

    Mascot mascot;
    juce::TextEditor chatInput;
    juce::TextButton sendButton { "Send" }, micButton, presetButton, saveButton { "SAVE" },
                     undoButton { "UNDO" }, redoButton { "REDO" }, themeButton { "THEME" }, licenseButton;
    BypassButton bypass;
    juce::OwnedArray<ModuleTab> tabs;
    int currentModule = tuneM;

    TuneDisplay tuneDisplay;
    ChoicePill keyPill;
    EqCurve eqCurve;
    ReductionBar grBar, essBar;
    DashMeter inMeter, outMeter;

    juce::OwnedArray<SuiteKnob> knobs;
    juce::OwnedArray<ChoicePill> choices;
    juce::OwnedArray<ModuleSwitch> switches;
    std::map<juce::Component*, int> moduleOf;
    std::map<juce::String, juce::Component*> byId;

    juce::String bubbleText;
    bool bubbleHighlight = true;

    // attachments last: destroyed first
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> choiceAttachments;
    std::unique_ptr<juce::ParameterAttachment> keyAttachment, scaleAttachment;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::AlertWindow> dialog;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace vocal
