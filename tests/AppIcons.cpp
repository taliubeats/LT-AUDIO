// Developer tool: draws the app icons of LT COSMOS, LT LYRA and LT WARP in the family's space style.
// Usage: lt_app_icons <out-folder>   -> icon-cosmos.png, icon-lyra.png, icon-warp.png (1024 x 1024)
#include <lt_shared/lt_shared.h>

using namespace lt::ui;
using juce::Colour;
using Rect = juce::Rectangle<float>;

namespace
{
constexpr float S = 1024.0f;
constexpr float pi = juce::MathConstants<float>::pi;

juce::Path tilePath() { juce::Path p; p.addRoundedRectangle (Rect (0, 0, S, S).reduced (S * 0.02f), S * 0.2f); return p; }

void spaceTile (juce::Graphics& g, int seed)
{
    auto p = Palette::dark();
    g.saveState();
    g.reduceClipRegion (tilePath());
    g.fillAll (p.background);
    g.drawImageAt (NebulaBackground::renderNebula ((int) S, (int) S, p, seed, -0.35f, 0.9f), 0, 0);
    juce::Random rnd (seed);
    for (int i = 0; i < 260; ++i)
    {
        auto sz = rnd.nextFloat() < 0.9f ? 2.0f + rnd.nextFloat() * 3.0f : 5.0f + rnd.nextFloat() * 3.0f;
        g.setColour (juce::Colours::white.withAlpha (0.2f + rnd.nextFloat() * 0.7f));
        g.fillEllipse (rnd.nextFloat() * S, rnd.nextFloat() * S, sz, sz);
    }
    g.restoreState();
}

void rim (juce::Graphics& g)
{
    auto p = Palette::dark();
    auto r = Rect (0, 0, S, S).reduced (S * 0.02f);
    Glass::drawRectGlow (g, r.reduced (12.0f), S * 0.19f, p.accent, 28.0f, 0.55f);
    g.setColour (p.nebulaC.withAlpha (0.9f));
    g.drawRoundedRectangle (r.reduced (16.0f), S * 0.19f, 10.0f);
}

void glow (juce::Graphics& g, juce::Point<float> c, float r, Colour col, float alpha)
{
    juce::ColourGradient grad (col.withAlpha (alpha), c.x, c.y, col.withAlpha (0.0f), c.x + r, c.y, true);
    g.setGradientFill (grad);
    g.fillEllipse (Rect (r * 2, r * 2).withCentre (c));
}

// ---------------------------------------------------------------------------- COSMOS: a radial spectrum around a star
void cosmos (juce::Graphics& g)
{
    spaceTile (g, 31);
    g.saveState();
    g.reduceClipRegion (tilePath());
    auto c = juce::Point<float> (S * 0.5f, S * 0.5f);
    const int bars = 56;
    for (int i = 0; i < bars; ++i)
    {
        auto a = -pi * 0.5f + 2.0f * pi * (float) i / bars;
        // a spectrum-like shape: strong lows, a mid bump, airy top, mirrored for symmetry
        auto t = (float) i / bars;
        auto m = std::abs (t - 0.5f) * 2.0f;
        auto h = 0.35f + 0.45f * std::pow (1.0f - m, 1.4f) + 0.18f * std::sin (t * 9.0f * pi) * (1.0f - m) + 0.1f * std::sin (t * 23.0f);
        auto r0 = S * 0.2f, r1 = r0 + S * 0.2f * juce::jlimit (0.2f, 1.0f, h);
        auto col = Colour (0xffb9a8ff).interpolatedWith (Colour (0xffffb35c), juce::jlimit (0.0f, 1.0f, (h - 0.4f) * 1.6f));
        juce::Path bar;
        bar.addRoundedRectangle (-S * 0.011f, -r1, S * 0.022f, r1 - r0, S * 0.011f);
        bar.applyTransform (juce::AffineTransform::rotation (a + pi * 0.5f).translated (c.x, c.y));
        g.setColour (col.withAlpha (0.92f));
        g.fillPath (bar);
    }
    // LUFS arc: three quarters of a ring, partly lit
    juce::Path arcBg, arcOn;
    auto ra = S * 0.435f;
    arcBg.addCentredArc (c.x, c.y, ra, ra, 0, -pi * 0.75f, pi * 0.75f, true);
    arcOn.addCentredArc (c.x, c.y, ra, ra, 0, -pi * 0.75f, pi * 0.32f, true);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.strokePath (arcBg, juce::PathStrokeType (S * 0.022f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (Colour (0xff8b5cf6), c.x - ra, c.y + ra, Colour (0xffffb35c), c.x + ra, c.y - ra, false));
    g.strokePath (arcOn, juce::PathStrokeType (S * 0.022f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // the core
    glow (g, c, S * 0.22f, Colour (0xffa78bfa), 0.55f);
    glow (g, c, S * 0.1f, juce::Colours::white, 0.9f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (Rect (S * 0.07f, S * 0.07f).withCentre (c));
    g.restoreState();
    rim (g);
}

// ---------------------------------------------------------------------------- LYRA: the Lyra constellation over a staff
void lyra (juce::Graphics& g)
{
    spaceTile (g, 57);
    g.saveState();
    g.reduceClipRegion (tilePath());
    // faint five-line staff across the tile
    for (int i = 0; i < 5; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.fillRect (Rect (S * 0.08f, S * (0.62f + 0.04f * i), S * 0.84f, S * 0.006f));
    }
    // Vega (bright), the little triangle, and the parallelogram that makes the lyre
    juce::Point<float> vega (S * 0.36f, S * 0.24f), e1 (S * 0.47f, S * 0.3f), z1 (S * 0.43f, S * 0.4f),
                       a (S * 0.44f, S * 0.44f), b (S * 0.62f, S * 0.48f), cc (S * 0.66f, S * 0.74f), d (S * 0.46f, S * 0.7f);
    juce::Path lines;
    lines.startNewSubPath (vega); lines.lineTo (e1); lines.lineTo (z1); lines.lineTo (vega);
    lines.startNewSubPath (z1); lines.lineTo (a);
    lines.startNewSubPath (a); lines.lineTo (b); lines.lineTo (cc); lines.lineTo (d); lines.closeSubPath();
    g.setColour (Colour (0xffc4b5fd).withAlpha (0.85f));
    g.strokePath (lines, juce::PathStrokeType (S * 0.008f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // lyre strings inside the parallelogram
    for (int i = 1; i < 4; ++i)
    {
        auto t = (float) i / 4.0f;
        auto top = a + (b - a) * t, bottom = d + (cc - d) * t;
        g.setColour (Colour (0xffffd59a).withAlpha (0.55f));
        g.drawLine ({ top, bottom }, S * 0.004f);
    }
    for (auto [p, r] : { std::pair<juce::Point<float>, float> { vega, 0.05f }, { e1, 0.022f }, { z1, 0.024f }, { a, 0.026f }, { b, 0.026f }, { cc, 0.024f }, { d, 0.024f } })
    {
        glow (g, p, S * r * 2.6f, Colour (0xffa78bfa), 0.6f);
        glow (g, p, S * r, juce::Colours::white, 0.95f);
    }
    // a whole note on the staff, in the corner
    auto n = juce::Point<float> (S * 0.72f, S * 0.66f);
    juce::Path head;
    head.addEllipse (-S * 0.06f, -S * 0.04f, S * 0.12f, S * 0.08f);
    juce::Path hole;
    hole.addEllipse (-S * 0.025f, -S * 0.03f, S * 0.05f, S * 0.06f);
    hole.applyTransform (juce::AffineTransform::rotation (-0.6f));
    head.addPath (hole);
    head.setUsingNonZeroWinding (false);
    g.setColour (Colour (0xffffd59a));
    g.fillPath (head, juce::AffineTransform::translation (n));
    g.restoreState();
    rim (g);
}

// ---------------------------------------------------------------------------- WARP: a hyperspace tunnel
void warp (juce::Graphics& g)
{
    spaceTile (g, 83);
    g.saveState();
    g.reduceClipRegion (tilePath());
    auto c = juce::Point<float> (S * 0.5f, S * 0.5f);
    juce::Random rnd (9);
    for (int i = 0; i < 140; ++i)
    {
        auto ang = rnd.nextFloat() * 2.0f * pi;
        auto r0 = S * (0.06f + rnd.nextFloat() * 0.2f), len = S * (0.08f + rnd.nextFloat() * 0.35f);
        auto dir = juce::Point<float> (std::cos (ang), std::sin (ang));
        auto col = rnd.nextFloat() < 0.3f ? Colour (0xffffb35c) : Colour (0xffc4b5fd);
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.0f), c.x + dir.x * r0, c.y + dir.y * r0, col.withAlpha (0.85f),
                                                 c.x + dir.x * (r0 + len), c.y + dir.y * (r0 + len), false));
        g.drawLine ({ c + dir * r0, c + dir * (r0 + len) }, S * (0.004f + rnd.nextFloat() * 0.006f));
    }
    // tunnel frames shrinking into the distance
    for (int k = 0; k < 7; ++k)
    {
        auto t = std::pow (0.74f, (float) k);
        auto side = S * 0.7f * t;
        g.setColour (Colour (0xffa78bfa).withAlpha (0.2f + 0.12f * (6 - k) / 6.0f));
        g.drawRoundedRectangle (Rect (side, side).withCentre (c), side * 0.2f, S * 0.008f * (0.4f + t));
    }
    glow (g, c, S * 0.14f, juce::Colours::white, 0.85f);
    g.restoreState();
    rim (g);
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File out (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    out.createDirectory();
    std::pair<const char*, void (*) (juce::Graphics&)> icons[] = { { "cosmos", cosmos }, { "lyra", lyra }, { "warp", warp } };
    for (auto& [name, fn] : icons)
    {
        juce::Image img (juce::Image::ARGB, (int) S, (int) S, true);
        {
            juce::Graphics g (img);
            fn (g);
        }
        auto f = out.getChildFile (juce::String ("icon-") + name + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::cout << "wrote " << f.getFullPathName() << std::endl;
    }
    return 0;
}
