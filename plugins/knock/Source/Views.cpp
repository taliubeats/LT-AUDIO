#include "Views.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace knock
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    juce::Image texClouds, texDetail;
    constexpr int texW = 512, texH = 256;

    float hash2 (int x, int y, int seed)
    {
        auto h = (juce::uint32) x * 374761393u + (juce::uint32) y * 668265263u + (juce::uint32) seed * 1442695041u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return (float) (h & 0xffffff) / (float) 0xffffff;
    }
    float noise (float x, float y, int seed, int wrapX)
    {
        auto xi = (int) std::floor (x), yi = (int) std::floor (y);
        auto xf = x - (float) xi, yf = y - (float) yi;
        xf = xf * xf * (3 - 2 * xf); yf = yf * yf * (3 - 2 * yf);
        auto w = [wrapX] (int v) { return ((v % wrapX) + wrapX) % wrapX; };
        auto a = hash2 (w (xi), yi, seed), b = hash2 (w (xi + 1), yi, seed), c = hash2 (w (xi), yi + 1, seed), d = hash2 (w (xi + 1), yi + 1, seed);
        return juce::jmap (yf, juce::jmap (xf, a, b), juce::jmap (xf, c, d));
    }
    float fbm (float x, float y, int seed, int wrapX)
    {
        float s = 0, a = 0.5f, n = 0;
        for (int i = 0; i < 5; ++i) { s += a * noise (x, y, seed + i * 13, wrapX); n += a; x *= 2; y *= 2; wrapX *= 2; a *= 0.5f; }
        return s / n;
    }
}

// ================================================================== knob

KnockKnob::KnockKnob (const juce::String& l, float d) : label (l), diameter (d)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    setMouseDragSensitivity (200);
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<float> KnockKnob::knobArea() const
{
    auto b = getLocalBounds().toFloat();
    return juce::Rectangle<float> (diameter, diameter).withCentre ({ b.getCentreX(), b.getY() + diameter * 0.5f + 8.0f });
}

bool KnockKnob::hitTest (int x, int y)
{
    auto k = knobArea();
    return k.getCentre().getDistanceFrom ({ (float) x, (float) y }) < diameter * 0.56f;
}

void KnockKnob::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto k = knobArea();
    lt::ui::GlassLookAndFeel::drawGlassKnob (g, k, (float) valueToProportionOfLength (getValue()), p, 0.9f);
    auto show = isMouseOverOrDragging();
    g.setColour (show ? p.value : p.text);
    g.setFont (Fonts::regular (diameter > 60 ? 15.5f : 14.0f));
    g.drawFittedText (show ? getTextFromValue (getValue()) : label, getLocalBounds().withTop ((int) k.getBottom() + 6),
                      juce::Justification::centredTop, 1);
}

// ================================================================== planet

void PlanetView::buildTexture()
{
    if (texClouds.isValid()) return;
    texClouds = juce::Image (juce::Image::SingleChannel, texW, texH, true);
    texDetail = juce::Image (juce::Image::SingleChannel, texW, texH, true);
    juce::Image::BitmapData a (texClouds, juce::Image::BitmapData::writeOnly);
    juce::Image::BitmapData b (texDetail, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < texH; ++y)
        for (int x = 0; x < texW; ++x)
        {
            auto u = (float) x / (float) texW * 8.0f, v = (float) y / (float) texH * 4.0f;
            auto warp = fbm (u * 0.7f, v * 1.4f, 5, 6);
            auto n = fbm (u + warp * 2.0f, v * 1.8f + warp, 9, 8);
            auto m = fbm (u * 2.0f + 3.0f, v * 3.0f + warp * 2.0f, 21, 16);
            a.setPixelColour (x, y, juce::Colours::white.withAlpha (juce::jlimit (0.0f, 1.0f, (n - 0.3f) * 1.8f)));
            b.setPixelColour (x, y, juce::Colours::white.withAlpha (juce::jlimit (0.0f, 1.0f, std::pow (juce::jmax (0.0f, m - 0.5f) * 2.6f, 1.4f))));
        }
}

PlanetView::PlanetView()
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    setMouseDragSensitivity (280);
}

