#include "Views.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace orbit
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace { constexpr float pi = juce::MathConstants<float>::pi; }

// ================================================================== knob

OrbitKnob::OrbitKnob (const juce::String& l, bool b) : label (l), big (b)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    setMouseDragSensitivity (big ? 220 : 180);
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<float> OrbitKnob::knobArea() const
{
    auto b = getLocalBounds().toFloat();
    auto s = big ? 112.0f : 62.0f;
    auto cy = big ? b.getCentreY() : b.getY() + s * 0.5f + 4.0f;
    return juce::Rectangle<float> (s, s).withCentre ({ b.getCentreX(), cy });
}

bool OrbitKnob::hitTest (int x, int y)
{
    auto k = knobArea();
    return k.getCentre().getDistanceFrom ({ (float) x, (float) y }) < k.getWidth() * 0.55f;
}

void OrbitKnob::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto k = knobArea();
    lt::ui::GlassLookAndFeel::drawGlassKnob (g, k, (float) valueToProportionOfLength (getValue()), p, big ? 1.0f : 0.75f);

    auto text = formatValue ? formatValue (getValue()) : getTextFromValue (getValue());
    if (big)
    {
        g.setColour (p.value);
        g.setFont (Fonts::regular (30.0f));
        g.drawFittedText (text, k.reduced (18.0f).toNearestInt(), juce::Justification::centred, 1);
    }
    else
    {
        auto show = isMouseOverOrDragging();
        g.setColour (show ? p.value : p.text);
        g.setFont (Fonts::regular (15.0f));
        g.drawFittedText (show ? text : label, getLocalBounds().withTop ((int) k.getBottom() + 6), juce::Justification::centredTop, 1);
    }
}

// ================================================================== orbit view

OrbitView::OrbitView()
{
    setInterceptsMouseClicks (false, false);
    for (int r = 1; r <= 8; ++r)
        echoes.push_back ({ 0.0, random.nextFloat() * 2.0f * pi, true, r });
}

int OrbitView::ringCount() const
{
    return st.frozen ? 8 : juce::jlimit (2, 8, 2 + juce::roundToInt (st.feedback * 6.0f));
}

float OrbitView::ringRadius (float k) const
{
    auto maxR = (float) getWidth() * 0.5f - 8.0f;
    return 58.0f + (maxR - 58.0f) * (k - 1.0f) / 7.0f;
}

juce::Point<float> OrbitView::onRing (float k, float angle) const
{
    auto c = getLocalBounds().toFloat().getCentre();
    auto rx = ringRadius (k), ry = rx * 0.40f;
    auto tilt = -0.07f;
    auto x = rx * std::cos (angle), y = ry * std::sin (angle);
    return { c.x + x * std::cos (tilt) - y * std::sin (tilt), c.y + x * std::sin (tilt) + y * std::cos (tilt) };
}

void OrbitView::tick (const OrbitState& s)
{
    st = s;
    time += 1.0 / 30.0;
    pulse = juce::jmax (pulse * 0.88f, s.level);

    if (s.hits != lastHits && ! s.frozen)
    {
        echoes.push_back ({ time, random.nextFloat() * 2.0f * pi, false, 0 });
        pulse = 1.0f;
    }
    lastHits = s.hits;

    auto delay = juce::jmax (0.02, s.delaySeconds);
    echoes.erase (std::remove_if (echoes.begin(), echoes.end(), [&] (const Echo& e)
    {
        if (e.ambient) return false;
        auto k = (int) ((time - e.born) / delay);
        auto alpha = std::pow (juce::jmax (0.01f, s.feedback), (float) juce::jmax (0, k - 1));
        return ! s.frozen && (k > 8 || (k >= 1 && alpha < 0.04f));
    }), echoes.end());
    if (echoes.size() > 160) echoes.erase (echoes.begin() + 8, echoes.begin() + 40);
    repaint();
}

