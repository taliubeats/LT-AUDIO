namespace lt::ui
{

Logo::Spec Logo::specFor (const juce::String& id)
{
    if (id == "TYPEBEATS") return { "TYPE BEATS", Glyph::none, -1, false };
    if (id == "PULSAR")    return { "PULSAR", Glyph::star, 4, true };
    if (id == "KNOCK")     return { "KNOCK", Glyph::orbitRing, 2, false };
    if (id == "NEBULA")    return { "NEBULA", Glyph::galaxy, 3, false };
    if (id == "ORBIT")     return { "ORBIT", Glyph::planetRing, 0, false };
    if (id == "VOID")      return { "VOID", Glyph::blackHole, 1, false };
    if (id == "NOVA")      return { "NOVA", Glyph::star, 1, false };
    if (id == "VOCALSUITE") return { "VOCAL SUITE", Glyph::none, -1, false };
    if (id == "QUASAR")    return { "QUASAR", Glyph::none, -1, false };
    if (id == "TUNE")      return { "TUNE", Glyph::sine, -1, false };
    if (id == "STUDIO")    return { "LICENSE STUDIO", Glyph::key, 1, false };
    if (id == "ALL")       return { "ALL PLUGINS", Glyph::none, -1, false };
    return { id, Glyph::none, -1, false };
}

juce::String Logo::displayNameFor (const juce::String& id)
{
    return "LT " + specFor (id).name;
}

void Logo::drawSymbol (juce::Graphics& g, juce::Rectangle<float> r, Glyph glyph, const Palette& p)
{
    auto c = r.getCentre();
    auto s = juce::jmin (r.getWidth(), r.getHeight());
    auto white = p.isLight ? juce::Colours::white : juce::Colours::white;

    switch (glyph)
    {
        case Glyph::orbitRing:
        case Glyph::planetRing:
        {
            auto planet = s * (glyph == Glyph::planetRing ? 0.30f : 0.36f);
            if (glyph == Glyph::planetRing)
            {
                juce::ColourGradient grad (p.accent.brighter (0.6f), c.x - planet * 0.5f, c.y - planet * 0.5f,
                                           p.accent.darker (0.4f), c.x + planet, c.y + planet, true);
                g.setGradientFill (grad);
                g.fillEllipse (c.x - planet, c.y - planet, planet * 2.0f, planet * 2.0f);
            }
            else
            {
                g.setColour (white);
                g.drawEllipse (c.x - planet, c.y - planet, planet * 2.0f, planet * 2.0f, juce::jmax (1.2f, s * 0.07f));
            }

            juce::Path ring;
            ring.addEllipse (-s * 0.5f, -s * 0.14f, s, s * 0.28f);
            ring.applyTransform (juce::AffineTransform::rotation (-0.45f).translated (c.x, c.y));
            Glass::drawGlow (g, c, s * 0.6f, p.accent, 0.35f);
            g.setColour (p.accent.brighter (0.4f));
            g.strokePath (ring, juce::PathStrokeType (juce::jmax (1.0f, s * 0.05f)));
            break;
        }

        case Glyph::blackHole:
        {
            Glass::drawGlow (g, c, s * 0.75f, p.accent, 0.6f);
            juce::Path disk;
            disk.addEllipse (-s * 0.5f, -s * 0.15f, s, s * 0.3f);
            disk.applyTransform (juce::AffineTransform::rotation (-0.35f).translated (c.x, c.y));
            g.setColour (p.accent.brighter (0.5f));
            g.strokePath (disk, juce::PathStrokeType (juce::jmax (1.4f, s * 0.08f)));
            auto hole = s * 0.30f;
            g.setColour (juce::Colours::black);
            g.fillEllipse (c.x - hole, c.y - hole, hole * 2.0f, hole * 2.0f);
            g.setColour (p.accent.brighter (0.8f).withAlpha (0.9f));
            g.drawEllipse (c.x - hole, c.y - hole, hole * 2.0f, hole * 2.0f, juce::jmax (1.0f, s * 0.04f));
            break;
        }

        case Glyph::star:
        {
            Glass::drawGlow (g, c, s * 0.5f, p.accent, 0.7f);
            juce::Path star;
            auto a = s * 0.5f, b = s * 0.09f;
            star.startNewSubPath (c.x, c.y - a);
            star.quadraticTo (c.x + b, c.y - b, c.x + a, c.y);
            star.quadraticTo (c.x + b, c.y + b, c.x, c.y + a);
            star.quadraticTo (c.x - b, c.y + b, c.x - a, c.y);
            star.quadraticTo (c.x - b, c.y - b, c.x, c.y - a);
            star.closeSubPath();
            g.setColour (white);
            g.fillPath (star);
            break;
        }

        case Glyph::galaxy:
        {
            Glass::drawGlow (g, c, s * 0.6f, p.accent, 0.7f);
            for (int arm = 0; arm < 2; ++arm)
            {
                juce::Path spiral;
                for (int i = 0; i <= 40; ++i)
                {
                    auto t = (float) i / 40.0f;
                    auto ang = t * 4.2f + (float) arm * juce::MathConstants<float>::pi;
                    auto rad = t * s * 0.48f;
                    auto pt = c.getPointOnCircumference (rad, ang);
                    if (i == 0) spiral.startNewSubPath (pt); else spiral.lineTo (pt);
                }
                g.setColour (p.accent.brighter (0.6f));
                g.strokePath (spiral, juce::PathStrokeType (juce::jmax (1.0f, s * 0.06f), juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
            }
            g.setColour (white);
            g.fillEllipse (c.x - s * 0.08f, c.y - s * 0.08f, s * 0.16f, s * 0.16f);
            break;
        }

        case Glyph::key:
        {
            auto ringR = s * 0.2f;
            auto ringC = c.translated (-s * 0.18f, -s * 0.18f);
            g.setColour (white);
            g.drawEllipse (ringC.x - ringR, ringC.y - ringR, ringR * 2.0f, ringR * 2.0f, juce::jmax (1.2f, s * 0.07f));
            juce::Path shaft;
            shaft.startNewSubPath (ringC.getPointOnCircumference (ringR, juce::MathConstants<float>::pi * 0.75f));
            shaft.lineTo (c.translated (s * 0.36f, s * 0.36f));
            shaft.startNewSubPath (c.translated (s * 0.2f, s * 0.2f));
            shaft.lineTo (c.translated (s * 0.33f, s * 0.07f));
            g.strokePath (shaft, juce::PathStrokeType (juce::jmax (1.2f, s * 0.07f)));
            break;
        }

        case Glyph::sine:
        {
            Glass::drawGlow (g, c, s * 0.55f, p.accent, 0.5f);
            juce::Path wave;
            for (int i = 0; i <= 48; ++i)
            {
                auto t = (float) i / 48.0f;
                auto env = std::sin (t * juce::MathConstants<float>::pi);
                auto pt = juce::Point<float> (r.getX() + t * r.getWidth(),
                                              c.y - std::sin (t * juce::MathConstants<float>::twoPi * 1.5f) * env * s * 0.32f);
                if (i == 0) wave.startNewSubPath (pt); else wave.lineTo (pt);
            }
            g.setColour (p.isLight ? juce::Colours::white : p.accent.brighter (0.5f));
            g.strokePath (wave, juce::PathStrokeType (juce::jmax (1.2f, s * 0.07f), juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
            break;
        }

        case Glyph::none:
            break;
    }
}

float Logo::draw (juce::Graphics& g, juce::Rectangle<float> area, const Spec& spec, const Palette& p,
                  juce::Justification just)
{
    auto h = area.getHeight();
    auto ltFont = Fonts::bold (h * 0.9f);
    auto nameFont = spec.boldName ? Fonts::bold (h * 0.9f) : Fonts::light (h * 0.9f);
    if (spec.boldName)
        nameFont.setItalic (true);

    auto widthOf = [] (const juce::Font& f, const juce::String& s)
    {
        return juce::GlyphArrangement::getStringWidth (f, s);
    };

    auto ltW = widthOf (ltFont, "LT");
    auto gap = spec.glyph == Glyph::sine ? h * 0.62f : h * 0.28f;
    auto nameW = widthOf (nameFont, spec.name);
    auto total = ltW + gap + nameW;

    auto x = area.getX();
    if (just.testFlags (juce::Justification::horizontallyCentred)) x = area.getCentreX() - total * 0.5f;
    else if (just.testFlags (juce::Justification::right))          x = area.getRight() - total;

    auto ltArea = juce::Rectangle<float> (x, area.getY(), ltW + 4.0f, h);
    auto ltColour = p.isLight ? juce::Colours::white : p.accent;
    g.setColour (ltColour.withAlpha (0.25f));
    g.setFont (ltFont);
    for (auto d : { -1.5f, 1.5f })
        g.drawText ("LT", ltArea.translated (d, 0), juce::Justification::centredLeft, false);
    g.setColour (ltColour);
    g.drawText ("LT", ltArea, juce::Justification::centredLeft, false);

    if (spec.glyph == Glyph::sine)
        drawSymbol (g, juce::Rectangle<float> (x + ltW + gap * 0.08f, area.getY() + h * 0.2f, gap * 0.84f, h * 0.6f), Glyph::sine, p);

    auto nx = x + ltW + gap;
    auto nameColour = p.isLight ? p.value : juce::Colours::white;
    g.setFont (nameFont);

    for (int i = 0; i < spec.name.length(); ++i)
    {
        auto ch = spec.name.substring (i, i + 1);
        auto cw = widthOf (nameFont, ch);
        auto cell = juce::Rectangle<float> (nx, area.getY(), cw, h);

        auto replaced = i == spec.glyphIndex && ch == "O"
                         && (spec.glyph == Glyph::orbitRing || spec.glyph == Glyph::planetRing || spec.glyph == Glyph::blackHole);

        if (replaced)
        {
            auto sz = juce::jmin (cw * 0.92f, h * 0.62f);
            drawSymbol (g, cell.withSizeKeepingCentre (sz, sz).translated (0, h * 0.04f), spec.glyph, p);
            if (spec.glyph == Glyph::orbitRing)
            {
                g.setColour (nameColour);
                g.drawText (ch, cell, juce::Justification::centredLeft, false);
            }
        }
        else
        {
            if (p.isLight)
            {
                g.setColour (juce::Colour (0x30000000));
                g.drawText (ch, cell.withWidth (cw + 4.0f).translated (0, 1.5f), juce::Justification::centredLeft, false);
            }
            g.setColour (nameColour);
            g.drawText (ch, cell.withWidth (cw + 4.0f), juce::Justification::centredLeft, false);

            if (i == spec.glyphIndex && (spec.glyph == Glyph::star || spec.glyph == Glyph::galaxy || spec.glyph == Glyph::key))
                drawSymbol (g, juce::Rectangle<float> (h * 0.36f, h * 0.36f)
                                   .withCentre ({ cell.getRight() - cw * 0.1f, area.getY() + h * 0.14f }), spec.glyph, p);
        }

        nx += cw;
    }

    return total;
}

void Logo::drawIcon (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& id, const Palette& p, bool selected)
{
    auto corner = r.getWidth() * 0.24f;

    juce::ColourGradient bg (p.nebulaA.withAlpha (0.55f), r.getX(), r.getY(),
                             p.background.brighter (0.05f), r.getRight(), r.getBottom(), false);
    bg.addColour (0.5, p.nebulaB.withAlpha (0.7f));
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, corner);

    g.setColour (selected ? p.accent.brighter (0.3f) : p.accent.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, selected ? 1.6f : 1.0f);

    auto inner = r.reduced (r.getWidth() * 0.2f);
    auto glyph = specFor (id).glyph;

    if (id == "TYPEBEATS")
    {
        juce::Path wave;
        for (int i = 0; i <= 40; ++i)
        {
            auto t = (float) i / 40.0f;
            auto env = std::sin (t * juce::MathConstants<float>::pi);
            auto y = inner.getCentreY() + std::sin (t * 18.0f) * env * inner.getHeight() * 0.38f;
            auto pt = juce::Point<float> (inner.getX() + t * inner.getWidth(), y);
            if (i == 0) wave.startNewSubPath (pt); else wave.lineTo (pt);
        }
        g.setColour (p.accent.brighter (0.7f));
        g.strokePath (wave, juce::PathStrokeType (juce::jmax (1.0f, r.getWidth() * 0.04f)));
    }
    else if (id == "ALL")
    {
        drawSymbol (g, inner, Glyph::star, p);
    }
    else
    {
        drawSymbol (g, inner, glyph == Glyph::orbitRing ? Glyph::planetRing : glyph, p);
    }
}

} // namespace lt::ui
