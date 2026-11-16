#pragma once

#include "PluginProcessor.h"
#include "Views.h"

namespace orbit
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
    void attachTime();
    void showPresetMenu();
    void savePreset();
    void showActivation (bool);

    Processor& proc;
    Palette palette;
    lt::ui::GlassLookAndFeel lnf;
    Content content;
    lt::ui::NebulaBackground background { 83 };
    Decor decor { *this };
    OrbitView orbitView;

    OrbitKnob time { "TIME", true }, feedback { "FEEDBACK", true }, mix { "MIX", true },
              lowCut { "LOW CUT", false }, highCut { "HIGH CUT", false }, width { "WIDTH", false },
              duck { "DUCK", false }, wobble { "WOBBLE", false }, drive { "DRIVE", false };
    FreezePill freeze;
    StereoMeter inMeter, outMeter;
    juce::TextButton presetButton, saveButton { "SAVE" }, themeButton { "THEME" }, licenseButton,
                     syncButton { "SYNC" }, msButton { "MS" };
    juce::TextButton modeButtons[Modes::count], feelButtons[3];

    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;
    std::unique_ptr<juce::SliderParameterAttachment> timeAttachment;
    std::unique_ptr<juce::ButtonParameterAttachment> freezeAttachment;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment, feelAttachment, syncAttachment;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    bool timeIsSync = false, timeAttached = false;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace orbit
