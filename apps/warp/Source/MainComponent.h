#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <lt_shared/lt_shared.h>
#include "Converter.h"

namespace warp
{

/** Comic-book skins: COLOR, INK, NOIR. */
struct Skin
{
    juce::String name;
    juce::Colour paper, dots, panel, ink, text, dim, a1, a2, a3, onAccent;
    static const Skin& get (int);
};

class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }

    void saveScreenshot (const juce::File&);
    void loadDemo();
    void setSkin (int s) { skin = s; repaint(); }

    static constexpr int baseWidth = 1000, baseHeight = 660;

private:
    struct Hit { juce::Rectangle<float> r; std::function<void()> fn; };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void browse();
    void chooseFolder();
    void save();
    void load();
    void applyTrim();
    void showActivation (bool);
    juce::Point<float> toBase (juce::Point<float>) const;

    // drawing helpers
    void panel (juce::Graphics&, juce::Rectangle<float>, juce::Colour fill, float shadow = 6.0f) const;
    void chip (juce::Graphics&, juce::Rectangle<float>, const juce::String&, bool on, juce::Colour accent, std::function<void()>);
    void halftone (juce::Graphics&, juce::Rectangle<float>, juce::Colour, float spacing, float maxR, bool fadeDown) const;
    void burst (juce::Graphics&, juce::Point<float>, float r, juce::Colour fill, const juce::String& text, float rotation) const;

    Queue queue { 2 };
    std::vector<Queue::Item> demo;
    std::vector<Hit> hits;
    int skin = 0;
    bool persist = true;      // the screenshot preview never touches the saved settings
    Target audioTarget = Target::wav24, imageTarget = Target::png;
    bool dragOver = false;
    int scroll = 0;
    float doneFlash = 0;
    int lastDoneCount = 0;
    juce::Point<float> mouse;

    juce::TextEditor trimStart, trimEnd;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::PropertiesFile> settings;

    lt::licensing::LicenseManager license { lt::licensing::Products::warp, "LT WARP" };
    lt::ui::Palette ltPalette = lt::ui::Palette::dark();
    std::unique_ptr<lt::ui::ActivationComponent> activation;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace warp
