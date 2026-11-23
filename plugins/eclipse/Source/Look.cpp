#include "Look.h"
#include "Decor.h"

namespace eclipse::look
{

using juce::Colour;
namespace Colours = juce::Colours;

void spaced (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Font f, float px, juce::Justification j)
{
    f.setExtraKerningFactor (px / juce::jmax (1.0f, f.getHeight()));
    g.setFont (f);
    g.drawText (t, r, j, false);
}

juce::Path chamfer (juce::Rectangle<float> r, float c)
{
    c = juce::jmin (c, r.getWidth() * 0.5f, r.getHeight() * 0.5f);
    juce::Path p;
    p.startNewSubPath (r.getX() + c, r.getY());
    p.lineTo (r.getRight() - c, r.getY());
    p.lineTo (r.getRight(), r.getY() + c);
    p.lineTo (r.getRight(), r.getBottom() - c);
    p.lineTo (r.getRight() - c, r.getBottom());
    p.lineTo (r.getX() + c, r.getBottom());
    p.lineTo (r.getX(), r.getBottom() - c);
    p.lineTo (r.getX(), r.getY() + c);
    p.closeSubPath();
    return p;
}

static const juce::Image& noise()
{
    static juce::Image img = []
    {
        juce::Image im (juce::Image::ARGB, 128, 128, true);
        juce::Random r (11);
        for (int y = 0; y < 128; ++y)
            for (int x = 0; x < 128; ++x)
            {
                const auto v = (juce::uint8) r.nextInt (256);
                im.setPixelAt (x, y, Colour (v, v, v, (juce::uint8) 255));
            }
        return im;
    }();
    return img;
}

static void tileNoise (juce::Graphics& g, const juce::Path& clip, float alpha)
{
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (clip);
    g.setTiledImageFill (noise(), 0, 0, alpha);
    g.fillPath (clip);
}

void dot (juce::Graphics& g, Colour c, juce::Point<float> p, float r, bool glow)
{
    if (glow)
    {
        g.setColour (c.withAlpha (0.25f)); g.fillEllipse (p.x - r * 2.2f, p.y - r * 2.2f, r * 4.4f, r * 4.4f);
    }
    g.setColour (c); g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
}

void label (juce::Graphics& g, const Skin& s, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, float size, float spacing, Colour c)
{
    g.setColour (c.isTransparent() ? s.dim : c);
    spaced (g, t.toUpperCase(), r, Fonts::semiBold (size), spacing, j);
}

void chassis (juce::Graphics& g, const Skin& s, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (s.chassis1, 0, r.getY(), s.chassis2, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 14);
    juce::ColourGradient hi (s.hi.withMultipliedAlpha (s.light ? 0.6f : 1.0f), r.getCentreX(), r.getY() - 60,
                             s.hi.withAlpha (0.0f), r.getCentreX(), r.getY() + 380, true);
    g.setGradientFill (hi);
    g.fillRoundedRectangle (r, 14);
    juce::Path p; p.addRoundedRectangle (r, 14);
    tileNoise (g, p, s.light ? 0.035f : 0.05f);
    g.setColour (s.hi.withMultipliedAlpha (0.8f)); g.drawLine (r.getX() + 14, r.getY() + 1, r.getRight() - 14, r.getY() + 1, 1);
}

void screw (juce::Graphics& g, const Skin& s, juce::Point<float> c)
{
    const float r = 4.5f;
    g.setColour (Colours::black.withAlpha (0.5f)); g.fillEllipse (c.x - r, c.y - r + 1, r * 2, r * 2);
    juce::ColourGradient cg (Colours::white.withAlpha (0.7f), c.x - 1.5f, c.y - 1.5f, s.screw.darker (0.6f), c.x + r, c.y + r, true);
    cg.addColour (0.45, s.screw);
    g.setGradientFill (cg); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawLine (c.x - r * 0.65f, c.y + r * 0.35f, c.x + r * 0.65f, c.y - r * 0.35f, 1.0f);
}

void panel (juce::Graphics& g, const Skin& s, juce::Rectangle<float> r, bool screws)
{
    auto outer = chamfer (r, 12);
    g.setColour (Colours::black.withAlpha (s.light ? 0.10f : 0.45f));
    g.fillPath (outer, juce::AffineTransform::translation (0, 3));
    juce::ColourGradient bevel (s.hi.withMultipliedAlpha (s.light ? 1.0f : 1.4f), 0, r.getY(), s.lo, 0, r.getBottom(), false);
    bevel.addColour (0.3, Colours::transparentBlack);
    g.setGradientFill (bevel); g.fillPath (outer);

    auto face = r.reduced (2);
    auto inner = chamfer (face, 11);
    g.setGradientFill (juce::ColourGradient (s.panel1, 0, face.getY(), s.panel2, 0, face.getBottom(), false));
    g.fillPath (inner);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (inner);
        juce::ColourGradient glow (s.hi.withMultipliedAlpha (s.light ? 0.5f : 0.6f), face.getX() + face.getWidth() * 0.3f, face.getY(),
                                   s.hi.withAlpha (0.0f), face.getX() + face.getWidth() * 0.3f, face.getY() + 170, true);
        g.setGradientFill (glow); g.fillRect (face);
    }
    g.setColour (s.hi.withMultipliedAlpha (0.9f)); g.drawLine (face.getX() + 11, face.getY() + 0.5f, face.getRight() - 11, face.getY() + 0.5f, 1);
    g.setColour (Colours::black.withAlpha (0.18f)); g.drawLine (face.getX() + 11, face.getBottom() - 0.5f, face.getRight() - 11, face.getBottom() - 0.5f, 1);
    if (screws)
    {
        screw (g, s, { face.getX() + 10, face.getY() + 10 });
        screw (g, s, { face.getRight() - 10, face.getY() + 10 });
    }
}

