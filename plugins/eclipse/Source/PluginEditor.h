#pragma once

#include "PluginProcessor.h"
#include "Views.h"
#include "PatternEditor.h"
#include "SkinsPanel.h"
#include "SimilarPanel.h"

namespace eclipse
{

class Editor : public juce::AudioProcessorEditor,
               public juce::DragAndDropContainer,
               private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;

    void paint (juce::Graphics&) override {}
    void resized() override {}

    /** For the snapshot tool: shows the pattern editor / skins panel. */
    void showPatternEditor (bool);
    void showSkins (bool);
    void showSimilar (bool);
    void applySkin (const juce::String& id);
    juce::Component& getContent() { return content; }

private:
    struct Content : public juce::Component
    {
        explicit Content (Editor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent& e) override { if (owner.kitBox.contains (e.position)) owner.showKitMenu(); }
        Editor& owner;
    };

    void timerCallback() override;
    void applyScale();
    void kitChanged();
    void showKitMenu();
    void stepKit (int);
    void saveKit();
    void showMainMenu();
    void showActivation (bool);

    Processor& proc;
    look::LookAndFeel lnf;
    Content content { *this };
    SourceView source;
    PadGrid pads;
    PadEditor padEditor;
    PatternEditor patternEditor;
    SeqStrip seq;
    SkinsPanel skins;
    SimilarPanel similarPanel;
    KeyButton prevKit, nextKit, saveKey, skinsKey, menuKey;
    Fader outFader;
    std::unique_ptr<juce::SliderParameterAttachment> outAttach;
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::Rectangle<float> kitBox { 512, 16, 400, 42 };
    double lastTime = 0;
    bool wasCloning = false;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace eclipse
