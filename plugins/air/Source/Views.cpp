#include "Views.h"

namespace air
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    const juce::Colour white = juce::Colours::white;
    const juce::Colour purple (0xff7b2cff), violet (0xff5b21b6), deepViolet (0xff3b0d8a), lavender (0xffc4b0ff),
                       rimColour (0xfff3eeff), dim (0x99d6cfee), text (0xffeae6f5);

    float smoothTo (float current, float target, float k) { return current + k * (target - current); }

    /** Value-noise texture for planet surfaces. */
    float hash (int x, int y, int seed)
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
        auto a = hash (xi, yi, seed), b = hash (xi + 1, yi, seed), c = hash (xi, yi + 1, seed), d = hash (xi + 1, yi + 1, seed);
        return juce::jmap (yf, juce::jmap (xf, a, b), juce::jmap (xf, c, d));
    }
    float fbm (float x, float y, int seed)
    {
        float s = 0, amp = 0.5f, norm = 0;
        for (int i = 0; i < 5; ++i) { s += amp * noise (x, y, seed + i * 17); norm += amp; x *= 2.03f; y *= 2.03f; amp *= 0.5f; }
        return s / norm;
    }
}

// ================================================================== atmosphere

AtmosphereView::AtmosphereView()
{
    setInterceptsMouseClicks (false, false);
    spectrum.fill (0.0f);
}

AtmosphereView::Geometry AtmosphereView::geometry() const
{
    // a huge circle: its top (the HIGH AIR rim) sits at 18 % of the height, it leaves the panel at 68 % on the sides
    auto w = (float) getWidth(), h = (float) getHeight();
    auto top = h * 0.19f, side = h * 0.68f;
    auto half = w * 0.5f, sag = side - top;
    auto rim = (half * half + sag * sag) / (2.0f * sag);
    return { { half, top + rim }, rim, rim - h * 0.42f };
}

float AtmosphereView::surfaceY (float x) const
{
    auto g = geometry();
    auto dx = x - g.centre.x;
    return g.centre.y - std::sqrt (juce::jmax (0.0f, g.surface * g.surface - dx * dx));
}

void AtmosphereView::resized() { renderPlanet(); }

void AtmosphereView::renderPlanet()
{
    auto w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    planet = juce::Image (juce::Image::ARGB, w, h, true);
    auto g = geometry();
    juce::Image::BitmapData px (planet, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            auto dx = (float) x - g.centre.x, dy = (float) y - g.centre.y;
            auto d = std::sqrt (dx * dx + dy * dy);
            if (d > g.surface + 1.0f) continue;
            auto edge = juce::jlimit (0.0f, 1.0f, g.surface + 1.0f - d);           // anti-aliased limb
            auto depth = juce::jlimit (0.0f, 1.0f, (g.surface - d) / ((float) h * 0.45f));
            auto n = fbm ((float) x * 0.012f, (float) y * 0.035f, 7);               // continents / clouds, flattened by perspective
            auto n2 = fbm ((float) x * 0.05f, (float) y * 0.12f, 19);
            auto lightness = 0.05f + 0.10f * n + 0.04f * n2;
            lightness *= 0.55f + 0.45f * (1.0f - depth);                              // brighter towards the limb (scattered light)
            auto limbGlow = std::pow (1.0f - depth, 10.0f);
            auto c = juce::Colour::fromFloatRGBA (lightness * 0.85f + 0.25f * limbGlow, lightness * 0.8f + 0.08f * limbGlow,
                                                  lightness * 1.15f + 0.45f * limbGlow, edge);
            px.setPixelColour (x, y, c);
        }
}