void panelArt (juce::Graphics& g, const Skin& s, juce::Rectangle<float> r, juce::Point<int> pos)
{
    decor::drawInPanel (g, s, chamfer (r.reduced (2), 11), pos, s.light ? 0.4f : 0.3f);
}

void lcd (juce::Graphics& g, const Skin& s, juce::Rectangle<float> r, bool grid)
{
    g.setColour (s.hi.withMultipliedAlpha (0.6f)); g.drawRoundedRectangle (r.translated (0, 1), 6, 1);
    g.setGradientFill (juce::ColourGradient (s.lcd2, 0, r.getY(), s.lcd1, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 6);
    if (grid)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.toNearestInt());
        g.setColour (s.lcdLine);
        for (float x = r.getX() + 40; x < r.getRight(); x += 40) g.fillRect (x, r.getY(), 1.0f, r.getHeight());
        for (float y = r.getY() + 29; y < r.getBottom(); y += 29) g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
    }
    // inner shadow
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (Colours::black.withAlpha (0.28f - 0.06f * (float) i));
        g.drawRoundedRectangle (r.reduced ((float) i * 0.8f + 0.5f).withTrimmedBottom ((float) (3 - i)), 6, 1);
    }
}

void lcdGlass (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::Path p;
    p.startNewSubPath (r.getX(), r.getY());
    p.lineTo (r.getRight(), r.getY());
    p.lineTo (r.getRight(), r.getY() + r.getHeight() * 0.18f);
    p.lineTo (r.getX(), r.getY() + r.getHeight() * 0.32f);
    p.closeSubPath();
    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip; clip.addRoundedRectangle (r, 6);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (Colours::white.withAlpha (0.07f), 0, r.getY(), Colours::white.withAlpha (0.015f), 0, r.getY() + r.getHeight() * 0.3f, false));
    g.fillPath (p);
}

