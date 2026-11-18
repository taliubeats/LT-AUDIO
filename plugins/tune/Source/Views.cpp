#include "Views.h"

namespace tune
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;

    /** Knob arcs: purple in dark mode, yellow in light mode (white arcs vanish on sky blue). */
    Palette knobPalette (const Palette& p)
    {
        auto k = p;
        if (p.isLight) k.accent = juce::Colour (0xffffc300);
        return k;
    }

    juce::Colour glowColour (const Palette& p) { return p.isLight ? juce::Colour (0xffffd60a) : p.accent; }
}

juce::Rectangle<float> drawNoteName (juce::Graphics& g, const juce::String& name, juce::Point<float> c, float h,
                                     juce::Colour colour, float alpha)
{
    auto letter = name.substring (0, 1);
    auto sharp = name.length() > 1;
    auto big = Fonts::light (h);
    auto small = Fonts::light (h * 0.52f);
    auto lw = juce::GlyphArrangement::getStringWidth (big, letter);
    auto sw = sharp ? juce::GlyphArrangement::getStringWidth (small, "#") : 0.0f;
    auto total = lw + sw;
    auto x = c.x - total * 0.5f;
    auto top = c.y - h * 0.5f;

    g.setColour (colour.withMultipliedAlpha (alpha));
    g.setFont (big);
    g.drawText (letter, juce::Rectangle<float> (x, top, lw + 4.0f, h), juce::Justification::centredLeft, false);
    if (sharp)
    {
        g.setFont (small);
        g.drawText ("#", juce::Rectangle<float> (x + lw, top + h * 0.02f, sw + 4.0f, h * 0.52f), juce::Justification::centredLeft, false);
    }
    return { x, top, total, h };
}

// ================================================================== knob

TuneKnob::TuneKnob (const juce::String& l) : label (l)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    setMouseDragSensitivity (180);
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<float> TuneKnob::knobArea() const
{
    auto b = getLocalBounds().toFloat();
    if (compact) return b.withSizeKeepingCentre (juce::jmin (b.getWidth(), b.getHeight()), juce::jmin (b.getWidth(), b.getHeight()));
    auto s = juce::jmin (b.getWidth(), b.getHeight() - 26.0f);
    return juce::Rectangle<float> (s, s).withCentre ({ b.getCentreX(), b.getY() + s * 0.5f });
}

bool TuneKnob::hitTest (int x, int y)
{
    auto k = knobArea();
    return k.getCentre().getDistanceFrom ({ (float) x, (float) y }) < k.getWidth() * 0.6f;
}

void TuneKnob::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto k = knobArea();
    auto kp = knobPalette (p);
    auto pos = (float) valueToProportionOfLength (getValue());

    if (bipolar)
    {
        // arc grows from the top centre
        lt::ui::GlassLookAndFeel::drawGlassKnob (g, k, 0.0f, kp, 0.6f);
        auto c = k.getCentre();
        auto r = k.getWidth() * 0.5f - 3.0f;
        auto a0 = pi * 2.0f, a1 = pi * 1.25f + pos * pi * 1.5f;
        if (std::abs (a1 - a0) > 0.01f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
            g.setColour (kp.accent.withAlpha (0.25f));
            g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (kp.accent);
            g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
    else
    {
        lt::ui::GlassLookAndFeel::drawGlassKnob (g, k, pos, kp, 0.8f);
    }

    if (compact) return;
    auto text = formatValue ? formatValue (getValue()) : getTextFromValue (getValue());
    g.setColour (p.isLight ? juce::Colour (0xff111111) : p.value);
    g.setFont (Fonts::regular (k.getWidth() * 0.21f));
    g.drawFittedText (text, k.reduced (k.getWidth() * 0.24f).translated (k.getWidth() * 0.02f, 0.0f).toNearestInt(), juce::Justification::centred, 1);

    g.setColour (isMouseOverOrDragging() ? (p.isLight ? juce::Colour (0xff111111) : juce::Colours::white) : p.text);
    Fonts::drawSpaced (g, label, getLocalBounds().toFloat().withTop (k.getBottom() + 6.0f).withHeight (20.0f),
                       Fonts::regular (15.0f), 0.03f, juce::Justification::centredTop);
}

// ================================================================== retune planet

RetunePlanet::RetunePlanet (juce::RangedAudioParameter& prm)
    : param (prm), attachment (prm, [this] (float v) { norm = param.convertTo0to1 (v); repaint(); })
{
    attachment.sendInitialUpdate();
    setRepaintsOnMouseActivity (true);
}

void RetunePlanet::tick()
{
    glow *= 0.9f;
    spin += 0.004f + glow * 0.02f;
    repaint();
}

void RetunePlanet::mouseDown (const juce::MouseEvent&)
{
    dragStart = norm;
    attachment.beginGesture();
}

void RetunePlanet::mouseDrag (const juce::MouseEvent& e)
{
    // up / right = towards ROBOT (smaller ms)
    auto delta = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / (e.mods.isShiftDown() ? 900.0f : 260.0f);
    auto n = juce::jlimit (0.0f, 1.0f, dragStart - delta);
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (n));
}

