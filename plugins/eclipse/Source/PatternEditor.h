#pragma once

#include "Views.h"

namespace eclipse
{

/**
    The pattern editor (pencil): replaces the pads. One row per pad; click adds / removes,
    drag up and down sets the velocity, Cmd + drag draws a roll. VARY makes variations.
*/
class PatternEditor : public juce::Component
{
public:
    explicit PatternEditor (Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void refresh();                                   // pattern or kit changed
    std::function<void()> onDone, onChange;

private:
    Processor& proc;
    std::array<std::unique_ptr<KeyButton>, 5> resKeys;    // 1/8 1/16 1/32 1/64 T
    KeyButton resetKey, doneKey, allKey, kickKey, snareKey, hatsKey, origKey, prevKey, nextKey, barsKey;
    std::array<std::unique_ptr<KeyButton>, 3> rollKeys;
    std::array<std::unique_ptr<KeyButton>, 4> shapeKeys;
    Fader amount;
    int res = 4;                     // grid step in 1/64 ticks
    bool triplet = false;
    int historyPos = 0;
    VaryOptions opts;
    juce::Random rnd;
    std::vector<int> rows;           // pads shown
    int scroll = 0;
    juce::Rectangle<float> grid, labels;
    // drag state
    int dragRow = -1, dragTick = -1, dragNote = -1, lastRollTick = -1;
    float dragVel = 0.8f;
    bool rollMode = false;

    Pattern& current();
    float step() const { return triplet ? (float) res * 2.0f / 3.0f : (float) res; }
    int tickAt (float x) const;
    int rowAt (float y) const;
    void doVary (VaryTarget);
    void pushHistory (const Pattern&);
    void updateKeys();
};

} // namespace eclipse