void key (juce::Graphics& g, const Skin& s, juce::Rectangle<float> r, const juce::String& text, bool on, bool down, bool over,
          bool led, float fontSize, Colour onColour)
{
    const Colour acc = onColour.isTransparent() ? s.acc : onColour;
    const float press = on || down ? 1.5f : 0.0f;
    auto body = r.withTrimmedBottom (3).translated (0, press);
    if (! on)
    {
        g.setColour (Colours::black.withAlpha (s.light ? 0.15f : 0.55f));
        g.fillRoundedRectangle (body.translated (0, 4).expanded (0.5f), 6);
        g.setColour (s.padEdge); g.fillRoundedRectangle (body.translated (0, 3 - press), 5);
    }
    else
    {
        for (int i = 3; i > 0; --i) { g.setColour (acc.withAlpha (0.10f)); g.fillRoundedRectangle (body.expanded ((float) i * 3), 5 + (float) i * 3); }
    }
    if (on) g.setGradientFill (juce::ColourGradient (acc.interpolatedWith (Colours::white, 0.18f), 0, body.getY(), acc.darker (0.15f), 0, body.getBottom(), false));
    else    g.setGradientFill (juce::ColourGradient (s.key1.brighter (over ? 0.08f : 0.0f), 0, body.getY(), s.key2, 0, body.getBottom(), false));
    g.fillRoundedRectangle (body, 5);
    g.setColour (on ? Colours::white.withAlpha (0.4f) : s.hi.withMultipliedAlpha (s.light ? 1.0f : 1.6f));
    g.drawLine (body.getX() + 4, body.getY() + 0.8f, body.getRight() - 4, body.getY() + 0.8f, 1);
    g.setColour (Colours::black.withAlpha (on ? 0.35f : 0.25f)); g.drawRoundedRectangle (body.reduced (0.5f), 5, 1);

    auto tr = body;
    if (led)
    {
        auto l = tr.removeFromLeft (tr.getHeight()).withSizeKeepingCentre (6, 6).translated (8, 0);
        if (on) { g.setColour (Colours::white.withAlpha (0.35f)); g.fillEllipse (l.expanded (3)); g.setColour (Colours::white); }
        else g.setColour (Colours::black.withAlpha (0.45f));
        g.fillEllipse (l);
        tr.removeFromLeft (-6);
    }
    const Colour tc = on ? (acc.getPerceivedBrightness() > 0.75f ? Colours::black : Colours::white) : s.keyTx;
    g.setColour (tc);
    spaced (g, text, tr, Fonts::bold (fontSize), fontSize * 0.09f, juce::Justification::centred);
}

