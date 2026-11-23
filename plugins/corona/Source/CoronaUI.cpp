#include "CoronaUI.h"
#include "CoronaData.h"

namespace corona::ui
{
namespace
{
constexpr float kPi = juce::MathConstants<float>::pi;
const juce::Colour INK (0xffe6dcc5);

// tiny deterministic random so every knob keeps its own scratches
struct Lcg { juce::uint32 s; explicit Lcg (juce::uint32 seed) : s (seed ? seed : 1) {}
             float next() { s = s * 1664525u + 1013904223u; return (float) ((s >> 8) & 0xffffff) / 16777216.0f; } };

inline float lum (juce::Colour c) { return 0.3f * c.getRed() + 0.6f * c.getGreen() + 0.1f * c.getBlue(); }
inline juce::Point<float> polar (float cx, float cy, float r, float deg)
{
    const float a = juce::degreesToRadians (deg);
    return { cx + r * std::sin (a), cy - r * std::cos (a) };
}

// ---- meter geometry (measured on the picture) ----
constexpr float MPX = 690, MPY = 286, MRX = 149, MRY = 129;
const float MARKS[10] = { 20, 15, 10, 7, 5, 4, 3, 2, 1, 0 };
const float MTH[10]   = { -44, -31, -21.5f, -10.5f, -2.8f, 5.6f, 13.8f, 24.5f, 34.5f, 43.6f };
const float TUBES[4]  = { 358, 453, 926, 1061 };

float meterAngle (float gr)
{
    gr = juce::jlimit (0.0f, 20.0f, gr);
    for (int i = 0; i < 9; ++i)
        if (gr <= MARKS[i] && gr >= MARKS[i + 1])
            return MTH[i] + (MTH[i + 1] - MTH[i]) * (MARKS[i] - gr) / (MARKS[i] - MARKS[i + 1]);
    return MTH[9];
}

juce::Path meterFace()
{
    juce::Path p;
    p.startNewSubPath (549, 287); p.lineTo (549, 150);
    p.cubicTo (549, 96, 610, 93, 690, 93); p.cubicTo (770, 93, 828, 96, 828, 150);
    p.lineTo (828, 287); p.closeSubPath();
    return p;
}

// ---- photo clean-up ----
void inpaintNeedle (juce::Image& img)
{
    // rebuild every pixel of the painted needle by blending the clean face on both sides of it
    const float x0 = 580, y0 = 195, x1 = 669, y1 = 285, len = std::hypot (x1 - x0, y1 - y0);
    const float ux = (x1 - x0) / len, uy = (y1 - y0) / len, px = -uy, py = ux;
    juce::Image src = img.createCopy();
    juce::Image::BitmapData s (src, juce::Image::BitmapData::readOnly), d (img, juce::Image::BitmapData::writeOnly);
    auto half = [] (float t) { return t < 9 ? 3.2f : t < 36 ? 3.2f + 6.0f * juce::jmin (1.0f, (t - 9) / 8) : 7.5f; };
    for (int yy = 175; yy < 300; ++yy)
        for (int xx = 540; xx < (int) MPX - 14; ++xx)
        {
            const float t = (xx - x0) * ux + (yy - y0) * uy, sd = (xx - x0) * px + (yy - y0) * py;
            if (t < 1 || t > len) continue;
            const float hw = half (t);
            if (std::abs (sd) > hw) continue;
            const float e = hw + 3, w = (sd + e) / (2 * e);
            auto A = s.getPixelColour (juce::roundToInt (x0 + ux * t + px * e), juce::roundToInt (y0 + uy * t + py * e));
            auto B = s.getPixelColour (juce::roundToInt (x0 + ux * t - px * e), juce::roundToInt (y0 + uy * t - py * e));
            d.setPixelColour (xx, yy, t > len - 24 ? B : B.interpolatedWith (A, w));   // near the pivot only the face side, never the cap
        }
}

juce::Image makeCold (const juce::Image& hot)
{
    // remove the red tube light: tubes as if cold (and the pilot lamp off)
    juce::Image c = hot.createCopy();
    juce::Image::BitmapData d (c, juce::Image::BitmapData::readWrite);
    auto strip = [&d] (int x0, int y0, int x1, int y1, float kr, float kg, float kb)
    {
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x)
            {
                auto p = d.getPixelColour (x, y);
                const float r = p.getRed(), g = p.getGreen(), b = p.getBlue(), e = r - (g + b) * 0.5f;
                if (e <= 8) continue;
                const float k = e - 8;
                d.setPixelColour (x, y, juce::Colour ((juce::uint8) juce::jlimit (0.0f, 255.0f, r - k * kr),
                                                      (juce::uint8) juce::jlimit (0.0f, 255.0f, g - k * kg),
                                                      (juce::uint8) juce::jlimit (0.0f, 255.0f, b - k * kb)));
            }
    };
    strip (0, 53, W, FACE_Y, 0.92f, 0.32f, 0.18f);
    strip (1045, 455, 1115, 525, 0.85f, 0.25f, 0.15f);
    return c;
}

void featherAlpha (juce::Image& img, std::function<float (int, int)> alphaAt)
{
    juce::Image::BitmapData d (img, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
        {
            auto p = d.getPixelColour (x, y);
            d.setPixelColour (x, y, p.withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, alphaAt (x, y))));
        }
}

