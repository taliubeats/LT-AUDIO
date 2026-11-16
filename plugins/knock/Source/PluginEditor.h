#pragma once

#include "PluginProcessor.h"
#include "Views.h"

namespace knock
{

class Editor : public juce::AudioProcessorEditor,
               private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int baseWidth = 1100, baseHeight = 615;

private:
    struct Content : public juce::Component {};
    struct Decor : public juce::Component
    {
        explicit Decor (Editor& e) : owner (e) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        Editor& owner;
    };

    void timerCallback() override;
    void layout();
    void applyTheme();
    void chaos();
    void undoChaos();
    void showPresetMenu();
    void savePreset();
    void showFocus (bool);
    void showActivation (bool);

    Processor& proc;
    Palette palette;
    lt::ui::GlassLookAndFeel lnf;
    Content content;
    lt::ui::NebulaBackground background { 29 };
    Decor decor { *this };
    PlanetView planet;
    lt::ui::FrequencyFocusView focusView;

    KnockKnob punch { "PUNCH", 76 }, saturation { "SATURATION", 76 }, distortion { "DISTORTION", 60 },
              bright { "BRIGHT", 76 }, low { "LOW", 76 }, glue { "GLUE", 76 }, width { "WIDTH", 76 },
              crush { "CRUSH", 50 }, lofi { "LO-FI", 50 }, stutter { "STUTTER", 50 },
              filter { "FILTER", 50 }, ring { "RING", 50 }, reverse { "REVERSE", 50 },
              reverb { "REVERB", 50 }, delay { "DELAY", 50 }, input { "INPUT", 46 }, output { "OUTPUT", 46 };
    DistTypeList distList;
    LockButton lockTone, lockFx, lockSpace;
    StereoMeter inMeter, outMeter;
    juce::TextButton presetButton, saveButton { "SAVE" }, eqButton { "EQ" }, themeButton { "THEME" }, licenseButton,
                     chaosButton { "CHAOS" }, undoButton, delayTimeButton;
    juce::ToggleButton modeToggle { "DRUMS | MELODY" };

    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;
    std::unique_ptr<juce::ButtonParameterAttachment> modeAttachment;
    std::unique_ptr<juce::ParameterAttachment> distAttachment, delayTimeAttachment;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    std::vector<std::map<juce::String, float>> undoStack;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace knock
