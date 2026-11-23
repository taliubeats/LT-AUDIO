#include "Views.h"

namespace halo
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    const juce::Colour lilac (0xffc9b6ff), white = juce::Colours::white;

    juce::Colour textCol (const Palette& p) { return p.isLight ? juce::Colour (0xff111111) : p.text; }

    void drawPlanet (juce::Graphics& g, juce::Point<float> c, float r, const Palette&, float glow, float spin, int seed,
                     bool lit = false)
    {
        auto disc = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
        juce::ColourGradient body (lit ? juce::Colour (0xfff3ecff) : juce::Colour (0xff4a4458), c.x - r * 0.5f, c.y - r * 0.6f,
                                   lit ? juce::Colour (0xff8b5cf6) : juce::Colour (0xff0d0b14), c.x + r * 0.6f, c.y + r * 0.8f, true);
        g.setGradientFill (body);
        g.fillEllipse (disc);
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addEllipse (disc);
            g.reduceClipRegion (clip);
            juce::Random rnd (seed + 11);
            for (int i = 0; i < 5; ++i)
            {
                auto y = c.y - r + r * 2.0f * (0.18f + 0.16f * (float) i) + std::sin (spin * 2.0f + (float) i) * r * 0.04f;
                g.setColour ((lit ? white : juce::Colour (0xff8a80a8)).withAlpha (0.06f + 0.04f * rnd.nextFloat()));
                g.fillEllipse (c.x - r * 1.2f + std::sin (spin + (float) i) * r * 0.1f, y - r * 0.07f, r * 2.4f, r * 0.14f);
            }
            for (int i = 0; i < 6; ++i)   // craters
            {
                auto cr = r * (0.06f + 0.08f * rnd.nextFloat());
                auto pt = c.translated ((rnd.nextFloat() - 0.5f) * r * 1.3f, (rnd.nextFloat() - 0.5f) * r * 1.3f);
                g.setColour (juce::Colours::black.withAlpha (lit ? 0.06f : 0.18f));
                g.fillEllipse (juce::Rectangle<float> (cr * 2, cr * 2).withCentre (pt));
            }
        }
        g.setColour (white.withAlpha (0.22f + 0.3f * glow));
        g.drawEllipse (disc.reduced (0.5f), 1.2f);
        auto hl = disc.reduced (r * 0.3f).withTrimmedBottom (r * 0.75f);
        juce::ColourGradient sheen (white.withAlpha (lit ? 0.45f : 0.16f), hl.getCentreX(), hl.getY(),
                                    juce::Colours::transparentWhite, hl.getCentreX(), hl.getBottom(), false);
        g.setGradientFill (sheen);
        g.fillEllipse (hl);
    }

    juce::Path ringPath (juce::Point<float> c, float rx, float ry, float tilt, bool front)
    {
        juce::Path ring;
        auto start = front ? 0.0f : pi, end = front ? pi : 2.0f * pi;
        for (int i = 0; i <= 48; ++i)
        {
            auto t = start + (end - start) * (float) i / 48.0f;
            juce::Point<float> pt (rx * std::cos (t), ry * std::sin (t));
            if (i == 0) ring.startNewSubPath (pt); else ring.lineTo (pt);
        }
        ring.applyTransform (juce::AffineTransform::rotation (tilt).translated (c.x, c.y));
        return ring;
    }

    void strokeGlow (juce::Graphics& g, const juce::Path& path, juce::Colour c, float width, float alpha)
    {
        g.setColour (c.withAlpha (alpha * 0.3f));
        g.strokePath (path, juce::PathStrokeType (width * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.brighter (0.4f).withAlpha (alpha));
        g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

juce::Colour glowColour (const Palette& p) { return p.isLight ? juce::Colour (0xffffd60a) : p.accent; }

// ================================================================== icons

void IconButton::drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour col, float th)
{
    auto c = r.getCentre();
    auto s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    juce::Path path;
    juce::PathStrokeType stroke (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (col);
    switch (icon)
    {
        case Icon::undo:
        case Icon::redo:
        {
            auto dir = icon == Icon::undo ? 1.0f : -1.0f;
            path.addCentredArc (c.x, c.y + s * 0.1f, s * 0.62f, s * 0.55f, 0.0f, dir > 0 ? -pi * 0.55f : pi * 0.55f,
                                dir > 0 ? pi * 0.6f : -pi * 0.6f, true);
            g.strokePath (path, stroke);
            auto tip = juce::Point<float> (c.x - dir * s * 0.55f, c.y - s * 0.2f);
            juce::Path head;
            head.startNewSubPath (tip.translated (dir * s * 0.05f, -s * 0.42f));
            head.lineTo (tip);
            head.lineTo (tip.translated (dir * s * 0.42f, -s * 0.05f));
            g.strokePath (head, stroke);
            break;
        }
        case Icon::regen:
        {
            path.addCentredArc (c.x, c.y, s * 0.6f, s * 0.6f, 0.0f, -pi * 0.35f, pi * 1.35f, true);
            g.strokePath (path, stroke);
            auto tip = c.getPointOnCircumference (s * 0.6f, -pi * 0.35f);
            juce::Path head;
            head.startNewSubPath (tip.translated (-s * 0.38f, -s * 0.1f));
            head.lineTo (tip);
            head.lineTo (tip.translated (s * 0.02f, -s * 0.4f));
            g.strokePath (head, stroke);
            break;
        }
        case Icon::settings:
        {
            const int teeth = 8;
            for (int i = 0; i < teeth * 2; ++i)
            {
                auto a = (float) i / (float) (teeth * 2) * 2.0f * pi;
                auto rad = (i % 2 == 0) ? s * 0.7f : s * 0.52f;
                auto p1 = c.getPointOnCircumference (rad, a), p2 = c.getPointOnCircumference (rad, a + pi / (float) teeth);
                if (i == 0) path.startNewSubPath (p1); else path.lineTo (p1);
                path.lineTo (p2);
            }
            path.closeSubPath();
            g.strokePath (path, juce::PathStrokeType (th * 0.8f, juce::PathStrokeType::mitered));
            g.drawEllipse (c.x - s * 0.22f, c.y - s * 0.22f, s * 0.44f, s * 0.44f, th * 0.8f);
            break;
        }
        case Icon::power:
            path.addCentredArc (c.x, c.y + s * 0.05f, s * 0.6f, s * 0.6f, 0.0f, pi * 0.2f, pi * 1.8f, true);
            g.strokePath (path, stroke);
            g.drawLine (c.x, c.y - s * 0.7f, c.x, c.y - s * 0.05f, th);
            break;
        case Icon::prev:
        case Icon::next:
        {
            auto d = icon == Icon::prev ? -1.0f : 1.0f;
            path.startNewSubPath (c.x - d * s * 0.2f, c.y - s * 0.45f);
            path.lineTo (c.x + d * s * 0.25f, c.y);
            path.lineTo (c.x - d * s * 0.2f, c.y + s * 0.45f);
            g.strokePath (path, stroke);
            break;
        }
        case Icon::play:
            path.addTriangle (c.x - s * 0.38f, c.y - s * 0.55f, c.x - s * 0.38f, c.y + s * 0.55f, c.x + s * 0.55f, c.y);
            g.fillPath (path);
            break;
        case Icon::stop:
            g.fillRoundedRectangle (juce::Rectangle<float> (s * 0.95f, s * 0.95f).withCentre (c), s * 0.12f);
            break;
        case Icon::drag:
        {
            // small audio file with a wave
            auto f = juce::Rectangle<float> (s * 1.05f, s * 1.35f).withCentre (c);
            juce::Path doc;
            doc.startNewSubPath (f.getX(), f.getY());
            doc.lineTo (f.getRight() - s * 0.3f, f.getY());
            doc.lineTo (f.getRight(), f.getY() + s * 0.3f);
            doc.lineTo (f.getRight(), f.getBottom());
            doc.lineTo (f.getX(), f.getBottom());
            doc.closeSubPath();
            g.strokePath (doc, juce::PathStrokeType (th * 0.8f));
            juce::Path wave;
            for (int i = 0; i <= 12; ++i)
            {
                auto t = (float) i / 12.0f;
                auto pt = juce::Point<float> (f.getX() + f.getWidth() * (0.18f + 0.64f * t),
                                              f.getCentreY() + std::sin (t * 10.0f) * s * 0.22f * std::sin (t * pi));
                if (i == 0) wave.startNewSubPath (pt); else wave.lineTo (pt);
            }
            g.strokePath (wave, juce::PathStrokeType (th * 0.7f));
            break;
        }
        case Icon::close:
            g.drawLine (c.x - s * 0.45f, c.y - s * 0.45f, c.x + s * 0.45f, c.y + s * 0.45f, th);
            g.drawLine (c.x + s * 0.45f, c.y - s * 0.45f, c.x - s * 0.45f, c.y + s * 0.45f, th);
            break;
        case Icon::mic:
            g.drawRoundedRectangle (juce::Rectangle<float> (s * 0.55f, s * 0.95f).withCentre (c.translated (0, -s * 0.2f)), s * 0.27f, th);
            path.addCentredArc (c.x, c.y - s * 0.05f, s * 0.5f, s * 0.5f, 0.0f, pi * 0.55f, pi * 1.45f, true);
            g.strokePath (path, stroke);
            g.drawLine (c.x, c.y + s * 0.45f, c.x, c.y + s * 0.7f, th);
            break;
    }
}

void IconButton::paintButton (juce::Graphics& g, bool hover, bool down)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    if (round)
    {
        auto d = juce::jmin (b.getWidth(), b.getHeight());
        auto disc = b.withSizeKeepingCentre (d, d);
        if (active) Glass::drawGlow (g, disc.getCentre(), d * 0.8f, glowColour (p), 0.45f);
        Glass::drawPanel (g, disc, d * 0.5f, p, hover ? 2.0f : 1.2f, active);
        if (down) { g.setColour (juce::Colours::black.withAlpha (0.12f)); g.fillEllipse (disc); }
        b = disc.reduced (d * 0.27f);
    }
    auto col = white;
    if (! isEnabled()) col = col.withAlpha (0.3f);
    else if (! hover) col = col.withAlpha (0.8f);
    drawIcon (g, icon, b, col, 1.6f);
}

// ================================================================== knobs

PlanetKnob::PlanetKnob (juce::String l, int s) : label (std::move (l)), seed (s)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    setVelocityBasedMode (false);
    setMouseDragSensitivity (220);
}

