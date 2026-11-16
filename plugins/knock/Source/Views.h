#pragma once

#include <lt_shared/lt_shared.h>

namespace knock
{

using lt::ui::Palette;

class KnockKnob : public juce::Slider
{
public:
    KnockKnob (const juce::String& label, float diameter);
    const Palette* palette = nullptr;
    juce::String label;
    juce::Rectangle<float> knobArea() const;
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;

private:
    float diameter;
};

/**
    The central KNOCK control (dry / wet of everything). The planet is born as the value grows:
    empty sphere (0 %) -> nebula (25 %) -> planet (50 %) -> planet with a ring (75 %) -> lit and beating (100 %).
*/
class PlanetView : public juce::Slider
{
public:
    PlanetView();
    const Palette* palette = nullptr;
    void tick (float level, int hits);
    void burst();
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;

private:
    static void buildTexture();
    struct Dust { float angle, radius, speed, size, life; juce::Point<float> vel; bool free; };
    std::vector<Dust> dust;
    juce::Random random;
    double time = 0;
    float spin = 0, pulse = 0;
    int lastHits = 0;
};

/** SOFT / HARD / FUZZ / TUBE radio list. */
class DistTypeList : public juce::Component
{
public:
    const Palette* palette = nullptr;
    int selected = 0;
    std::function<void (int)> onChange;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
};

/** Small padlock for the CHAOS locks. */
class LockButton : public juce::Button
{
public:
    LockButton() : juce::Button ("lock") { setClickingTogglesState (true); setTooltip ("Lock: CHAOS will not change this section"); }
    const Palette* palette = nullptr;
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

class StereoMeter : public juce::Component
{
public:
    const Palette* palette = nullptr;
    void setLevels (float l, float r);
    void paint (juce::Graphics&) override;

private:
    float lv[2] {}, pk[2] {};
};

} // namespace knock
