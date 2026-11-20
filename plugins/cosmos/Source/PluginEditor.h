#pragma once

#include "PluginProcessor.h"
#include "PanelStrip.h"

namespace cosmos
{

class Editor : public juce::AudioProcessorEditor,
               private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    static constexpr int defaultWidth = 1180, defaultHeight = 176, barHeight = 26;

private:
    struct Item
    {
        juce::String text;
        juce::Rectangle<float> bounds;
        std::function<void()> click;
        bool active = false, accent = false;
    };

    void timerCallback() override;
    void pullAudio();
    void buildBar();
    void showAddMenu();
    void showSkinMenu();
    void showSizeMenu();
    void showTargetMenu();
    void applySize (int w, int h);
    void setPinned (bool);
    void setDocked (bool);
    void setFullscreen (bool);
    void showAudioSettings();
    void showActivation (bool);
    juce::DocumentWindow* window() const;
    void runSnapshot (const juce::String& file);

    Processor& proc;
    Analyzer analyzer;
    Readings readings;
    PanelStrip strip;
    std::vector<Item> items;
    int hoverItem = -1;
    std::vector<float> scratchL, scratchR;
    juce::int64 analyzerBase = 0;      // processor samples read before this editor started
    int frame = 0;
    double startTime = juce::Time::getMillisecondCounterHiRes();
    bool docked = false, fullscreen = false;
    juce::Rectangle<int> undockedBounds;

    lt::ui::Palette ltPalette = lt::ui::Palette::dark();
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace cosmos
