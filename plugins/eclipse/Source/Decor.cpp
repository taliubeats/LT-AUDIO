#include "Decor.h"

namespace eclipse::decor
{

using juce::Colour;
namespace Colours = juce::Colours;
using P = juce::Point<float>;

namespace
{
constexpr float pi = juce::MathConstants<float>::pi;

juce::Path star (P c, float r, int points, float inner)
{
    juce::Path p;
    p.addStar (c, points, r * inner, r, -pi / 2);
    return p;
}

void sprayDrip (juce::Graphics& g, Colour c, P top, float len, float w)
{
    g.setColour (c);
    g.fillRoundedRectangle (top.x - w / 2, top.y, w, len, w / 2);
    g.fillEllipse (top.x - w * 0.9f, top.y + len - w * 0.9f, w * 1.8f, w * 1.8f);
}

void scribble (juce::Graphics& g, Colour c, juce::Random& r, juce::Rectangle<float> area, float thick)
{
    juce::Path p;
    P cur (area.getX(), area.getCentreY());
    p.startNewSubPath (cur);
    const int n = 6 + r.nextInt (5);
    for (int i = 0; i < n; ++i)
    {
        const P to (area.getX() + area.getWidth() * (float) (i + 1) / (float) n, area.getY() + r.nextFloat() * area.getHeight());
        const P c1 (cur.x + (to.x - cur.x) * 0.3f, area.getY() + r.nextFloat() * area.getHeight());
        const P c2 (cur.x + (to.x - cur.x) * 0.7f, area.getY() + r.nextFloat() * area.getHeight());
        p.cubicTo (c1, c2, to);
        cur = to;
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void heart (juce::Graphics& g, Colour c, P ctr, float s)
{
    juce::Path p;
    p.startNewSubPath (ctr.x, ctr.y + s * 0.9f);
    p.cubicTo (ctr.x - s * 1.4f, ctr.y, ctr.x - s * 0.7f, ctr.y - s * 1.0f, ctr.x, ctr.y - s * 0.35f);
    p.cubicTo (ctr.x + s * 0.7f, ctr.y - s * 1.0f, ctr.x + s * 1.4f, ctr.y, ctr.x, ctr.y + s * 0.9f);
    g.setColour (c); g.fillPath (p);
}

void sparkle (juce::Graphics& g, Colour c, P ctr, float r)
{
    g.setColour (c.withMultipliedAlpha (0.35f)); g.fillEllipse (ctr.x - r * 0.6f, ctr.y - r * 0.6f, r * 1.2f, r * 1.2f);
    g.setColour (c); g.fillPath (star (ctr, r, 4, 0.18f));
}

void palm (juce::Graphics& g, Colour c, P base, float h, float lean)
{
    juce::Path trunk;
    trunk.startNewSubPath (base);
    const P top (base.x + lean, base.y - h);
    trunk.quadraticTo (base.x + lean * 0.2f, base.y - h * 0.6f, top.x, top.y);
    g.setColour (c); g.strokePath (trunk, juce::PathStrokeType (h * 0.045f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    for (int i = 0; i < 7; ++i)
    {
        const float a = -pi * 0.95f + (float) i * pi * 0.95f / 6.0f + 0.1f;
        juce::Path leaf;
        const P tip (top.x + std::cos (a) * h * 0.42f, top.y + std::sin (a) * h * 0.25f + h * 0.12f);
        const P mid (top.x + std::cos (a) * h * 0.22f, top.y - h * 0.08f);
        leaf.startNewSubPath (top);
        leaf.quadraticTo (mid.x, mid.y - h * 0.04f, tip.x, tip.y);
        leaf.quadraticTo (mid.x, mid.y + h * 0.05f, top.x, top.y);
        g.fillPath (leaf);
    }
}

void rose (juce::Graphics& g, Colour petal, P c, float r)
{
    for (int ring = 3; ring >= 0; --ring)
    {
        const float rr = r * (0.35f + 0.22f * (float) ring);
        const int n = 5 + ring;
        for (int i = 0; i < n; ++i)
        {
            const float a = (float) i / (float) n * 2 * pi + (float) ring * 0.6f;
            juce::Path p;
            p.addCentredArc (c.x + std::cos (a) * rr * 0.35f, c.y + std::sin (a) * rr * 0.35f, rr * 0.62f, rr * 0.48f, a, 0, pi * 1.2f, true);
            g.setColour (petal.darker (0.15f * (float) (3 - ring)));
            g.fillPath (p);
            g.setColour (Colours::black.withAlpha (0.35f));
            g.strokePath (p, juce::PathStrokeType (1.0f));
        }
    }
}

void vine (juce::Graphics& g, Colour c, juce::Random& r, P from, P to)
{
    juce::Path p;
    p.startNewSubPath (from);
    const P c1 (from.x + (to.x - from.x) * 0.3f + (r.nextFloat() - 0.5f) * 120, from.y + (to.y - from.y) * 0.3f);
    const P c2 (from.x + (to.x - from.x) * 0.7f + (r.nextFloat() - 0.5f) * 120, from.y + (to.y - from.y) * 0.7f);
    p.cubicTo (c1, c2, to);
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // thorns
    juce::PathFlatteningIterator it (p);
    float acc = 0;
    P last = from;
    while (it.next())
    {
        const P pt ((float) it.x2, (float) it.y2);
        acc += pt.getDistanceFrom (last);
        if (acc > 26)
        {
            acc = 0;
            const P d = (pt - last) / juce::jmax (0.001f, pt.getDistanceFrom (last));
            const P n (-d.y, d.x);
            const float side = r.nextBool() ? 1.0f : -1.0f;
            juce::Path t;
            t.addTriangle (pt.x - d.x * 4, pt.y - d.y * 4, pt.x + d.x * 4, pt.y + d.y * 4, pt.x + n.x * 9 * side + d.x * 4, pt.y + n.y * 9 * side + d.y * 4);
            g.fillPath (t);
        }
        last = pt;
    }
}

void flame (juce::Graphics& g, Colour inner, Colour outer, P base, float w, float h)
{
    juce::Path p;
    p.startNewSubPath (base.x - w / 2, base.y);
    p.cubicTo (base.x - w * 0.6f, base.y - h * 0.4f, base.x - w * 0.05f, base.y - h * 0.55f, base.x + w * 0.05f, base.y - h);
    p.cubicTo (base.x + w * 0.15f, base.y - h * 0.6f, base.x + w * 0.65f, base.y - h * 0.45f, base.x + w / 2, base.y);
    p.closeSubPath();
    g.setGradientFill (juce::ColourGradient (inner, base.x, base.y, outer.withAlpha (0.0f), base.x, base.y - h, false));
    g.fillPath (p);
}

void chainLink (juce::Graphics& g, Colour c, P ctr, float len, float angle)
{
    juce::Path p;
    p.addRoundedRectangle (-len / 2, -len * 0.22f, len, len * 0.44f, len * 0.22f);
    p.applyTransform (juce::AffineTransform::rotation (angle).translated (ctr));
    g.setColour (c); g.strokePath (p, juce::PathStrokeType (len * 0.12f));
}

void hazard (juce::Graphics& g, Colour c, juce::Rectangle<float> r)
{
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r.toNearestInt());
    g.setColour (Colour (0xff111111)); g.fillRect (r);
    g.setColour (c);
    for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 22)
    {
        juce::Path p;
        p.addQuadrilateral (x, r.getBottom(), x + 11, r.getBottom(), x + 11 + r.getHeight(), r.getY(), x + r.getHeight(), r.getY());
        g.fillPath (p);
    }
}

void guilloche (juce::Graphics& g, Colour c, juce::Rectangle<float> r, int lines, float amp, float freq)
{
    g.setColour (c);
    for (int k = 0; k < lines; ++k)
    {
        juce::Path p;
        const float ph = (float) k * 0.35f;
        for (float x = r.getX(); x <= r.getRight(); x += 4)
        {
            const float t = (x - r.getX()) / r.getWidth();
            const float y = r.getCentreY() + std::sin (t * freq * 2 * pi + ph) * amp * std::sin (t * pi) + std::sin (t * freq * 6.3f + ph * 2) * amp * 0.3f;
            if (x == r.getX()) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.strokePath (p, juce::PathStrokeType (0.8f));
    }
}

void lollipop (juce::Graphics& g, Colour a, Colour b, P c, float r)
{
    g.setColour (Colours::white.withAlpha (0.8f)); g.fillRoundedRectangle (c.x - 2, c.y, 4, r * 2.2f, 2);
    g.setColour (a); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    juce::Path sw;
    for (float t = 0; t < 4 * pi; t += 0.15f)
    {
        const float rr = r * t / (4 * pi);
        const P q (c.x + std::cos (t) * rr, c.y + std::sin (t) * rr);
        if (t == 0) sw.startNewSubPath (q); else sw.lineTo (q);
    }
    g.setColour (b); g.strokePath (sw, juce::PathStrokeType (r * 0.18f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void cone (juce::Graphics& g, Colour scoop, P c, float s)
{
    juce::Path cn; cn.addTriangle (c.x - s * 0.6f, c.y, c.x + s * 0.6f, c.y, c.x, c.y + s * 1.6f);
    g.setColour (Colour (0xffd9a35f)); g.fillPath (cn);
    g.setColour (Colour (0xffb07a3c));
    for (int i = 1; i < 4; ++i) g.drawLine (c.x - s * 0.6f + (float) i * s * 0.3f, c.y, c.x, c.y + s * 1.6f, 1.0f);
    g.setColour (scoop); g.fillEllipse (c.x - s * 0.7f, c.y - s * 1.0f, s * 1.4f, s * 1.2f);
}

void drumstick (juce::Graphics& g, Colour c, P a, P b)
{
    g.setColour (c); g.drawLine ({ a, b }, 7.0f);
    g.fillEllipse (b.x - 6, b.y - 6, 12, 12);
}

// ------------------------------------------------------------------ each skin
void paint (juce::Graphics& g, const Skin& s, juce::Rectangle<float> R)
{
    juce::Random r (juce::DefaultHashFunctions::generateHash (s.id, 100000) + 7);
    const auto a = s.acc;
    const float W = R.getWidth(), H = R.getHeight();
    auto rnd = [&] (float lo, float hi) { return lo + r.nextFloat() * (hi - lo); };

    if (s.id == "verde")
    {   // spray paint: tags, drips, stars, crosses, splatter
        for (int i = 0; i < 14; ++i) scribble (g, a.withAlpha (rnd (0.18f, 0.4f)), r, { rnd (0, W - 260), rnd (0, H - 80), rnd (140, 300), rnd (40, 90) }, rnd (3, 7));
        for (int i = 0; i < 40; ++i) sprayDrip (g, a.withAlpha (rnd (0.25f, 0.55f)), { rnd (0, W), rnd (0, H * 0.8f) }, rnd (20, 140), rnd (3, 7));
        for (int i = 0; i < 18; ++i)
        {
            g.setColour (a.withAlpha (rnd (0.3f, 0.6f)));
            g.strokePath (star ({ rnd (0, W), rnd (0, H) }, rnd (14, 36), 5, 0.42f), juce::PathStrokeType (3.0f));
        }
        for (int i = 0; i < 14; ++i)
        {
            const P c (rnd (0, W), rnd (0, H)); const float sz = rnd (10, 22);
            g.setColour (Colours::black.withAlpha (0.55f));
            g.drawLine (c.x - sz, c.y - sz, c.x + sz, c.y + sz, 4); g.drawLine (c.x + sz, c.y - sz, c.x - sz, c.y + sz, 4);
        }
        for (int i = 0; i < 400; ++i) { g.setColour (a.withAlpha (rnd (0.15f, 0.5f))); const float d = rnd (1, 4); g.fillEllipse (rnd (0, W), rnd (0, H), d, d); }
    }
    else if (s.id == "isla")
    {   // sunset, mountains, palms, a tree frog
        g.setGradientFill (juce::ColourGradient (Colour (0xffff9a5a).withAlpha (0.25f), W * 0.5f, H * 0.62f, Colour (0x00ff9a5a), W * 0.5f, H * 0.1f, false));
        g.fillRect (R);
        g.setColour (Colour (0xffffc94d).withAlpha (0.35f)); g.fillEllipse (W * 0.42f, H * 0.42f, 200, 200);
        for (int k = 0; k < 2; ++k)
        {
            juce::Path m; m.startNewSubPath (0, H);
            for (float x = 0; x <= W; x += 40) m.lineTo (x, H * (0.62f + 0.06f * (float) k) - std::abs (std::sin (x * 0.004f + (float) k)) * 140 - std::sin (x * 0.013f) * 30);
            m.lineTo (W, H); m.closeSubPath();
            g.setColour (Colour (0xff163a2c).withAlpha (0.55f + 0.25f * (float) k)); g.fillPath (m);
        }
        palm (g, Colour (0xff0b1a14).withAlpha (0.8f), { W * 0.06f, H }, 420, 60);
        palm (g, Colour (0xff0b1a14).withAlpha (0.75f), { W * 0.94f, H }, 380, -50);
        palm (g, Colour (0xff0b1a14).withAlpha (0.6f), { W * 0.55f, H }, 300, 30);
        // little frog silhouette
        g.setColour (Colour (0xff0b1a14).withAlpha (0.8f));
        g.fillEllipse (W * 0.33f, H * 0.52f, 40, 26); g.fillEllipse (W * 0.33f + 26, H * 0.52f - 8, 18, 16);
    }
    else if (s.id == "suite")
    {   // city lights through a window, silk folds
        for (int i = 0; i < 80; ++i)
        {
            const float d = rnd (6, 40);
            g.setColour (Colour (0xffd4af37).withAlpha (rnd (0.04f, 0.18f)));
            g.fillEllipse (rnd (0, W), rnd (0, H * 0.55f), d, d);
        }
        for (int k = 0; k < 9; ++k)
        {
            juce::Path p;
            const float y0 = H * 0.55f + (float) k * 40;
            p.startNewSubPath (0, y0);
            p.cubicTo (W * 0.3f, y0 - 80 + (float) k * 9, W * 0.6f, y0 + 90, W, y0 - 30);
            g.setColour (Colour (0xfff2c9a0).withAlpha (0.06f + 0.02f * (float) (k % 3)));
            g.strokePath (p, juce::PathStrokeType (18.0f - (float) k));
        }
    }
    else if (s.id == "rosa")
    {   // sparkles, hearts, Y2K stars
        for (int i = 0; i < 40; ++i) sparkle (g, Colours::white.withAlpha (rnd (0.5f, 0.9f)), { rnd (0, W), rnd (0, H) }, rnd (5, 16));
        for (int i = 0; i < 18; ++i) heart (g, a.withAlpha (rnd (0.25f, 0.5f)), { rnd (0, W), rnd (0, H) }, rnd (8, 20));
        for (int i = 0; i < 10; ++i) { g.setColour (Colours::white.withAlpha (0.35f)); g.strokePath (star ({ rnd (0, W), rnd (0, H) }, rnd (14, 30), 4, 0.35f), juce::PathStrokeType (2.0f)); }
    }
    else if (s.id == "disco")
    {   // laser beams from the top and mirror-ball dots
        for (int i = 0; i < 9; ++i)
        {
            const P o (W * 0.5f + rnd (-60, 60), -20);
            const P e (rnd (-200, W + 200), H + 40);
            g.setGradientFill (juce::ColourGradient ((i % 2 ? a : Colour (0xff9d5cff)).withAlpha (0.35f), o.x, o.y, Colours::transparentBlack, e.x, e.y, false));
            juce::Path beam; beam.addTriangle (o.x - 2, o.y, o.x + 2, o.y, e.x, e.y);
            beam = juce::Path();
            beam.startNewSubPath (o); beam.lineTo (e.x - 16, e.y); beam.lineTo (e.x + 16, e.y); beam.closeSubPath();
            g.fillPath (beam);
        }
        for (int i = 0; i < 260; ++i)
        {
            const float d = rnd (2, 6);
            g.setColour ((r.nextBool() ? Colours::white : a).withAlpha (rnd (0.2f, 0.6f)));
            g.fillRect (rnd (0, W), rnd (0, H), d, d);
        }
    }
    else if (s.id == "neblina")
    {   // fog
        for (int i = 0; i < 26; ++i)
        {
            const P c (rnd (-100, W + 100), rnd (0, H));
            const float rr = rnd (120, 320);
            g.setGradientFill (juce::ColourGradient (Colour (0xffcfe8ff).withAlpha (rnd (0.05f, 0.12f)), c.x, c.y, Colour (0x00cfe8ff), c.x + rr, c.y, true));
            g.fillEllipse (c.x - rr, c.y - rr * 0.45f, rr * 2, rr * 0.9f);
        }
    }
    else if (s.id == "calle")
    {   // chains, hazard tape, smoke
        for (int k = 0; k < 3; ++k)
        {
            P p (rnd (0, W), -20);
            const float ang = rnd (0.9f, 2.2f);
            for (int i = 0; i < 22; ++i)
            {
                chainLink (g, Colour (0xffb8b8b8).withAlpha (0.35f), p, 34, ang + (i % 2 ? pi / 2 : 0.0f) * 0.0f);
                p += P (std::cos (ang), std::sin (ang)) * 26;
            }
        }
        hazard (g, a.withAlpha (0.85f), { 0, H - 20, W, 20 });
        hazard (g, a.withAlpha (0.85f), { 0, 0, 260, 14 });
        hazard (g, a.withAlpha (0.85f), { W - 260, 0, 260, 14 });
        for (int i = 0; i < 14; ++i)
        {
            const P c (rnd (0, W), rnd (0, H)); const float rr = rnd (80, 220);
            g.setGradientFill (juce::ColourGradient (Colours::white.withAlpha (0.05f), c.x, c.y, Colours::transparentWhite, c.x + rr, c.y, true));
            g.fillEllipse (c.x - rr, c.y - rr * 0.5f, rr * 2, rr);
        }
    }
    else if (s.id == "sabores")
    {   // candy shop
        const Colour cs[] = { Colour (0xffff6b81), Colour (0xff7fffd4), Colour (0xffffb347), Colour (0xffb48cff) };
        for (int i = 0; i < 16; ++i) lollipop (g, cs[i % 4].withAlpha (0.45f), Colours::white.withAlpha (0.55f), { rnd (0, W), rnd (0, H) }, rnd (14, 26));
        for (int i = 0; i < 10; ++i) cone (g, cs[(i + 1) % 4].withAlpha (0.5f), { rnd (0, W), rnd (0, H) }, rnd (12, 20));
        for (int i = 0; i < 30; ++i)
        {
            g.setColour (cs[i % 4].withAlpha (0.4f));
            const float w = rnd (8, 14);
            g.fillRoundedRectangle (rnd (0, W), rnd (0, H), w, w * 2.2f, w / 2);
        }
    }
    else if (s.id == "espina")
    {   // thorny vines climbing the corners and roses
        vine (g, Colour (0xff2c4a1e).withAlpha (0.9f), r, { 0, H }, { 120, 0 });
        vine (g, Colour (0xff2c4a1e).withAlpha (0.9f), r, { W, H }, { W - 120, 0 });
        vine (g, Colour (0xff2c4a1e).withAlpha (0.8f), r, { 0, H * 0.3f }, { W * 0.35f, H });
        vine (g, Colour (0xff2c4a1e).withAlpha (0.8f), r, { W, H * 0.4f }, { W * 0.7f, H });
        const P spots[] = { { 30, 120 }, { 70, 520 }, { 40, 860 }, { W - 40, 160 }, { W - 70, 560 }, { W - 30, 900 }, { W * 0.5f, H - 10 } };
        for (auto p : spots) rose (g, a.interpolatedWith (Colour (0xffb0102a), 0.5f), p, rnd (18, 28));
        // lace pattern
        g.setColour (Colours::black.withAlpha (0.35f));
        for (float y = 0; y < H; y += 26)
            for (float x = (float) ((int) (y / 26) % 2) * 13; x < W; x += 26)
                g.drawEllipse (x, y, 10, 10, 1);
    }
    else if (s.id == "billete")
    {   // banknote engraving, $ marks, a big blackletter L
        for (int k = 0; k < 6; ++k) guilloche (g, Colour (0xffe2b33c).withAlpha (0.12f), { 0, H * (float) k / 6, W, H / 6 }, 7, 34, 3.0f + (float) k);
        g.setColour (Colour (0xffe2b33c).withAlpha (0.12f));
        g.setFont (juce::FontOptions ("Times New Roman", 980.0f, juce::Font::bold));
        g.drawText ("L", R.toNearestInt().withTrimmedLeft ((int) (W * 0.04f)), juce::Justification::centredLeft, false);
        g.setFont (juce::FontOptions ("Times New Roman", 44.0f, juce::Font::bold));
        for (int i = 0; i < 26; ++i) { g.setColour (Colour (0xff4cc27a).withAlpha (rnd (0.15f, 0.35f))); g.drawText ("$", (int) rnd (0, W), (int) rnd (0, H), 40, 50, juce::Justification::centred); }
    }
    else if (s.id == "santo")
    {   // light from above, stained glass, halos, small crosses
        for (int i = 0; i < 14; ++i)
        {
            const float x0 = W * 0.5f + rnd (-120, 120);
            const float x1 = rnd (-200, W + 200);
            juce::Path ray; ray.startNewSubPath (x0 - 10, -10); ray.lineTo (x0 + 10, -10); ray.lineTo (x1 + 60, H); ray.lineTo (x1 - 60, H); ray.closeSubPath();
            g.setGradientFill (juce::ColourGradient (Colours::white.withAlpha (0.35f), x0, 0, Colours::transparentWhite, x0, H * 0.8f, false));
            g.fillPath (ray);
        }
        const Colour glass[] = { Colour (0xff8fd3ff), Colour (0xffd4af37), Colour (0xffe88fa0), Colour (0xff9be3b5) };
        for (int i = 0; i < 40; ++i)
        {
            juce::Path p;
            const P c (rnd (0, W), rnd (0, H * 0.5f)); const float sz = rnd (14, 30);
            p.addTriangle (c.x, c.y - sz, c.x - sz, c.y + sz * 0.6f, c.x + sz, c.y + sz * 0.6f);
            g.setColour (glass[i % 4].withAlpha (0.12f)); g.fillPath (p);
            g.setColour (Colour (0xffb89b5a).withAlpha (0.18f)); g.strokePath (p, juce::PathStrokeType (1.5f));
        }
        for (int i = 0; i < 8; ++i)
        {
            const P c (rnd (0, W), rnd (0, H));
            g.setColour (Colour (0xffd4af37).withAlpha (0.35f)); g.drawEllipse (c.x - 22, c.y - 6, 44, 12, 2.5f);
        }
        for (int i = 0; i < 10; ++i)
        {
            const P c (rnd (0, W), rnd (0, H));
            g.setColour (Colour (0xffd4af37).withAlpha (0.25f));
            g.fillRect (c.x - 1.5f, c.y - 10, 3.0f, 22.0f); g.fillRect (c.x - 7, c.y - 4, 14.0f, 3.0f);
        }
    }
    else if (s.id == "infierno")
    {   // blue flames from the bottom, embers, cracks
        for (int i = 0; i < 40; ++i) flame (g, a.withAlpha (0.65f), Colour (0xff8a4dff), { rnd (0, W), H + 10 }, rnd (50, 120), rnd (120, 320));
        for (int i = 0; i < 30; ++i) flame (g, Colour (0xff9fd0ff).withAlpha (0.4f), a, { rnd (0, W), H + 10 }, rnd (20, 50), rnd (60, 150));
        for (int i = 0; i < 160; ++i) { g.setColour (Colour (0xffffa24d).withAlpha (rnd (0.2f, 0.7f))); const float d = rnd (1.5f, 3.5f); g.fillEllipse (rnd (0, W), rnd (0, H), d, d); }
        for (int k = 0; k < 10; ++k)
        {
            juce::Path c; P p (rnd (0, W), rnd (0, H)); c.startNewSubPath (p);
            for (int i = 0; i < 8; ++i) { p += P (rnd (-40, 40), rnd (-40, 40)); c.lineTo (p); }
            g.setColour (Colours::black.withAlpha (0.45f)); g.strokePath (c, juce::PathStrokeType (1.5f));
        }
    }
    else if (s.id == "bombo")
    {   // studio: drumsticks, cables, speaker cones
        for (int i = 0; i < 4; ++i)
        {
            const P c (rnd (60, W - 60), rnd (60, H - 60)); const float l = rnd (120, 200), an = rnd (0, pi);
            drumstick (g, Colour (0xffd9c3a0).withAlpha (0.18f), c - P (std::cos (an), std::sin (an)) * l * 0.5f, c + P (std::cos (an), std::sin (an)) * l * 0.5f);
            drumstick (g, Colour (0xffd9c3a0).withAlpha (0.18f), c - P (std::cos (an + 0.5f), std::sin (an + 0.5f)) * l * 0.5f, c + P (std::cos (an + 0.5f), std::sin (an + 0.5f)) * l * 0.5f);
        }
        for (int i = 0; i < 6; ++i)
        {
            juce::Path p; p.startNewSubPath (rnd (0, W), H + 10);
            p.cubicTo (rnd (0, W), rnd (0, H), rnd (0, W), rnd (0, H), rnd (0, W), -10);
            g.setColour (Colours::black.withAlpha (0.5f)); g.strokePath (p, juce::PathStrokeType (6.0f));
            g.setColour (a.withAlpha (0.12f)); g.strokePath (p, juce::PathStrokeType (1.5f));
        }
        for (auto c : { P (70, H - 90), P (W - 70, H - 90), P (W - 70, 120) })
            for (int k = 6; k > 0; --k)
            {
                g.setColour ((k % 2 ? Colours::black : a).withAlpha (k % 2 ? 0.35f : 0.1f));
                const float rr = (float) k * 14;
                g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 3);
            }
    }
}

struct Cache { juce::String id; int w = 0, h = 0; juce::Image img; };
}

const juce::Image& image (const Skin& s, int w, int h)
{
    static std::vector<Cache> cache;
    for (auto& c : cache) if (c.id == s.id && c.w == w && c.h == h) return c.img;
    Cache c; c.id = s.id; c.w = w; c.h = h;
    if (! s.neutral)
    {
        c.img = juce::Image (juce::Image::ARGB, w, h, true);
        juce::Graphics g (c.img);
        paint (g, s, { 0, 0, (float) w, (float) h });
    }
    if (cache.size() > 6) cache.erase (cache.begin());
    cache.push_back (c);
    return cache.back().img;
}

void drawChassis (juce::Graphics& g, const Skin& s, juce::Rectangle<int> content)
{
    if (s.neutral) return;
    const auto& img = image (s, content.getWidth(), content.getHeight());
    if (img.isValid()) { g.setOpacity (1.0f); g.drawImageAt (img, content.getX(), content.getY()); }
}

void drawInPanel (juce::Graphics& g, const Skin& s, const juce::Path& face, juce::Point<int> offset, float alpha)
{
    if (s.neutral) return;
    const auto& img = image (s, 1376, 1000);
    if (! img.isValid()) return;
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (face);
    g.setOpacity (alpha);
    g.drawImageAt (img, -offset.x, -offset.y);
}

} // namespace eclipse::decor