void AtmosphereView::tick (const AtmosphereState& s)
{
    time += 1.0f / 30.0f;
    for (int i = 0; i < 3; ++i)
    {
        shown[i] = smoothTo (shown[i], s.bypassed ? 0.0f : s.amount[i], 0.25f);
        glow[i] = smoothTo (glow[i], s.bypassed ? 0.0f : juce::jlimit (0.0f, 1.0f, s.activity[i] * 2.5f), 0.2f);
    }
    lufs = s.lufs;
    bypassed = s.bypassed;
    if (s.level)
        for (int i = 0; i < points; ++i)
        {
            auto hz = 60.0f * std::pow (20000.0f / 60.0f, (float) i / (float) (points - 1));
            auto l = s.level (hz);
            spectrum[(size_t) i] = l > spectrum[(size_t) i] ? smoothTo (spectrum[(size_t) i], l, 0.6f) : smoothTo (spectrum[(size_t) i], l, 0.12f);
        }
    repaint();
}

void AtmosphereView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto geo = geometry();
    auto c = geo.centre;
    auto rim = geo.rim, surf = geo.surface;

    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (b, 14.0f);
        g.reduceClipRegion (clip);

        // space behind the horizon: a faint violet haze
        juce::ColourGradient haze (violet.withAlpha (0.0f), c.x, b.getY(), violet.withAlpha (0.12f), c.x, b.getBottom(), false);
        g.setGradientFill (haze);
        g.fillRect (b);

        // the three atmosphere layers, one radial gradient from the planet surface outwards
        auto outer = rim + 70.0f;
        auto at = [&] (float r) { return (double) juce::jlimit (0.0f, 1.0f, r / outer); };
        auto body = 0.45f + 0.55f * shown[0] + 0.25f * glow[0];
        auto mid = 0.55f + 0.6f * shown[1] + 0.25f * glow[1];
        auto high = 0.45f + 0.55f * shown[2] + 0.3f * glow[2];
        auto bodyH = (rim - surf) * (0.32f + 0.22f * shown[0]);        // the violet glow grows with BODY
        auto midIn = surf + (rim - surf) * (0.42f - 0.08f * shown[1]);
        juce::ColourGradient atmo (juce::Colours::transparentBlack, c.x, c.y, juce::Colours::transparentBlack, c.x + outer, c.y, true);
        atmo.addColour (at (surf - 2.0f), juce::Colours::transparentBlack);
        atmo.addColour (at (surf), juce::Colour (0xff8f4dff).withAlpha (juce::jmin (1.0f, 0.95f * body)));
        atmo.addColour (at (surf + bodyH * 0.45f), purple.withAlpha (juce::jmin (1.0f, 0.7f * body)));
        atmo.addColour (at (surf + bodyH), violet.withAlpha (juce::jmin (1.0f, 0.42f * body)));
        atmo.addColour (at (midIn), juce::Colour (0xff8e72e0).withAlpha (juce::jmin (1.0f, 0.38f * mid)));
        atmo.addColour (at (rim - 14.0f), lavender.withAlpha (juce::jmin (1.0f, 0.5f * mid)));
        atmo.addColour (at (rim - 3.0f), rimColour.withAlpha (juce::jmin (1.0f, 0.35f * mid + 0.25f * high)));
        atmo.addColour (at (rim), rimColour.withAlpha (juce::jmin (1.0f, 0.95f * high)));
        atmo.addColour (at (rim + 3.0f), lavender.withAlpha (0.35f * high));
        atmo.addColour (at (rim + 22.0f + 18.0f * shown[2]), purple.withAlpha (0.10f * high));
        atmo.addColour (1.0, purple.withAlpha (0.0f));
        g.setGradientFill (atmo);
        g.fillEllipse (juce::Rectangle<float> (outer * 2.0f, outer * 2.0f).withCentre (c));

        // HIGH AIR: thin bright rim line
        g.setColour (white.withAlpha (0.25f * high));
        g.drawEllipse (juce::Rectangle<float> (rim * 2.0f, rim * 2.0f).withCentre (c), 6.0f);
        g.setColour (rimColour.withAlpha (juce::jmin (1.0f, 0.55f + 0.45f * high)));
        g.drawEllipse (juce::Rectangle<float> (rim * 2.0f, rim * 2.0f).withCentre (c), 1.6f + 1.4f * shown[2]);
        // MID AIR: a second faint shell
        g.setColour (lavender.withAlpha (0.35f * mid));
        auto midR = (midIn + rim) * 0.5f + 6.0f;
        g.drawEllipse (juce::Rectangle<float> (midR * 2.0f, midR * 2.0f).withCentre (c), 1.0f);

        // the planet
        g.drawImageAt (planet, 0, 0);
        g.setColour (purple.withAlpha (0.55f + 0.4f * shown[0]));
        g.drawEllipse (juce::Rectangle<float> (surf * 2.0f, surf * 2.0f).withCentre (c), 1.2f);

        // live spectrum: a curve of purple stardust floating in the atmosphere
        juce::Path curve;
        std::array<juce::Point<float>, points> pts;
        for (int i = 0; i < points; ++i)
        {
            auto x = b.getX() + b.getWidth() * (float) i / (float) (points - 1);
            auto l = juce::jlimit (0.0f, 1.0f, (spectrum[(size_t) i] - 0.3f) / 0.6f);
            auto y = surfaceY (x) + 14.0f - l * (b.getHeight() * 0.36f);
            pts[(size_t) i] = { x, y };
        }
        curve.startNewSubPath (pts[0]);
        for (int i = 1; i < points - 1; ++i)
        {
            auto mid2 = (pts[(size_t) i] + pts[(size_t) i + 1]) * 0.5f;
            curve.quadraticTo (pts[(size_t) i], mid2);
        }
        curve.lineTo (pts[points - 1]);
        auto dust = bypassed ? 0.35f : 1.0f;
        g.setColour (purple.withAlpha (0.22f * dust));
        g.strokePath (curve, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (lavender.withAlpha (0.55f * dust));
        g.strokePath (curve, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Random rnd (17);
        for (int i = 0; i < 1700; ++i)
        {
            auto t = rnd.nextFloat() * (float) (points - 1);
            auto k = juce::jmin (points - 2, (int) t);
            auto pt = pts[(size_t) k] + (pts[(size_t) k + 1] - pts[(size_t) k]) * (t - (float) k);
            auto spread = (rnd.nextFloat() - 0.5f) * (rnd.nextFloat() * 22.0f);
            auto drift = std::sin (time * (0.6f + rnd.nextFloat()) + (float) i) * 2.0f;
            auto sz = 0.7f + rnd.nextFloat() * rnd.nextFloat() * 2.4f;
            auto a = (0.25f + 0.6f * rnd.nextFloat()) * (1.0f - std::abs (spread) / 14.0f) * dust;
            if (a <= 0.02f) continue;
            g.setColour ((rnd.nextFloat() < 0.25f ? white : lavender).withAlpha (juce::jlimit (0.0f, 1.0f, a)));
            g.fillEllipse (pt.x + drift - sz * 0.5f, pt.y + spread - sz * 0.5f, sz, sz);
        }
    }

    // labels in the layers
    auto label = [&] (const juce::String& t, float y, float alpha)
    {
        g.setColour (white.withAlpha (alpha));
        Fonts::drawSpaced (g, t, { b.getCentreX() - 120.0f, y, 240.0f, 22.0f }, Fonts::regular (15.5f), 0.04f, juce::Justification::centred);
    };
    auto h = b.getHeight();
    label ("HIGH AIR", h * 0.045f, 0.75f + 0.25f * shown[2]);
    label ("MID AIR", h * 0.278f, 0.7f + 0.3f * shown[1]);
    label ("BODY", h * 0.44f, 0.7f + 0.3f * shown[0]);

    // scale + loudness
    g.setColour (white.withAlpha (0.55f));
    g.setFont (Fonts::regular (13.0f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("100  \xc2\xb7  500  \xc2\xb7  1k  \xc2\xb7  5k  \xc2\xb7  10k  \xc2\xb7  20k")),
                juce::Rectangle<float> (b.getCentreX() - 200.0f, b.getBottom() - 26.0f, 400.0f, 18.0f), juce::Justification::centred);

    auto value = lufs > -70.0f ? juce::String (lufs, 1) + " LUFS" : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94 LUFS"));
    auto vf = Fonts::regular (13.5f);
    auto vw = juce::GlyphArrangement::getStringWidth (vf, value);
    auto area = juce::Rectangle<float> (b.getRight() - 16.0f - vw, 10.0f, vw + 2.0f, 18.0f);
    g.setColour (white.withAlpha (0.9f));
    g.setFont (vf);
    g.drawText (value, area, juce::Justification::centredLeft);
    g.setColour (white.withAlpha (0.55f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("Input \xc2\xb7 ")), area.withX (area.getX() - 120.0f).withWidth (118.0f), juce::Justification::centredRight);

    // glass edge of the panel
    g.setColour (white.withAlpha (0.28f));
    g.drawRoundedRectangle (b.reduced (0.5f), 14.0f, 1.0f);
}

