namespace lt::ui
{

void Glass::drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner, const Palette& p,
                       float fillAlpha, bool highlighted)
{
    // body: a faint vertical gradient so it reads as glass rather than a flat box
    juce::ColourGradient body (p.glassFill.withMultipliedAlpha (1.4f * fillAlpha), r.getX(), r.getY(),
                               p.glassFill.withMultipliedAlpha (0.6f * fillAlpha), r.getX(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, corner);

    // top sheen
    auto sheen = r.withHeight (juce::jmin (r.getHeight() * 0.5f, 60.0f));
    juce::ColourGradient top (juce::Colours::white.withAlpha (p.isLight ? 0.18f : 0.06f), sheen.getX(), sheen.getY(),
                              juce::Colours::transparentWhite, sheen.getX(), sheen.getBottom(), false);
    g.setGradientFill (top);
    g.fillRoundedRectangle (sheen, corner);

    if (highlighted)
        drawRectGlow (g, r, corner, p.accent, 10.0f, 0.55f);

    // thin light edge, brighter at the top
    juce::ColourGradient edge (p.glassEdge.withMultipliedAlpha (highlighted ? 1.8f : 1.2f), r.getX(), r.getY(),
                               p.glassEdge.withMultipliedAlpha (0.5f), r.getX(), r.getBottom(), false);
    g.setGradientFill (edge);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, highlighted ? 1.4f : 1.0f);

    if (highlighted)
    {
        g.setColour (p.accent.withAlpha (0.9f));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.4f);
    }
}

void Glass::drawPill (juce::Graphics& g, juce::Rectangle<float> r, const Palette& p, bool selected,
                      bool hovered, bool pressed)
{
    auto corner = r.getHeight() * 0.5f;

    if (selected)
    {
        drawRectGlow (g, r, corner, p.accent, 12.0f, p.isLight ? 0.5f : 0.75f);
        juce::ColourGradient fill (p.accent.brighter (0.15f), r.getX(), r.getY(),
                                   p.accent.darker (p.isLight ? 0.02f : 0.25f), r.getX(), r.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, corner);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    }
    else
    {
        drawPanel (g, r, corner, p, hovered ? 1.8f : 1.0f);
    }

    if (pressed)
    {
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (r, corner);
    }
}

void Glass::drawGlow (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour colour, float alpha)
{
    juce::ColourGradient grad (colour.withAlpha (alpha), c.x, c.y,
                               colour.withAlpha (0.0f), c.x + radius, c.y, true);
    grad.addColour (0.35, colour.withAlpha (alpha * 0.45f));
    g.setGradientFill (grad);
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
}

void Glass::drawRectGlow (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour colour,
                          float spread, float alpha)
{
    constexpr int steps = 6;
    for (int i = steps; i >= 1; --i)
    {
        auto t = (float) i / (float) steps;
        g.setColour (colour.withAlpha (alpha * 0.12f * (1.0f - t + 0.15f)));
        g.fillRoundedRectangle (r.expanded (spread * t), corner + spread * t);
    }
}

} // namespace lt::ui