void RetunePlanet::mouseUp (const juce::MouseEvent&)        { attachment.endGesture(); }
void RetunePlanet::mouseDoubleClick (const juce::MouseEvent&) { attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); }

void RetunePlanet::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, norm - w.deltaY * 0.08f)));
}

void RetunePlanet::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    auto c = juce::Point<float> (b.getCentreX(), b.getY() + 108.0f);
    const float planetR = 62.0f, arcR = 80.0f;
    auto glowC = glowColour (p);
    auto pos = 1.0f - norm;   // 0 = NATURAL (left) .. 1 = ROBOT (right)

    // value
    g.setFont (Fonts::regular (19.0f));
    auto valueArea = juce::Rectangle<float> (b.getX(), b.getY() - 2.0f, b.getWidth(), 24.0f);
    if (p.isLight)
    {
        g.setColour (juce::Colour (0x55000000));
        g.drawText (param.getCurrentValueAsText(), valueArea.translated (0.0f, 1.2f), juce::Justification::centred);
    }
    g.setColour (p.isLight ? juce::Colour (0xffffc300) : p.value);
    g.drawText (param.getCurrentValueAsText(), valueArea, juce::Justification::centred);

    Glass::drawGlow (g, c, planetR * (1.7f + 0.4f * glow), glowC, 0.25f + 0.45f * glow);

    // arc NATURAL -> ROBOT
    const float a0 = pi * 1.25f, a1 = pi * 2.75f;
    auto ang = a0 + pos * (a1 - a0);
    juce::Path track, arc;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
    g.setColour (p.text.withAlpha (0.14f));
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.002f)
    {
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, ang, true);
        g.setColour (glowC.withAlpha (0.3f));
        g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (glowC.brighter (0.3f));
        g.strokePath (arc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ring behind the planet
    auto ringPath = [&] (bool front)
    {
        juce::Path ring;
        const float rx = 128.0f, ry = 34.0f;
        auto start = front ? 0.0f : pi, end = front ? pi : 2.0f * pi;
        for (int i = 0; i <= 60; ++i)
        {
            auto t = start + (end - start) * (float) i / 60.0f;
            juce::Point<float> pt (rx * std::cos (t), ry * std::sin (t));
            if (i == 0) ring.startNewSubPath (pt); else ring.lineTo (pt);
        }
        ring.applyTransform (juce::AffineTransform::rotation (-0.28f).translated (c.x, c.y));
        return ring;
    };
    auto strokeRing = [&] (const juce::Path& ring)
    {
        g.setColour (glowC.withAlpha (0.25f + 0.3f * glow));
        g.strokePath (ring, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (glowC.brighter (0.5f).withAlpha (0.9f));
        g.strokePath (ring, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    strokeRing (ringPath (false));

    // planet body
    auto disc = juce::Rectangle<float> (planetR * 2.0f, planetR * 2.0f).withCentre (c);
    if (p.isLight)
    {
        juce::ColourGradient body (juce::Colour (0xfff2fbff), c.x - planetR * 0.5f, c.y - planetR * 0.6f,
                                   juce::Colour (0xff7fc6e8), c.x + planetR * 0.6f, c.y + planetR * 0.8f, true);
        g.setGradientFill (body);
    }
    else
    {
        juce::ColourGradient body (juce::Colour (0xff4a4458), c.x - planetR * 0.5f, c.y - planetR * 0.6f,
                                   juce::Colour (0xff0d0b14), c.x + planetR * 0.6f, c.y + planetR * 0.8f, true);
        g.setGradientFill (body);
    }
    g.fillEllipse (disc);
    // faint drifting bands on the surface
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addEllipse (disc);
        g.reduceClipRegion (clip);
        for (int i = 0; i < 4; ++i)
        {
            auto y = c.y - planetR + planetR * 2.0f * (0.25f + 0.17f * (float) i) + std::sin (spin * 3.0f + (float) i) * 3.0f;
            g.setColour ((p.isLight ? juce::Colours::white : juce::Colour (0xff8a80a8)).withAlpha (0.08f));
            g.fillEllipse (c.x - planetR * 1.2f + std::sin (spin + (float) i) * 8.0f, y - 5.0f, planetR * 2.4f, 10.0f);
        }
    }
    g.setColour (juce::Colours::white.withAlpha (p.isLight ? 0.7f : 0.25f));
    g.drawEllipse (disc.reduced (0.5f), 1.2f);
    auto hl = disc.reduced (planetR * 0.3f).withTrimmedBottom (planetR * 0.75f);
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (p.isLight ? 0.5f : 0.16f), hl.getCentreX(), hl.getY(),
                                juce::Colours::transparentWhite, hl.getCentreX(), hl.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillEllipse (hl);

    // pointer
    auto tick0 = c.getPointOnCircumference (planetR * 0.62f, ang), tick1 = c.getPointOnCircumference (planetR * 0.86f, ang);
    g.setColour (p.isLight ? juce::Colour (0xff111111) : juce::Colours::white);
    g.drawLine ({ tick0, tick1 }, 2.4f);

    strokeRing (ringPath (true));

    // labels
    g.setColour (p.text);
    g.setFont (Fonts::regular (13.0f));
    g.drawText ("NATURAL", juce::Rectangle<float> (c.x - 130.0f, c.y + 64.0f, 100.0f, 18.0f), juce::Justification::centred);
    g.drawText ("ROBOT", juce::Rectangle<float> (c.x + 30.0f, c.y + 64.0f, 100.0f, 18.0f), juce::Justification::centred);
    g.setColour (isMouseOverOrDragging() ? (p.isLight ? juce::Colour (0xff111111) : juce::Colours::white) : p.text);
    Fonts::drawSpaced (g, "RETUNE SPEED", juce::Rectangle<float> (b.getX(), c.y + 86.0f, b.getWidth(), 22.0f),
                       Fonts::regular (17.0f), 0.04f, juce::Justification::centred);
}

// ================================================================== tuner

TunerView::TunerView() { setInterceptsMouseClicks (false, false); }

void TunerView::update (bool isVoiced, int target, float inputMidi)
{
    time += 1.0f / 30.0f;
    voiced = isVoiced && target >= 0;
    if (voiced)
    {
        note = juce::String (juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }[target % 12]);
        cents = juce::jlimit (-100.0f, 100.0f, (inputMidi - (float) target) * 100.0f);
    }
    presence += ((voiced ? 1.0f : 0.25f) - presence) * 0.15f;
    shownCents += ((voiced ? cents : 0.0f) - shownCents) * (voiced ? 0.35f : 0.08f);
    repaint();
}

void TunerView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    auto glowC = glowColour (p);
    auto off = std::abs (shownCents);
    auto inTune = voiced ? juce::jlimit (0.0f, 1.0f, 1.0f - (off - 6.0f) / 30.0f) : 0.0f;

    // giant note
    auto shake = voiced ? juce::jlimit (0.0f, 1.0f, (off - 12.0f) / 50.0f) * 3.5f : 0.0f;
    auto c = juce::Point<float> (b.getCentreX() + std::sin (time * 47.0f) * shake, 92.0f + std::cos (time * 39.0f) * shake * 0.5f);
    Glass::drawGlow (g, c, 120.0f + 30.0f * inTune, glowC, (0.18f + 0.5f * inTune) * presence);
    auto noteColour = p.isLight ? juce::Colour (0xffffc300) : juce::Colours::white;
    if (p.isLight)
        drawNoteName (g, note, c.translated (0.0f, 2.0f), 150.0f, juce::Colour (0x40000000), presence);
    for (auto d : { -1.5f, 1.5f })   // soft halo
        drawNoteName (g, note, c.translated (d, 0.0f), 150.0f, glowC.brighter (0.6f), 0.18f * presence);
    drawNoteName (g, note, c, 150.0f, noteColour, juce::jmax (0.3f, presence));

    // cents meter
    auto bar = juce::Rectangle<float> (40.0f, 168.0f, b.getWidth() - 80.0f, 26.0f);
    Glass::drawPanel (g, bar, 5.0f, p, 1.2f);
    g.setColour (p.text.withAlpha (0.6f));
    g.drawLine (bar.getX() + 3.0f, bar.getY() - 2.0f, bar.getX() + 3.0f, bar.getBottom() + 2.0f, 2.0f);
    g.drawLine (bar.getRight() - 3.0f, bar.getY() - 2.0f, bar.getRight() - 3.0f, bar.getBottom() + 2.0f, 2.0f);

    auto xOf = [&] (float cv) { return bar.getCentreX() + cv / 100.0f * (bar.getWidth() * 0.5f - 8.0f); };
    auto ix = xOf (shownCents);
    auto ind = juce::Rectangle<float> (50.0f, bar.getHeight() - 6.0f).withCentre ({ ix, bar.getCentreY() });
    Glass::drawRectGlow (g, ind, 4.0f, glowC, 10.0f, 0.6f * presence);
    juce::ColourGradient fill (glowC.brighter (0.4f), ind.getX(), ind.getY(), glowC.darker (0.2f), ind.getX(), ind.getBottom(), false);
    g.setGradientFill (fill);
    g.setOpacity (juce::jmax (0.35f, presence));
    g.fillRoundedRectangle (ind, 4.0f);
    g.setOpacity (1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.6f * presence));
    g.drawRoundedRectangle (ind, 4.0f, 1.0f);

    // ticks and labels
    for (int t = -100; t <= 100; t += 5)
    {
        auto x = xOf ((float) t);
        auto major = t % 50 == 0, mid = t % 25 == 0;
        auto len = major ? 11.0f : mid ? 8.0f : 5.0f;
        g.setColour (p.text.withAlpha (major ? 0.8f : 0.45f));
        g.drawLine (x, bar.getBottom() + 8.0f, x, bar.getBottom() + 8.0f + len, 1.0f);
        if (major)
        {
            g.setFont (Fonts::regular (15.0f));
            auto txt = t == 0 ? juce::String ("0") : (t > 0 ? "+" : "") + juce::String (t);
            g.drawText (txt, juce::Rectangle<float> (x - 30.0f, bar.getBottom() + 22.0f, 60.0f, 18.0f), juce::Justification::centred);
        }
    }
}