void PlanetKnob::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    auto labelH = juce::jmax (16.0f, b.getHeight() * 0.16f);
    auto area = b.withTrimmedBottom (labelH);
    auto r = juce::jmin (area.getWidth() * 0.27f, area.getHeight() * 0.36f);
    auto c = area.getCentre();
    auto norm = (float) valueToProportionOfLength (getValue());
    auto glowC = glowColour (p);
    auto hot = isMouseOverOrDragging() ? 1.0f : 0.0f;

    Glass::drawGlow (g, c, r * 2.0f, glowC, 0.18f + 0.25f * norm + 0.1f * hot);
    const float tilt = -0.22f, rx = r * 1.75f, ry = r * 0.5f;
    auto ringW = juce::jmax (1.6f, r * 0.07f);
    strokeGlow (g, ringPath (c, rx, ry, tilt, false), glowC, ringW, 0.35f + 0.6f * norm);
    drawPlanet (g, c, r, p, norm, (float) seed, seed);
    strokeGlow (g, ringPath (c, rx, ry, tilt, true), glowC, ringW, 0.35f + 0.6f * norm);

    // value arc on the ring (front part brighter as value grows) + value text while interacting
    if (hot > 0.0f)
    {
        auto txt = formatValue ? formatValue (getValue()) : getTextFromValue (getValue());
        g.setColour (white);
        g.setFont (Fonts::regular (juce::jmax (11.0f, r * 0.42f)));
        g.drawText (txt, juce::Rectangle<float> (r * 2.4f, r).withCentre (c), juce::Justification::centred);
    }
    g.setColour (textCol (p));
    Fonts::drawSpaced (g, label, b.withTop (b.getBottom() - labelH - 2.0f), Fonts::regular (juce::jmin (16.0f, labelH * 0.9f)),
                       0.03f, juce::Justification::centred);
}