// ================================================================== planet knob

PlanetKnob::PlanetKnob (int s) : seed (s)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    setVelocityBasedMode (false);
    setMouseDragSensitivity (260);
    setDoubleClickReturnValue (false, 0.0);
}

juce::Point<float> PlanetKnob::centre() const { return { (float) getWidth() * 0.5f, (float) getHeight() * 0.47f }; }
float PlanetKnob::radius() const              { return juce::jmin ((float) getWidth() * 0.39f, (float) getHeight() * 0.45f); }

void PlanetKnob::renderTexture (float r)
{
    auto d = (int) std::ceil (r * 2.0f) + 2;
    texture = juce::Image (juce::Image::ARGB, d, d, true);
    juce::Image::BitmapData px (texture, juce::Image::BitmapData::writeOnly);
    auto cc = (float) d * 0.5f;
    for (int y = 0; y < d; ++y)
        for (int x = 0; x < d; ++x)
        {
            auto dx = ((float) x - cc) / r, dy = ((float) y - cc) / r;
            auto rr = dx * dx + dy * dy;
            if (rr > 1.02f) continue;
            auto edge = juce::jlimit (0.0f, 1.0f, (1.0f - std::sqrt (rr)) * r + 0.5f);
            auto z = std::sqrt (juce::jmax (0.0f, 1.0f - rr));
            // sphere mapping so the texture wraps around the ball
            auto u = std::atan2 (dx, z) * 1.3f, v = dy * 1.6f;
            auto n = fbm (u * 2.4f + (float) seed, v * 2.4f, seed);
            auto lightDir = juce::jmax (0.0f, -0.55f * dx - 0.65f * dy + 0.5f * z);    // lit from the top-left
            auto rimLight = std::pow (1.0f - z, 3.0f) * juce::jmax (0.0f, 0.4f + 0.6f * (dx - dy) * 0.7f);   // purple rim, bottom-right
            auto base = 0.07f + 0.09f * n;
            auto lr = base * (0.5f + 0.9f * lightDir) + 0.25f * rimLight;
            auto lg = base * (0.45f + 0.8f * lightDir) + 0.08f * rimLight;
            auto lb = base * (0.7f + 1.1f * lightDir) + 0.55f * rimLight;
            px.setPixelColour (x, y, juce::Colour::fromFloatRGBA (juce::jmin (1.0f, lr), juce::jmin (1.0f, lg), juce::jmin (1.0f, lb), edge));
        }
}

