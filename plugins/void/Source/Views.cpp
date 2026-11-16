#include "Views.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace lt_void
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr int mapSize = 440;
    constexpr float holeFrac = 0.21f;           // hole radius / disk radius
    juce::Image mapMain, mapFine, mapDust;

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

    float fbm (float x, float y, int seed, int oct)
    {
        float s = 0, a = 0.5f, n = 0;
        for (int i = 0; i < oct; ++i) { s += a * noise (x, y, seed + i * 17); n += a; x *= 2.07f; y *= 2.07f; a *= 0.5f; }
        return s / n;
    }

}

void BlackHoleView::buildMaps()
{
    if (mapMain.isValid()) return;

    mapMain = juce::Image (juce::Image::SingleChannel, mapSize, mapSize, true);
    mapFine = juce::Image (juce::Image::SingleChannel, mapSize, mapSize, true);
    mapDust = juce::Image (juce::Image::SingleChannel, mapSize, mapSize, true);
    juce::Image::BitmapData a (mapMain, juce::Image::BitmapData::writeOnly);
    juce::Image::BitmapData b (mapFine, juce::Image::BitmapData::writeOnly);
    juce::Image::BitmapData d (mapDust, juce::Image::BitmapData::writeOnly);
    juce::Random rnd (7);

    for (int y = 0; y < mapSize; ++y)
        for (int x = 0; x < mapSize; ++x)
        {
            auto dx = ((float) x + 0.5f) / (float) mapSize * 2.0f - 1.0f;
            auto dy = ((float) y + 0.5f) / (float) mapSize * 2.0f - 1.0f;
            auto r = std::sqrt (dx * dx + dy * dy);
            if (r >= 1.0f || r < holeFrac * 0.8f) continue;

            auto theta = std::atan2 (dy, dx);
            auto logr = std::log (r / holeFrac);

            // a logarithmic spiral: the further in, the more it has turned
            auto phi1 = theta + 2.6f * logr;
            auto phi2 = theta + 3.4f * logr + 1.3f;
            auto n1 = fbm (2.0f * std::cos (phi1) + 4.0f + logr * 1.2f, 2.0f * std::sin (phi1) + 4.0f + logr * 3.5f, 3, 5);
            auto n2 = fbm (3.5f * std::cos (phi2) + 9.0f + logr * 2.0f, 3.5f * std::sin (phi2) + 2.0f + logr * 7.0f, 11, 5);

            // brightest just outside the hole, fading to the edge
            auto inner = juce::jlimit (0.0f, 1.0f, (r - holeFrac * 0.92f) / (holeFrac * 0.35f));
            auto outer = std::pow (juce::jlimit (0.0f, 1.0f, (1.0f - r) / (1.0f - holeFrac)), 1.6f);
            auto prof = inner * (0.25f + 0.75f * std::exp (-std::pow ((r - holeFrac * 1.5f) / 0.30f, 2.0f))) * outer;

            auto streak = std::pow (juce::jlimit (0.0f, 1.0f, (n1 - 0.32f) * 2.2f), 1.5f);
            auto fine = std::pow (juce::jlimit (0.0f, 1.0f, (n2 - 0.45f) * 3.0f), 2.0f);

            a.setPixelColour (x, y, juce::Colours::white.withAlpha (juce::jlimit (0.0f, 1.0f, prof * (0.18f + 1.1f * streak))));
            b.setPixelColour (x, y, juce::Colours::white.withAlpha (juce::jlimit (0.0f, 1.0f, prof * 1.3f * fine)));
            if (rnd.nextFloat() < 0.012f * prof + 0.002f * outer)
                d.setPixelColour (x, y, juce::Colours::white.withAlpha (0.4f + 0.6f * rnd.nextFloat()));
        }
}

BlackHoleView::BlackHoleView()
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    setMouseDragSensitivity (320);
}

juce::Rectangle<float> BlackHoleView::getRingArea() const
{
    auto s = 352.0f;
    return juce::Rectangle<float> (s, s).withCentre (getLocalBounds().toFloat().getCentre());
}

bool BlackHoleView::hitTest (int x, int y)
{
    auto r = getRingArea();
    return r.getCentre().getDistanceFrom ({ (float) x, (float) y }) < r.getWidth() * 0.5f;
}

