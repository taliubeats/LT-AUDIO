#pragma once

#include "Views.h"

namespace eclipse
{

/** FIND SIMILAR: the sounds of your library closest to the selected pad. Click = listen, USE = put it on the pad. */
class SimilarPanel : public juce::Component, private juce::Timer
{
public:
    explicit SimilarPanel (Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void search();                              // for the selected pad
    std::function<void()> onDone;
private:
    void timerCallback() override;
    Processor& proc;
    KeyButton chooseKey, sameKey, useKey, doneKey, againKey;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<SimilarSearch::Match> results;
    int selected = -1, scroll = 0, forPad = -1;
    bool wasScanning = false;
    juce::Rectangle<float> list;
    static constexpr float rowH = 34.0f;
};

} // namespace eclipse