void knob (juce::Graphics& g, const Skin& s, juce::Rectangle<float> r, float v, bool big, Colour ring)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    const auto c = r.getCentre();
    const float R = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const float a0 = -juce::MathConstants<float>::pi * 0.75f, a1 = juce::MathConstants<float>::pi * 0.75f, av = a0 + (a1 - a0) * v;
    auto pol = [&] (float a, float rr) { return juce::Point<float> (c.x + rr * std::sin (a), c.y - rr * std::cos (a)); };

    if (big)
    {   // LED segment ring
        const int n = 31;
        for (int i = 0; i < n; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / (float) (n - 1);
            const bool lit = a <= av + 1.0e-4f;
            auto p0 = pol (a, R - 2), p1 = pol (a, R - 10);
            if (lit) { g.setColour (ring.withAlpha (0.3f)); g.drawLine ({ p0, p1 }, 7.0f); }
            g.setColour (lit ? ring : s.ringOff);
            g.drawLine ({ p0, p1 }, 3.2f);
        }
    }
    else
    {
        juce::Path off; off.addCentredArc (c.x, c.y, R - 2.5f, R - 2.5f, 0, a0, a1, true);
        g.setColour (s.ringOff); g.strokePath (off, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (v > 0.005f)
        {
            juce::Path on; on.addCentredArc (c.x, c.y, R - 2.5f, R - 2.5f, 0, a0, av, true);
            g.setColour (ring.withAlpha (0.28f)); g.strokePath (on, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (ring); g.strokePath (on, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
    const float sk = R * (big ? 0.72f : 0.70f), cap = R * (big ? 0.5f : 0.48f);
    // shadow
    g.setColour (Colours::black.withAlpha (s.light ? 0.25f : 0.6f));
    g.fillEllipse (c.x - sk, c.y - sk + (big ? 6.0f : 3.0f), sk * 2, sk * 2);
    // knurled skirt
    g.setGradientFill (juce::ColourGradient (s.skirt1, 0, c.y - sk, s.skirt2, 0, c.y + sk, false));
    g.fillEllipse (c.x - sk, c.y - sk, sk * 2, sk * 2);
    const int knurls = big ? 48 : 28;
    g.setColour (Colours::black.withAlpha (0.38f));
    for (int i = 0; i < knurls; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) knurls;
        g.drawLine ({ pol (a, sk - 0.5f), pol (a, sk - (big ? 6.0f : 3.5f)) }, big ? 1.6f : 1.1f);
    }
    g.setColour (Colours::white.withAlpha (0.12f)); g.drawEllipse (c.x - sk, c.y - sk, sk * 2, sk * 2, 1);
    // cap
    juce::ColourGradient cg (s.cap1, c.x - cap * 0.3f, c.y - cap * 0.55f, s.cap2, c.x + cap * 0.6f, c.y + cap, true);
    cg.addColour (0.55, s.cap1.interpolatedWith (s.cap2, 0.55f));
    g.setGradientFill (cg); g.fillEllipse (c.x - cap, c.y - cap, cap * 2, cap * 2);
    g.setColour (Colours::black.withAlpha (0.4f)); g.drawEllipse (c.x - cap, c.y - cap, cap * 2, cap * 2, 1);
    g.setColour (Colours::white.withAlpha (0.09f)); g.drawEllipse (c.x - cap * 0.8f, c.y - cap * 0.8f, cap * 1.6f, cap * 1.6f, big ? 5.0f : 2.5f);
    g.setColour (Colours::white.withAlpha (0.16f)); g.fillEllipse (c.x - cap * 0.75f, c.y - cap * 0.72f, cap * 1.1f, cap * 0.5f);
    // pointer
    auto p0 = pol (av, cap * 0.25f), p1 = pol (av, cap * 0.92f);
    if (big) { g.setColour (ring.withAlpha (0.35f)); g.drawLine ({ p0, p1 }, 8.0f); g.setColour (ring); g.drawLine ({ p0, p1 }, 3.5f); }
    else     { g.setColour (s.light ? Colour (0xff2a2a35) : s.tx); g.drawLine ({ p0, p1 }, 2.2f); }
}

void fader (juce::Graphics& g, const Skin& s, juce::Rectangle<float> r, float v)
{
    auto t = r.withSizeKeepingCentre (r.getWidth(), 8);
    g.setColour (s.hi.withMultipliedAlpha (0.6f)); g.drawRoundedRectangle (t.translated (0, 1), 4, 1);
    g.setGradientFill (juce::ColourGradient (Colours::black, 0, t.getY(), Colour (0xff141414), 0, t.getBottom(), false));
    g.fillRoundedRectangle (t, 4);
    auto f = t.reduced (2).withWidth (juce::jmax (0.0f, (t.getWidth() - 4) * v));
    g.setColour (s.acc.withAlpha (0.3f)); g.fillRoundedRectangle (f.expanded (2), 4);
    g.setGradientFill (juce::ColourGradient (s.acc, f.getX(), 0, s.acc.interpolatedWith (Colours::white, 0.4f), f.getRight(), 0, false));
    g.fillRoundedRectangle (f, 3);
    const float x = t.getX() + 2 + (t.getWidth() - 4) * v;
    auto cap = juce::Rectangle<float> (14, 24).withCentre ({ x, t.getCentreY() });
    g.setColour (Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (cap.translated (0, 3), 3);
    juce::ColourGradient m (Colour (0xff9a9da3), cap.getX(), 0, Colour (0xffd4d6da), cap.getRight(), 0, false);
    m.addColour (0.45, Colour (0xfff2f3f5)); m.addColour (0.55, Colour (0xff8d9096));
    g.setGradientFill (m); g.fillRoundedRectangle (cap, 3);
    g.setColour (Colour (0xff222222)); g.fillRect (cap.getX() + 2, cap.getCentreY() - 1, cap.getWidth() - 4, 2.0f);
}

void meter (juce::Graphics& g, const Skin& s, juce::Rectangle<float> r, float l, float rr)
{
    g.setColour (s.hi.withMultipliedAlpha (0.5f)); g.drawRoundedRectangle (r.translated (0, 1), 4, 1);
    g.setColour (Colours::black); g.fillRoundedRectangle (r, 4);
    auto in = r.reduced (3);
    const float w = (in.getWidth() - 3) / 2;
    for (int c = 0; c < 2; ++c)
    {
        auto bar = juce::Rectangle<float> (in.getX() + (float) c * (w + 3), in.getY(), w, in.getHeight());
        juce::ColourGradient cg (Colour (0xff2bd96b), 0, bar.getBottom(), Colour (0xffff4a3d), 0, bar.getY(), false);
        cg.addColour (0.6, Colour (0xff2bd96b)); cg.addColour (0.62, Colour (0xfff5d33d)); cg.addColour (0.82, Colour (0xfff5d33d)); cg.addColour (0.84, Colour (0xffff4a3d));
        g.setGradientFill (cg); g.fillRect (bar);
        const float lvl = juce::jlimit (0.0f, 1.0f, juce::jmap (juce::Decibels::gainToDecibels (c == 0 ? l : rr, -60.0f), -60.0f, 3.0f, 0.0f, 1.0f));
        g.setColour (Colour (0xff111111)); g.fillRect (bar.withHeight (bar.getHeight() * (1 - lvl)));
    }
}

void pad (juce::Graphics& g, const Skin& s, juce::Rectangle<float> r, bool full, bool selected, float lit, Colour fam)
{
    const float press = lit > 0.5f ? 5.0f : 0.0f;
    auto base = r.withTrimmedBottom (7);
    // drop shadow + the rubber's side
    g.setColour (Colours::black.withAlpha (s.light ? 0.18f : 0.6f));
    g.fillRoundedRectangle (base.translated (0, 9 - press * 0.6f).expanded (1), 11);
    g.setColour (s.padEdge); g.fillRoundedRectangle (base.translated (0, 7 - press), 10);
    auto top = base.translated (0, press);
    juce::Path shape; shape.addRoundedRectangle (top, 10);
    g.setGradientFill (juce::ColourGradient (full ? s.pad1 : s.padEmpty, 0, top.getY(), s.pad2, 0, top.getBottom(), false));
    g.fillPath (shape);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (shape);
        juce::ColourGradient sheen (Colours::white.withAlpha (s.light ? 0.35f : 0.09f), top.getCentreX(), top.getY() - 10,
                                    Colours::white.withAlpha (0.0f), top.getCentreX(), top.getY() + top.getHeight() * 0.7f, true);
        g.setGradientFill (sheen); g.fillRect (top);
        if (lit > 0.001f)
        {   // the light lives inside the rubber
            juce::ColourGradient lg (fam.withAlpha (0.72f * lit), top.getCentreX(), top.getY() + top.getHeight() * 0.6f,
                                     fam.withAlpha (0.12f * lit), top.getX(), top.getY(), true);
            g.setGradientFill (lg); g.fillRect (top);
        }
        g.setGradientFill (juce::ColourGradient (Colours::transparentBlack, 0, top.getBottom() - 18, Colours::black.withAlpha (0.22f), 0, top.getBottom(), false));
        g.fillRect (top);
    }
    tileNoise (g, shape, 0.045f);
    g.setColour (Colours::white.withAlpha (s.light ? 0.7f : 0.16f));
    g.drawLine (top.getX() + 9, top.getY() + 0.8f, top.getRight() - 9, top.getY() + 0.8f, 1);
    if (lit > 0.001f)
    {
        g.setColour (fam.interpolatedWith (Colours::white, 0.25f).withAlpha (0.9f * lit)); g.drawRoundedRectangle (top.reduced (0.75f), 10, 1.5f);
        g.setColour (fam.withAlpha (0.35f * lit)); g.drawRoundedRectangle (top.reduced (3), 8, 4.0f);
    }
    else if (selected)
    {
        g.setColour (fam); g.drawRoundedRectangle (top.reduced (0.75f), 10, 1.5f);
    }
    else
    {
        g.setColour (Colours::black.withAlpha (0.25f)); g.drawRoundedRectangle (top.reduced (0.5f), 10, 1);
    }
}

void waveform (juce::Graphics& g, const juce::AudioBuffer<float>* b, juce::Rectangle<float> r, Colour c, float glow, float gain)
{
    if (b == nullptr || b->getNumSamples() == 0) return;
    const int cols = juce::jmax (1, (int) (r.getWidth() * 2));
    const int n = b->getNumSamples();
    std::vector<float> lo ((size_t) cols, 0.0f), hi ((size_t) cols, 0.0f);
    float peak = 1.0e-6f;
    for (int x = 0; x < cols; ++x)
    {
        const int a = (int) ((juce::int64) x * n / cols), e = juce::jmax (a + 1, (int) ((juce::int64) (x + 1) * n / cols));
        float mn = 0, mx = 0;
        for (int ch = 0; ch < b->getNumChannels(); ++ch)
        {
            auto range = juce::FloatVectorOperations::findMinAndMax (b->getReadPointer (ch, a), juce::jmin (e, n) - a);
            mn = juce::jmin (mn, range.getStart()); mx = juce::jmax (mx, range.getEnd());
        }
        lo[(size_t) x] = mn; hi[(size_t) x] = mx;
        peak = juce::jmax (peak, mx, -mn);
    }
    const float scale = gain / peak * r.getHeight() * 0.47f;
    juce::Path p;
    const float m = r.getCentreY();
    p.startNewSubPath (r.getX(), m);
    for (int x = 0; x < cols; ++x) p.lineTo (r.getX() + (float) x * r.getWidth() / (float) cols, m - hi[(size_t) x] * scale);
    for (int x = cols - 1; x >= 0; --x) p.lineTo (r.getX() + (float) x * r.getWidth() / (float) cols, m - lo[(size_t) x] * scale);
    p.closeSubPath();
    if (glow > 0)
    {
        g.setColour (c.withAlpha (0.16f)); g.strokePath (p, juce::PathStrokeType (glow));
        g.setColour (c.withAlpha (0.10f)); g.strokePath (p, juce::PathStrokeType (glow * 2));
    }
    g.setGradientFill (juce::ColourGradient (c, 0, r.getY(), c, 0, r.getBottom(), false));
    juce::ColourGradient cg (c, 0, r.getY(), c, 0, r.getBottom(), false);
    cg.addColour (0.5, c.withMultipliedAlpha (0.72f));
    g.setGradientFill (cg);
    g.fillPath (p);
    g.setColour (c.withAlpha (0.45f)); g.fillRect (r.getX(), m - 0.5f, r.getWidth(), 1.0f);
}

// ------------------------------------------------------------------ LookAndFeel
void LookAndFeel::setSkin (const Skin& s)
{
    skin = &s;
    setColour (juce::PopupMenu::backgroundColourId, s.panel1);
    setColour (juce::PopupMenu::textColourId, s.tx);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, s.acc);
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::white);
    setColour (juce::AlertWindow::backgroundColourId, s.panel1);
    setColour (juce::AlertWindow::textColourId, s.tx);
    setColour (juce::AlertWindow::outlineColourId, s.keyEdge);
    setColour (juce::TextEditor::backgroundColourId, s.lcd1);
    setColour (juce::TextEditor::textColourId, Colours::white);
    setColour (juce::TextEditor::outlineColourId, s.keyEdge);
    setColour (juce::TextEditor::focusedOutlineColourId, s.acc);
    setColour (juce::TextEditor::highlightColourId, s.acc.withAlpha (0.5f));
    setColour (juce::CaretComponent::caretColourId, s.acc);
    setColour (juce::Label::textColourId, s.tx);
}

juce::Font LookAndFeel::getPopupMenuFont() { return Fonts::semiBold (15); }
juce::Font LookAndFeel::getAlertWindowTitleFont() { return Fonts::bold (17); }
juce::Font LookAndFeel::getAlertWindowMessageFont() { return Fonts::regular (14); }
juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int) { return Fonts::bold (12); }

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    if (skin == nullptr) return LookAndFeel_V4::drawPopupMenuBackground (g, w, h);
    g.fillAll (skin->panel2);
    g.setColour (skin->keyEdge); g.drawRect (0, 0, w, h);
}

