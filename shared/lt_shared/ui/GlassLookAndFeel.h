#pragma once

namespace lt::ui
{

/**
    LookAndFeel for the whole LT family: glass pills, glass text fields, glass knobs
    with glowing arcs, Montserrat everywhere. Call setPalette() when the theme changes.

    Button properties understood:
      "accent" (bool)  -> always drawn as a filled accent button (e.g. ACTIVATE)
      "pill"   (bool)  -> default true
*/
class GlassLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GlassLookAndFeel();

    void setPalette (const Palette&);
    const Palette& getPalette() const noexcept { return palette; }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hover, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool hover, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hover, bool down) override;

    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    int getDefaultScrollbarWidth() override { return 8; }

    /** Shared knob drawing so custom components can reuse it. */
    static void drawGlassKnob (juce::Graphics&, juce::Rectangle<float> bounds, float proportion,
                               const Palette&, float glow = 1.0f, float startAngle = juce::MathConstants<float>::pi * 1.25f,
                               float endAngle = juce::MathConstants<float>::pi * 2.75f);

private:
    void applyColours();
    Palette palette = Palette::dark();
};

} // namespace lt::ui
