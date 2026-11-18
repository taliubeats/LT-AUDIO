#include "Views.h"
#include "NovaEngine.h"

namespace nova
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float startAngle = pi * 1.25f, endAngle = pi * 2.75f;
    juce::Image surface;
    constexpr int texSize = 256;

    float hash2 (int x, int y, int seed)
    {
        auto h = (juce::uint32) x * 374761393u + (juce::uint32) y * 668265263u + (juce::uint32) seed * 1442695041u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return (float) (h & 0xffffff) / (float) 0xffffff;
    }
    float noise (float x, float y, int seed)
    {
        auto xi = (int) std::floor (x), yi = (int) std::floor (y);
        auto xf = x - (float) xi, yf = y - (float) yi;
        xf = xf * xf * (3 - 2 * xf); yf = yf * yf * (3 - 2 * yf);
        auto a = hash2 (xi, yi, seed), b = hash2 (xi + 1, yi, seed), c = hash2 (xi, yi + 1, seed), d = hash2 (xi + 1, yi + 1, seed);
        return juce::jmap (yf, juce::jmap (xf, a, b), juce::jmap (xf, c, d));
    }
    float fbm (float x, float y, int seed)
    {
        float s = 0, a = 0.5f, n = 0;
        for (int i = 0; i < 5; ++i) { s += a * noise (x, y, seed + i * 17); n += a; x *= 2.02f; y *= 2.02f; a *= 0.5f; }
        return s / n;
    }
}

juce::Colour Heat::star (float t)
{
    t = juce::jlimit (0.0f, 1.0f, t);
    const juce::Colour stops[] = { juce::Colour (0xff3a0a06), juce::Colour (0xffa3250c), juce::Colour (0xffff6a1a),
                                   juce::Colour (0xffffb347), juce::Colour (0xfffff1c9) };
    auto f = t * 4.0f;
    auto i = juce::jmin (3, (int) f);
    return stops[i].interpolatedWith (stops[i + 1], f - (float) i);
}

// ================================================================== knob

NovaKnob::NovaKnob (const juce::String& l, float d) : label (l), diameter (d)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (startAngle, endAngle, true);
    setMouseDragSensitivity (220);
    setVelocityBasedMode (false);
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<float> NovaKnob::knobArea() const
{
    auto b = getLocalBounds().toFloat();
    // generous padding above/around so the glow behind the knob never gets clipped
    return juce::Rectangle<float> (diameter, diameter).withCentre ({ b.getCentreX(), b.getY() + diameter * 1.0f });
}

bool NovaKnob::hitTest (int x, int y)
{
    return knobArea().getCentre().getDistanceFrom ({ (float) x, (float) y }) < diameter * 0.58f;
}

void NovaKnob::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto p = *palette;
    p.accent = Heat::arc (p);
    auto k = knobArea();
    lt::ui::GlassLookAndFeel::drawGlassKnob (g, k, (float) valueToProportionOfLength (getValue()), p, 0.8f, startAngle, endAngle);

    auto show = isMouseOverOrDragging();
    auto labelTop = (int) k.getBottom() + (leftTag.isNotEmpty() ? 18 : 10);
    if (leftTag.isNotEmpty())
    {
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (11.5f));
        g.drawText (leftTag, juce::Rectangle<float> (k.getX() - 30.0f, k.getBottom() - 6.0f, 60.0f, 16.0f), juce::Justification::centred);
        g.drawText (rightTag, juce::Rectangle<float> (k.getRight() - 30.0f, k.getBottom() - 6.0f, 60.0f, 16.0f), juce::Justification::centred);
    }
    g.setColour (show ? p.value : p.text);
    g.setFont (Fonts::regular (diameter > 80 ? 17.0f : 15.5f));
    g.drawFittedText (show ? getTextFromValue (getValue()) : label, getLocalBounds().withTop (labelTop), juce::Justification::centredTop, 1);
}

// ================================================================== star