// ================================================================== note orbs

float NoteOrbs::radius() const { return juce::jmin (36.0f, (float) getWidth() / 18.0f); }

juce::Point<float> NoteOrbs::centreOf (int pc) const
{
    static constexpr int naturalIndex[] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    auto w = (float) getWidth();
    auto step = radius() * 2.5f;
    auto x0 = w * 0.5f - step * 3.0f;
    auto rowLow = (float) getHeight() - radius() - 6.0f, rowHigh = rowLow - radius() * 2.15f;
    if (naturalIndex[pc] >= 0) return { x0 + step * (float) naturalIndex[pc], rowLow };
    // sharps sit between their neighbours
    auto left = naturalIndex[pc - 1];
    return { x0 + step * ((float) left + 0.5f), rowHigh };
}

int NoteOrbs::hitOrb (juce::Point<float> pt) const
{
    for (int i = 0; i < 12; ++i)
        if (centreOf (i).getDistanceFrom (pt) < radius() * 1.05f) return i;
    return -1;
}

void NoteOrbs::mouseDown (const juce::MouseEvent& e)
{
    auto i = hitOrb (e.position);
    if (i >= 0 && onToggle) { onToggle (i); sparkle[(size_t) i] = 1.0f; }
}

void NoteOrbs::mouseMove (const juce::MouseEvent& e)
{
    auto h = hitOrb (e.position);
    if (h != hover) { hover = h; setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); }
}

