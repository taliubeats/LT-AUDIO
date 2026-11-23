#pragma once

#include "PluginProcessor.h"
#include "Views.h"

namespace air
{

/** LT AIR editor (see design/air/main-dark.webp). Dark only, like HALO and VOID. */
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
    void refresh();
    void stepPreset (int delta);
    void showPresetMenu();
    void showSettingsMenu();
    void savePreset();
    void showActivation (bool);
    void toggleBypass();
    void linkMoved (int section);

    Processor& proc;
    Palette palette = Palette::dark();
    lt::ui::GlassLookAndFeel lnf;
    Content content;
    lt::ui::NebulaBackground background { 83 };
    Decor decor { *this };

    AtmosphereView atmosphere;
    std::array<PlanetKnob, 3> knobs { PlanetKnob (3), PlanetKnob (8), PlanetKnob (13) };
    std::array<std::unique_ptr<ModelPills>, 3> pills;
    SmallKnob trim, mix { "MIX" }, output;
    BarMeter inMeter, outMeter;
    IconButton prevPreset { IconButton::Icon::prev }, nextPreset { IconButton::Icon::next },
               undoButton { IconButton::Icon::undo }, redoButton { IconButton::Icon::redo },
               settingsButton { IconButton::Icon::settings }, powerButton { IconButton::Icon::power },
               bypassButton { IconButton::Icon::power, true };
    ABButton abButton;
    PresetName presetButton;
    PillButton saveButton { "SAVE" }, linkButton { "LINK" };
    juce::TextButton licenseButton;

    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;
    std::array<std::unique_ptr<juce::ParameterAttachment>, 3> modelAttachments;
    std::unique_ptr<juce::ButtonParameterAttachment> linkAttachment;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    std::array<float, 3> lastValue {};
    std::array<bool, 3> dragging {};
    bool linking = false;
    juce::uint32 lastTick = 0;
    juce::String shownPreset;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace air
