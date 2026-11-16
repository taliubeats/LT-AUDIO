#include "Views.h"

namespace pulsar
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace
{
    constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
    constexpr float endAngle = juce::MathConstants<float>::pi * 2.75f;

    juce::Colour waveColour (const Palette& p)  { return p.isLight ? p.value : juce::Colour (0xffd6d3de); }
    juce::Colour curveColour (const Palette& p) { return p.isLight ? juce::Colours::white : juce::Colours::white; }
    juce::Colour curveGlow (const Palette& p)   { return p.isLight ? juce::Colours::white : p.accent; }
}

juce::Path makeCurvePath (const Curve& c, juce::Rectangle<float> r, int steps)
{
    juce::Path p;
    for (int i = 0; i <= steps; ++i)
    {
        auto phase = (float) i / (float) steps;
        auto y = c.evaluate (i == steps ? 1.0f - 1.0e-5f : phase);
        auto pt = juce::Point<float> (r.getX() + phase * r.getWidth(), r.getBottom() - y * r.getHeight());
        if (i == 0) p.startNewSubPath (pt);
        else        p.lineTo (pt);
    }
    return p;
}

// ================================================================== MixKnob

MixKnob::MixKnob()
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (startAngle, endAngle, true);
    setMouseDragSensitivity (260);
    setVelocityBasedMode (false);
    setDoubleClickReturnValue (true, 75.0);
    setPopupDisplayEnabled (false, false, nullptr);
}

juce::Rectangle<float> MixKnob::getKnobArea() const
{
    auto size = juce::jmin ((float) getHeight() * 0.80f, 230.0f);
    return juce::Rectangle<float> (size, size).withCentre ({ (float) getWidth() * 0.5f, (float) getHeight() * 0.46f });
}

bool MixKnob::hitTest (int x, int y)
{
    auto k = getKnobArea();
    return k.getCentre().getDistanceFrom ({ (float) x, (float) y }) < k.getWidth() * 0.5f + 6.0f;
}

void MixKnob::resized()
{
    disc = {};
}

void MixKnob::renderDisc()
{
    auto d = juce::roundToInt (getKnobArea().getWidth() * 0.80f * 2.0f);   // drawn at 2x for sharpness
    if (d <= 4) return;

    discIsLight = palette->isLight;
    disc = juce::Image (juce::Image::ARGB, d, d, true);
    juce::Graphics g (disc);
    auto c = juce::Point<float> ((float) d * 0.5f, (float) d * 0.5f);
    auto r = (float) d * 0.5f;

    juce::Path circle;
    circle.addEllipse (0, 0, (float) d, (float) d);
    g.reduceClipRegion (circle);

    auto base = discIsLight ? juce::Colour (0xffc9d6e2) : juce::Colour (0xff15151b);
    auto shine = discIsLight ? juce::Colour (0xffffffff) : juce::Colour (0xff6c6c78);
    g.fillAll (base);

    // brushed metal: brightness changes with the angle, with fine circular grain
    {
        juce::Image::BitmapData px (disc, juce::Image::BitmapData::readWrite);
        juce::Random rnd (11);
        std::vector<float> grain (512);
        for (auto& v : grain) v = rnd.nextFloat() - 0.5f;
        for (int y = 0; y < d; ++y)
            for (int x = 0; x < d; ++x)
            {
                auto dx = (float) x - c.x, dy = (float) y - c.y;
                auto dist = std::sqrt (dx * dx + dy * dy);
                if (dist > r) continue;
                auto a = std::atan2 (dy, dx);
                auto ring = grain[(size_t) ((int) (dist * 1.7f) & 511)];
                auto b = 0.5f + 0.34f * std::cos (2.0f * (a - 0.6f)) + 0.10f * std::cos (6.0f * a + 1.0f) + 0.07f * ring;
                b = juce::jlimit (0.0f, 1.0f, b);
                px.setPixelColour (x, y, base.interpolatedWith (shine, b * (discIsLight ? 0.9f : 0.55f)));
            }
    }

    // soft centre and darker edge, like a machined knob
    juce::ColourGradient vignette (juce::Colours::transparentBlack, c.x, c.y, juce::Colours::black.withAlpha (discIsLight ? 0.18f : 0.55f),
                                   c.x + r, c.y, true);
    vignette.addColour (0.75, juce::Colours::transparentBlack);
    g.setGradientFill (vignette);
    g.fillEllipse (0, 0, (float) d, (float) d);

    // rim
    juce::ColourGradient rim (juce::Colours::white.withAlpha (discIsLight ? 0.95f : 0.55f), 0, 0,
                              juce::Colours::white.withAlpha (0.05f), 0, (float) d, false);
    g.setGradientFill (rim);
    g.drawEllipse (2.0f, 2.0f, (float) d - 4.0f, (float) d - 4.0f, 3.0f);
}