bool PlanetView::hitTest (int x, int y)
{
    return getLocalBounds().toFloat().getCentre().getDistanceFrom ({ (float) x, (float) y }) < 125.0f;
}

void PlanetView::burst()
{
    for (int i = 0; i < 160; ++i)
    {
        auto a = random.nextFloat() * 2.0f * pi;
        auto sp = 2.0f + random.nextFloat() * 7.0f;
        dust.push_back ({ a, 95.0f, 0, 1.0f + random.nextFloat() * 2.2f, 1.0f, { std::cos (a) * sp, std::sin (a) * sp * 0.8f }, true });
    }
    pulse = 1.0f;
}

void PlanetView::tick (float level, int hits)
{
    auto v = (float) valueToProportionOfLength (getValue());
    time += 1.0 / 30.0;
    spin += 0.004f + 0.006f * v;
    if (hits != lastHits) pulse = juce::jmax (pulse, 0.4f + 0.6f * v);
    lastHits = hits;
    pulse = juce::jmax (pulse * 0.88f, level * v);

    // dust orbiting in the ring (more as the ring appears)
    auto ringAmt = juce::jlimit (0.0f, 1.0f, (v - 0.55f) / 0.25f);
    auto births = (int) (ringAmt * 3.0f + random.nextFloat());
    for (int i = 0; i < births && dust.size() < 500; ++i)
        dust.push_back ({ random.nextFloat() * 2.0f * pi, 1.0f + random.nextFloat() * 0.75f, 0.006f + random.nextFloat() * 0.01f,
                          0.6f + random.nextFloat() * 1.6f, 1.0f, {}, false });
    for (auto& d : dust)
    {
        if (d.free) { d.radius += d.vel.getDistanceFromOrigin(); d.vel *= 0.96f; d.life -= 0.02f; }
        else { d.angle += d.speed * (1.0f + pulse); d.life -= 0.006f; }
    }
    dust.erase (std::remove_if (dust.begin(), dust.end(), [] (const Dust& d) { return d.life <= 0; }), dust.end());
    repaint();
}