void StarView::buildTexture()
{
    if (surface.isValid()) return;
    surface = juce::Image (juce::Image::ARGB, texSize, texSize, true);
    juce::Image::BitmapData d (surface, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < texSize; ++y)
        for (int x = 0; x < texSize; ++x)
        {
            auto u = (float) x / texSize * 7.0f, v = (float) y / texSize * 7.0f;
            auto w = fbm (u * 0.8f, v * 0.8f, 3);
            auto n = fbm (u + w * 2.5f, v + w * 2.5f, 11);              // granulation / convection cells
            auto cells = std::pow (juce::jlimit (0.0f, 1.0f, (n - 0.35f) * 2.2f), 1.3f);
            d.setPixelColour (x, y, juce::Colours::white.withAlpha (cells));
        }
}

StarView::StarView()
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (startAngle, endAngle, true);
    setMouseDragSensitivity (300);
    buildTexture();
}

bool StarView::hitTest (int x, int y)
{
    return getLocalBounds().toFloat().getCentre().getDistanceFrom ({ (float) x, (float) y }) < arcRadius + 18.0f;
}

void StarView::mouseDown (const juce::MouseEvent& e)
{
    // clicking on the arc jumps there, dragging anywhere on the star turns it
    auto c = getLocalBounds().toFloat().getCentre();
    auto dist = c.getDistanceFrom (e.position);
    if (dist > arcRadius - 22.0f)
    {
        auto ang = std::atan2 (e.position.y - c.y, e.position.x - c.x) + pi * 0.5f;
        while (ang < startAngle) ang += 2.0f * pi;
        while (ang > startAngle + 2.0f * pi) ang -= 2.0f * pi;
        if (ang <= endAngle)
        {
            setValue (proportionOfLengthToValue ((ang - startAngle) / (endAngle - startAngle)), juce::sendNotificationSync);
            return;
        }
    }
    juce::Slider::mouseDown (e);
}

void StarView::tick (float e)
{
    auto v = (float) valueToProportionOfLength (getValue());
    time += 1.0 / 30.0;
    energy += (e - energy) * 0.25f;
    pulse = juce::jmax (pulse * 0.9f, energy);

    auto c = getLocalBounds().toFloat().getCentre();
    auto r = 66.0f + 92.0f * v;
    auto rate = v * v * (6.0f + 14.0f * energy) + (v > 0.15f ? 0.4f : 0.0f);
    auto births = (int) rate + (random.nextFloat() < rate - std::floor (rate) ? 1 : 0);
    for (int i = 0; i < births && sparks.size() < 700; ++i)
    {
        auto a = random.nextFloat() * 2.0f * pi;
        auto sp = 0.8f + random.nextFloat() * (2.0f + 4.0f * v);
        auto start = c + juce::Point<float> (std::cos (a), std::sin (a)) * (r * (0.92f + random.nextFloat() * 0.1f));
        sparks.push_back ({ start, { std::cos (a) * sp, std::sin (a) * sp }, 1.0f, 0.8f + random.nextFloat() * (1.2f + 1.8f * v),
                            random.nextFloat() < 0.25f });
    }
    for (auto& s : sparks)
    {
        s.pos += s.vel;
        s.vel *= 0.985f;
        s.life -= 0.012f + 0.01f * (1.0f - v);
    }
    sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [] (const Spark& s) { return s.life <= 0.0f; }), sparks.end());

    if (std::abs (v - shown) > 0.0001f || energy > 0.001f || ! sparks.empty() || v > 0.05f)
        repaint();
    shown = v;
}

