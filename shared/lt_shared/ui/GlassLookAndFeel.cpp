namespace lt::ui
{

GlassLookAndFeel::GlassLookAndFeel()
{
    applyColours();
}

void GlassLookAndFeel::setPalette (const Palette& p)
{
    palette = p;
    applyColours();
}

void GlassLookAndFeel::applyColours()
{
    const auto& p = palette;
    setColour (juce::ResizableWindow::backgroundColourId, p.background);
    setColour (juce::Label::textColourId, p.text);
    setColour (juce::TextButton::textColourOffId, p.text);
    setColour (juce::TextButton::textColourOnId, p.accentText);
    setColour (juce::TextEditor::textColourId, p.isLight ? p.text : juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::outlineColourId, p.glassEdge);
    setColour (juce::TextEditor::focusedOutlineColourId, p.accent);
    setColour (juce::TextEditor::highlightColourId, p.accent.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, p.isLight ? p.text : juce::Colours::white);
    setColour (juce::CaretComponent::caretColourId, p.isLight ? p.text : juce::Colours::white);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, p.text);
    setColour (juce::ComboBox::arrowColourId, p.text);
    setColour (juce::PopupMenu::backgroundColourId, p.background.withAlpha (0.96f));
    setColour (juce::PopupMenu::textColourId, p.isLight ? p.text : juce::Colours::white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.accent.withAlpha (0.4f));
    setColour (juce::PopupMenu::highlightedTextColourId, p.isLight ? p.text : juce::Colours::white);
    setColour (juce::ScrollBar::thumbColourId, p.glassEdge);
    setColour (juce::AlertWindow::backgroundColourId, p.background);
    setColour (juce::AlertWindow::textColourId, p.isLight ? p.text : juce::Colours::white);
    setColour (juce::AlertWindow::outlineColourId, p.glassEdge);
}

void GlassLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                             bool hover, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    auto accent = (bool) b.getProperties().getWithDefault ("accent", false);
    auto selected = accent || b.getToggleState();

    if (accent && ! b.isEnabled())
        selected = false;

    if ((bool) b.getProperties().getWithDefault ("textOnly", false))
    {
        if (hover && b.isEnabled())
            Glass::drawGlow (g, r.getCentre(), r.getWidth() * 0.45f, palette.accent, 0.18f);
        return;
    }

    Glass::drawPill (g, r, palette, selected, hover && b.isEnabled(), down);
}

juce::Font GlassLookAndFeel::getTextButtonFont (juce::TextButton& b, int h)
{
    auto accent = (bool) b.getProperties().getWithDefault ("accent", false);
    if (b.getProperties().contains ("fontSize"))
        return Fonts::regular ((float) b.getProperties()["fontSize"]);
    auto size = juce::jmin ((float) h * 0.42f, accent ? 17.0f : 14.0f);
    return accent ? Fonts::semiBold (size) : Fonts::regular (size);
}

void GlassLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    auto accent = (bool) b.getProperties().getWithDefault ("accent", false);
    auto selected = (accent || b.getToggleState()) && b.isEnabled();

    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour ((selected ? palette.accentText : palette.text).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    if ((bool) b.getProperties().getWithDefault ("chevron", false))
    {
        auto area = b.getLocalBounds().reduced (16, 2);
        auto c = juce::Point<float> ((float) area.getRight() - 6.0f, (float) area.getCentreY());
        juce::Path v;
        v.startNewSubPath (c.x - 5.0f, c.y - 2.5f);
        v.lineTo (c.x, c.y + 2.5f);
        v.lineTo (c.x + 5.0f, c.y - 2.5f);
        g.strokePath (v, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawFittedText (b.getButtonText(), area.withTrimmedRight (20), juce::Justification::centredLeft, 1);
        return;
    }
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (8, 2), juce::Justification::centred, 1);
}

void GlassLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool)
{
    // a small glass switch followed by the label
    auto r = b.getLocalBounds().toFloat();
    auto sw = juce::Rectangle<float> (r.getRight() - 40.0f, r.getCentreY() - 10.0f, 38.0f, 20.0f);
    auto on = b.getToggleState();

    Glass::drawPill (g, sw, palette, on, hover, false);
    auto knob = on ? sw.removeFromRight (20.0f).reduced (3.0f) : sw.removeFromLeft (20.0f).reduced (3.0f);
    g.setColour (on ? palette.accentText : palette.text.withAlpha (0.7f));
    g.fillEllipse (knob);

    g.setColour (palette.text);
    g.setFont (Fonts::regular (13.0f));
    g.drawText (b.getButtonText(), r.withRight (r.getRight() - 46.0f), juce::Justification::centredRight);
}

void GlassLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& ed)
{
    if (ed.isReadOnly() || (bool) ed.getProperties().getWithDefault ("bare", false))
        return; // read-only / bare editors sit inside their own glass boxes

    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    Glass::drawPanel (g, r, 8.0f, palette, ed.hasKeyboardFocus (true) ? 1.6f : 1.0f, ed.hasKeyboardFocus (true));
}

void GlassLookAndFeel::drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&)
{
    // outline already drawn by the glass panel
}

void GlassLookAndFeel::drawGlassKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float pos,
                                      const Palette& p, float glow, float startAngle, float endAngle)
{
    auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto r = bounds.withSizeKeepingCentre (size, size);
    auto c = r.getCentre();
    auto arcRadius = size * 0.5f - 3.0f;
    auto discRadius = arcRadius * 0.78f;
    auto angle = startAngle + pos * (endAngle - startAngle);

    // light behind the knob grows with the value
    Glass::drawGlow (g, c, arcRadius * (1.2f + 0.5f * pos), p.accent, (0.10f + 0.30f * pos) * glow);

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (p.text.withAlpha (p.isLight ? 0.18f : 0.14f));
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc with glow
    if (pos > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (p.accent.withAlpha (0.25f * glow));
        g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (p.accent);
        g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // glass disc
    auto disc = juce::Rectangle<float> (discRadius * 2.0f, discRadius * 2.0f).withCentre (c);
    juce::ColourGradient body (juce::Colours::white.withAlpha (p.isLight ? 0.55f : 0.16f), disc.getX(), disc.getY(),
                               juce::Colours::white.withAlpha (p.isLight ? 0.18f : 0.03f), disc.getRight(), disc.getBottom(), false);
    g.setGradientFill (body);
    g.fillEllipse (disc);

    juce::ColourGradient rim (juce::Colours::white.withAlpha (0.55f), disc.getX(), disc.getY(),
                              juce::Colours::white.withAlpha (0.05f), disc.getX(), disc.getBottom(), false);
    g.setGradientFill (rim);
    g.drawEllipse (disc.reduced (0.5f), 1.0f);

    // highlight
    auto hl = disc.reduced (discRadius * 0.25f).withTrimmedBottom (discRadius * 0.7f);
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (p.isLight ? 0.35f : 0.12f), hl.getCentreX(), hl.getY(),
                                juce::Colours::transparentWhite, hl.getCentreX(), hl.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillEllipse (hl);

    // pointer dot
    auto dotR = juce::jmax (1.8f, size * 0.03f);
    auto dot = c.getPointOnCircumference (discRadius * 0.78f, angle);
    g.setColour (p.isLight ? juce::Colour (0xff111111) : juce::Colours::white);
    g.fillEllipse (dot.x - dotR, dot.y - dotR, dotR * 2.0f, dotR * 2.0f);
}

void GlassLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    drawGlassKnob (g, juce::Rectangle<int> (x, y, w, h).toFloat(), pos, palette, 1.0f, startAngle, endAngle);
}

void GlassLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    Glass::drawPanel (g, r, r.getHeight() * 0.5f, palette, box.isMouseOver (true) ? 1.6f : 1.0f);

    auto arrow = juce::Rectangle<float> ((float) w - 26.0f, (float) h * 0.5f - 3.0f, 10.0f, 6.0f);
    juce::Path p;
    p.startNewSubPath (arrow.getX(), arrow.getY());
    p.lineTo (arrow.getCentreX(), arrow.getBottom());
    p.lineTo (arrow.getRight(), arrow.getY());
    g.setColour (palette.text.withAlpha (0.8f));
    g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font GlassLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return Fonts::regular (juce::jmin (15.0f, (float) box.getHeight() * 0.45f));
}

void GlassLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (14, 1, box.getWidth() - 44, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void GlassLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (palette.background.withAlpha (0.97f));
    g.setColour (palette.glassEdge);
    g.drawRect (0, 0, w, h);
}

juce::Font GlassLookAndFeel::getPopupMenuFont()               { return Fonts::regular (15.0f); }
juce::Font GlassLookAndFeel::getLabelFont (juce::Label& l)    { return Fonts::regular (juce::jmin (15.0f, (float) l.getHeight() * 0.6f)); }

void GlassLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                      int thumbStart, int thumbSize, bool over, bool)
{
    auto thumb = vertical ? juce::Rectangle<int> (x, thumbStart, w, thumbSize)
                          : juce::Rectangle<int> (thumbStart, y, thumbSize, h);
    g.setColour (palette.glassEdge.withMultipliedAlpha (over ? 1.6f : 1.0f));
    g.fillRoundedRectangle (thumb.toFloat().reduced (2.0f), 3.0f);
}

} // namespace lt::ui