void MixKnob::tick (float beat, float lvl, float gain)
{
    if (palette == nullptr) return;
    auto v = (float) valueToProportionOfLength (getValue());
    level = lvl;
    gainNow = gain;
    pulse = juce::jmax (pulse * 0.86f, beat * (0.35f + 0.65f * v));

    auto k = getKnobArea();
    auto c = k.getCentre();
    auto R = k.getWidth() * 0.5f;

    // particles: more and faster with MIX, a burst on every beat
    auto births = (int) (v * 3.0f + random.nextFloat() + pulse * 26.0f * v);
    for (int i = 0; i < births && particles.size() < 420; ++i)
    {
        auto a = random.nextFloat() * juce::MathConstants<float>::twoPi;
        auto dir = juce::Point<float> (std::cos (a), std::sin (a) * 0.62f);   // flattened, like the nebula band
        auto speed = (0.5f + random.nextFloat() * 2.2f) * (0.45f + v) * (1.0f + pulse * 1.5f);
        particles.push_back ({ c + dir * (R * (0.95f + random.nextFloat() * 0.25f)), dir * speed,
                               1.0f, (0.7f + random.nextFloat() * 1.9f) * (0.6f + 0.6f * v), random.nextFloat() < 0.35f });
    }

    for (auto& p : particles)
    {
        p.pos += p.vel;
        p.vel *= 0.985f;
        p.life -= 0.012f + 0.012f * (1.0f - v);
    }
    particles.erase (std::remove_if (particles.begin(), particles.end(), [] (const Particle& p) { return p.life <= 0.0f; }),
                     particles.end());
    repaint();
}