void BlackHoleView::tick (const HoleState& s)
{
    st = s;
    auto dt = 1.0f / 30.0f;
    time += dt;

    // rotation: MOD RATE speeds it up, FREEZE keeps it turning slowly
    auto speed = 0.10f + 0.22f * std::sqrt (s.modRate) + 0.15f * s.level;
    rotA += dt * speed;
    rotB += dt * speed * 1.55f;
    rotC += dt * speed * 0.7f;

    // a flash on every hit, fading like the tail (longer DECAY = slower fade)
    if (s.hits != lastHits && ! s.frozen)
        flash = juce::jmin (1.0f, flash + 0.65f);
    lastHits = s.hits;
    auto fade = std::exp (-dt * 3.0f / juce::jlimit (0.3f, 12.0f, s.decay * 0.6f));
    flash *= s.frozen ? 1.0f : fade;
    freezeGlow += ((s.frozen ? 1.0f : 0.0f) - freezeGlow) * 0.08f;

    // dust falling into the hole
    auto births = (int) (1.0f + s.level * 6.0f + flash * 3.0f + random.nextFloat());
    for (int i = 0; i < births && grains.size() < 300; ++i)
        grains.push_back ({ random.nextFloat() * 2.0f * pi, 0.7f + random.nextFloat() * 0.35f,
                            0.4f + random.nextFloat() * 0.8f, 0.6f + random.nextFloat() * 1.6f, 1.0f });
    for (auto& g : grains)
    {
        auto inward = s.frozen ? 0.0f : 0.0025f * (1.0f + 1.2f * (1.0f - g.radius));
        g.radius -= inward;
        g.angle += dt * g.speed * (0.8f + 1.5f / juce::jmax (0.35f, g.radius));
        if (g.radius < holeFrac * 1.05f) g.life -= 0.15f;
        else g.life = juce::jmin (1.0f, g.life + 0.05f);
    }
    grains.erase (std::remove_if (grains.begin(), grains.end(), [] (const Grain& g) { return g.life <= 0.0f; }), grains.end());
    repaint();
}