void PlanetView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    buildTexture();
    auto& p = *palette;
    auto v = (float) valueToProportionOfLength (getValue());
    auto c = getLocalBounds().toFloat().getCentre();
    auto R = 88.0f * (1.0f + 0.03f * pulse);
    auto sm = [v] (float a, float b) { auto t = juce::jlimit (0.0f, 1.0f, (v - a) / (b - a)); return t * t * (3 - 2 * t); };
    auto nebulaAmt = sm (0.05f, 0.35f), planetAmt = sm (0.35f, 0.6f), ringAmt = sm (0.6f, 0.8f), litAmt = sm (0.85f, 1.0f);
    auto body = p.isLight ? juce::Colour (0xffffd60a) : p.accent;
    auto hi = p.isLight ? juce::Colours::white : p.nebulaC;

    Glass::drawGlow (g, c, R * (1.5f + 0.5f * litAmt + 0.3f * pulse), body, 0.12f + 0.25f * planetAmt + 0.3f * litAmt * (0.6f + 0.4f * pulse));

    // ring: the back half (behind the planet)
    auto ringPath = [&] (bool front)
    {
        juce::Path rp;
        auto rx = R * 2.05f, ry = R * 0.42f;
        for (int i = 0; i <= 90; ++i)
        {
            auto a = (front ? 0.0f : pi) + pi * (float) i / 90.0f;
            auto pt = juce::Point<float> (rx * std::cos (a), ry * std::sin (a));
            if (i == 0) rp.startNewSubPath (pt); else rp.lineTo (pt);
        }
        rp.applyTransform (juce::AffineTransform::rotation (-0.28f).translated (c.x, c.y));
        return rp;
    };
    auto drawRing = [&] (bool front)
    {
        if (ringAmt <= 0.01f) return;
        auto rp = ringPath (front);
        g.setColour (hi.withAlpha (0.18f * ringAmt));
        g.strokePath (rp, juce::PathStrokeType (14.0f));
        g.setColour (hi.withAlpha (0.55f * ringAmt));
        g.strokePath (rp, juce::PathStrokeType (3.0f));
        g.setColour (juce::Colours::white.withAlpha (0.7f * ringAmt));
        g.strokePath (rp, juce::PathStrokeType (1.0f));
        for (auto& d : dust)
        {
            if (d.free) continue;
            auto front2 = std::sin (d.angle) > 0;
            if (front2 != front) continue;
            auto rx = R * 2.05f * d.radius * 0.95f, ry = R * 0.42f * d.radius * 0.95f;
            auto pt = juce::Point<float> (rx * std::cos (d.angle), ry * std::sin (d.angle))
                          .transformedBy (juce::AffineTransform::rotation (-0.28f).translated (c.x, c.y));
            g.setColour (hi.withAlpha (d.life * 0.8f * ringAmt));
            g.fillEllipse (pt.x - d.size * 0.5f, pt.y - d.size * 0.5f, d.size, d.size);
        }
    };
    drawRing (false);

    // sphere
    auto sphere = juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addEllipse (sphere);
        g.reduceClipRegion (clip);

        // empty glass sphere
        g.setColour ((p.isLight ? juce::Colours::white : juce::Colour (0xff1a1030)).withAlpha (0.25f + 0.55f * planetAmt));
        g.fillEllipse (sphere);
        if (planetAmt > 0.01f)
        {
            juce::ColourGradient base (body.darker (0.2f).withAlpha (planetAmt), c.x - R * 0.4f, c.y - R * 0.4f,
                                       body.darker (0.9f).withAlpha (planetAmt), c.x + R, c.y + R, true);
            g.setGradientFill (base);
            g.fillEllipse (sphere);
        }

        // nebula / planet clouds scrolling = rotation
        auto scroll = std::fmod (spin, 1.0f) * R * 4.0f;
        auto tex = [&] (const juce::Image& img, juce::Colour col, float alpha, float scale, float off)
        {
            if (alpha <= 0.01f) return;
            g.setColour (col.withAlpha (juce::jlimit (0.0f, 1.0f, alpha)));
            auto w = R * 4.0f * scale, h = R * 2.0f * scale;
            for (int k = -1; k <= 1; ++k)
            {
                auto x0 = c.x - R - std::fmod (scroll * scale + off, w) + (float) k * w;
                g.drawImageTransformed (img, juce::AffineTransform::scale (w / (float) texW, h / (float) texH)
                                                 .translated (x0, c.y - h * 0.5f), true);
            }
        };
        tex (texClouds, hi, nebulaAmt * (0.55f + 0.35f * planetAmt), 1.0f, 0.0f);
        tex (texDetail, juce::Colours::white, nebulaAmt * 0.35f + planetAmt * 0.45f, 1.0f, R * 0.7f);
        tex (texClouds, body.brighter (0.4f), planetAmt * 0.5f, 0.6f, R * 1.3f);

        // 3D shading: dark limb + shadow side, highlight top-left
        juce::ColourGradient limb (juce::Colours::transparentBlack, c.x - R * 0.3f, c.y - R * 0.3f,
                                   juce::Colours::black.withAlpha (0.65f * (0.3f + 0.7f * planetAmt)), c.x + R * 1.05f, c.y + R * 1.05f, true);
        limb.addColour (0.55, juce::Colours::transparentBlack);
        g.setGradientFill (limb);
        g.fillEllipse (sphere);
        juce::ColourGradient spec (juce::Colours::white.withAlpha (0.35f), c.x - R * 0.45f, c.y - R * 0.5f,
                                   juce::Colours::transparentWhite, c.x - R * 0.45f + R * 0.7f, c.y - R * 0.5f, true);
        g.setGradientFill (spec);
        g.fillEllipse (sphere);

        if (litAmt > 0.01f)
        {
            g.setColour (hi.withAlpha (0.25f * litAmt * (0.5f + 0.5f * pulse)));
            g.fillEllipse (sphere);
        }
    }
    g.setColour ((p.isLight ? juce::Colours::white : hi).withAlpha (0.55f + 0.3f * litAmt));
    g.drawEllipse (sphere.reduced (0.5f), 1.4f);

    drawRing (true);

    // value arc around everything
    auto arcR = 118.0f;
    auto start = pi * 1.25f, end = pi * 2.75f, angle = start + v * (end - start);
    juce::Path track, arc;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0, start, end, true);
    g.setColour (p.text.withAlpha (0.14f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    arc.addCentredArc (c.x, c.y, arcR, arcR, 0, start, angle, true);
    auto ac = p.isLight ? juce::Colours::white : p.accent;
    g.setColour (ac.withAlpha (0.3f));
    g.strokePath (arc, juce::PathStrokeType (11.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (ac);
    g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // CHAOS burst particles
    for (auto& d : dust)
        if (d.free)
        {
            auto pos = c + juce::Point<float> (std::cos (d.angle), std::sin (d.angle) * 0.8f) * d.radius;
            g.setColour (hi.withAlpha (juce::jlimit (0.0f, 1.0f, d.life)));
            g.fillEllipse (pos.x - d.size * 0.5f, pos.y - d.size * 0.5f, d.size, d.size);
        }

    auto txt = juce::String (juce::roundToInt (getValue())) + "%";
    g.setFont (Fonts::light (50.0f));
    if (p.isLight)
    {
        g.setColour (juce::Colour (0xff5a4300).withAlpha (0.55f));
        g.drawText (txt, sphere.translated (0, 2.0f), juce::Justification::centred);
    }
    g.setColour (p.isLight ? juce::Colours::white : p.value);
    g.drawText (txt, sphere, juce::Justification::centred);
}

// ================================================================== small parts

void DistTypeList::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto h = (float) getHeight() / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        auto r = juce::Rectangle<float> (0, (float) i * h, (float) getWidth(), h);
        auto dot = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ 6.0f, r.getCentreY() });
        auto on = i == selected;
        if (on) Glass::drawGlow (g, dot.getCentre(), 9.0f, p.accent, 0.8f);
        g.setColour (on ? (p.isLight ? juce::Colours::white : p.accent.brighter (0.4f)) : p.text.withAlpha (0.35f));
        if (on) g.fillEllipse (dot); else g.drawEllipse (dot, 1.0f);
        g.setColour (on ? p.text : p.text.withAlpha (0.6f));
        g.setFont (Fonts::regular (12.5f));
        g.drawText (juce::StringArray ({ "SOFT", "HARD", "FUZZ", "TUBE" })[i], r.withTrimmedLeft (16.0f), juce::Justification::centredLeft);
    }
}

