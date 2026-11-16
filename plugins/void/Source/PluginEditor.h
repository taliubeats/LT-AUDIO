#pragma once

#include "PluginProcessor.h"
#include "Views.h"

namespace lt_void
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
    static Palette paletteFor (int type);

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
    void setType (int type);
    void startTransition (int type);
    void attachPreDelay();
    void showTypeMenu();
    void showPresetMenu();
    void savePreset();
    void showActivation (bool);
    void repaintAll();

    Processor& proc;
    Palette palette, fromPalette, toPalette;
    float transition = 1.0f;
    int shownType = -1;
    lt::ui::GlassLookAndFeel lnf;
    Content content;
    lt::ui::NebulaBackground backA { 61 }, backB { 61 };
    lt::ui::NebulaBackground* front = &backA;
    lt::ui::NebulaBackground* back = &backB;
    Decor decor { *this };

    BlackHoleView hole;
    VoidKnob mix { "MIX", true }, preDelay { "PRE-DELAY", true }, size { "SIZE", true },
             highFreq { "HIGH FREQ", false }, lowFreq { "LOW FREQ", false }, attack { "ATTACK", false },
             diffusion { "DIFFUSION", false }, modRate { "RATE", false }, modDepth { "DEPTH", false },
             lowCut { "LOW CUT", false }, highCut { "HIGH CUT", false }, width { "WIDTH", false }, duck { "DUCK", false };
    LevelMeter inMeter, outMeter;
    FreezeButton freeze;
    juce::TextButton syncButton { "SYNC" }, typePrev { "<" }, typeNext { ">" }, typeName, typeMenu { "v" },
                     presetButton, saveButton { "SAVE" }, licenseButton;

    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;
    std::unique_ptr<juce::SliderParameterAttachment> preDelayAttachment;
    std::unique_ptr<juce::ButtonParameterAttachment> freezeAttachment, syncAttachment;
    std::unique_ptr<juce::ParameterAttachment> typeAttachment;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    juce::Rectangle<int> typeBox, presetBox;
    bool syncShown = false;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace lt_void
