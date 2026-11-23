#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Skins.h"

namespace eclipse::decor
{

/**
    The art of each artist skin (graffiti, palms, roses, flames...), drawn in code.
    One image per skin, the size of the whole interface, made once and cached.
    Neutral skins have none.
*/
const juce::Image& image (const Skin&, int width, int height);

/** Behind the panels (on the chassis). */
void drawChassis (juce::Graphics&, const Skin&, juce::Rectangle<int> content);

/** Inside a panel: the same art, faint, clipped to the panel face. offset = panel position in the interface. */
void drawInPanel (juce::Graphics&, const Skin&, const juce::Path& face, juce::Point<int> offset, float alpha);

} // namespace eclipse::decor