void MixKnob::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    if (disc.isNull() || discIsLight != p.isLight) renderDisc();

    auto v = (float) valueToProportionOfLength (getValue());
    auto k = getKnobArea();
    auto c = k.getCentre();
    auto R = k.getWidth() * 0.5f;
    auto glowColour = p.isLight ? juce::Colours::white : p.accent;

    // light behind the knob: grows with the value, beats with the rhythm
    Glass::drawGlow (g, c, R * (1.25f + 0.55f * v + 0.25f * pulse), glowColour, 0.12f + 0.30f * v + 0.22f * pulse);

    for (auto& pt : particles)
    {
        auto col = p.isLight ? juce::Colours::white
                             : (pt.warm ? juce::Colour (0xffff9a7a) : p.nebulaC);
        g.setColour (col.withAlpha (juce::jlimit (0.0f, 1.0f, pt.life) * (p.isLight ? 0.95f : 0.8f)));
        g.fillEllipse (pt.pos.x - pt.size * 0.5f, pt.pos.y - pt.size * 0.5f, pt.size, pt.size);
    }

    // arc
    auto arcR = R - 4.0f;
    auto angle = startAngle + v * (endAngle - startAngle);
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour ((p.isLight ? juce::Colours::white : juce::Colour (0xff3a3446)).withAlpha (p.isLight ? 0.45f : 0.8f));
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (v > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        auto arcColour = p.isLight ? juce::Colours::white : p.accent.brighter (0.2f);
        g.setColour (arcColour.withAlpha (0.22f + 0.2f * pulse));
        g.strokePath (arc, juce::PathStrokeType (16.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (arcColour);
        g.strokePath (arc, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // dark ring + metal disc
    auto discR = R * 0.80f;
    g.setColour (juce::Colours::black.withAlpha (p.isLight ? 0.12f : 0.6f));
    g.fillEllipse (juce::Rectangle<float> (discR * 2.0f + 14.0f, discR * 2.0f + 14.0f).withCentre (c.translated (0, 3.0f)));
    g.setColour ((p.isLight ? juce::Colours::white : juce::Colour (0xff24202c)).withAlpha (0.9f));
    g.drawEllipse (juce::Rectangle<float> (discR * 2.0f + 8.0f, discR * 2.0f + 8.0f).withCentre (c), 3.0f);
    g.drawImage (disc, juce::Rectangle<float> (discR * 2.0f, discR * 2.0f).withCentre (c));

    // value
    auto text = juce::String (juce::roundToInt (getValue())) + "%";
    auto font = Fonts::light (R * 0.46f);
    auto textArea = juce::Rectangle<float> (discR * 2.0f, R * 0.6f).withCentre (c);
    if (p.isLight)
    {
        g.setColour (juce::Colour (0xff8a6d00).withAlpha (0.35f));
        g.setFont (font);
        g.drawText (text, textArea.translated (0, 1.5f), juce::Justification::centred);
    }
    g.setColour (p.isLight ? juce::Colour (0xffffc300) : juce::Colours::white);
    g.setFont (font);
    g.drawText (text, textArea, juce::Justification::centred);
}

// ================================================================== CurveView

CurveView::CurveView (DuckEngine& e) : engine (e) {}

juce::Rectangle<float> CurveView::plot() const
{
    return getLocalBounds().toFloat().reduced (2.0f, 8.0f);
}

float CurveView::phaseForX (float x) const
{
    auto r = plot();
    return juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth());
}

juce::Point<float> CurveView::pointPosition (const Curve& c, int i) const
{
    auto r = plot();
    auto& pt = c.getPoints()[(size_t) i];
    auto x = pt.x + c.getOffset();
    auto last = i == (int) c.getPoints().size() - 1;
    if (! (last && c.getOffset() <= 0.0f))
        x -= std::floor (x);
    return { r.getX() + x * r.getWidth(), r.getBottom() - pt.y * r.getHeight() };
}

int CurveView::pointAt (juce::Point<float> pos) const
{
    if (! getCurve) return -1;
    auto& c = getCurve();
    int best = -1;
    float bestD = 10.0f;
    for (int i = 0; i < (int) c.getPoints().size(); ++i)
    {
        auto d = pointPosition (c, i).getDistanceFrom (pos);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

int CurveView::segmentNear (juce::Point<float> pos) const
{
    if (! getCurve) return -1;
    auto& c = getCurve();
    auto r = plot();
    auto phase = phaseForX (pos.x);
    auto y = r.getBottom() - c.evaluate (phase) * r.getHeight();
    if (std::abs (pos.y - y) > 14.0f)
        return -1;
    auto x = phase - c.getOffset();
    x -= std::floor (x);
    return c.segmentAt (x);
}

void CurveView::change (const Curve& c)
{
    if (onCurveChanged) onCurveChanged (c);
    repaint();
}

void CurveView::tick()
{
    for (int i = 0; i < DuckEngine::displayBins; ++i)
    {
        shownIn[(size_t) i] = engine.getInputWave (i);
        shownOut[(size_t) i] = engine.getOutputWave (i);
    }
    repaint();
}

void CurveView::paint (juce::Graphics& g)
{
    if (palette == nullptr || ! getCurve) return;
    auto& p = *palette;
    auto& c = getCurve();
    auto r = plot();

    g.setColour (p.isLight ? juce::Colours::white.withAlpha (0.10f) : juce::Colours::black.withAlpha (0.38f));
    g.fillRoundedRectangle (r.expanded (2.0f, 8.0f), 6.0f);

    for (int i = 1; i < 32; ++i)
    {
        g.setColour (p.text.withAlpha (i % 4 == 0 ? 0.10f : 0.045f));
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 32.0f), r.getY(), r.getBottom());
    }

    // real audio, oscilloscope style: original dim behind, processed (ducked by the curve) in front
    if (showWave)
    {
        auto cy = r.getCentreY();
        auto amp = r.getHeight() * 0.46f;
        auto makeWave = [&] (const std::array<float, DuckEngine::displayBins>& v)
        {
            juce::Path w;
            auto n = DuckEngine::displayBins;
            for (int i = 0; i < n; ++i)
            {
                auto x = r.getX() + ((float) i + 0.5f) / (float) n * r.getWidth();
                auto s = juce::jlimit (-1.0f, 1.0f, v[(size_t) i]);
                auto y = cy - std::copysign (std::pow (std::abs (s), 0.75f), s) * amp;
                if (i == 0) w.startNewSubPath (x, y); else w.lineTo (x, y);
            }
            return w;
        };

        g.setColour (p.isLight ? juce::Colours::white.withAlpha (0.35f) : juce::Colour (0xff8a8794).withAlpha (0.35f));
        g.strokePath (makeWave (shownIn), juce::PathStrokeType (1.0f));
        auto wave = makeWave (shownOut);
        g.setColour (waveColour (p).withAlpha (0.25f));
        g.strokePath (wave, juce::PathStrokeType (3.0f));
        g.setColour (waveColour (p));
        g.strokePath (wave, juce::PathStrokeType (1.3f));
    }

    if (showWave && engine.isSidechainActive())
    {
        g.setColour (waveColour (p));
        g.setFont (Fonts::semiBold (11.5f));
        g.drawText ("SIDECHAIN", r.reduced (10.0f, 6.0f).removeFromTop (16.0f), juce::Justification::topLeft);
    }

    // the curve
    auto path = makeCurvePath (c, r, 400);
    auto fill = path;
    fill.lineTo (r.getRight(), r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();
    juce::ColourGradient grad (curveGlow (p).withAlpha (p.isLight ? 0.18f : 0.14f), 0, r.getY(),
                               curveGlow (p).withAlpha (0.0f), 0, r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (fill);

    g.setColour (curveGlow (p).withAlpha (0.45f));
    g.strokePath (path, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (curveColour (p));
    g.strokePath (path, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // hovered segment (it can be bent by dragging)
    if (hoverSegment >= 0 && hoverPoint < 0 && mode == Mode::none)
    {
        auto a = pointPosition (c, hoverSegment), b = pointPosition (c, hoverSegment + 1);
        g.setColour (curveColour (p).withAlpha (0.18f));
        if (b.x > a.x)
            g.fillRect (juce::Rectangle<float> (a.x, r.getY(), b.x - a.x, r.getHeight()));
    }

    // playhead
    auto ph = engine.getDisplayPhase();
    auto px = r.getX() + ph * r.getWidth();
    g.setColour (p.text.withAlpha (0.25f));
    g.drawVerticalLine ((int) px, r.getY(), r.getBottom());
    auto py = r.getBottom() - c.evaluate (ph) * r.getHeight();
    Glass::drawGlow (g, { px, py }, 12.0f, curveColour (p), 0.6f);
    g.setColour (curveColour (p));
    g.fillEllipse (px - 3.5f, py - 3.5f, 7.0f, 7.0f);

    // points (shown when the mouse is over the editor)
    if (isMouseOverOrDragging())
    {
        for (int i = 0; i < (int) c.getPoints().size(); ++i)
        {
            auto pos = pointPosition (c, i);
            auto hot = i == hoverPoint || (mode == Mode::point && i == dragIndex);
            auto rad = hot ? 7.0f : 5.0f;
            g.setColour (p.isLight ? juce::Colour (0xff4fc3f7) : juce::Colour (0xff0d0a14));
            g.fillEllipse (pos.x - rad, pos.y - rad, rad * 2.0f, rad * 2.0f);
            g.setColour (curveColour (p));
            g.drawEllipse (pos.x - rad, pos.y - rad, rad * 2.0f, rad * 2.0f, 2.0f);
        }

        g.setColour (p.isLight ? p.text.withAlpha (0.7f) : p.textDim);
        g.setFont (Fonts::regular (11.5f));
        g.drawText (juce::CharPointer_UTF8 ("Drag points \xc2\xb7 double-click: add / remove \xc2\xb7 drag a line: bend \xc2\xb7 SHIFT + drag: move in time \xc2\xb7 right-click: more"),
                    r.reduced (10.0f, 6.0f).removeFromBottom (14.0f), juce::Justification::bottomLeft);
    }
}

void CurveView::mouseMove (const juce::MouseEvent& e)
{
    auto hp = pointAt (e.position);
    auto hs = hp < 0 ? segmentNear (e.position) : -1;
    if (hp != hoverPoint || hs != hoverSegment)
    {
        hoverPoint = hp;
        hoverSegment = hs;
        setMouseCursor (e.mods.isShiftDown() ? juce::MouseCursor::LeftRightResizeCursor
                        : hp >= 0 ? juce::MouseCursor::DraggingHandCursor
                        : hs >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    }
    repaint();
}

void CurveView::mouseExit (const juce::MouseEvent&)
{
    hoverPoint = hoverSegment = -1;
    repaint();
}

void CurveView::mouseDown (const juce::MouseEvent& e)
{
    if (! getCurve) return;
    dragStartCurve = getCurve();
    dragStart = e.position;
    mode = Mode::none;

    if (e.mods.isPopupMenu())
    {
        showMenu (pointAt (e.position));
        return;
    }

    if (e.mods.isShiftDown())
    {
        mode = Mode::shift;
        dragStartValue = dragStartCurve.getOffset();
        return;
    }

    dragIndex = pointAt (e.position);
    if (dragIndex >= 0)
    {
        mode = Mode::point;
        return;
    }

    dragIndex = segmentNear (e.position);
    if (dragIndex >= 0)
    {
        mode = Mode::bend;
        dragStartValue = dragStartCurve.getPoints()[(size_t) dragIndex].tension;
    }
}

void CurveView::mouseDrag (const juce::MouseEvent& e)
{
    auto r = plot();
    auto c = dragStartCurve;

    if (mode == Mode::shift)
    {
        c.setOffset (dragStartValue + (e.position.x - dragStart.x) / r.getWidth());
        change (c);
    }
    else if (mode == Mode::point)
    {
        auto x = phaseForX (e.position.x) - c.getOffset();
        x -= std::floor (x);
        auto y = juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight());
        c.movePoint (dragIndex, x, y);
        change (c);
    }
    else if (mode == Mode::bend)
    {
        auto& pts = c.getPoints();
        auto rising = pts[(size_t) dragIndex + 1].y > pts[(size_t) dragIndex].y;
        auto dy = (e.position.y - dragStart.y) / 110.0f;
        c.setTension (dragIndex, dragStartValue + (rising ? dy : -dy));
        change (c);
    }
}

void CurveView::mouseUp (const juce::MouseEvent&)
{
    mode = Mode::none;
    dragIndex = -1;
    repaint();
}

void CurveView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! getCurve) return;
    auto c = getCurve();
    auto r = plot();
    auto hit = pointAt (e.position);

    if (hit >= 0)
        c.removePoint (hit);
    else
    {
        auto x = phaseForX (e.position.x) - c.getOffset();
        x -= std::floor (x);
        c.addPoint (x, juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight()));
    }
    hoverPoint = -1;
    change (c);
}

void CurveView::showMenu (int pointIndex)
{
    auto& c = getCurve();
    juce::PopupMenu m;
    auto removable = pointIndex > 0 && pointIndex < (int) c.getPoints().size() - 1;
    m.addItem (1, "Remove point", removable);
    m.addItem (6, "Straighten line", hoverSegment >= 0 || pointIndex >= 0);
    m.addSeparator();
    m.addItem (2, "Flip (up / down)");
    m.addItem (3, "Reverse (left / right)");
    m.addItem (4, "Reset time shift", c.getOffset() > 0.0f);
    m.addItem (5, "Reset to a straight line");

    auto segment = hoverSegment >= 0 ? hoverSegment : juce::jmin (pointIndex, (int) c.getPoints().size() - 2);
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, pointIndex, segment] (int r)
    {
        auto curve = getCurve();
        auto pts = curve.getPoints();
        if (r == 1) curve.removePoint (pointIndex);
        else if (r == 2)
        {
            for (auto& p : pts) p.y = 1.0f - p.y;
            curve = Curve (pts, curve.getOffset());
        }
        else if (r == 3)
        {
            std::vector<CurvePoint> rev;
            for (int i = (int) pts.size() - 1; i >= 0; --i)
            {
                auto t = i > 0 ? -pts[(size_t) i - 1].tension : 0.0f;
                rev.push_back ({ 1.0f - pts[(size_t) i].x, pts[(size_t) i].y, t });
            }
            curve = Curve (rev, curve.getOffset());
        }
        else if (r == 4) curve.setOffset (0.0f);
        else if (r == 5) curve = Curve ({ { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } });
        else if (r == 6 && segment >= 0) curve.setTension (segment, 0.0f);
        else return;
        change (curve);
    });
}

// ================================================================== RateTabs

RateTabs::RateTabs()
{
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<float> RateTabs::tabArea (int i) const
{
    // shown fastest first: 1/16 1/8 1/4 1/2 1/1
    auto w = (float) getWidth() / (float) Rates::count;
    return { (float) i * w, 0.0f, w, (float) getHeight() };
}

int RateTabs::rateForTab (int i) const noexcept { return Rates::count - 1 - i; }

void RateTabs::setRate (int r)
{
    if (r != rate) { rate = r; repaint(); }
}

void RateTabs::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto mouse = getMouseXYRelative().toFloat();
    for (int i = 0; i < Rates::count; ++i)
    {
        auto r = tabArea (i);
        auto on = rateForTab (i) == rate;
        auto hot = isMouseOver() && r.contains (mouse);
        g.setColour (on ? (p.isLight ? p.text : juce::Colours::white)
                        : (p.isLight ? p.text : juce::Colours::white).withAlpha (hot ? 0.7f : 0.38f));
        g.setFont (on ? Fonts::semiBold (13.0f) : Fonts::regular (13.0f));
        g.drawText (Rates::names()[rateForTab (i)], r, juce::Justification::centred);
        if (on)
        {
            auto tw = juce::GlyphArrangement::getStringWidth (Fonts::semiBold (13.0f), Rates::names()[rateForTab (i)]);
            g.fillRect (juce::Rectangle<float> (tw, 1.5f).withCentre ({ r.getCentreX(), r.getBottom() - 3.0f }));
        }
    }
}

void RateTabs::mouseUp (const juce::MouseEvent& e)
{
    for (int i = 0; i < Rates::count; ++i)
        if (tabArea (i).contains (e.position) && onChange)
            onChange (rateForTab (i));
}

// ================================================================== ShapeButton

ShapeButton::ShapeButton (int i) : juce::Button (Curve::shapeName (i)), index (i)
{
    setTooltip (Curve::shapeName (i));
}

void ShapeButton::paintButton (juce::Graphics& g, bool hover, bool down)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    auto on = getToggleState();

    if (on)
        Glass::drawRectGlow (g, r, 9.0f, p.isLight ? juce::Colours::white : p.accent, 8.0f, 0.8f);
    Glass::drawPanel (g, r, 9.0f, p, hover ? 1.8f : 1.1f);
    if (on)
    {
        g.setColour (p.isLight ? juce::Colours::white : p.accent.brighter (0.3f));
        g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 2.0f);
    }
    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.1f));
        g.fillRoundedRectangle (r, 9.0f);
    }

    auto icon = r.reduced (r.getWidth() * 0.2f, r.getHeight() * 0.24f);
    auto shape = Curve::shape (index);
    auto path = makeCurvePath (shape, icon, 60);

    if (index >= 4)
    {
        auto fill = path;
        fill.lineTo (icon.getRight(), icon.getBottom());
        fill.lineTo (icon.getX(), icon.getBottom());
        fill.closeSubPath();
        g.setColour ((p.isLight ? juce::Colour (0xff1d6f99) : juce::Colour (0xff4a4458)).withAlpha (0.7f));
        g.fillPath (fill);
    }

    g.setColour (on ? (p.isLight ? juce::Colours::white : p.accent.brighter (0.6f))
                    : (p.isLight ? juce::Colour (0xff111111) : juce::Colour (0xffd8d4e2)));
    g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace pulsar