void PlanetKnob::paint (juce::Graphics& g)
{
    auto c = centre();
    auto ringR = radius();
    auto planetR = ringR * 0.8f;
    if (texture.isNull() || std::abs ((float) texture.getWidth() - (std::ceil (planetR * 2.0f) + 2.0f)) > 0.5f)
        renderTexture (planetR);

    auto params = getRotaryParameters();
    auto norm = (float) valueToProportionOfLength (getValue());
    auto hot = isMouseOverOrDragging() ? 1.0f : 0.0f;
    auto angle = params.startAngleRadians + norm * (params.endAngleRadians - params.startAngleRadians);

    Glass::drawGlow (g, c, planetR * 1.6f, purple, 0.16f + 0.2f * norm + 0.08f * hot);

    // track + value arc
    juce::Path track;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, params.startAngleRadians, params.endAngleRadians, true);
    g.setColour (white.withAlpha (0.16f));
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (norm > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, params.startAngleRadians, angle, true);
        g.setColour (purple.withAlpha (0.35f));
        g.strokePath (arc, juce::PathStrokeType (11.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffa27bff).withAlpha (0.8f));
        g.strokePath (arc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffe6dbff));
        g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    auto dot = c.getPointOnCircumference (ringR, angle);
    Glass::drawGlow (g, dot, 12.0f, juce::Colour (0xffc9b6ff), 0.8f);
    g.setColour (white);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (dot));

    // the planet
    g.drawImageAt (texture, juce::roundToInt (c.x - (float) texture.getWidth() * 0.5f), juce::roundToInt (c.y - (float) texture.getHeight() * 0.5f));
    g.setColour (juce::Colour (0xffb79cff).withAlpha (0.25f + 0.2f * hot));
    g.drawEllipse (juce::Rectangle<float> (planetR * 2.0f, planetR * 2.0f).withCentre (c), 1.0f);

    // value
    g.setColour (white);
    g.setFont (Fonts::light (planetR * 0.82f));
    g.drawText (juce::String (juce::roundToInt (getValue())), juce::Rectangle<float> (planetR * 2.0f, planetR).withCentre (c),
                juce::Justification::centred, false);

    // 0 and 100
    g.setColour (white.withAlpha (0.6f));
    g.setFont (Fonts::regular (13.0f));
    auto p0 = c.getPointOnCircumference (ringR, params.startAngleRadians);
    auto p1 = c.getPointOnCircumference (ringR, params.endAngleRadians);
    g.drawText ("0", juce::Rectangle<float> (40.0f, 16.0f).withCentre (p0.translated (-4.0f, 10.0f)), juce::Justification::centred);
    g.drawText ("100", juce::Rectangle<float> (40.0f, 16.0f).withCentre (p1.translated (4.0f, 10.0f)), juce::Justification::centred);
}

