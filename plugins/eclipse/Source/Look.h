#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <lt_shared/lt_shared.h>
#include "Skins.h"

namespace eclipse::look
{

using lt::ui::Fonts;

/** Text with letter spacing in pixels. */
void spaced (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Font, float px, juce::Justification);

/** Chamfered rectangle (cut corners), the shape of every panel and pad. */
juce::Path chamfer (juce::Rectangle<float>, float cut);

void chassis (juce::Graphics&, const Skin&, juce::Rectangle<float>);
void panel (juce::Graphics&, const Skin&, juce::Rectangle<float>, bool screws = true);
/** The skin's art, faint, inside a panel at this position of the interface. */
void panelArt (juce::Graphics&, const Skin&, juce::Rectangle<float>, juce::Point<int> position);
void lcd (juce::Graphics&, const Skin&, juce::Rectangle<float>, bool grid = true);
void lcdGlass (juce::Graphics&, juce::Rectangle<float>);                      // reflection on top of the content
void key (juce::Graphics&, const Skin&, juce::Rectangle<float>, const juce::String& text, bool on, bool down, bool over,
          bool led = false, float fontSize = 11.5f, juce::Colour onColour = {});
void knob (juce::Graphics&, const Skin&, juce::Rectangle<float>, float value01, bool big, juce::Colour ring);
void fader (juce::Graphics&, const Skin&, juce::Rectangle<float>, float value01);
void meter (juce::Graphics&, const Skin&, juce::Rectangle<float>, float l, float r);
void screw (juce::Graphics&, const Skin&, juce::Point<float>);
void label (juce::Graphics&, const Skin&, const juce::String&, juce::Rectangle<float>, juce::Justification = juce::Justification::centredLeft,
            float size = 10.0f, float spacing = 2.0f, juce::Colour = {});
void dot (juce::Graphics&, juce::Colour, juce::Point<float> centre, float r, bool glow = true);

/** MPC-style rubber pad. lit 0..1 = the light inside (only when hit). */
void pad (juce::Graphics&, const Skin&, juce::Rectangle<float>, bool full, bool selected, float lit, juce::Colour family);

/** A one-shot as min/max columns, filled with the colour. */
void waveform (juce::Graphics&, const juce::AudioBuffer<float>*, juce::Rectangle<float>, juce::Colour, float glow = 0, float gain = 1.0f);

/** The LookAndFeel used by popup menus, text editors and alert windows. */
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void setSkin (const Skin&);
    const Skin* skin = nullptr;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
};

} // namespace eclipse::look