SmallKnob::SmallKnob (juce::String l) : label (std::move (l))
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    setMouseDragSensitivity (180);
}

void SmallKnob::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    auto knob = b.withTrimmedBottom (12.0f);
    auto d = juce::jmin (knob.getWidth(), knob.getHeight());
    auto kb = knob.withSizeKeepingCentre (d, d);
    auto norm = (float) valueToProportionOfLength (getValue());
    lt::ui::GlassLookAndFeel::drawGlassKnob (g, kb, norm, p, isMouseOverOrDragging() ? 1.3f : 1.0f);
    if (isMouseOverOrDragging())
    {
        auto txt = formatValue ? formatValue (getValue()) : getTextFromValue (getValue());
        g.setColour (white);
        g.setFont (Fonts::regular (9.5f));
        g.drawText (txt, kb.expanded (14, 0), juce::Justification::centred);
    }
    g.setColour (p.textDim);
    g.setFont (Fonts::regular (8.5f));
    g.drawText (label, b.withTop (b.getBottom() - 11.0f), juce::Justification::centred);
}

// ================================================================== switches

void SegmentSwitch::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    auto font = Fonts::regular (juce::jmin (16.0f, b.getHeight() * 0.42f));
    if (pillStyle)
    {
        Glass::drawPanel (g, b, b.getHeight() * 0.3f, p, 0.9f);
        auto half = b.getWidth() / 2.0f;
        auto sel = juce::Rectangle<float> (b.getX() + (float) selected * half, b.getY(), half, b.getHeight()).reduced (3.0f);
        Glass::drawRectGlow (g, sel, sel.getHeight() * 0.25f, glowColour (p), 8.0f, 0.35f);
        g.setColour (white.withAlpha (0.1f));
        g.fillRoundedRectangle (sel, sel.getHeight() * 0.25f);
        g.setColour (white.withAlpha (0.35f));
        g.drawRoundedRectangle (sel, sel.getHeight() * 0.25f, 1.0f);
        g.setFont (font.boldened());
        for (int i = 0; i < 2; ++i)
        {
            auto area = juce::Rectangle<float> (b.getX() + (float) i * half, b.getY(), half, b.getHeight());
            g.setColour (i == selected ? white : p.textDim.withAlpha (0.5f));
            g.drawText (i == 0 ? left : right, area, juce::Justification::centred);
        }
        return;
    }
    // text style: "LEFT | RIGHT" with the selected side lit in the accent colour
    g.setFont (font);
    auto wl = juce::GlyphArrangement::getStringWidth (font, left), wr = juce::GlyphArrangement::getStringWidth (font, right);
    auto total = wl + wr + 22.0f;
    auto x = b.getCentreX() - total / 2.0f;
    auto enabledSide = [&] (int s) { return ! isEnabledSide || isEnabledSide (s); };
    g.setColour (selected == 0 ? lilac : (enabledSide (0) ? p.text : p.textDim.withAlpha (0.35f)));
    g.drawText (left, juce::Rectangle<float> (x, b.getY(), wl + 2, b.getHeight()), juce::Justification::centredLeft);
    g.setColour (p.textDim);
    g.drawText ("|", juce::Rectangle<float> (x + wl, b.getY(), 22.0f, b.getHeight()), juce::Justification::centred);
    g.setColour (selected == 1 ? lilac : (enabledSide (1) ? p.text : p.textDim.withAlpha (0.35f)));
    g.drawText (right, juce::Rectangle<float> (x + wl + 22.0f, b.getY(), wr + 2, b.getHeight()), juce::Justification::centredLeft);
}

void SegmentSwitch::mouseUp (const juce::MouseEvent& e)
{
    int side = e.position.x < (float) getWidth() * 0.5f ? 0 : 1;
    if (isEnabledSide && ! isEnabledSide (side)) return;
    setSelected (side);
    if (onChange) onChange (side);
}

void GlassSwitch::paintButton (juce::Graphics& g, bool hover, bool)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    auto h = juce::jmin (b.getHeight(), b.getWidth() * 0.55f);
    b = b.withSizeKeepingCentre (h * 1.85f, h);
    auto on = getToggleState();
    auto glowC = glowColour (p);
    if (on)
    {
        Glass::drawRectGlow (g, b, h * 0.5f, glowC, 6.0f, 0.45f);
        g.setColour (glowC.withAlpha (0.55f));
        g.fillRoundedRectangle (b, h * 0.5f);
    }
    else Glass::drawPanel (g, b, h * 0.5f, p, hover ? 2.0f : 1.4f);
    g.setColour (white.withAlpha (0.45f));
    g.drawRoundedRectangle (b.reduced (0.5f), h * 0.5f, 1.0f);
    auto knob = juce::Rectangle<float> (h - 4.0f, h - 4.0f).withCentre ({ on ? b.getRight() - h * 0.5f : b.getX() + h * 0.5f, b.getCentreY() });
    juce::ColourGradient kg (white, knob.getX(), knob.getY(), on ? glowC.brighter (0.6f) : juce::Colour (0xffbdb7cc),
                             knob.getX(), knob.getBottom(), false);
    g.setGradientFill (kg);
    g.fillEllipse (knob);
}

void HMeter::setLevel (float l)
{
    level = juce::jmax (l, level * 0.85f);
    peak = juce::jmax (l, peak * 0.985f);
    repaint();
}

void HMeter::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    auto frac = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (level, -60.0f) + 60.0f) / 60.0f);
    auto pk = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (peak, -60.0f) + 60.0f) / 60.0f);
    const int ticks = 24;
    auto w = b.getWidth() / (float) ticks;
    for (int i = 0; i < ticks; ++i)
    {
        auto t = (float) i / (float) (ticks - 1);
        auto on = t <= frac;
        auto c = on ? (t > 0.92f ? p.danger : juce::Colour (0xffb794ff)) : p.text.withAlpha (0.14f);
        g.setColour (c);
        g.fillRoundedRectangle (b.getX() + w * (float) i, b.getY(), juce::jmax (1.5f, w * 0.62f), b.getHeight(), 1.0f);
    }
    g.setColour (white);
    g.fillRect (b.getX() + pk * (b.getWidth() - 2.0f), b.getY() - 1.0f, 2.0f, b.getHeight() + 2.0f);
}

