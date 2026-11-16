#pragma once

#include "PluginProcessor.h"
#include "Views.h"
#include "ModViews.h"
#include "PresetBrowser.h"

namespace nebula
{

/** The 8 PITCH STEP bars (drag up / down, -12 .. +12). */
class StepBars : public juce::Component, private juce::Timer
{
public:
    StepBars (juce::AudioProcessorValueTreeState&, NebulaEngine&);
    const Palette* palette = nullptr;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    void setFrom (juce::Point<float>);
    juce::AudioProcessorValueTreeState& state;
    NebulaEngine& engine;
};

/** Shape of the tape stop (speed over time); drag up / down to change CURVE. */
class TapeCurveView : public juce::Component
{
public:
    explicit TapeCurveView (juce::AudioProcessorValueTreeState& s) : state (s) {}
    const Palette* palette = nullptr;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::AudioProcessorValueTreeState& state;
    float startCurve = 0;
};

class Editor : public juce::AudioProcessorEditor,
               public juce::DragAndDropContainer,
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
    struct Page : public juce::Component
    {
        Page (Editor& e, bool adv) : owner (e), advanced (adv) {}
        void paint (juce::Graphics&) override;
        Editor& owner;
        bool advanced;
    };

    void timerCallback() override;
    void layout();
    void applyTheme();
    void setAdvanced (bool);
    void showPresetMenu();
    void openBrowser();
    void stepPreset (int);
    void savePreset();
    void showActivation (bool);
    MacroKnob& knob (Page&, const char* id, const juce::String& label, float diameter);
    juce::TextButton& toggle (Page&, const char* id, const juce::String& text);

    Processor& proc;
    Palette palette;
    lt::ui::GlassLookAndFeel lnf;
    Content content;
    lt::ui::NebulaBackground background { 71 };
    Page simple { *this, false }, advanced { *this, true };
    GalaxyView galaxy, miniGalaxy;

    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, themeButton { "THEME" },
                     simpleButton { "SIMPLE" }, advancedButton { "ADVANCED" }, licenseButton, tapeNow { "STOP NOW" };
    std::unique_ptr<ChoiceRow> stepRow, everyRow, tapeRow, advEvery, tapeTrigger, advStepRate, advScale;
    juce::OwnedArray<MacroKnob> knobs;
    juce::OwnedArray<juce::TextButton> toggles;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> buttonAttachments;
    StepBars stepBars;
    TapeCurveView tapeCurve;
    juce::Slider gdMixSlider;
    PresetBrowser browser;
    FxChainView fxChain;
    ModStepsView mod1View;
    ModCurveView mod2View;
    juce::OwnedArray<ModChip> chips;
    juce::TextButton mod1Random { "RANDOM" }, mod1Clear { "CLEAR" };
    std::unique_ptr<ChoiceRow> mod1Rate, mod2Rate, rndRate;
    void setupModulation (MacroKnob&, const char* id);
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    float inL = 0, inR = 0, outL = 0, outR = 0;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace nebula
