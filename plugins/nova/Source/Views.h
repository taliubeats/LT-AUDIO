#pragma once

#include <lt_shared/lt_shared.h>

namespace nova
{

using lt::ui::Palette;

/** Warm colours used only by NOVA's star and arcs. */
struct Heat
{
    static juce::Colour arc (const Palette& p)      { return p.isLight ? juce::Colour (0xffffc300) : juce::Colour (0xffffa24a); }
    static juce::Colour arcHot (const Palette& p)   { return p.isLight ? juce::Colours::white : juce::Colour (0xffffe2a8); }
    /** Black body colour: 0 = dim red ember .. 1 = white-gold. */
    static juce::Colour star (float t);
};

/** Glass knob with a warm arc and a label (or the value while touched). */
class NovaKnob : public juce::Slider
{
public:
    NovaKnob (const juce::String& label, float diameter);
    const Palette* palette = nullptr;
    juce::String label, leftTag, rightTag;          // e.g. EVEN / ODD under CHARACTER
    juce::Rectangle<float> knobArea() const;
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;

private:
    float diameter;
};

/**
    The DRIVE control: a star that is born as the value grows.
    0 % = a dim ember, 50 % = an orange sun with flares, 100 % = a white-gold supernova
    with plasma loops and particles. It pulses with the audio.
*/
class StarView : public juce::Slider
{
public:
    StarView();
    const Palette* palette = nullptr;
    void tick (float energy);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr float arcRadius = 205.0f;

private:
    static void buildTexture();
    struct Spark { juce::Point<float> pos, vel; float life, size; bool purple; };
    std::vector<Spark> sparks;
    juce::Random random;
    double time = 0;
    float pulse = 0, energy = 0, shown = -1;
};

/** TAPE · TUBE · ... · EXCITER pills. */
class TypeBar : public juce::Component
{
public:
    TypeBar();
    const Palette* palette = nullptr;
    std::function<void (int)> onChange;
    void setSelected (int);
    void resized() override;

private:
    juce::OwnedArray<juce::TextButton> buttons;
};

/** Vertical L/R meter with a dB scale, like the design. */
class DbMeter : public juce::Component
{
public:
    const Palette* palette = nullptr;
    bool showScale = false;
    void setLevels (float l, float r);
    void paint (juce::Graphics&) override;

private:
    float lv[2] {}, pk[2] {};
};

/** The AUTO GAIN pill switch. */
class PillSwitch : public juce::Button
{
public:
    PillSwitch() : juce::Button ("switch") { setClickingTogglesState (true); }
    const Palette* palette = nullptr;
    void paintButton (juce::Graphics&, bool hover, bool down) override;
};

} // namespace nova