void ValueBox::paintButton (juce::Graphics& g, bool hover, bool down)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    Glass::drawPanel (g, b, 9.0f, p, hover ? 1.6f : 1.0f);
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.1f)); g.fillRoundedRectangle (b, 9.0f); }
    auto inner = b.reduced (12.0f, 4.0f);
    if (caption.isNotEmpty())
    {
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (10.5f));
        g.drawText (caption, inner.removeFromTop (inner.getHeight() * 0.42f), juce::Justification::bottomLeft);
    }
    g.setColour (p.text);
    g.setFont (Fonts::regular (caption.isNotEmpty() ? 15.5f : 17.0f));
    g.drawFittedText (value, inner.toNearestInt(), caption.isNotEmpty() ? juce::Justification::centredLeft : juce::Justification::centred, 1, 0.8f);
}

void GenerateButton::paintButton (juce::Graphics& g, bool hover, bool down)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    auto corner = b.getHeight() * 0.5f;
    auto glowC = glowColour (p);
    auto pulse = attention ? 0.5f + 0.5f * std::sin (time * 3.0f) : 0.0f;
    Glass::drawRectGlow (g, b, corner, glowC, 14.0f + 6.0f * pulse, (hover ? 0.75f : 0.55f) + 0.25f * pulse);
    juce::ColourGradient fill (juce::Colour (0xff8f4dff), b.getX(), b.getY(), juce::Colour (0xff5b16d6), b.getX(), b.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (b, corner);
    if (busy && progress > 0.0f)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (b.withWidth (b.getWidth() * progress).toNearestInt());
        g.setColour (white.withAlpha (0.16f));
        g.fillRoundedRectangle (b, corner);
    }
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.15f)); g.fillRoundedRectangle (b, corner); }
    g.setColour (white.withAlpha (0.55f));
    g.drawRoundedRectangle (b.reduced (0.5f), corner, 1.2f);
    g.setColour (white);
    Fonts::drawSpaced (g, busy ? "CANCEL" : getButtonText(), b, Fonts::regular (juce::jmin (24.0f, b.getHeight() * 0.42f)), 0.03f,
                       juce::Justification::centred);
}

// ================================================================== drag handle

void DragHandle::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    auto hot = isMouseOver() || dragging;
    auto col = hot ? white : p.text.withAlpha (0.8f);
    if (compact)
    {
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (8.5f));
        g.drawText (label, b.withTrimmedRight (b.getHeight()), juce::Justification::centredRight);
        IconButton::drawIcon (g, IconButton::Icon::drag, b.removeFromRight (b.getHeight()).reduced (2.0f), col, 1.1f);
        return;
    }
    Glass::drawPanel (g, b.reduced (1.0f), 8.0f, p, hot ? 1.8f : 1.0f, hot);
    auto icon = b.removeFromLeft (b.getHeight()).reduced (b.getHeight() * 0.24f);
    IconButton::drawIcon (g, IconButton::Icon::drag, icon, col, 1.4f);
    g.setColour (col);
    g.setFont (Fonts::regular (juce::jmin (14.0f, b.getHeight() * 0.34f)));
    g.drawFittedText (label, b.withTrimmedRight (4.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
}

void DragHandle::mouseDown (const juce::MouseEvent&) { dragging = false; }

void DragHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6 || ! makeFile) return;
    dragging = true;
    repaint();
    auto f = makeFile();
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
                                                                    [safe = juce::Component::SafePointer<DragHandle> (this)]
                                                                    { if (safe) { safe->dragging = false; safe->repaint(); } });
    else
        dragging = false;
}

void DragHandle::mouseUp (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() < 6 && onClickNoDrag) onClickNoDrag();
    dragging = false;
    repaint();
}

// ================================================================== waveform + lyrics

void WaveView::setData (std::shared_ptr<const Analysis> a, std::vector<Word> w, std::vector<Region> r)
{
    if (a != an) image = {};
    an = std::move (a);
    words = std::move (w);
    regions = std::move (r);
    repaint();
}

void WaveView::setPlayhead (double s, bool isPlaying)
{
    if (std::abs (s - playhead) > 1e-3 || isPlaying != playing) { playhead = s; playing = isPlaying; repaint(); }
}

void WaveView::tick() { time += 1.0f / 30.0f; if (an == nullptr) repaint(); }

juce::Rectangle<float> WaveView::waveArea() const
{
    auto b = getLocalBounds().toFloat();
    return words.empty() ? b : b.withTrimmedBottom (vertical ? 36.0f : 18.0f);
}

juce::Rectangle<float> WaveView::wordsArea() const
{
    auto b = getLocalBounds().toFloat();
    return b.withTop (b.getBottom() - (vertical ? 34.0f : 17.0f));
}

void WaveView::renderImage()
{
    auto area = waveArea();
    auto w = juce::jmax (1, (int) area.getWidth()), h = juce::jmax (1, (int) area.getHeight());
    image = juce::Image (juce::Image::ARGB, w, h, true);
    if (an == nullptr) return;
    juce::Graphics g (image);
    const auto& x = an->audio;
    const int cols = w;
    env.assign ((size_t) cols, 0.0f);
    const size_t per = std::max<size_t> ((size_t) 1, x.size() / (size_t) cols);
    float mx = 1e-6f;
    for (int c = 0; c < cols; ++c)
    {
        double s = 0;
        for (size_t i = (size_t) c * per; i < std::min (x.size(), (size_t) (c + 1) * per); ++i) s = std::max (s, std::abs (x[i]));
        env[(size_t) c] = (float) s;
        mx = juce::jmax (mx, (float) s);
    }
    juce::Random rnd (17);
    const float midY = (float) h * 0.5f;
    for (int c = 0; c < cols; ++c)
    {
        auto amp = std::pow (env[(size_t) c] / mx, 0.7f) * midY * 0.92f;
        int dots = 2 + (int) (amp * 0.5f);
        for (int k = 0; k < dots; ++k)
        {
            auto y = midY + (rnd.nextFloat() * 2.0f - 1.0f) * amp * std::sqrt (rnd.nextFloat());
            auto a = 0.25f + 0.6f * rnd.nextFloat();
            auto col = rnd.nextFloat() < 0.3f ? white : juce::Colour (0xffa074ff).interpolatedWith (juce::Colour (0xffe1d2ff), rnd.nextFloat());
            g.setColour (col.withAlpha (a));
            auto sz = rnd.nextFloat() < 0.04f ? 2.2f : 1.0f;
            g.fillRect ((float) c, y, sz, sz);
        }
    }
}

