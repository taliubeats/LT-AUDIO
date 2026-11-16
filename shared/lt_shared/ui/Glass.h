#pragma once

namespace lt::ui
{

/** Drawing helpers for the "almost invisible glass" look. */
struct Glass
{
    /** A frosted, translucent rounded panel with a thin light edge. */
    static void drawPanel (juce::Graphics&, juce::Rectangle<float>, float corner, const Palette&,
                           float fillAlpha = 1.0f, bool highlighted = false);

    /** A pill button. `selected` fills it with the accent and adds a glow. */
    static void drawPill (juce::Graphics&, juce::Rectangle<float>, const Palette&, bool selected,
                          bool hovered, bool pressed);

    /** Soft radial glow (for lights behind knobs, planets, buttons). */
    static void drawGlow (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour, float alpha);

    /** Soft glow following a rounded rectangle. */
    static void drawRectGlow (juce::Graphics&, juce::Rectangle<float>, float corner, juce::Colour, float spread, float alpha);
};

} // namespace lt::ui
