#pragma once

#include <lt_shared/lt_shared.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace nebula
{

using lt::ui::Palette;

struct GalaxyState
{
    float motion = 0.45f, texture = 0.7f, space = 0.8f, dirt = 0.1f, tapeSpeed = 1.0f, level = 0;
    bool frozen = false;
    int grains = 0, stepFlash = 0;
};

/**
    The galaxy in the centre of LT NEBULA: every grain is a star. It always turns; MOTION = speed,
    TEXTURE = density, SPACE = size, DIRT = sparkle, PITCH STEP = flash, TAPE STOP = slows, FREEZE = frozen.
*/
class GalaxyView : public juce::Component
{
public:
    GalaxyView();
    const Palette* palette = nullptr;
    bool framed = true;
    void tick (const GalaxyState&);
    void paint (juce::Graphics&) override;
    void resized() override { images.clear(); }

private:
    void render();
    struct Flash { float angle, radius, life, size; };
    std::vector<juce::Image> images;   // [0] sparse stars, [1] dense stars, [2] core
    bool renderedLight = false;
    std::vector<Flash> flashes;
    juce::Random random;
    GalaxyState st;
    float rotation = 0, pulse = 0, scale = 1;
    int lastGrains = 0, lastFlash = 0;
};

/** Big glass macro knob with the value inside and the name under it. */
class MacroKnob : public juce::Slider
{
public:
    MacroKnob (const juce::String& label, float diameter = 100.0f);
    const Palette* palette = nullptr;
    juce::String label;
    bool showLabel = true;
    juce::Rectangle<float> knobArea() const;
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;

private:
    float diameter;
};

/** A row of pill buttons bound to one choice parameter, showing only some of its options. */
class ChoiceRow : public juce::Component
{
public:
    ChoiceRow (juce::RangedAudioParameter&, std::vector<int> indices, juce::StringArray labels);
    void resized() override;
    std::function<void (int)> onChoose;   // optional extra action

private:
    juce::RangedAudioParameter& param;
    std::vector<int> indices;
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

} // namespace nebula