void OrbitView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto c = getLocalBounds().toFloat().getCentre();
    auto rings = ringCount();
    auto line = p.isLight ? juce::Colours::white : p.text;
    auto satCol = p.isLight ? juce::Colour (0xffffe14d) : p.nebulaC;
    auto delay = (float) juce::jmax (0.02, st.delaySeconds);
    auto speed = 0.35f / std::sqrt (delay);          // shorter TIME = faster orbits

    // rings
    for (int k = 1; k <= 8; ++k)
    {
        auto on = k <= rings;
        juce::Path ring;
        for (int i = 0; i <= 120; ++i)
        {
            auto pt = onRing ((float) k, (float) i / 120.0f * 2.0f * pi);
            if (i == 0) ring.startNewSubPath (pt); else ring.lineTo (pt);
        }
        g.setColour (line.withAlpha (on ? 0.42f - 0.035f * (float) k : 0.06f));
        g.strokePath (ring, juce::PathStrokeType (1.1f));
    }

    auto drawSatellite = [&] (juce::Point<float> pos, float alpha, float size, float k, float angle, float dir)
    {
        // trail along the ring
        for (int t = 1; t <= 14; ++t)
        {
            auto tp = onRing (k, angle - dir * (float) t * 0.035f);
            auto a = alpha * (1.0f - (float) t / 15.0f) * 0.45f;
            auto sz = size * (1.0f - (float) t / 18.0f);
            g.setColour (satCol.withAlpha (a));
            g.fillEllipse (tp.x - sz * 0.5f, tp.y - sz * 0.5f, sz, sz);
        }
        Glass::drawGlow (g, pos, size * 3.0f, satCol, alpha * 0.7f);
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.fillEllipse (pos.x - size * 0.5f, pos.y - size * 0.5f, size, size);
    };

    for (auto& e : echoes)
    {
        float k, alpha, angle;
        auto dir = 1.0f;
        if (e.ambient)
        {
            if (e.ring > rings) continue;
            k = (float) e.ring;
            alpha = 0.35f;
            angle = e.angle + (float) time * speed / std::pow (k, 0.7f);
        }
        else
        {
            auto age = (float) (time - e.born) / delay;
            auto idx = (int) age;
            if (idx < 1) continue;                               // not echoed yet
            auto move = juce::jlimit (0.0f, 1.0f, (age - (float) idx) / 0.25f);   // glide out to the next ring
            auto ringF = (float) idx - 1.0f + move;
            if (st.frozen) ringF = juce::jmin (ringF, (float) rings);
            if (st.mode == 4) ringF = (float) rings + 1.0f - ringF;              // REVERSE: inward
            k = juce::jlimit (1.0f, 8.0f, ringF);
            alpha = st.frozen ? 0.9f : std::pow (juce::jmax (0.01f, st.feedback), (float) (idx - 1));
            angle = e.angle + (float) (time - e.born) * speed / std::pow (k, 0.7f);
            if (st.mode == 3 && (idx & 1)) { angle += pi; dir = -1.0f; }        // PING-PONG: bounce sides
        }
        drawSatellite (onRing (k, angle), alpha, e.ambient ? 6.0f : 10.0f, k, angle, dir);
    }

    // planet = the original sound
    auto r = 30.0f * (1.0f + 0.12f * pulse);
    auto planetCol = p.isLight ? juce::Colour (0xffffd60a) : p.accent;
    Glass::drawGlow (g, c, r * 3.2f, planetCol, 0.45f + 0.35f * pulse);
    juce::ColourGradient body (planetCol.brighter (0.8f), c.x - r * 0.4f, c.y - r * 0.5f, planetCol.darker (0.6f), c.x + r, c.y + r, true);
    g.setGradientFill (body);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    juce::Path pr;
    pr.addEllipse (-r * 1.7f, -r * 0.38f, r * 3.4f, r * 0.76f);
    pr.applyTransform (juce::AffineTransform::rotation (-0.18f).translated (c.x, c.y));
    g.setColour ((p.isLight ? juce::Colours::white : p.nebulaC).withAlpha (0.85f));
    g.strokePath (pr, juce::PathStrokeType (2.0f));
}

// ================================================================== meter

void StereoMeter::setLevels (float l, float r)
{
    float in[2] = { l, r };
    for (int c = 0; c < 2; ++c)
    {
        auto db = juce::Decibels::gainToDecibels (in[c], -60.0f);
        auto v = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        lv[c] = v > lv[c] ? v : lv[c] * 0.9f + v * 0.1f;
        pk[c] = juce::jmax (v, pk[c] - 0.008f);
    }
    repaint();
}

void StereoMeter::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto col = p.isLight ? juce::Colour (0xfffff176) : p.accent;
    for (int c = 0; c < 2; ++c)
    {
        auto r = juce::Rectangle<float> ((float) c * 10.0f + 2.0f, 0.0f, 5.0f, (float) getHeight());
        g.setColour ((p.isLight ? juce::Colours::white : p.text).withAlpha (0.12f));
        g.fillRoundedRectangle (r, 2.5f);
        auto f = r.withTop (r.getBottom() - r.getHeight() * lv[c]);
        Glass::drawRectGlow (g, f, 2.5f, col, 4.0f, 0.6f * lv[c]);
        juce::ColourGradient grad (col.brighter (0.6f), 0, r.getY(), col, 0, r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (f, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillRect (r.getX() - 1.0f, r.getBottom() - r.getHeight() * pk[c] - 1.0f, r.getWidth() + 2.0f, 1.5f);
    }
}

// ================================================================== freeze

void FreezePill::paintButton (juce::Graphics& g, bool hover, bool down)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    auto on = getToggleState();
    auto glow = p.isLight ? juce::Colours::white : p.accent;
    if (on) Glass::drawRectGlow (g, r, r.getHeight() * 0.5f, glow, 12.0f, 1.0f);
    Glass::drawPanel (g, r, r.getHeight() * 0.5f, p, on ? 2.5f : (hover ? 1.8f : 1.2f), on);
    if (on) { g.setColour (glow.withAlpha (0.3f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f); }
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.1f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f); }

    juce::Path inf;
    auto ir = r.withSizeKeepingCentre (34.0f, 14.0f);
    for (int i = 0; i <= 120; ++i)
    {
        auto t = (float) i / 120.0f * 2.0f * pi;
        auto d = 1.0f + std::sin (t) * std::sin (t);
        auto pt = juce::Point<float> (ir.getCentreX() + std::cos (t) / d * ir.getWidth() * 0.5f,
                                      ir.getCentreY() + std::sin (t) * std::cos (t) / d * ir.getHeight());
        if (i == 0) inf.startNewSubPath (pt); else inf.lineTo (pt);
    }
    g.setColour (p.isLight ? (on ? juce::Colour (0xffffd60a) : juce::Colours::white) : juce::Colours::white);
    g.strokePath (inf, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace orbit
