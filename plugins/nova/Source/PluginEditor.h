#pragma once

#include "PluginProcessor.h"
#include "Views.h"

namespace nova
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
        Editor& owner;
    };

    void timerCallback() override;
    void layout();
    void applyTheme();
    void showPresetMenu();
    void savePreset();
    void showFocus (bool);
    void showActivation (bool);

    Processor& proc;
    Palette palette;
    lt::ui::GlassLookAndFeel lnf;
    Content content;
    lt::ui::NebulaBackground background { 43 };
    Decor decor { *this };

    StarView star;
    TypeBar typeBar;
    NovaKnob tone { "TONE", 96 }, warmth { "WARMTH", 96 }, air { "AIR", 96 }, character { "CHARACTER", 96 },
             input { "INPUT", 78 }, mix { "MIX", 78 }, output { "OUTPUT", 78 };
    PillSwitch autoGain;
    DbMeter inMeter, outMeter;
    lt::ui::FrequencyFocusView focusView;
    juce::TextButton presetButton, saveButton { "SAVE" }, eqButton { "EQ" }, themeButton { "THEME" }, licenseButton;

    // attachments are declared after every control so they are destroyed first
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;
    std::unique_ptr<juce::ButtonParameterAttachment> autoGainAttachment;
    std::unique_ptr<juce::ParameterAttachment> typeAttachment;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace nova