void StarView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    auto c = b.getCentre();
    auto v = (float) valueToProportionOfLength (getValue());
    auto t = (float) time;
    auto heat = juce::jlimit (0.0f, 1.0f, 0.12f + 0.88f * v + 0.08f * pulse);
    auto r = 66.0f + 92.0f * v + 3.0f * pulse;
    auto core = Heat::star (heat);
    auto hot = Heat::star (juce::jmin (1.0f, heat + 0.25f));

    // --- value arc
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (p.text.withAlpha (p.isLight ? 0.25f : 0.16f));
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (v > 0.001f)
    {
        auto end = startAngle + v * (endAngle - startAngle);
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, arcRadius, arcRadius, 0.0f, startAngle, end, true);
        g.setColour (Heat::arc (p).withAlpha (0.22f));
        g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Heat::arcHot (p));
        g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        auto dot = c.getPointOnCircumference (arcRadius, end);
        Glass::drawGlow (g, dot, 16.0f, Heat::arcHot (p), 0.9f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (dot.x - 3.0f, dot.y - 3.0f, 6.0f, 6.0f);
    }

    // --- corona: layered glows (purple halo blending into the nebula, then orange, then white-gold)
    Glass::drawGlow (g, c, r * (2.9f + 0.3f * v), juce::Colour (0xff9a4cff), 0.15f + 0.30f * v);
    Glass::drawGlow (g, c, r * (2.3f + 0.25f * pulse), juce::Colour (0xffff7a26), 0.18f + 0.62f * v);
    Glass::drawGlow (g, c, r * (1.75f + 0.15f * pulse), hot, 0.30f + 0.70f * v);
    Glass::drawGlow (g, c, r * 1.3f, juce::Colours::white, 0.10f + 0.45f * v * v);

    // --- rays: thin tapered spikes from the core, flickering
    auto rays = 10 + (int) (10.0f * v);
    for (int i = 0; i < rays; ++i)
    {
        auto seed = (float) i * 1.618f;
        auto ang = seed * 2.0f + t * 0.03f * (i % 2 ? 1.0f : -1.0f);
        auto len = r * (1.15f + (0.25f + 0.55f * std::abs (std::sin (t * 1.3f + seed * 3.0f))) * (0.2f + 0.9f * v));
        auto w = r * (0.05f + 0.04f * v);
        auto tip = c.getPointOnCircumference (len, ang);
        auto l = c.getPointOnCircumference (w, ang - pi * 0.5f), rr = c.getPointOnCircumference (w, ang + pi * 0.5f);
        juce::Path ray;
        ray.startNewSubPath (l); ray.lineTo (tip); ray.lineTo (rr); ray.closeSubPath();
        juce::ColourGradient rg (hot.withAlpha (0.55f * v + 0.05f), c.x, c.y, hot.withAlpha (0.0f), tip.x, tip.y, false);
        g.setGradientFill (rg);
        g.fillPath (ray);
    }

    // --- prominences: thick fiery loops leaving the surface (mostly orange, some purple)
    auto loops = 4 + (int) (10.0f * v);
    for (int i = 0; i < loops; ++i)
    {
        auto seed = (float) i * 2.399f;
        auto a0 = seed + t * (0.04f + 0.03f * (float) (i % 3)) * (i % 2 ? 1.0f : -1.0f);
        auto span = 0.28f + 0.22f * std::sin (seed * 3.1f);
        auto height = r * (0.18f + (0.5f + 0.4f * std::sin (t * 0.8f + seed)) * (0.15f + 0.75f * v));
        auto p0 = c.getPointOnCircumference (r * 0.94f, a0);
        auto p1 = c.getPointOnCircumference (r * 0.94f, a0 + span);
        auto mid = c.getPointOnCircumference (r + height, a0 + span * 0.5f);
        juce::Path loop;
        loop.startNewSubPath (p0);
        loop.cubicTo (p0 + (mid - c) * 0.42f, p1 + (mid - c) * 0.42f, p1);
        auto purple = i % 4 == 1;
        auto outer = purple ? juce::Colour (0xffb565ff) : juce::Colour (0xffff7a26);
        auto inner = purple ? juce::Colour (0xffe9d2ff) : juce::Colour (0xffffe08a);
        g.setColour (outer.withAlpha (0.10f + 0.22f * v));
        g.strokePath (loop, juce::PathStrokeType (12.0f + 8.0f * v, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (outer.withAlpha (0.35f + 0.4f * v));
        g.strokePath (loop, juce::PathStrokeType (4.0f + 3.0f * v, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (inner.withAlpha (0.45f + 0.5f * v));
        g.strokePath (loop, juce::PathStrokeType (1.3f + 0.8f * v, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- the star body: hot white core -> yellow -> orange limb, with darker convection cells
    auto body = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    {
        auto white = juce::Colour (0xfffffbf0).interpolatedWith (core, 1.0f - v);
        juce::ColourGradient grad (white, c.x, c.y, core.darker (0.15f), c.x + r, c.y, true);
        grad.addColour (0.45, hot);
        grad.addColour (0.8, core);
        g.setGradientFill (grad);
        g.fillEllipse (body);

        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addEllipse (body);
        g.reduceClipRegion (clip);
        auto rot = juce::AffineTransform::rotation (t * 0.06f, (float) texSize * 0.5f, (float) texSize * 0.5f)
                     .scaled (body.getWidth() * 1.4f / texSize).translated (c.x - body.getWidth() * 0.7f, c.y - body.getWidth() * 0.7f);
        g.setColour (Heat::star (juce::jmax (0.0f, heat - 0.35f)).withAlpha (0.45f));        // darker cells
        g.drawImageTransformed (surface, rot, true);
        auto rot2 = juce::AffineTransform::rotation (-t * 0.045f + 1.3f, (float) texSize * 0.5f, (float) texSize * 0.5f)
                      .scaled (body.getWidth() * 1.15f / texSize).translated (c.x - body.getWidth() * 0.575f, c.y - body.getWidth() * 0.575f);
        g.setColour (juce::Colours::white.withAlpha (0.25f * v));                              // bright filaments
        g.drawImageTransformed (surface, rot2, true);
        g.setColour (juce::Colour (0xffb07cff).withAlpha (0.14f * v));
        g.drawImageTransformed (surface, rot, true);

        // fiery limb (no black edge: real stars glow at the rim in this look)
        juce::ColourGradient limb (juce::Colours::transparentWhite, c.x, c.y, juce::Colour (0xffff5a14).withAlpha (0.55f), c.x + r, c.y, true);
        limb.addColour (0.72, juce::Colours::transparentWhite);
        g.setGradientFill (limb);
        g.fillEllipse (body);
        Glass::drawGlow (g, c, r * (0.75f + 0.1f * pulse), juce::Colours::white, 0.25f + 0.7f * v * v);
    }

    // --- value + label under the star, drawn above the glow so they always read
    {
        auto txt = juce::String (juce::roundToInt (getValue())) + "%";
        auto valueArea = juce::Rectangle<float> (200.0f, 44.0f).withCentre (c.translated (0.0f, 183.0f));
        auto labelArea = juce::Rectangle<float> (200.0f, 30.0f).withCentre (c.translated (0.0f, 223.0f));
        g.setFont (Fonts::light (34.0f));
        g.setColour (juce::Colours::black.withAlpha (p.isLight ? 0.35f : 0.6f));
        g.drawText (txt, valueArea.translated (0.0f, 2.0f), juce::Justification::centred);
        g.setColour (p.isLight ? juce::Colours::white : p.value);
        g.drawText (txt, valueArea, juce::Justification::centred);
        g.setColour (juce::Colours::black.withAlpha (p.isLight ? 0.3f : 0.5f));
        Fonts::drawSpaced (g, "DRIVE", labelArea.translated (0.0f, 1.5f), Fonts::regular (21.0f), 0.06f, juce::Justification::centred);
        g.setColour (p.isLight ? p.text : juce::Colours::white.withAlpha (0.9f));
        Fonts::drawSpaced (g, "DRIVE", labelArea, Fonts::regular (21.0f), 0.06f, juce::Justification::centred);
    }

    // --- sparks flying out
    for (auto& s : sparks)
    {
        auto col = s.purple ? juce::Colour (0xffd2a6ff) : (s.size > 1.8f ? juce::Colour (0xffffd27a) : juce::Colour (0xffff8a3a));
        if (s.size > 1.8f) Glass::drawGlow (g, s.pos, s.size * 3.0f, col, 0.35f * s.life);
        g.setColour (col.withAlpha (juce::jlimit (0.0f, 1.0f, s.life)));
        g.fillEllipse (s.pos.x - s.size * 0.5f, s.pos.y - s.size * 0.5f, s.size, s.size);
    }
}

// ================================================================== type bar

TypeBar::TypeBar()
{
    for (int i = 0; i < SatTypes::names().size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (SatTypes::names()[i]));
        b->setClickingTogglesState (false);
        b->setTooltip (SatTypes::description (i));
        b->getProperties().set ("fontSize", 16.5f);
        b->onClick = [this, i] { setSelected (i); if (onChange) onChange (i); };
        addAndMakeVisible (b);
    }
}

void TypeBar::setSelected (int index)
{
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == index, juce::dontSendNotification);
}

void TypeBar::resized()
{
    // widths follow the length of each word, like the design
    const float weights[] = { 1.0f, 1.0f, 1.05f, 1.45f, 1.3f, 1.0f, 1.0f, 1.05f };
    float total = 0;
    for (auto w : weights) total += w;
    auto gap = 14.0f;
    auto avail = (float) getWidth() - gap * (float) (buttons.size() - 1);
    auto x = 0.0f;
    for (int i = 0; i < buttons.size(); ++i)
    {
        auto w = avail * weights[i] / total;
        buttons[i]->setBounds (juce::Rectangle<float> (x, 0.0f, w, (float) getHeight()).toNearestInt());
        x += w + gap;
    }
}

// ================================================================== meter

void DbMeter::setLevels (float l, float r)
{
    float in[] = { l, r };
    for (int i = 0; i < 2; ++i)
    {
        lv[i] = juce::jmax (in[i], lv[i] * 0.86f);
        pk[i] = juce::jmax (in[i], pk[i] * 0.985f);
    }
    repaint();
}

void DbMeter::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    auto barW = 6.0f;
    auto toY = [&] (float level)
    {
        auto db = juce::jlimit (-48.0f, 0.0f, juce::Decibels::gainToDecibels (level, -100.0f));
        return b.getBottom() - (db + 48.0f) / 48.0f * b.getHeight();
    };

    for (int i = 0; i < 2; ++i)
    {
        auto x = b.getX() + (float) i * (barW + 6.0f);
        auto bar = juce::Rectangle<float> (x, b.getY(), barW, b.getHeight());
        g.setColour (p.text.withAlpha (0.12f));
        g.fillRoundedRectangle (bar, 3.0f);
        auto y = toY (lv[i]);
        juce::ColourGradient grad (Heat::arc (p), x, b.getY(), p.isLight ? juce::Colours::white : p.accent, x, b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bar.withTop (y), 3.0f);
        g.setColour (p.text.withAlpha (0.8f));
        g.fillRect (x, toY (pk[i]) - 1.0f, barW, 2.0f);
    }

    if (showScale)
    {
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (10.5f));
        for (auto db : { 0, -6, -18, -24 })
        {
            auto y = b.getBottom() - ((float) db + 48.0f) / 48.0f * b.getHeight();
            g.drawText (juce::String (db), juce::Rectangle<float> (b.getRight() + 2.0f, y - 7.0f, 30.0f, 14.0f), juce::Justification::centred);
        }
        g.drawText ("dB", juce::Rectangle<float> (b.getRight() + 2.0f, b.getBottom() - 12.0f, 30.0f, 14.0f), juce::Justification::centred);
    }
}

// ================================================================== switch

void PillSwitch::paintButton (juce::Graphics& g, bool hover, bool)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    auto on = getToggleState();
    if (on)
    {
        Glass::drawRectGlow (g, r, r.getHeight() * 0.5f, p.accent, 10.0f, 0.7f);
        juce::ColourGradient grad (p.accent.brighter (0.2f), r.getX(), r.getY(), p.accent.darker (0.2f), r.getRight(), r.getY(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    else
    {
        Glass::drawPanel (g, r, r.getHeight() * 0.5f, p, hover ? 1.8f : 1.2f);
    }
    auto knob = (on ? r.removeFromRight (r.getHeight()) : r.removeFromLeft (r.getHeight())).reduced (4.0f);
    juce::ColourGradient kg (on ? juce::Colour (0xffffd28a) : juce::Colours::white.withAlpha (0.7f), knob.getX(), knob.getY(),
                             on ? juce::Colour (0xffff9a4a) : juce::Colours::white.withAlpha (0.4f), knob.getRight(), knob.getBottom(), false);
    g.setGradientFill (kg);
    g.fillEllipse (knob);
}

} // namespace nova