void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool sep, bool active, bool hl, bool ticked,
                                     bool sub, const juce::String& text, const juce::String& sc, const juce::Drawable* icon, const Colour* tc)
{
    if (skin == nullptr || sep) return LookAndFeel_V4::drawPopupMenuItem (g, area, sep, active, hl, ticked, sub, text, sc, icon, tc);
    auto r = area.reduced (3, 1).toFloat();
    if (hl && active) { g.setColour (skin->acc); g.fillRoundedRectangle (r, 4); }
    g.setColour (hl && active ? Colours::white : active ? skin->tx : skin->dim);
    g.setFont (getPopupMenuFont());
    auto tr = r.reduced (10, 0);
    if (ticked) { dot (g, hl ? Colours::white : skin->acc, { tr.getX() + 3, tr.getCentreY() }, 3.5f, false); }
    tr.removeFromLeft (16);
    g.drawText (text, tr, juce::Justification::centredLeft, true);
    if (sub) g.drawText (juce::CharPointer_UTF8 ("\xe2\x80\xba"), tr, juce::Justification::centredRight, false);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const Colour&, bool over, bool down)
{
    if (skin == nullptr) return;
    key (g, *skin, b.getLocalBounds().toFloat(), {}, b.getToggleState(), down, over);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    if (skin == nullptr) return;
    g.setColour (b.getToggleState() ? Colours::white : skin->keyTx);
    spaced (g, b.getButtonText().toUpperCase(), b.getLocalBounds().toFloat().withTrimmedBottom (3), Fonts::bold (12), 1.0f, juce::Justification::centred);
}

} // namespace eclipse::look