void BlackHoleView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    buildMaps();
    auto& p = *palette;

    auto ring = getRingArea();
    auto c = ring.getCentre();
    auto ringR = ring.getWidth() * 0.5f - 6.0f;
    auto diskR = ringR * (1.15f + 0.3f * st.size);                     // SIZE grows the nebula
    auto glowCol = p.isLight ? p.accent : p.nebulaA;
    auto highlight = p.isLight ? juce::Colour (0xfffff6c8) : p.nebulaC;
    auto bright = 0.75f + 0.35f * st.brightness + 0.45f * flash + 0.2f * freezeGlow;

    Glass::drawGlow (g, c, diskR * 1.05f, glowCol, 0.30f + 0.2f * flash);

    auto drawLayer = [&] (const juce::Image& img, float rot, float scale, juce::Colour col, float alpha)
    {
        auto s = diskR * 2.0f * scale / (float) mapSize;
        auto t = juce::AffineTransform::translation (-(float) mapSize * 0.5f, -(float) mapSize * 0.5f)
                     .rotated (rot).scaled (s, s * 0.96f).translated (c.x, c.y);
        g.setColour (col.withAlpha (juce::jlimit (0.0f, 1.0f, alpha)));
        g.drawImageTransformed (img, t, true);
    };

    drawLayer (mapMain, rotA, 1.0f, glowCol, 1.0f);
    drawLayer (mapMain, rotA + 0.35f, 0.92f, glowCol.brighter (0.5f), 0.55f * bright);
    drawLayer (mapMain, -rotC * 0.6f + 2.1f, 0.78f, highlight, 0.45f * bright);
    drawLayer (mapFine, rotB, 1.0f, highlight, juce::jmin (1.0f, 1.0f * bright));
    drawLayer (mapFine, rotB * 0.8f + 1.0f, 0.85f, juce::Colours::white, 0.35f * bright);
    drawLayer (mapDust, rotC, 1.0f, juce::Colours::white, 0.8f);

    // grains spiralling in
    for (auto& gr : grains)
    {
        auto pos = c.getPointOnCircumference (gr.radius * diskR, gr.angle);
        g.setColour (highlight.withAlpha (gr.life * 0.85f));
        g.fillEllipse (pos.x - gr.size * 0.5f, pos.y - gr.size * 0.5f, gr.size, gr.size);
    }

    // the hole itself + photon ring
    auto holeR = diskR * holeFrac * (1.0f + 0.03f * std::sin ((float) time * 1.7f) * freezeGlow);
    Glass::drawGlow (g, c, holeR * 1.6f, highlight, 0.35f + 0.4f * flash);
    juce::ColourGradient hole (juce::Colours::black, c.x, c.y, juce::Colours::black.withAlpha (0.0f), c.x + holeR * 1.18f, c.y, true);
    hole.addColour (0.82, juce::Colours::black);
    g.setGradientFill (hole);
    g.fillEllipse (juce::Rectangle<float> (holeR * 2.36f, holeR * 2.36f).withCentre (c));
    g.setColour (highlight.withAlpha (0.5f + 0.4f * flash));
    g.drawEllipse (juce::Rectangle<float> (holeR * 2.04f, holeR * 2.04f).withCentre (c), 1.2f);

    // DECAY ring
    auto v = (float) valueToProportionOfLength (getValue());
    auto start = pi * 1.25f, end = pi * 2.75f, angle = start + v * (end - start);
    auto inner = ringR * 0.9f;
    g.setColour ((p.isLight ? juce::Colours::white : p.text).withAlpha (0.18f));
    g.drawEllipse (juce::Rectangle<float> (inner * 2.0f, inner * 2.0f).withCentre (c), 1.0f);

    juce::Path track, arc;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0, start, end, true);
    g.setColour ((p.isLight ? juce::Colours::white : p.text).withAlpha (0.12f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    arc.addCentredArc (c.x, c.y, ringR, ringR, 0, start, angle, true);
    auto arcCol = p.isLight ? juce::Colours::white : p.accent;
    g.setColour (arcCol.withAlpha (0.25f));
    g.strokePath (arc, juce::PathStrokeType (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (arcCol);
    g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    auto dot = c.getPointOnCircumference (ringR, angle);
    Glass::drawGlow (g, dot, 14.0f, arcCol, 0.8f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (dot.x - 4.0f, dot.y - 4.0f, 8.0f, 8.0f);
}

// ================================================================== knobs

VoidKnob::VoidKnob (const juce::String& l, bool b) : label (l), big (b)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    setMouseDragSensitivity (big ? 220 : 180);
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<float> VoidKnob::knobArea() const
{
    auto b = getLocalBounds().toFloat();
    auto labelH = big ? 30.0f : 20.0f;
    auto s = big ? 96.0f : juce::jmin (b.getWidth(), b.getHeight() - labelH);
    auto top = big ? b.getY() + 22.0f : b.getY();
    return juce::Rectangle<float> (s, s).withCentre ({ b.getCentreX(), top + s * 0.5f });
}

bool VoidKnob::hitTest (int x, int y)
{
    return knobArea().expanded (4).contains ((float) x, (float) y);
}

void VoidKnob::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto k = knobArea();
    auto v = (float) valueToProportionOfLength (getValue());
    lt::ui::GlassLookAndFeel::drawGlassKnob (g, k, v, p, big ? 1.0f : 0.7f);

    auto text = formatValue ? formatValue (getValue()) : getTextFromValue (getValue());
    if (big)
    {
        g.setColour (p.value);
        g.setFont (Fonts::regular (k.getHeight() * 0.22f));
        g.drawFittedText (text, k.reduced (k.getWidth() * 0.2f).toNearestInt(), juce::Justification::centred, 1);
    }

    auto labelArea = getLocalBounds().toFloat().withTop (k.getBottom() + (big ? 4.0f : 1.0f));
    auto showValue = ! big && (isMouseOverOrDragging());
    g.setColour (showValue ? p.value : p.text);
    g.setFont (big ? Fonts::regular (17.0f) : Fonts::regular (12.5f));
    g.drawFittedText (showValue ? text : label, labelArea.toNearestInt(), juce::Justification::centredTop, 1);
}

// ================================================================== meter

void LevelMeter::setLevel (float linear)
{
    auto db = juce::Decibels::gainToDecibels (linear, -60.0f);
    auto l = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
    level = l > level ? l : level * 0.9f + l * 0.1f;
    if (l >= peak) { peak = l; peakHold = 30; }
    else if (--peakHold < 0) peak = juce::jmax (l, peak - 0.01f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto r = getLocalBounds().toFloat().withSizeKeepingCentre (6.0f, (float) getHeight());
    g.setColour ((p.isLight ? juce::Colours::white : p.text).withAlpha (0.12f));
    g.fillRoundedRectangle (r, 3.0f);

    auto fill = r.withTop (r.getBottom() - r.getHeight() * level);
    juce::ColourGradient grad (p.accent.brighter (0.6f), 0, r.getY(), p.accent.withAlpha (0.6f), 0, r.getBottom(), false);
    Glass::drawRectGlow (g, fill, 3.0f, p.accent, 5.0f, 0.5f * level);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (fill, 3.0f);

    auto py = r.getBottom() - r.getHeight() * peak;
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.fillRect (r.getX() - 2.0f, py - 1.0f, r.getWidth() + 4.0f, 1.5f);
}

// ================================================================== freeze

juce::Path infinityPath (juce::Rectangle<float> r)
{
    juce::Path path;
    for (int i = 0; i <= 120; ++i)
    {
        auto t = (float) i / 120.0f * 2.0f * pi;
        auto d = 1.0f + std::sin (t) * std::sin (t);
        auto x = std::cos (t) / d, y = std::sin (t) * std::cos (t) / d;
        auto pt = juce::Point<float> (r.getCentreX() + x * r.getWidth() * 0.5f, r.getCentreY() + y * r.getHeight());
        if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
    }
    path.closeSubPath();
    return path;
}

void FreezeButton::paintButton (juce::Graphics& g, bool hover, bool down)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    auto on = getToggleState();

    if (on) Glass::drawRectGlow (g, r, 9.0f, p.accent, 12.0f, 0.9f);
    Glass::drawPanel (g, r, 9.0f, p, on ? 2.4f : (hover ? 1.8f : 1.2f), on);
    if (on)
    {
        g.setColour (p.accent.withAlpha (0.35f));
        g.fillRoundedRectangle (r, 9.0f);
    }
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.1f)); g.fillRoundedRectangle (r, 9.0f); }

    auto inf = infinityPath (r.withSizeKeepingCentre (r.getWidth() * 0.46f, r.getHeight() * 0.42f));
    g.setColour (juce::Colours::white.withAlpha (on ? 1.0f : 0.8f));
    g.strokePath (inf, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace lt_void