void text (juce::Graphics& g, const juce::String& t, float x, float y, float size, float kerning = 0.0f)
{
    auto f = labelFont (size).withExtraKerningFactor (kerning);
    g.setFont (f);
    const auto r = juce::Rectangle<float> (x - 120, y - size, 240, size * 2);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawText (t, r.translated (0.6f, 0.9f), juce::Justification::centred, false);
    g.setColour (INK);
    g.drawText (t, r, juce::Justification::centred, false);
}

juce::Image buildFace (const juce::Image& photo)
{
    const int fh = H - FACE_Y;
    juce::Image face (juce::Image::ARGB, W, fh, true);
    {
        // brushed dark metal: per-row streaks + fine grain, slightly warm, darker toward the bottom and edges
        juce::Image::BitmapData d (face, juce::Image::BitmapData::writeOnly);
        Lcg r (4242); float row = 0;
        for (int y = 0; y < fh; ++y)
        {
            row = row * 0.6f + (r.next() - 0.5f) * 5.0f; float n = 0;
            for (int x = 0; x < W; ++x)
            {
                n = n * 0.96f + (r.next() - 0.5f) * 1.6f;
                const float vx = (x - 688.0f) / 688.0f;
                const float v = 31 - 9.0f * y / fh - 5 * vx * vx + row * 0.6f + n * 1.3f + (r.next() - 0.5f) * 2.2f;
                d.setPixelColour (x, y, juce::Colour ((juce::uint8) juce::jlimit (0.0f, 255.0f, v * 1.03f),
                                                      (juce::uint8) juce::jlimit (0.0f, 255.0f, v),
                                                      (juce::uint8) juce::jlimit (0.0f, 255.0f, v * 0.95f)));
            }
        }
    }
    juce::Graphics g (face);
    // the photo's top ridge, melted into the metal
    {
        auto ridge = photo.getClippedImage ({ 0, FACE_Y, W, 26 }).createCopy().convertedToFormat (juce::Image::ARGB);
        featherAlpha (ridge, [] (int, int y) { return y < 12 ? 1.0f : 1.0f - (y - 12) / 14.0f; });
        g.drawImageAt (ridge, 0, 0);
    }
    Lcg r (77);
    for (int i = 0; i < 40; ++i)
    {
        const float x = r.next() * W, y = 30 + r.next() * (fh - 40), l = 20 + r.next() * 120, a = (r.next() - 0.5f) * 0.5f;
        g.setColour (juce::Colours::white.withAlpha (0.015f + 0.03f * r.next()));
        g.drawLine (x, y, x + l * std::cos (a), y + l * std::sin (a), 0.6f);
    }
    // cream inset frame
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (58, 20, 1260, 327, 14, 4.5f);
    g.setColour (juce::Colour (0xffc9bb9a));            g.drawRoundedRectangle (58, 20, 1260, 327, 14, 2.4f);
    // screws
    auto screw = [&g] (float x, float y, float a)
    {
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 3, { 0, 1 }).drawForRectangle (g, juce::Rectangle<int> ((int) x - 9, (int) y - 9, 18, 18));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3e3c40), x - 2, y - 2, juce::Colour (0xff0b0a0c), x + 9, y + 9, true));
        g.fillEllipse (x - 9, y - 9, 18, 18);
        juce::Path slot; slot.addRectangle (-6.0f, -1.2f, 12.0f, 2.4f);
        g.setColour (juce::Colour (0xff050405)); g.fillPath (slot, juce::AffineTransform::rotation (a).translated (x, y));
    };
    screw (20, 30, 0.7f); screw (1356, 30, -0.5f); screw (20, 335, 1.9f); screw (1356, 335, 1.2f);
    // maker tag
    g.setColour (juce::Colours::white.withAlpha (0.04f)); g.fillRoundedRectangle (80, 27, 172, 26, 3);
    text (g, juce::CharPointer_UTF8 ("LT AUDIO \xc2\xb7 TYPE 6"), 166, 40, 17, 0.06f);
    // logo + caption and the pilot lamp come from the photo, feathered into the metal
    {
        auto logo = photo.getClippedImage ({ 520, 588, 340, 132 }).createCopy().convertedToFormat (juce::Image::ARGB);
        featherAlpha (logo, [] (int x, int y) { return juce::jmin (1.0f, x / 22.0f, (339 - x) / 22.0f) * juce::jmin (1.0f, y / 22.0f, (131 - y) / 22.0f); });
        g.drawImageAt (logo, 520, 588 - FACE_Y);
        auto lamp = photo.getClippedImage ({ 1080 - 36, 490 - 36, 72, 72 }).createCopy().convertedToFormat (juce::Image::ARGB);
        featherAlpha (lamp, [] (int x, int y) { const float d = std::hypot (x - 36.0f, y - 36.0f); return 1.0f - (d - 31.0f) / 5.0f; });
        g.drawImageAt (lamp, 1080 - 36, 490 - 36 - FACE_Y);
    }
    // ---- scales, all from the knob centres ----
    auto fy = [] (float y) { return y - (float) FACE_Y; };
    auto arcLabels = [&] (const char* id, const juce::StringArray& labels, float rad, float size)
    {
        const auto& k = spec (id);
        for (int i = 0; i < labels.size(); ++i)
        {
            const float a = k.steps.empty() ? k.a0 + (k.a1 - k.a0) * i / (labels.size() - 1) : k.steps[(size_t) i];
            const auto p = polar ((float) k.x, fy ((float) k.y), rad, a);
            text (g, labels[i], p.x, p.y, size);
        }
    };
    arcLabels ("input", { "-24", "-18", "-12", "-6", "0", "+6", "+12", "+18", "+24" }, 86, 15.5f);
    juce::StringArray thr; for (int i = 0; i <= 40; i += 2) thr.add (juce::String (i));
    arcLabels ("threshold", thr, 85, 14.5f);
    arcLabels ("makeup", { "0", "3", "6", "9", "12", "15", "18", "21", "24" }, 86, 15.5f);
    arcLabels ("mix", { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" }, 86, 15.5f);
    arcLabels ("debump", { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" }, 45, 13.5f);
    arcLabels ("filter", { "OFF", "50", "100", "150", "200", "250" }, 56, 15);
    {
        const auto& k = spec ("debump");
        g.setColour (INK);
        for (int i = 0; i <= 10; ++i)
        {
            const float a = k.a0 + (k.a1 - k.a0) * i / 10.0f;
            g.drawLine (juce::Line<float> (polar ((float) k.x, fy ((float) k.y), 33, a), polar ((float) k.x, fy ((float) k.y), 37, a)), 1.2f);
        }
    }
    for (auto* id : { "attack", "release" })
    {
        const auto& k = spec (id);
        text (g, "SLOW", k.x - 44.0f, fy (k.y + 39.0f), 16); text (g, "FAST", k.x + 44.0f, fy (k.y + 39.0f), 16);
    }
    auto dot = [&] (const char* id, float deg) { const auto& k = spec (id); auto p = polar ((float) k.x, fy ((float) k.y), 43, deg);
                                                 g.setColour (INK); g.fillEllipse (p.x - 3, p.y - 3, 6, 6); };
    dot ("attack", 0); dot ("release", -40); dot ("release", 0); dot ("release", 40);
    for (auto [id, n] : { std::pair { "attack", "ATTACK" }, { "release", "RELEASE" }, { "debump", "DE-BUMP" }, { "filter", "FILTER" } })
        text (g, n, (float) spec (id).x, fy (553), 17, 0.03f);
    text (g, "POWER", 1080, fy (553), 17, 0.03f);
    for (auto [id, n] : { std::pair { "input", "INPUT" }, { "threshold", "THRESHOLD" }, { "makeup", "MAKE UP" }, { "mix", "DRY / WET" } })
        text (g, n, (float) spec (id).x, fy (734), 18, 0.03f);
    text (g, "POWER", 1197, fy (446), 16, 0.03f); text (g, "OFF", 1153, fy (491), 16); text (g, "ON", 1243, fy (491), 16);
    text (g, "OUT", 314, fy (661), 16); text (g, "AUTO GAIN", 314, fy (734), 18, 0.03f);
    return face;
}
} // namespace

juce::Font labelFont (float size)
{
    static const bool hasAvenir = juce::Font::findAllTypefaceNames().contains ("Avenir Next Condensed");
    if (hasAvenir) return juce::Font (juce::FontOptions ("Avenir Next Condensed", size, juce::Font::plain).withStyle ("Medium"));
    return juce::Font (juce::FontOptions (size * 0.92f)).withHorizontalScale (0.86f);
}

const Art& Art::get() { static Art art; return art; }

Art::Art()
{
    auto photo = juce::ImageCache::getFromMemory (CoronaData::coronaface_png, CoronaData::coronaface_pngSize);
    hot = photo.rescaled (W, H).convertedToFormat (juce::Image::ARGB).createCopy();
    inpaintNeedle (hot);
    cold = makeCold (hot);
    face = buildFace (hot);
}

const std::array<KnobSpec, 8>& knobSpecs()
{
    static const std::array<KnobSpec, 8> k {{
        { "attack",    312, 491, 32, 0, 0, { -130, 0, 130 }, false },
        { "release",   545, 491, 32, 0, 0, { -130, -40, 0, 40, 130 }, false },
        { "debump",    692, 491, 27, -146, 146, {}, false },
        { "filter",    845, 491, 32, 0, 0, { -109, -69, -27, 24, 70, 109 }, false },
        { "input",     192, 645, 63, -114, 114, {}, true },
        { "threshold", 427, 645, 63, -147, 147, {}, true },
        { "makeup",    964, 645, 63, -113, 113, {}, true },
        { "mix",      1196, 645, 63, -145, 145, {}, true },
    }};
    return k;
}

const KnobSpec& spec (const juce::String& id)
{
    for (auto& k : knobSpecs()) if (id == k.id) return k;
    return knobSpecs()[0];
}

// =================================================================== knob
Knob::Knob (const KnobSpec& sp) : s (sp)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity (s.steps.empty() ? 240 : 120);
    setPopupDisplayEnabled (true, true, nullptr, 900);
    setScrollWheelEnabled (true);
    const int R = juce::roundToInt (s.r * 1.5f);
    setBounds (s.x - R, s.y - R, R * 2, R * 2);

    // the knob's own wear, in its own frame (turns with it), drawn at 2x
    const float r = s.r;
    const int S = (int) std::ceil (r * 2.2f) * 2;
    wear = juce::Image (juce::Image::ARGB, S, S, true);
    juce::Graphics g (wear);
    g.addTransform (juce::AffineTransform::scale (2.0f).translated (S * 0.5f, S * 0.5f));
    Lcg rnd ((juce::uint32) (s.x * 31 + s.y * 17));
    const float area = (r / 60) * (r / 60);
    for (int i = 0; i < (int) (2600 * area + 300); ++i)
    {
        const float a = rnd.next() * 6.283f, rr = std::sqrt (rnd.next()) * r;
        g.setColour (rnd.next() < 0.5f ? juce::Colours::white.withAlpha (0.02f + 0.03f * rnd.next()) : juce::Colours::black.withAlpha (0.15f + 0.2f * rnd.next()));
        g.fillRect (rr * std::sin (a), -rr * std::cos (a), 0.6f, 0.6f);
    }
    for (int i = 0; i < (s.big ? 26 : 10); ++i)
    {
        const float rr = r * (0.12f + 0.6f * rnd.next()), a0 = rnd.next() * 6.283f, span = 0.15f + 0.7f * rnd.next();
        juce::Path arc; arc.addCentredArc (0, 0, rr, rr, 0, a0, a0 + span, true);
        g.setColour (juce::Colours::white.withAlpha (0.04f + 0.07f * rnd.next()));
        g.strokePath (arc, juce::PathStrokeType (0.35f + 0.4f * rnd.next()));
    }
    for (int i = 0; i < (s.big ? 9 : 4); ++i)
    {
        const float a = rnd.next() * 6.283f, rr = r * (0.2f + 0.7f * rnd.next()), len = r * (0.12f + 0.3f * rnd.next()), dir = rnd.next() * 6.283f;
        const float x = rr * std::sin (a), y = -rr * std::cos (a);
        g.setColour (juce::Colours::white.withAlpha (0.05f + 0.1f * rnd.next()));
        g.drawLine (x, y, x + len * std::cos (dir), y + len * std::sin (dir), 0.3f + 0.35f * rnd.next());
    }
    for (int i = 0; i < (s.big ? 5 : 2); ++i)
    {
        const float a = rnd.next() * 6.283f, rr = r * (0.86f + 0.1f * rnd.next());
        juce::Path nick; nick.addCentredArc (0, 0, rr, rr, 0, a, a + 0.06f + 0.08f * rnd.next(), true);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.strokePath (nick, juce::PathStrokeType (0.8f));
    }
    for (int i = 0; i < (s.big ? 14 : 6); ++i)
    {
        const float a = rnd.next() * 6.283f, rr = r * rnd.next() * 0.9f, d = 0.35f + 0.5f * rnd.next();
        g.setColour (juce::Colour (0xffc8beaa).withAlpha (0.08f + 0.1f * rnd.next()));
        g.fillEllipse (rr * std::sin (a) - d, -rr * std::cos (a) - d, d * 2, d * 2);
    }
    for (int i = 0; i < (s.big ? 3 : 2); ++i) chips.push_back ({ 0.3f + 0.65f * rnd.next(), (rnd.next() - 0.5f) * 1.2f, 0.3f + 0.35f * rnd.next() });
}

void Knob::paint (juce::Graphics& g)
{
    const float R = getWidth() * 0.5f, cx = R, cy = R, r = s.r;
    float deg;
    if (! s.steps.empty())
        deg = s.steps[(size_t) juce::jlimit (0, (int) s.steps.size() - 1, juce::roundToInt (getValue() - getMinimum()))];
    else
        deg = s.a0 + (s.a1 - s.a0) * (float) ((getValue() - getMinimum()) / (getMaximum() - getMinimum()));
    const float ang = juce::degreesToRadians (deg);

    const int n = s.big ? 9 : 40;
    const float amp = s.big ? 0.06f : 0.03f;
    juce::Path skirt;
    for (int i = 0; i <= 720; ++i)
    {
        const float t = i * kPi / 360.0f, rr = r * (1 - amp + amp * std::cos (n * (t - ang)));
        const float x = cx + rr * std::sin (t), y = cy - rr * std::cos (t);
        if (i == 0) skirt.startNewSubPath (x, y); else skirt.lineTo (x, y);
    }
    skirt.closeSubPath();

    juce::DropShadow (juce::Colours::black.withAlpha (0.7f), (int) (r * 0.55f), { (int) (r * 0.08f), (int) (r * 0.26f) }).drawForPath (g, skirt);
    juce::DropShadow (juce::Colours::black.withAlpha (0.95f), (int) (r * 0.1f), { 0, (int) (r * 0.04f) + 1 }).drawForPath (g, skirt);

    {
        juce::ColourGradient gr (juce::Colour (0xff2d2a2a), cx - r * 0.8f, cy - r, juce::Colour (0xff050505), cx + r * 0.6f, cy + r, false);
        gr.addColour (0.4, juce::Colour (0xff141213));
        g.setGradientFill (gr); g.fillPath (skirt);
    }
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (skirt);
        for (int i = 0; i < n; ++i)
        {
            const float t = ang + i * 2 * kPi / n, face = std::cos (t + 0.8f);
            const float x = cx + r * 0.92f * std::sin (t), y = cy - r * 0.92f * std::cos (t), rad = r * (s.big ? 0.3f : 0.1f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffffaf0).withAlpha ((s.big ? 0.11f : 0.08f) * juce::jmax (0.0f, face) + 0.02f), x, y,
                                                     juce::Colours::transparentWhite, x + rad, y, true));
            g.fillEllipse (x - rad, y - rad, rad * 2, rad * 2);
            if (s.big && face < 0)
            {
                g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.35f * -face), x, y, juce::Colours::transparentBlack, x + r * 0.3f, y, true));
                g.fillEllipse (x - r * 0.3f, y - r * 0.3f, r * 0.6f, r * 0.6f);
            }
        }
        g.drawImageTransformed (wear, juce::AffineTransform::translation (-wear.getWidth() * 0.5f, -wear.getHeight() * 0.5f)
                                          .scaled (0.5f).rotated (ang).translated (cx, cy));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x00786450), cx, cy + r * 0.4f, juce::Colour (0x14786450), cx, cy + r, false));
        g.fillRect (getLocalBounds());
    }
    g.setColour (juce::Colours::black.withAlpha (0.95f)); g.strokePath (skirt, juce::PathStrokeType (1.0f));
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (skirt);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x52e6e1d7), cx - r, cy - r, juce::Colour (0x00e6e1d7), cx + r * 0.2f, cy + r * 0.2f, false));
        g.strokePath (skirt, juce::PathStrokeType (s.big ? 2.4f : 1.5f));
    }
    // bevel + glossy cap
    const float capR = r * (s.big ? 0.72f : 0.68f), bev = capR + (s.big ? 3.2f : 2.0f);
    juce::Path bevel; bevel.addEllipse (cx - bev, cy - bev, bev * 2, bev * 2);
    juce::DropShadow (juce::Colours::black.withAlpha (0.8f), (int) (capR * 0.18f), { 0, (int) (capR * 0.06f) }).drawForPath (g, bevel);
    {
        juce::ColourGradient gr (juce::Colour (0xff55524f), cx, cy - capR, juce::Colour (0xff020202), cx, cy + capR, false);
        gr.addColour (0.35, juce::Colour (0xff1c1a1b));
        g.setGradientFill (gr); g.fillPath (bevel);
    }
    juce::Path cap; cap.addEllipse (cx - capR, cy - capR, capR * 2, capR * 2);
    {
        juce::ColourGradient gr (juce::Colour (0xff33302f), cx - capR * 0.3f, cy - capR * 0.42f, juce::Colour (0xff040404), cx + capR * 0.75f, cy + capR * 0.6f, true);
        gr.addColour (0.5, juce::Colour (0xff121011));
        g.setGradientFill (gr); g.fillPath (cap);
    }
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (cap);
        g.setOpacity (0.9f);
        g.drawImageTransformed (wear, juce::AffineTransform::translation (-wear.getWidth() * 0.5f, -wear.getHeight() * 0.5f)
                                          .scaled (0.5f).rotated (ang).translated (cx, cy));
        g.setOpacity (1.0f);
        const float sx = cx - capR * 0.38f, sy = cy - capR * 0.5f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x33fffaf0), sx, sy, juce::Colour (0x00fffaf0), sx + capR * 0.7f, sy, true));
        g.fillEllipse (sx - capR * 0.7f, sy - capR * 0.7f, capR * 1.4f, capR * 1.4f);
        juce::Path band; band.addCentredArc (cx + capR * 0.15f, cy + capR * 0.25f, capR * 0.95f, capR * 0.95f, 0, kPi * 1.62f, kPi * 2.02f, true);
        g.setColour (juce::Colour (0x12fffaf0)); g.strokePath (band, juce::PathStrokeType (capR * 0.12f));
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (cx - capR + 0.5f, cy - capR + 0.5f, capR * 2 - 1, capR * 2 - 1, 1.0f);
    }
    // pointer: aged cream paint, worn and chipped
    const float r0 = r * (s.big ? 0.2f : 0.16f), r1 = r * (s.big ? 0.94f : 0.9f), sx = std::sin (ang), sy = -std::cos (ang), lw = s.big ? 3.3f : 2.3f;
    const juce::Line<float> line (cx + r0 * sx, cy + r0 * sy, cx + r1 * sx, cy + r1 * sy);
    juce::Path pl; pl.startNewSubPath (line.getStart()); pl.lineTo (line.getEnd());
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.strokePath (pl, juce::PathStrokeType (lw + 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (0.6f, 1.0f));
    {
        juce::ColourGradient gr (juce::Colour (0xffc9c0ac), line.getStartX(), line.getStartY(), juce::Colour (0xffd8cfba), line.getEndX(), line.getEndY(), false);
        gr.addColour (0.6, juce::Colour (0xffe9e1cd));
        g.setGradientFill (gr);
        g.strokePath (pl, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (juce::Colour (0xa61e1b1a));
    for (auto& c : chips)
    {
        const float rr = r0 + (r1 - r0) * c[0], x = cx + rr * sx + (-sy) * c[1] * lw * 0.4f, y = cy + rr * sy + sx * c[1] * lw * 0.4f, cr = c[2] * (s.big ? 1.0f : 0.7f);
        g.fillEllipse (x - cr, y - cr, cr * 2, cr * 2);
    }
}

// =================================================================== lever
Lever::Lever (bool tilt, bool withPlate) : juce::Button ({}), tilting (tilt), plate (withPlate)
{
    setClickingTogglesState (true);
}

void Lever::paintButton (juce::Graphics& g, bool, bool)
{
    const float cx = tilting ? 43.0f : 40.0f, cy = tilting ? 45.0f : 46.0f;
    if (plate)
    {
        juce::Path pp; pp.addRoundedRectangle (cx - 19, cy - 17, 38, 34, 4);
        juce::DropShadow (juce::Colours::black.withAlpha (0.85f), 5, { 0, 2 }).drawForPath (g, pp);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2829), 0, cy - 17, juce::Colour (0xff080808), 0, cy + 17, false));
        g.fillPath (pp);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (cx - 18.5f, cy - 16.5f, 37, 33, 4, 1);
        for (float sy : { -12.0f, 12.0f })
            for (float sx : { -15.0f, 15.0f })
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9a9690), cx + sx - 0.5f, cy + sy - 0.8f, juce::Colour (0xff2b2a28), cx + sx + 2, cy + sy + 2, true));
                g.fillEllipse (cx + sx - 2, cy + sy - 2, 4, 4);
            }
    }
    else
    {
        juce::Path w; w.addEllipse (cx - 11, cy - 11, 22, 22);
        juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 4, { 0, 2 }).drawForPath (g, w);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a8783), cx - 3, cy - 4, juce::Colour (0xff141313), cx + 8, cy + 8, true));
        g.fillPath (w);
    }
    // hex nut
    juce::Path nut;
    const float nr = plate ? 8.5f : 8.0f;
    for (int i = 0; i < 6; ++i)
    {
        const float a = i * kPi / 3 + kPi / 6;
        if (i == 0) nut.startNewSubPath (cx + nr * std::cos (a), cy + nr * std::sin (a)); else nut.lineTo (cx + nr * std::cos (a), cy + nr * std::sin (a));
    }
    nut.closeSubPath();
    {
        juce::ColourGradient gr (juce::Colour (0xfff2efea), cx - 8, cy - 8, juce::Colour (0xff3b3936), cx + 8, cy + 8, false);
        gr.addColour (0.45, juce::Colour (0xff9c9893));
        g.setGradientFill (gr); g.fillPath (nut);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.strokePath (nut, juce::PathStrokeType (0.7f));
    }
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a6763), cx - 1, cy - 1, juce::Colour (0xff1c1b1a), cx + 4.8f, cy, true));
    g.fillEllipse (cx - 4.8f, cy - 4.8f, 9.6f, 9.6f);

    // lever
    const bool on = getToggleState();
    float a, len;
    if (tilting) { a = juce::degreesToRadians (on ? 24.0f : -24.0f); len = 24; }
    else         { a = on ? kPi : 0.0f; len = on ? 13.0f : 21.0f; }
    juce::Path lv;
    lv.startNewSubPath (-2.8f, 0); lv.lineTo (-3.8f, -len); lv.lineTo (3.8f, -len); lv.lineTo (2.8f, 0); lv.closeSubPath();
    const auto tr = juce::AffineTransform::rotation (a).translated (cx, cy);
    {
        juce::Path shadow (lv); shadow.applyTransform (tr);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 3, { 2, 3 }).drawForPath (g, shadow);
    }
    {
        juce::ColourGradient gr (juce::Colour (0xff6b6863), -4, 0, juce::Colour (0xff55524e), 4, 0, false);
        gr.addColour (0.4, juce::Colour (0xfff6f3ee)); gr.addColour (0.7, juce::Colour (0xffa9a59f));
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (tr);
        g.setGradientFill (gr); g.fillPath (lv);
    }
    const float tx = cx + len * std::sin (a), ty = cy - len * std::cos (a);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, tx - 1.6f, ty - 1.6f, juce::Colour (0xff4a4844), tx + 5, ty, true));
    g.fillEllipse (tx - 4.8f, ty - 4.2f, 9.6f, 8.4f);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawEllipse (tx - 4.8f, ty - 4.2f, 9.6f, 8.4f, 0.6f);
}