void NoteOrbs::mouseExit (const juce::MouseEvent&) { hover = -1; }

void NoteOrbs::update (const std::array<bool, 12>& m, int sungPc)
{
    time += 1.0f / 30.0f;
    if (sungPc != sung && sungPc >= 0) sparkle[(size_t) sungPc] = 1.0f;
    sung = sungPc;
    mask = m;
    for (int i = 0; i < 12; ++i)
    {
        lit[(size_t) i] += ((mask[(size_t) i] ? 1.0f : 0.0f) - lit[(size_t) i]) * 0.25f;
        sparkle[(size_t) i] *= 0.93f;
    }
    repaint();
}

void NoteOrbs::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto r = radius();
    auto glowC = glowColour (p);
    juce::Random rnd (17);

    for (int i = 0; i < 12; ++i)
    {
        auto c = centreOf (i);
        auto on = lit[(size_t) i];
        auto isSung = i == sung;
        auto flare = juce::jmax (sparkle[(size_t) i], isSung ? 0.55f + 0.15f * std::sin (time * 9.0f) : 0.0f);
        auto disc = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);

        // aura
        if (on > 0.02f || flare > 0.02f)
            Glass::drawGlow (g, c, r * (1.5f + 0.8f * flare), isSung ? glowC.brighter (0.3f) : glowC, 0.25f * on + 0.55f * flare);

        // body
        auto top = p.isLight ? juce::Colours::white.withAlpha (0.12f + 0.78f * on) : juce::Colour (0xff6d6680).interpolatedWith (p.accent.brighter (0.2f), on).withAlpha (0.35f + 0.45f * on);
        auto bottom = p.isLight ? juce::Colour (0xffb3ecff).withAlpha (0.1f + 0.4f * on) : juce::Colour (0xff14111c).interpolatedWith (p.accent.darker (0.6f), on).withAlpha (0.5f + 0.4f * on);
        if (isSung || flare > 0.4f)
        {
            top = top.interpolatedWith (p.isLight ? juce::Colour (0xffffe27a) : juce::Colours::white, flare * 0.8f);
            bottom = bottom.interpolatedWith (p.isLight ? juce::Colour (0xffffc300) : p.accent, flare * 0.6f);
        }
        juce::ColourGradient body (top, c.x - r * 0.4f, c.y - r * 0.6f, bottom, c.x + r * 0.5f, c.y + r, true);
        g.setGradientFill (body);
        g.fillEllipse (disc);
        g.setColour (juce::Colours::white.withAlpha (0.18f + 0.4f * on + 0.3f * flare + (hover == i ? 0.2f : 0.0f)));
        g.drawEllipse (disc.reduced (0.6f), 1.2f);
        auto hl = disc.reduced (r * 0.3f).withTrimmedBottom (r * 0.75f);
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.35f), hl.getCentreX(), hl.getY(),
                                    juce::Colours::transparentWhite, hl.getCentreX(), hl.getBottom(), false);
        g.setGradientFill (sheen);
        g.fillEllipse (hl);

        // star dust on the sung note
        if (flare > 0.05f)
            for (int k = 0; k < 14; ++k)
            {
                auto a = rnd.nextFloat() * 2.0f * pi + time * (0.6f + rnd.nextFloat());
                auto d = r * (0.9f + 0.5f * rnd.nextFloat());
                auto pt = c.getPointOnCircumference (d, a);
                auto s = 1.0f + rnd.nextFloat() * 1.6f;
                g.setColour (juce::Colours::white.withAlpha (flare * (0.3f + 0.6f * rnd.nextFloat())));
                g.fillEllipse (pt.x - s * 0.5f, pt.y - s * 0.5f, s, s);
            }

        auto name = juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }[i];
        auto textC = p.isLight ? juce::Colour (0xff111111).withAlpha (0.45f + 0.55f * on) : juce::Colours::white.withAlpha (0.3f + 0.7f * on);
        g.setColour (textC);
        g.setFont (Fonts::regular (r * 0.78f));
        g.drawText (name, disc, juce::Justification::centred, false);
    }
}