void WaveView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto area = waveArea();
    if (an == nullptr)
    {
        // empty: a calm dotted line and a hint
        g.setColour (p.textDim.withAlpha (0.4f));
        for (float xx = area.getX(); xx < area.getRight(); xx += 6.0f)
        {
            auto y = area.getCentreY() + std::sin (xx * 0.05f + time * 2.0f) * 3.0f;
            g.fillEllipse (xx, y, 1.6f, 1.6f);
        }
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (13.0f));
        g.drawText ("Drop a dry vocal (acapella) anywhere on LT HALO", area, juce::Justification::centred);
        return;
    }
    if (! image.isValid() || image.getWidth() != (int) area.getWidth()) renderImage();
    const double dur = an->seconds();
    auto xOf = [&] (double t) { return area.getX() + (float) (t / dur) * area.getWidth(); };

    // refuerzo regions under the wave
    for (auto& r : regions)
    {
        auto rr = juce::Rectangle<float> (xOf (r.start), area.getY(), xOf (r.end) - xOf (r.start), area.getHeight());
        g.setColour (glowColour (p).withAlpha (0.12f));
        g.fillRoundedRectangle (rr, 4.0f);
    }
    g.drawImageAt (image, (int) area.getX(), (int) area.getY());

    // melody curve + note dots
    const auto& midi = an->midi;
    double lo = 200, hi = 0;
    for (auto& n : an->notes) { lo = juce::jmin (lo, (double) n.midi); hi = juce::jmax (hi, (double) n.midi); }
    if (hi > lo)
    {
        lo -= 2; hi += 2;
        auto yOf = [&] (double m) { return area.getBottom() - (float) ((m - lo) / (hi - lo)) * area.getHeight() * 0.8f - area.getHeight() * 0.1f; };
        juce::Path curve;
        bool open = false;
        const int step = juce::jmax (1, (int) midi.size() / juce::jmax (1, (int) area.getWidth()));
        for (size_t i = 0; i < midi.size(); i += (size_t) step)
        {
            if (midi[i] <= 0) { open = false; continue; }
            auto pt = juce::Point<float> (xOf ((double) i * frameSec), yOf (midi[i]));
            if (! open) { curve.startNewSubPath (pt); open = true; } else curve.lineTo (pt);
        }
        g.setColour (white.withAlpha (0.25f));
        g.strokePath (curve, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved));
        g.setColour (white.withAlpha (0.85f));
        g.strokePath (curve, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved));
        for (auto& n : an->notes)
            if (n.ms() > 160)
            {
                auto c = juce::Point<float> (xOf ((n.start + n.end) * 0.5 * frameSec), yOf (n.mean));
                g.setColour (white);
                g.fillEllipse (juce::Rectangle<float> (3.4f, 3.4f).withCentre (c));
            }
    }

    // playhead
    if (playing || playhead > 0.0)
    {
        auto px = xOf (playhead);
        g.setColour (white.withAlpha (0.85f));
        g.fillRect (px, area.getY(), 1.4f, area.getHeight());
        Glass::drawGlow (g, { px, area.getCentreY() }, 16.0f, glowColour (p), 0.4f);
    }

    // lyrics
    if (! words.empty())
    {
        auto wa = wordsArea();
        g.setFont (Fonts::regular (vertical ? 11.0f : 10.0f));
        float lastRight = -1e9f;
        for (int i = 0; i < (int) words.size(); ++i)
        {
            auto& w = words[(size_t) i];
            auto x0 = xOf (w.start);
            auto tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), w.text) + 4.0f;
            auto inRegion = std::any_of (regions.begin(), regions.end(), [&] (const Region& r) { return w.start >= r.start - 0.02 && w.end <= r.end + 0.05; });
            auto row = vertical && x0 < lastRight ? 1 : 0;
            auto rect = juce::Rectangle<float> (x0, wa.getY() + (float) row * wa.getHeight() * 0.5f, tw, vertical ? wa.getHeight() * 0.5f : wa.getHeight());
            if (row == 0) lastRight = x0 + tw;
            if (inRegion || w.reinforce)
            {
                g.setColour (glowColour (p).withAlpha (w.reinforce ? 0.65f : 0.35f));
                g.fillRoundedRectangle (rect.reduced (0, 1), 3.0f);
            }
            g.setColour (i == hoverWord ? white : (inRegion ? white : p.textDim));
            g.drawText (w.text, rect, juce::Justification::centredLeft, false);
        }
    }
}

int WaveView::wordAt (juce::Point<float> pt) const
{
    if (an == nullptr || words.empty() || ! wordsArea().contains (pt)) return -1;
    auto area = waveArea();
    auto t = (double) ((pt.x - area.getX()) / area.getWidth()) * an->seconds();
    for (int i = 0; i < (int) words.size(); ++i)
        if (t >= words[(size_t) i].start - 0.05 && t <= words[(size_t) i].end + 0.05) return i;
    return -1;
}

