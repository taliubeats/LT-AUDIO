#pragma once

#include <lt_shared/lt_shared.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Modulation.h"
#include "NebulaFx.h"

namespace nebula
{

using lt::ui::Palette;

/**
    FX CHAIN: up to 8 effects, processed top to bottom.
    Click = choose / remove / move; drag left-right = amount; drag up-down = reorder; "+" adds an effect.
*/
class FxChainView : public juce::Component, private juce::Timer
{
public:
    explicit FxChainView (juce::AudioProcessorValueTreeState&);
    const Palette* palette = nullptr;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    int count() const;
    int rowAt (float y) const;
    juce::Rectangle<float> row (int i) const;
    int typeOf (int slot) const;
    float amountOf (int slot) const;
    void setSlot (int slot, int type, float amount);
    void showMenu (int slot);
    void compact();

    juce::AudioProcessorValueTreeState& state;
    int dragRow = -1, dropRow = -1;
    float dragStartAmount = 0;
    bool draggingAmount = false, reordering = false;
};

/** A small chip that starts dragging a modulation source; drop it on any knob. */
class ModChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ModChip (int src) : source (src) { setRepaintsOnMouseActivity (true); setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    const Palette* palette = nullptr;
    const Modulation* mod = nullptr;
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    int source;
    bool dragging = false;
};

/** MOD 1 (8 steps you draw) and MOD 2 (LFO curve, click to change shape). */
class ModStepsView : public juce::Component, private juce::Timer
{
public:
    ModStepsView (juce::AudioProcessorValueTreeState&, const Modulation&);
    const Palette* palette = nullptr;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { set (e.position); }
    void mouseDrag (const juce::MouseEvent& e) override { set (e.position); }

private:
    void timerCallback() override { repaint(); }
    void set (juce::Point<float>);
    juce::AudioProcessorValueTreeState& state;
    const Modulation& mod;
};

class ModCurveView : public juce::Component, private juce::Timer
{
public:
    ModCurveView (juce::AudioProcessorValueTreeState&, const Modulation&);
    const Palette* palette = nullptr;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    juce::AudioProcessorValueTreeState& state;
    const Modulation& mod;
};

} // namespace nebula