// =================================================================== lamp
Jewel::Jewel() : juce::Button ({}) { setClickingTogglesState (true); }

void Jewel::paintButton (juce::Graphics& g, bool, bool)
{
    const auto& art = Art::get();
    juce::Path c; c.addEllipse (4, 4, 64, 64);
    g.reduceClipRegion (c);
    g.drawImage (getToggleState() ? art.hot : art.cold, 0, 0, 72, 72, 1080 - 36, 490 - 36, 72, 72);
}

// =================================================================== chassis
Chassis::Chassis()
{
    setBounds (0, 0, W, FACE_Y);
    setInterceptsMouseClicks (false, false);
}

void Chassis::update (float gr, bool power, double dt)
{
    t += (float) dt;
    const float d = (float) dt;
    needle += (1 - std::exp (-d / (gr > needle ? 0.08f : 0.3f))) * (gr - needle);
    // thermal model: heats in ~1.2 s, cools in ~3 s; idle filament stays warm; POWER off = cold
    const float hT = power ? 0.2f + 0.8f * juce::jmin (1.0f, needle / 14) : 0.0f;
    const float gT = power ? 0.35f + 0.65f * juce::jmin (1.0f, needle / 12) : 0.0f;
    heat += (1 - std::exp (-d / (hT > heat ? 1.2f : 3.0f))) * (hT - heat);
    glow += (1 - std::exp (-d / (gT > glow ? 0.6f : 2.2f))) * (gT - glow);
    lamp += (1 - std::exp (-d / (power ? 0.4f : 1.5f))) * ((power ? 1.0f : 0.0f) - lamp);
    repaint();
}