// ================================================================== meter

void HMeter::setLevel (float l)
{
    level = juce::jlimit (0.0f, 1.0f, l);
    peak = juce::jmax (level, peak - 0.01f);
    repaint();
}

void HMeter::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    auto db = juce::Decibels::gainToDecibels (level, -60.0f);
    auto frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto pk = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (peak, -60.0f) + 60.0f) / 60.0f);
    const int ticks = 48;
    auto w = b.getWidth() / (float) ticks;
    for (int i = 0; i < ticks; ++i)
    {
        auto t = (float) i / (float) (ticks - 1);
        auto onTick = t <= frac;
        auto c = onTick ? (p.isLight ? juce::Colour (0xffffc300) : juce::Colours::white) : p.text.withAlpha (0.18f);
        if (onTick && t > 0.92f) c = p.danger;
        g.setColour (c);
        g.fillRect (b.getX() + w * (float) i, b.getY(), juce::jmax (1.0f, w * 0.45f), b.getHeight());
    }
    g.setColour (p.isLight ? juce::Colour (0xff111111) : juce::Colours::white);
    g.fillRect (b.getX() + pk * (b.getWidth() - 2.0f), b.getY() - 2.0f, 2.0f, b.getHeight() + 4.0f);
}

// ================================================================== selector pill