// ================================================================== model pills

ModelPills::ModelPills (const juce::StringArray& n) : names (n) {}

void ModelPills::resized()
{
    boxes.clear();
    auto f = Fonts::regular (12.5f);
    std::vector<float> widths;
    float total = 0;
    const float gap = 7.0f;
    for (auto& n : names)
    {
        auto w = juce::GlyphArrangement::getStringWidth (f, n) + (float) n.length() * 0.6f + 26.0f;
        widths.push_back (w);
        total += w;
    }
    total += gap * (float) (names.size() - 1);
    auto scale = juce::jmin (1.0f, (float) getWidth() / total);
    auto x = ((float) getWidth() - total * scale) * 0.5f;
    for (auto w : widths)
    {
        boxes.push_back ({ x, 1.0f, w * scale, (float) getHeight() - 2.0f });
        x += (w + gap) * scale;
    }
}

int ModelPills::hit (juce::Point<float> p) const
{
    for (size_t i = 0; i < boxes.size(); ++i)
        if (boxes[i].expanded (3.0f, 2.0f).contains (p)) return (int) i;
    return -1;
}

void ModelPills::mouseDown (const juce::MouseEvent& e)
{
    auto i = hit (e.position);
    if (i >= 0 && onSelect) onSelect (i);
}

void ModelPills::mouseMove (const juce::MouseEvent& e)
{
    auto i = hit (e.position);
    if (i != hover) { hover = i; repaint(); }
    setMouseCursor (i >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void ModelPills::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

void ModelPills::paint (juce::Graphics& g)
{
    for (size_t i = 0; i < boxes.size(); ++i)
    {
        auto r = boxes[i];
        auto corner = r.getHeight() * 0.5f;
        auto on = (int) i == selected;
        if (on)
        {
            Glass::drawRectGlow (g, r, corner, purple, 8.0f, 0.7f);
            juce::ColourGradient fill (juce::Colour (0xff6d3fd6).withAlpha (0.75f), r.getX(), r.getY(),
                                       juce::Colour (0xff4a1fa8).withAlpha (0.75f), r.getX(), r.getBottom(), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (r, corner);
            g.setColour (juce::Colour (0xffc9b6ff).withAlpha (0.85f));
            g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
        }
        else
        {
            g.setColour (white.withAlpha ((int) i == hover ? 0.14f : 0.08f));
            g.fillRoundedRectangle (r, corner);
            g.setColour (white.withAlpha (0.28f));
            g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
        }
        g.setColour (white.withAlpha (on ? 1.0f : 0.78f));
        Fonts::drawSpaced (g, names[(int) i], r, Fonts::regular (12.5f), 0.04f, juce::Justification::centred);
    }
}

// ================================================================== small knobs

void SmallKnob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto d = juce::jmin (b.getWidth(), b.getHeight()) - 4.0f;
    auto c = b.getCentre();
    auto r = d * 0.5f;
    auto params = getRotaryParameters();
    auto norm = (float) valueToProportionOfLength (getValue());
    auto angle = params.startAngleRadians + norm * (params.endAngleRadians - params.startAngleRadians);
    auto hot = isMouseOverOrDragging();

    // dark ball
    juce::ColourGradient ball (juce::Colour (0xff2a2338), c.x - r * 0.4f, c.y - r * 0.5f, juce::Colour (0xff050409), c.x + r * 0.5f, c.y + r * 0.7f, true);
    g.setGradientFill (ball);
    g.fillEllipse (juce::Rectangle<float> (r * 1.6f, r * 1.6f).withCentre (c));
    g.setColour (white.withAlpha (hot ? 0.3f : 0.16f));
    g.drawEllipse (juce::Rectangle<float> (r * 1.6f, r * 1.6f).withCentre (c), 1.0f);

    // arc
    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, params.startAngleRadians, params.endAngleRadians, true);
    g.setColour (white.withAlpha (0.12f));
    g.strokePath (track, juce::PathStrokeType (2.0f));
    auto from = bipolar ? (params.startAngleRadians + params.endAngleRadians) * 0.5f : params.startAngleRadians;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (purple.withAlpha (0.4f));
        g.strokePath (arc, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffd9caff));
        g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    auto dot = c.getPointOnCircumference (r, angle);
    g.setColour (white);
    g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (dot));

    if (inner.isNotEmpty())
    {
        g.setColour (white.withAlpha (0.9f));
        g.setFont (Fonts::regular (11.5f));
        g.drawText (inner, juce::Rectangle<float> (r * 2.0f, 14.0f).withCentre (c.translated (0.0f, -6.0f)), juce::Justification::centred);
        g.setFont (Fonts::regular (10.5f));
        g.drawText (juce::String (juce::roundToInt (getValue())) + "%", juce::Rectangle<float> (r * 2.0f, 14.0f).withCentre (c.translated (0.0f, 7.0f)),
                    juce::Justification::centred);
    }
}

