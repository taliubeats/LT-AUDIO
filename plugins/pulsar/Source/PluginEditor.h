#pragma once

#include "PluginProcessor.h"
#include "Views.h"

namespace pulsar
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

    /** The main page (header, knob, visualiser, shapes, rates). Paints the panels and labels. */
    struct MainPage : public juce::Component
    {
        explicit MainPage (Editor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        Editor& owner;
    };

    void timerCallback() override;
    void layout();
    void applyTheme();
    void syncFromProcessor();
    void showPresetMenu();
    void stepPreset (int delta);
    void savePreset();
    void showFocus (bool show);
    void showActivation (bool show);

    Processor& proc;
    Palette palette;
    lt::ui::GlassLookAndFeel lnf;
    Content content;
    lt::ui::NebulaBackground background { 47 };
    MainPage page { *this };
    lt::ui::FrequencyFocusView focusView;

    juce::TextButton eqButton { "EQ" }, prevButton { "<" }, nextButton { ">" }, presetButton, saveButton { "SAVE" },
                     themeButton { "THEME" }, licenseButton;
    juce::ToggleButton midiToggle { "MIDI TRIGGER" }, waveToggle { "WAVE" }, audioToggle { "AUDIO TRIGGER" };
    std::unique_ptr<juce::ButtonParameterAttachment> audioAttachment;
    MixKnob knob;
    CurveView curveView;
    std::vector<std::unique_ptr<ShapeButton>> shapeButtons;
    juce::TextButton rateButtons[Rates::count];

    std::unique_ptr<juce::SliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::ButtonParameterAttachment> midiAttachment;
    std::unique_ptr<juce::ParameterAttachment> rateAttachment;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    juce::Rectangle<int> header, knobPanel, wavePanel, bottomBar, presetBox;
    int lastCycle = 0;

    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace pulsar
