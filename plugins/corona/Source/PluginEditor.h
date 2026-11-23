#pragma once

#include "PluginProcessor.h"
#include "CoronaUI.h"

namespace corona
{

class Editor : public juce::AudioProcessorEditor,
               private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    struct BarLook : public juce::LookAndFeel_V4
    {
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
        juce::Font getPopupMenuFont() override;
    };
    struct Content : public juce::Component
    {
        explicit Content (Editor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        Editor& owner;
    };
    struct Segment
    {
        juce::String caption;
        std::vector<std::unique_ptr<juce::TextButton>> buttons;
        int x = 0;
    };

    void timerCallback() override;
    void applyScale();
    void stepPreset (int delta);
    void showPresetMenu();
    void savePreset();
    void switchAB (int slot);
    void showActivation (bool);
    void makeSegment (Segment&, const juce::String& caption, const juce::StringArray& items, int x, std::function<void (int)> onPick);

    Processor& proc;
    BarLook barLook;
    Content content { *this };
    ui::Chassis chassis;
    std::vector<std::unique_ptr<ui::Knob>> knobs;
    ui::Lever powerLever { true, true }, autoLever { false, false };
    ui::Jewel jewel;
    juce::TextButton prevButton { juce::CharPointer_UTF8 ("\xe2\x80\xb9") }, nextButton { juce::CharPointer_UTF8 ("\xe2\x80\xba") },
                     presetButton, saveButton { "SAVE" }, aButton { "A" }, bButton { "B" }, licenseButton;
    Segment modeSeg, ovsSeg, sizeSeg;

    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> buttonAttachments;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment, ovsAttachment;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    juce::ValueTree abSlots[2];
    int abCurrent = 0;
    double lastTime = 0;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace corona