// ================================================================== meters

void BarMeter::setLevel (float peak, float dt)
{
    auto db = juce::Decibels::gainToDecibels (peak, -60.0f);
    auto target = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    level = target > level ? target : juce::jmax (target, level - dt * 0.9f);     // fast up, ~1.1 s down
    if (level >= hold) { hold = level; holdTime = 1.2f; }
    else if ((holdTime -= dt) <= 0.0f) hold = juce::jmax (level, hold - dt * 0.5f);
    if (peak >= 0.999f) clip = 1.0f;
    repaint();
}

void BarMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto bar = b.withTrimmedRight (14.0f).withSizeKeepingCentre (b.getWidth() - 14.0f, 6.0f);
    g.setColour (white.withAlpha (0.12f));
    g.fillRoundedRectangle (bar, 3.0f);
    if (level > 0.001f)
    {
        auto fill = bar.withWidth (bar.getWidth() * level);
        juce::ColourGradient grad (juce::Colour (0xff5b21b6), bar.getX(), 0, juce::Colour (0xffd6c6ff), bar.getRight(), 0, false);
        grad.addColour (0.6, juce::Colour (0xff9d6bff));
        Glass::drawRectGlow (g, fill, 3.0f, purple, 4.0f, 0.45f);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 3.0f);
    }
    auto hx = bar.getX() + bar.getWidth() * hold;
    g.setColour (white.withAlpha (0.9f));
    g.fillRect (juce::Rectangle<float> (2.0f, bar.getHeight() + 4.0f).withCentre ({ juce::jmax (bar.getX() + 1.0f, hx - 1.0f), bar.getCentreY() }));
    g.setColour (clip > 0.5f ? juce::Colour (0xfff87171) : juce::Colour (0x66f87171));
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ b.getRight() - 4.0f, bar.getCentreY() }));
}

