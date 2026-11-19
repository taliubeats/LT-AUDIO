#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"
#include "Pages.h"
#include "Browser.h"

namespace quasar
{

class Editor : public juce::AudioProcessorEditor,
               public juce::DragAndDropContainer,
               private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;
    void showPage (const juce::String& page);

    static constexpr int baseWidth = 1200, baseHeight = 896;

private:
    struct Content : public juce::Component {};
    /** Header boxes, logo and glyphs: drawn above the animated background. */
    struct Header : public juce::Component
    {
        explicit Header (Editor& e) : owner (e) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        Editor& owner;
    };

    void timerCallback() override;
    void setSimple (bool);
    void showActivation (bool);
    void runSnapshot (const juce::String& file);

    Processor& proc;
    ui::LookAndFeel lnf;
    Content content;
    lt::ui::NebulaBackground background { 113 };
    Header header { *this };

    ui::OscPage oscPage { proc };
    ui::BottomPanels bottom { proc };
    ui::VoicingPanel voicing { proc };
    ui::MixPage mixPage { proc };
    ui::FxPage fxPage { proc };
    ui::MatrixPage matrixPage { proc };
    ui::GlobalPage globalPage { proc };
    ui::BrowserPage browser { proc };
    ui::SimplePage simplePage { proc };
    ui::ArpPanel arp { proc };
    ui::Keyboard keyboard { proc };

    juce::OwnedArray<juce::TextButton> tabs;
    juce::TextButton saveButton, favButton, presetButton, prevButton { "<" }, nextButton { ">" },
                     undoButton { "UNDO" }, redoButton { "REDO" }, browserButton { "BROWSER" }, licenseButton;
    ui::TextTabs viewTabs { { "SIMPLE", "ADVANCED" } };
    std::unique_ptr<ui::Knob> masterKnob;
    std::unique_ptr<ui::Meter> outMeter;
    ui::Bindings bindings;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    juce::String page { "OSC" }, pageBeforeBrowser { "OSC" };
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace quasar