void WaveView::mouseMove (const juce::MouseEvent& e)
{
    auto w = wordAt (e.position);
    if (w != hoverWord) { hoverWord = w; repaint(); }
    setMouseCursor (w >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void WaveView::mouseUp (const juce::MouseEvent& e)
{
    if (an == nullptr) return;
    auto w = wordAt (e.position);
    if (w >= 0) { if (onWordClicked) onWordClicked (w); return; }
    auto area = waveArea();
    if (onSeek) onSeek (juce::jlimit (0.0, an->seconds(), (double) ((e.position.x - area.getX()) / area.getWidth()) * an->seconds()));
}

// ================================================================== orbits

juce::Point<float> OrbitView::moonCentre (int i) const
{
    auto b = getLocalBounds().toFloat();
    auto c = b.getCentre();
    // two ellipses: inner (0..3) and outer (4..7), moons drift slowly
    const float radii[] = { 0.33f, 0.33f, 0.33f, 0.33f, 0.45f, 0.45f, 0.45f, 0.45f };
    const float base[] = { 3.75f, 4.55f, 0.4f, 1.6f, 2.6f, 5.3f, 0.0f, 3.25f };
    auto speed = i < 4 ? 0.03f : 0.018f;
    auto a = base[i] + time * speed;
    auto rx = b.getWidth() * radii[i], ry = b.getHeight() * radii[i] * 0.95f;
    return { c.x + rx * std::cos (a), c.y + ry * std::sin (a) * 0.72f };
}

int OrbitView::hitMoon (juce::Point<float> pt) const
{
    for (int i = 0; i < numStrips; ++i)
        if (moons[(size_t) i].on && pt.getDistanceFrom (moonCentre (i)) < 22.0f) return i;
    return -1;
}

void OrbitView::tick (float e)
{
    time += 1.0f / 30.0f;
    energy = energy * 0.85f + e * 0.15f;
    repaint();
}

void OrbitView::mouseMove (const juce::MouseEvent& e)
{
    hover = hitMoon (e.position);
    setMouseCursor (hover >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void OrbitView::mouseUp (const juce::MouseEvent& e)
{
    auto m = hitMoon (e.position);
    if (onMoonClicked) onMoonClicked (m);
}

void OrbitView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    auto c = b.getCentre();
    auto glowC = glowColour (p);

    // orbit ellipses
    for (int k = 0; k < 5; ++k)
    {
        const float rr[] = { 0.24f, 0.33f, 0.39f, 0.45f, 0.5f };
        auto f = rr[k];
        g.setColour (white.withAlpha (k == 1 || k == 3 ? 0.14f : 0.07f));
        g.drawEllipse (juce::Rectangle<float> (b.getWidth() * f * 2.0f, b.getHeight() * f * 0.95f * 0.72f * 2.0f).withCentre (c), 1.0f);
    }

    // beams to active moons
    for (int i = 0; i < numStrips; ++i)
    {
        auto& m = moons[(size_t) i];
        if (! m.on || m.muted) continue;
        auto mc = moonCentre (i);
        juce::Path beam;
        beam.startNewSubPath (c);
        beam.lineTo (mc);
        auto a = (m.valid ? 0.45f : 0.18f) * (soloMoon < 0 || soloMoon == i ? 1.0f : 0.3f);
        g.setColour (lilac.withAlpha (a * 0.35f));
        g.strokePath (beam, juce::PathStrokeType (7.0f));
        g.setColour (white.withAlpha (a));
        g.strokePath (beam, juce::PathStrokeType (1.6f));
    }

    // planet
    auto R = juce::jmin (b.getWidth(), b.getHeight()) * 0.18f;
    Glass::drawGlow (g, c, R * (2.1f + 0.4f * energy), glowC, 0.55f + 0.3f * energy);
    drawPlanet (g, c, R, p, 1.0f, time * 0.2f, 3, true);
    g.setColour (juce::Colour (0xff2a1450));
    g.setFont (Fonts::light (R * 1.15f));
    g.drawText (centreNote, juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c.translated (0, R * 0.04f)), juce::Justification::centred);

    // moons
    for (int i = 0; i < numStrips; ++i)
    {
        auto& m = moons[(size_t) i];
        auto mc = moonCentre (i);
        bool lit = m.on && ! m.muted && (soloMoon < 0 || soloMoon == i);
        float r = (hover == i ? 19.0f : 17.0f);
        if (lit) Glass::drawGlow (g, mc, r * 2.4f, glowC, m.valid ? 0.6f : 0.3f);
        juce::ColourGradient body (lit ? juce::Colour (0xffe9dcff) : juce::Colour (0xff6c6875), mc.x - r * 0.4f, mc.y - r * 0.5f,
                                   lit ? juce::Colour (0xff7b2cff) : juce::Colour (0xff1b1922), mc.x + r * 0.6f, mc.y + r * 0.8f, true);
        g.setGradientFill (body);
        g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (mc));
        g.setColour (white.withAlpha (lit ? 0.6f : 0.2f));
        g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (mc), 1.0f);
        if (m.on && m.dirty)
        {
            g.setColour (juce::Colour (0xffffd60a).withAlpha (0.55f + 0.45f * std::sin (time * 4.0f)));
            g.fillEllipse (juce::Rectangle<float> (6, 6).withCentre (mc.translated (r * 0.75f, -r * 0.75f)));
        }
        g.setColour (lit ? white : p.textDim.withAlpha (0.55f));
        g.setFont (Fonts::regular (15.0f));
        auto above = mc.y < c.y;
        g.drawFittedText (m.on ? m.label : juce::String ("OFF"),
                          juce::Rectangle<float> (120.0f, 34.0f).withCentre (mc.translated (0, above ? -r - 16.0f : r + 18.0f)).toNearestInt(),
                          juce::Justification::centred, 2, 0.85f);
    }
}

// ================================================================== strip

StripView::StripView()
{
    for (auto* c : { &part, &style, &voice }) addAndMakeVisible (c);
    addAndMakeVisible (smart);
    for (auto* k : { &level, &pan, &delay, &humanize }) addAndMakeVisible (k);
    for (auto* b : { &mute, &solo }) { b->setClickingTogglesState (true); addAndMakeVisible (b); }
    addAndMakeVisible (regen);
    addAndMakeVisible (power);
    drag.compact = true;
    addAndMakeVisible (drag);
    regen.setTooltip ("REGEN: otra toma distinta de esta voz");
    power.setTooltip ("Activar / desactivar esta voz");
    drag.setTooltip ("Arrastra esta voz al DAW (clic = exportar)");
}