// ================================================================== buttons

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
            path.addCentredArc (c.x, c.y + s * 0.05f, s * 0.7f, s * 0.7f, 0.0f, dir > 0 ? -pi * 0.6f : pi * 0.6f, dir > 0 ? pi * 0.85f : -pi * 0.85f, true);
            g.strokePath (path, stroke);
            auto tip = c.translated (0.0f, s * 0.05f).getPointOnCircumference (s * 0.7f, dir > 0 ? -pi * 0.6f : pi * 0.6f);
            juce::Path head;
            head.startNewSubPath (tip.translated (dir * s * 0.1f, -s * 0.4f));
            head.lineTo (tip);
            head.lineTo (tip.translated (dir * s * 0.42f, s * 0.04f));
            g.strokePath (head, stroke);
            break;
        }
        case Icon::settings:
        {
            const int teeth = 8;
            for (int i = 0; i < teeth * 2; ++i)
            {
                auto a = (float) i / (float) (teeth * 2) * 2.0f * pi;
                auto rad = (i % 2 == 0) ? s * 0.78f : s * 0.6f;
                auto p1 = c.getPointOnCircumference (rad, a), p2 = c.getPointOnCircumference (rad, a + pi / (float) teeth);
                if (i == 0) path.startNewSubPath (p1); else path.lineTo (p1);
                path.lineTo (p2);
            }
            path.closeSubPath();
            g.strokePath (path, juce::PathStrokeType (th * 0.85f, juce::PathStrokeType::mitered));
            g.drawEllipse (c.x - s * 0.26f, c.y - s * 0.26f, s * 0.52f, s * 0.52f, th * 0.85f);
            break;
        }
        case Icon::power:
            path.addCentredArc (c.x, c.y + s * 0.06f, s * 0.68f, s * 0.68f, 0.0f, pi * 0.22f, pi * 1.78f, true);
            g.strokePath (path, stroke);
            g.drawLine (c.x, c.y - s * 0.78f, c.x, c.y - s * 0.08f, th);
            break;
        case Icon::prev:
        case Icon::next:
        {
            auto d = icon == Icon::prev ? -1.0f : 1.0f;
            path.startNewSubPath (c.x - d * s * 0.2f, c.y - s * 0.42f);
            path.lineTo (c.x + d * s * 0.22f, c.y);
            path.lineTo (c.x - d * s * 0.2f, c.y + s * 0.42f);
            g.strokePath (path, stroke);
            break;
        }
    }
}