void SelectorPill::paintButton (juce::Graphics& g, bool hover, bool down)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    Glass::drawPill (g, b, p, false, hover, down);
    auto inner = b.reduced (b.getHeight() * 0.55f, 0.0f);
    float x = inner.getX();
    if (caption.isNotEmpty())
    {
        g.setColour (p.textDim);
        auto f = Fonts::regular (14.0f);
        g.setFont (f);
        auto cw = juce::GlyphArrangement::getStringWidth (f, caption);
        g.drawText (caption, inner.withWidth (cw + 2.0f), juce::Justification::centredLeft, false);
        x += cw + 12.0f;
    }
    g.setColour (p.isLight ? juce::Colour (0xff111111) : p.text);
    g.setFont (Fonts::regular (16.0f));
    g.drawFittedText (value, juce::Rectangle<float> (x, inner.getY(), inner.getRight() - x - 18.0f, inner.getHeight()).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.8f);
    juce::Path chev;
    auto cx = inner.getRight() - 6.0f, cy = inner.getCentreY();
    chev.startNewSubPath (cx - 5.0f, cy - 2.5f);
    chev.lineTo (cx, cy + 2.5f);
    chev.lineTo (cx + 5.0f, cy - 2.5f);
    g.strokePath (chev, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

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
            path.addCentredArc (c.x, c.y + s * 0.1f, s * 0.62f, s * 0.55f, 0.0f, dir > 0 ? -pi * 0.55f : pi * 0.55f, dir > 0 ? pi * 0.6f : -pi * 0.6f, true);
            g.strokePath (path, stroke);
            auto tip = juce::Point<float> (c.x - dir * s * 0.55f, c.y - s * 0.2f);
            juce::Path head;
            head.startNewSubPath (tip.translated (dir * s * 0.05f, -s * 0.42f));
            head.lineTo (tip);
            head.lineTo (tip.translated (dir * s * 0.42f, -s * 0.05f));
            g.strokePath (head, stroke);
            break;
        }
        case Icon::reset:
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
                auto a2 = a + pi / (float) teeth;
                auto p1 = c.getPointOnCircumference (rad, a), p2 = c.getPointOnCircumference (rad, a2);
                if (i == 0) path.startNewSubPath (p1); else path.lineTo (p1);
                path.lineTo (p2);
            }
            path.closeSubPath();
            g.strokePath (path, juce::PathStrokeType (th * 0.8f, juce::PathStrokeType::mitered));
            g.drawEllipse (c.x - s * 0.22f, c.y - s * 0.22f, s * 0.44f, s * 0.44f, th * 0.8f);
            break;
        }
        case Icon::power:
        {
            path.addCentredArc (c.x, c.y + s * 0.05f, s * 0.6f, s * 0.6f, 0.0f, pi * 0.2f, pi * 1.8f, true);
            g.strokePath (path, stroke);
            g.drawLine (c.x, c.y - s * 0.7f, c.x, c.y - s * 0.05f, th);
            break;
        }
        case Icon::close:
            g.drawLine (c.x - s * 0.45f, c.y - s * 0.45f, c.x + s * 0.45f, c.y + s * 0.45f, th);
            g.drawLine (c.x + s * 0.45f, c.y - s * 0.45f, c.x - s * 0.45f, c.y + s * 0.45f, th);
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
        b = disc.reduced (d * 0.26f);
    }
    auto col = p.isLight ? juce::Colour (0xff111111) : juce::Colours::white;
    if (! isEnabled()) col = col.withAlpha (0.3f);
    else if (! hover) col = col.withAlpha (0.85f);
    if (active && ! p.isLight) col = juce::Colours::white;
    drawIcon (g, icon, b, col, 1.6f);
}

// ================================================================== switch

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
        Glass::drawRectGlow (g, b, h * 0.5f, glowC, 8.0f, 0.5f);
        g.setColour (glowC.withAlpha (p.isLight ? 0.75f : 0.55f));
        g.fillRoundedRectangle (b, h * 0.5f);
    }
    else
    {
        Glass::drawPanel (g, b, h * 0.5f, p, hover ? 2.0f : 1.4f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.drawRoundedRectangle (b.reduced (0.5f), h * 0.5f, 1.0f);
    auto knob = juce::Rectangle<float> (h - 6.0f, h - 6.0f).withCentre ({ on ? b.getRight() - h * 0.5f : b.getX() + h * 0.5f, b.getCentreY() });
    juce::ColourGradient kg (juce::Colours::white, knob.getX(), knob.getY(), on ? glowC.brighter (0.6f) : juce::Colour (0xffbdb7cc), knob.getX(), knob.getBottom(), false);
    g.setGradientFill (kg);
    g.fillEllipse (knob);
}

} // namespace tune