void Chassis::paint (juce::Graphics& g)
{
    const auto& art = Art::get();
    g.reduceClipRegion (0, 54, W, FACE_Y - 54);   // the top bar is drawn by the editor
    const float fl = 0.985f + 0.015f * std::sin (t * 41) + 0.01f * (rng.nextFloat() - 0.5f), gl = glow * fl;
    g.drawImage (art.cold, 0, 0, W, FACE_Y, 0, 0, W, FACE_Y);
    g.setOpacity (juce::jmin (1.0f, gl / 0.8f));
    g.drawImage (art.hot, 0, 0, W, FACE_Y, 0, 0, W, FACE_Y);
    g.setOpacity (1.0f);
    if (gl > 0.8f)
    {
        // hotter than the photo: extra red bloom around the tubes and on the deck
        const float e = (gl - 0.8f) / 0.2f;
        for (float x : TUBES)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff2810).withAlpha (0.22f * e), x, 262, juce::Colour (0x00ff1e0a), x + 130, 262, true));
            g.fillRect (x - 140, 110.0f, 280.0f, 295.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff2810).withAlpha (0.25f * e), x, 392, juce::Colour (0x00ff1e0a), x + 110, 392, true));
            g.fillRect (x - 120, 375.0f, 240.0f, 30.0f);
        }
    }
    // the GR meter
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (meterFace());
    const float th = juce::degreesToRadians (meterAngle (needle));
    const float nx = MPX + MRX * 0.97f * std::sin (th), ny = MPY - MRY * 0.97f * std::cos (th);
    juce::Path n; n.startNewSubPath (MPX, MPY); n.lineTo (nx, ny);
    const float ux = nx - MPX, uy = ny - MPY, ul = std::hypot (ux, uy), bx = nx - ux / ul * 15, by = ny - uy / ul * 15;
    juce::Path head; head.addTriangle (nx, ny, bx - uy / ul * 5, by + ux / ul * 5, bx + uy / ul * 5, by - ux / ul * 5);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.strokePath (n, juce::PathStrokeType (2.4f), juce::AffineTransform::translation (3, 3)); g.fillPath (head, juce::AffineTransform::translation (3, 3));
    g.setColour (juce::Colour (0xff15110f));
    g.strokePath (n, juce::PathStrokeType (2.4f)); g.fillPath (head);
    {
        // pivot cap (brass half dome), a little wider than the painted one so it covers the old needle's root
        juce::Path cap; cap.addPieSegment (MPX - 25, MPY - 24, 50, 48, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, 0);
        juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 4, { 0, -1 }).drawForPath (g, cap);
        juce::ColourGradient gr (juce::Colour (0xffe2c9a0), MPX - 6, MPY - 22, juce::Colour (0xff7d6040), MPX + 14, MPY + 2, false);
        gr.addColour (0.5, juce::Colour (0xffb4936a));
        g.setGradientFill (gr); g.fillPath (cap);
        g.setColour (juce::Colour (0xff5a4530).withAlpha (0.8f)); g.strokePath (cap, juce::PathStrokeType (0.8f));
    }
    if (lamp > 0)
    {
        // warm, slightly brighter incandescent lamp behind the face
        g.setColour (juce::Colour (0xffffb060).withAlpha (0.07f * lamp)); g.fillRect (540, 85, 300, 210);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffaa50).withAlpha (0.16f * lamp), 690, 275, juce::Colour (0x00ff7828), 690, 75, true));
        g.fillRect (540, 85, 300, 210);
    }
    if (lamp < 1) { g.setColour (juce::Colour (0xff0a0806).withAlpha (0.78f * (1 - lamp))); g.fillRect (540, 85, 300, 210); }
}

} // namespace corona::ui