void StripView::resized()
{
    auto b = getLocalBounds().reduced (8, 8);
    part.setBounds (b.removeFromTop (26));
    b.removeFromTop (6);
    auto sw = b.removeFromTop (16);
    smart.setBounds (sw.removeFromRight (30));
    b.removeFromTop (6);
    style.setBounds (b.removeFromTop (24));
    b.removeFromTop (5);
    voice.setBounds (b.removeFromTop (24));
    b.removeFromTop (6);
    auto row1 = b.removeFromTop (46);
    level.setBounds (row1.removeFromLeft (row1.getWidth() / 2));
    pan.setBounds (row1);
    auto row2 = b.removeFromTop (46);
    delay.setBounds (row2.removeFromLeft (row2.getWidth() / 2));
    humanize.setBounds (row2);
    b.removeFromTop (6);
    auto ms = b.removeFromTop (24);
    mute.setBounds (ms.removeFromLeft (ms.getWidth() / 2).reduced (2, 0));
    solo.setBounds (ms.reduced (2, 0));
    b.removeFromTop (6);
    auto bottom = b.removeFromTop (18);
    power.setBounds (bottom.removeFromLeft (18));
    bottom.removeFromLeft (4);
    regen.setBounds (bottom.removeFromLeft (18));
    drag.setBounds (bottom.removeFromRight (52));
}

void StripView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    Glass::drawPanel (g, b.reduced (1.0f), 10.0f, p, enabledStrip ? 0.9f : 0.4f, valid && ! dirty && enabledStrip);
    if (enabledStrip && dirty)
    {
        g.setColour (juce::Colour (0xffffd60a).withAlpha (0.5f));
        g.drawRoundedRectangle (b.reduced (1.5f), 10.0f, 1.0f);
    }
    g.setColour (p.textDim);
    g.setFont (Fonts::regular (8.5f));
    g.drawText ("SMART / FIXED", juce::Rectangle<float> (9.0f, 40.0f, b.getWidth() - 44.0f, 16.0f), juce::Justification::centredLeft);
    // separator above the bottom row
    g.setColour (white.withAlpha (0.08f));
    g.drawHorizontalLine (getHeight() - 30, 8.0f, b.getWidth() - 8.0f);
}

void StripView::paintOverChildren (juce::Graphics& g)
{
    if (enabledStrip) return;
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (b.reduced (2.0f).withTrimmedTop (36.0f).withTrimmedBottom (30.0f), 8.0f);
}

// ================================================================== choir

void ChoirCluster::tick (float e)
{
    time += 1.0f / 30.0f;
    energy = energy * 0.85f + e * 0.15f;
    shown += ((float) voices - shown) * 0.15f;
    repaint();
}

void ChoirCluster::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    auto c = b.getCentre();
    auto R = juce::jmin (b.getWidth(), b.getHeight()) * 0.45f;
    Glass::drawGlow (g, c, R * 0.9f, glowColour (p), 0.35f + 0.25f * energy);
    // background dust
    juce::Random dust (5);
    for (int i = 0; i < 260; ++i)
    {
        auto a = dust.nextFloat() * 2 * pi + time * 0.02f;
        auto rr = R * std::sqrt (dust.nextFloat());
        auto pt = c + juce::Point<float> (std::cos (a), std::sin (a)) * rr;
        g.setColour (white.withAlpha (0.12f + 0.2f * dust.nextFloat()));
        g.fillEllipse (pt.x, pt.y, 1.2f, 1.2f);
    }
    // singers: bright twinkling stars, a spiral cluster
    juce::Random rnd (99);
    const int n = juce::roundToInt (shown);
    for (int i = 0; i < 64; ++i)
    {
        auto a = rnd.nextFloat() * 2 * pi;
        auto rr = R * 0.85f * std::pow (rnd.nextFloat(), 0.7f);
        auto phase = rnd.nextFloat() * 6.0f;
        if (i >= n) continue;
        auto spin = time * (0.05f + 0.1f * (1.0f - rr / R));
        auto pt = c + juce::Point<float> (std::cos (a + spin), std::sin (a + spin) * 0.8f) * rr;
        auto tw = 0.6f + 0.4f * std::sin (time * 2.5f + phase) + energy * 0.4f;
        auto s = 2.2f + 2.6f * tw;
        Glass::drawGlow (g, pt, s * 3.0f, lilac, 0.25f * tw);
        g.setColour (white.withAlpha (juce::jlimit (0.3f, 1.0f, tw)));
        g.fillEllipse (juce::Rectangle<float> (s, s).withCentre (pt));
    }
    g.setColour (white);
    g.fillEllipse (juce::Rectangle<float> (7, 7).withCentre (c));
    Glass::drawGlow (g, c, 26.0f, white, 0.5f);
}

juce::Rectangle<float> ChoirTypeCards::card (int i) const
{
    auto w = (float) getWidth() / 5.0f;
    return juce::Rectangle<float> ((float) i * w, 0.0f, w, (float) getHeight()).reduced (4.0f, 0.0f);
}

void ChoirTypeCards::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < 5; ++i) if (card (i).contains (e.position)) h = i;
    if (h != hover) { hover = h; repaint(); }
}

void ChoirTypeCards::mouseUp (const juce::MouseEvent& e)
{
    for (int i = 0; i < 5; ++i)
        if (card (i).contains (e.position))
        {
            selected = i;
            if (onSelect) onSelect (i);
            repaint();
        }
}

