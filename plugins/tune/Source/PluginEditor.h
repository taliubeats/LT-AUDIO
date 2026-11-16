#pragma once

#include "PluginProcessor.h"
#include "Views.h"
#include "MatchView.h"

namespace tune
{

class Editor : public juce::AudioProcessorEditor,
               private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int baseWidth = 1100, baseHeight = 614;

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
    void showVoiceMenu();
    void showKeyMenu();
    void showPresetMenu();
    void showSettingsMenu();
    void stepPreset (int delta);
    void savePreset();
    void showActivation (bool);
    void showMatch (bool);
    void applyMatch (const MatchResult&, std::array<bool, MatchView::rowCount>, float amount);

    Processor& proc;
    Palette palette;
    lt::ui::GlassLookAndFeel lnf;
    Content content;
    lt::ui::NebulaBackground background { 61 };
    Decor decor { *this };

    TunerView tuner;
    NoteOrbs orbs;
    RetunePlanet planet;
    TuneKnob humanize { "HUMANIZE" }, flex { "FLEX TUNE" }, vibrato { "VIBRATO" },
             formant { "FORMANT" }, transpose { "TRANSPOSE" }, mix { "MIX" }, detune { "" };
    GlassSwitch preserve;
    HMeter inMeter, outMeter;

    SelectorPill voicePill { "VOICE" }, keyPill { "KEY & SCALE" }, presetPill { "" };
    IconButton prevPreset { IconButton::Icon::prev, false }, nextPreset { IconButton::Icon::next, false };
    IconButton resetNotesButton { IconButton::Icon::reset, false };
    IconButton undoButton { IconButton::Icon::undo }, redoButton { IconButton::Icon::redo },
               settingsButton { IconButton::Icon::settings }, powerButton { IconButton::Icon::power };
    juce::TextButton saveButton { "SAVE" }, matchButton { "MATCH REF" }, themeButton { "THEME" }, licenseButton,
                     lowLatencyButton { "LOW LATENCY" }, hqButton { "HQ" };

    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;
    std::unique_ptr<juce::ButtonParameterAttachment> preserveAttachment;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<MatchView> match;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    int lastRetunes = 0;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace tune