void IconButton::paintButton (juce::Graphics& g, bool hover, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    auto col = white.withAlpha (isEnabled() ? (hover ? 1.0f : 0.85f) : 0.3f);
    if (round)
    {
        auto d = juce::jmin (b.getWidth(), b.getHeight());
        auto disc = b.withSizeKeepingCentre (d, d);
        if (lit) Glass::drawGlow (g, disc.getCentre(), d * 0.85f, purple, 0.55f);
        juce::ColourGradient fill (juce::Colour (lit ? 0xff6d3fd6 : 0xff3a3446), disc.getX(), disc.getY(),
                                   juce::Colour (lit ? 0xff3b0d8a : 0xff1a1622), disc.getX(), disc.getBottom(), false);
        g.setGradientFill (fill);
        g.fillEllipse (disc);
        g.setColour (white.withAlpha (hover ? 0.45f : 0.28f));
        g.drawEllipse (disc.reduced (0.5f), 1.0f);
        b = disc.reduced (d * 0.27f);
    }
    else if (icon == Icon::power && ! lit)
        col = col.withAlpha (0.4f);
    if (down) b = b.reduced (1.0f);
    drawIcon (g, icon, b, col, 1.7f);
}

void ABButton::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto half = b.getWidth() * 0.5f;
    auto f = Fonts::regular (19.0f);
    for (int i = 0; i < 2; ++i)
    {
        auto r = juce::Rectangle<float> (b.getX() + (float) i * half, b.getY(), half, b.getHeight());
        g.setColour (white.withAlpha (i == slot ? 1.0f : 0.4f));
        g.setFont (f);
        g.drawText (i == 0 ? "A" : "B", r, juce::Justification::centred);
    }
    g.setColour (white.withAlpha (0.35f));
    g.fillRect (juce::Rectangle<float> (1.0f, b.getHeight() * 0.6f).withCentre (b.getCentre()));
}

void ABButton::mouseDown (const juce::MouseEvent& e)
{
    auto s = e.position.x < (float) getWidth() * 0.5f ? 0 : 1;
    if (s == slot) s = 1 - slot;        // clicking the lit letter toggles too
    if (onSelect) onSelect (s);
}

void PillButton::paintButton (juce::Graphics& g, bool hover, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    auto corner = r.getHeight() * 0.5f;
    auto on = getToggleState();
    if (on)
    {
        Glass::drawRectGlow (g, r, corner, purple, 8.0f, 0.75f);
        g.setColour (juce::Colour (0xff6d3fd6).withAlpha (0.8f));
        g.fillRoundedRectangle (r, corner);
        g.setColour (juce::Colour (0xffd9caff));
    }
    else
    {
        g.setColour (white.withAlpha (hover ? 0.16f : 0.09f));
        g.fillRoundedRectangle (r, corner);
        g.setColour (white.withAlpha (0.35f));
    }
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.15f)); g.fillRoundedRectangle (r, corner); }
    g.setColour (white.withAlpha (0.92f));
    Fonts::drawSpaced (g, getButtonText(), r, Fonts::regular (fontSize), 0.05f, juce::Justification::centred);
}

void PresetName::paintButton (juce::Graphics& g, bool hover, bool)
{
    g.setColour (white.withAlpha (hover ? 1.0f : 0.94f));
    g.setFont (Fonts::regular (22.0f));
    g.drawFittedText (text, getLocalBounds(), juce::Justification::centred, 1, 0.7f);
}

void drawLogo (juce::Graphics& g, juce::Rectangle<float> area)
{
    lt::ui::Logo::draw (g, area, lt::ui::Logo::specFor ("AIR"), Palette::dark(), juce::Justification::centredLeft);
}

} // namespace air