void DistTypeList::mouseUp (const juce::MouseEvent& e)
{
    auto i = juce::jlimit (0, 3, (int) (e.position.y / ((float) getHeight() / 4.0f)));
    if (onChange) onChange (i);
}

void LockButton::paintButton (juce::Graphics& g, bool hover, bool)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto on = getToggleState();
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    auto col = on ? (p.isLight ? juce::Colours::white : p.accent.brighter (0.5f)) : p.text.withAlpha (hover ? 0.6f : 0.3f);
    auto bodyR = r.withTop (r.getCentreY() - 1.0f);
    if (on) Glass::drawGlow (g, bodyR.getCentre(), 10.0f, p.accent, 0.7f);
    g.setColour (col);
    g.fillRoundedRectangle (bodyR, 1.5f);
    juce::Path shackle;
    auto sw = bodyR.getWidth() * 0.62f;
    auto sx = bodyR.getCentreX() - sw * 0.5f;
    shackle.startNewSubPath (sx, bodyR.getY());
    shackle.lineTo (sx, r.getY() + sw * 0.5f);
    shackle.addCentredArc (bodyR.getCentreX(), r.getY() + sw * 0.5f, sw * 0.5f, sw * 0.5f, 0, -pi * 0.5f, on ? pi * 0.5f : pi * 0.3f);
    g.strokePath (shackle, juce::PathStrokeType (1.6f));
}

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
        auto r = juce::Rectangle<float> ((float) c * 14.0f + 2.0f, 0.0f, 7.0f, (float) getHeight());
        Glass::drawPanel (g, r, 3.5f, p, 1.0f);
        auto f = r.reduced (1.5f).withTop (r.getBottom() - r.getHeight() * lv[c]);
        Glass::drawRectGlow (g, f, 2.5f, col, 4.0f, 0.6f * lv[c]);
        juce::ColourGradient grad (col.brighter (0.6f), 0, r.getY(), col, 0, r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (f, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.fillRect (r.getX(), r.getBottom() - r.getHeight() * pk[c] - 1.0f, r.getWidth(), 1.5f);
    }
}

} // namespace knock