void ChoirTypeCards::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    for (int i = 0; i < 5; ++i)
    {
        auto r = card (i);
        bool sel = i == selected;
        if (sel)
        {
            Glass::drawRectGlow (g, r, 8.0f, glowColour (p), 10.0f, 0.6f);
            juce::ColourGradient fill (juce::Colour (0xff8f4dff), r.getX(), r.getY(), juce::Colour (0xff5b16d6), r.getX(), r.getBottom(), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (r, 8.0f);
        }
        else Glass::drawPanel (g, r, 8.0f, p, hover == i ? 1.8f : 1.0f);
        auto icon = r.withTrimmedBottom (r.getHeight() * 0.38f).reduced (r.getWidth() * 0.28f, r.getHeight() * 0.12f);
        auto ic = icon.getCentre();
        auto s = juce::jmin (icon.getWidth(), icon.getHeight()) * 0.5f;
        juce::PathStrokeType st (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (white.withAlpha (sel ? 1.0f : 0.85f));
        auto person = [&] (juce::Point<float> at, float k)
        {
            g.drawEllipse (juce::Rectangle<float> (s * 0.36f * k, s * 0.36f * k).withCentre (at.translated (0, -s * 0.38f * k)), 1.2f);
            juce::Path body;
            body.addCentredArc (at.x, at.y + s * 0.42f * k, s * 0.34f * k, s * 0.4f * k, 0.0f, -pi * 0.5f, pi * 0.5f, true);
            g.strokePath (body, st);
        };
        juce::Path path;
        switch (i)
        {
            case 0: person (ic.translated (-s * 0.35f, 0), 1.0f); person (ic.translated (s * 0.35f, 0), 1.0f); break;
            case 1:
                path.startNewSubPath (ic.x - s * 0.6f, ic.y + s * 0.7f); path.lineTo (ic.x - s * 0.6f, ic.y - s * 0.05f);
                path.lineTo (ic.x, ic.y - s * 0.55f); path.lineTo (ic.x + s * 0.6f, ic.y - s * 0.05f); path.lineTo (ic.x + s * 0.6f, ic.y + s * 0.7f);
                path.closeSubPath();
                g.strokePath (path, st);
                g.drawLine (ic.x, ic.y - s * 0.55f, ic.x, ic.y - s * 0.95f, 1.2f);
                g.drawLine (ic.x - s * 0.15f, ic.y - s * 0.8f, ic.x + s * 0.15f, ic.y - s * 0.8f, 1.2f);
                g.drawRect (juce::Rectangle<float> (s * 0.3f, s * 0.45f).withCentre (ic.translated (0, s * 0.48f)), 1.0f);
                break;
            case 2: for (int k = -1; k <= 1; ++k) person (ic.translated ((float) k * s * 0.45f, (float) std::abs (k) * s * 0.08f), 0.85f);
                    g.drawLine (ic.x - s * 0.7f, ic.y - s * 0.9f, ic.x - s * 0.5f, ic.y - s * 0.65f, 1.0f);
                    g.drawLine (ic.x + s * 0.7f, ic.y - s * 0.9f, ic.x + s * 0.5f, ic.y - s * 0.65f, 1.0f); break;
            case 3:
                g.drawEllipse (juce::Rectangle<float> (s * 1.7f, s * 0.75f).withCentre (ic.translated (0, s * 0.15f)), 1.3f);
                g.drawEllipse (juce::Rectangle<float> (s * 1.0f, s * 0.36f).withCentre (ic.translated (0, s * 0.15f)), 1.0f);
                for (int k = -2; k <= 2; ++k) g.drawLine (ic.x + (float) k * s * 0.35f, ic.y - s * 0.3f, ic.x + (float) k * s * 0.42f, ic.y - s * 0.7f, 1.0f);
                break;
            case 4:
                person (ic.translated (0, s * 0.1f), 0.95f);
                g.drawEllipse (juce::Rectangle<float> (s * 0.5f, s * 0.14f).withCentre (ic.translated (0, -s * 0.62f)), 1.0f);
                path.startNewSubPath (ic.x - s * 0.15f, ic.y); path.quadraticTo (ic.x - s * 0.9f, ic.y - s * 0.6f, ic.x - s * 0.75f, ic.y + s * 0.35f);
                path.startNewSubPath (ic.x + s * 0.15f, ic.y); path.quadraticTo (ic.x + s * 0.9f, ic.y - s * 0.6f, ic.x + s * 0.75f, ic.y + s * 0.35f);
                g.strokePath (path, st);
                break;
            default: break;
        }
        g.setColour (sel ? white : p.text);
        g.setFont (Fonts::regular (juce::jmin (14.0f, r.getHeight() * 0.18f)));
        g.drawText (choirTypeName ((ChoirType) i), r.withTop (r.getBottom() - r.getHeight() * 0.36f), juce::Justification::centred);
    }
}

SectionSlider::SectionSlider (juce::String n) : name (std::move (n))
{
    setSliderStyle (juce::Slider::LinearVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
}

void SectionSlider::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    auto track = b.withTrimmedBottom (40.0f).withSizeKeepingCentre (14.0f, b.getHeight() - 48.0f);
    Glass::drawPanel (g, track, 7.0f, p, 1.0f);
    auto norm = (float) valueToProportionOfLength (getValue());
    auto fill = track.withTop (track.getBottom() - track.getHeight() * norm).reduced (4.0f, 2.0f);
    g.setColour (lilac.withAlpha (0.55f));
    g.fillRoundedRectangle (fill, 3.0f);
    auto thumbY = track.getBottom() - track.getHeight() * norm;
    auto thumb = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ track.getCentreX(), juce::jlimit (track.getY() + 9.0f, track.getBottom() - 9.0f, thumbY) });
    Glass::drawGlow (g, thumb.getCentre(), 14.0f, glowColour (p), 0.5f);
    juce::ColourGradient kg (white, thumb.getX(), thumb.getY(), juce::Colour (0xffcfc6e6), thumb.getX(), thumb.getBottom(), false);
    g.setGradientFill (kg);
    g.fillEllipse (thumb);
    g.setColour (white);
    g.setFont (Fonts::regular (13.0f));
    g.drawText (juce::String (juce::roundToInt (getValue())) + "%", b.withTop (b.getBottom() - 38.0f).withHeight (18.0f), juce::Justification::centred);
    g.setColour (p.text);
    g.setFont (Fonts::regular (12.0f));
    g.drawText (name, b.withTop (b.getBottom() - 19.0f), juce::Justification::centred);
}

} // namespace halo
