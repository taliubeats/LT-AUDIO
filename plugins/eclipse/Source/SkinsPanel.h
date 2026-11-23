#pragma once

#include "Views.h"

namespace eclipse
{

/** The SKINS section: neutral and artist skins, each with its VIBE character. */
class SkinsPanel : public juce::Component
{
public:
    explicit SkinsPanel (Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    std::function<void (const juce::String&)> onPick;
    std::function<void()> onClose;
private:
    Processor& proc;
    KeyButton doneKey;
    std::vector<juce::Rectangle<float>> cards;
    std::vector<juce::Image> thumbs;
    int hover = -1;
};

} // namespace eclipse
