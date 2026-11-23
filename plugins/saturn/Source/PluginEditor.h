#pragma once

#include "PluginProcessor.h"
#include "Views.h"

namespace saturn
{

/** LT SATURN v2 editor: vintage tape machine (see design/saturn/main-v2.webp). */
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
    void showActivation (bool);

    Processor& proc;
    Vintage look;
    lt::ui::GlassLookAndFeel lnf;          // popup menus and dialogs
    Content content;
    Decor decor { *this };
    juce::Image panelTop, panelMid, panelBottom, woodLeft, woodRight;

    TapeScreen screen;
    VintageKnob age { "AGE", VintageKnob::Style::age }, output { "OUTPUT", VintageKnob::Style::mini },
                wow { "WOW", VintageKnob::Style::small }, flutter { "FLUTTER", VintageKnob::Style::small },
                drift { "DRIFT", VintageKnob::Style::small }, saturation { "SATURATION", VintageKnob::Style::small },
                tone { "TONE", VintageKnob::Style::small }, bandwidth { "BANDWIDTH", VintageKnob::Style::small },
                hiss { "HISS", VintageKnob::Style::small }, ensemble { "ENSEMBLE", VintageKnob::Style::small },
                mix { "MIX", VintageKnob::Style::small };
    VUMeter inMeter, outMeter;
    KeyButton presetsKey { "PRESETS", true }, saveKey { "SAVE", true }, themeKey { "THEME", true }, licenseKey { "" };
    juce::OwnedArray<KeyButton> tapeKeys;

    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;
    std::unique_ptr<juce::ParameterAttachment> tapeAttachment;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::uint32 lastTick = 0;

    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace saturn
