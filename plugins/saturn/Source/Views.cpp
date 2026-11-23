#include "Views.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace saturn
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    juce::Path flutedCircle (juce::Point<float> c, float r, int lobes, float depth, float phase = 0.0f)
    {
        juce::Path p;
        const int seg = 180;
        for (int i = 0; i <= seg; ++i)
        {
            auto a = (float) i / (float) seg * twoPi;
            auto rr = r * (1.0f - depth * (0.5f + 0.5f * std::cos ((float) lobes * (a - phase))));
            auto pt = c.getPointOnCircumference (rr, a);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        return p;
    }
}

// ================================================================== look

Vintage Vintage::cream()
{
    Vintage v;
    v.panel      = juce::Colour (0xffebe3cf);
    v.panelShade = juce::Colour (0xffcfc3a6);
    v.grime      = juce::Colour (0xff7a6446);
    v.text       = juce::Colour (0xff2a2520);
    v.textSoft   = juce::Colour (0xff5f564b);
    v.wood       = juce::Colour (0xff6b3f25);
    v.woodDark   = juce::Colour (0xff3a2112);
    v.key        = juce::Colour (0xfff4eedf);
    v.keyShade   = juce::Colour (0xffcdc2a8);
    v.keyText    = juce::Colour (0xff2e2924);
    v.accent     = juce::Colour (0xff8a3dff);
    v.accentGlow = juce::Colour (0xffb98aff);
    v.bezel      = juce::Colour (0xffb9b9b9);
    v.screwHead  = juce::Colour (0xffb5b2ab);
    return v;
}

Vintage Vintage::nightPanel()
{
    auto v = cream();
    v.night      = true;
    v.panel      = juce::Colour (0xff2b2825);
    v.panelShade = juce::Colour (0xff1c1a18);
    v.grime      = juce::Colour (0xff000000);
    v.text       = juce::Colour (0xffece4d2);
    v.textSoft   = juce::Colour (0xffb3a995);
    v.wood       = juce::Colour (0xff4a2816);
    v.woodDark   = juce::Colour (0xff1f0f07);
    v.key        = juce::Colour (0xff3b3733);
    v.keyShade   = juce::Colour (0xff23201d);
    v.keyText    = juce::Colour (0xffe9e0cc);
    v.bezel      = juce::Colour (0xff8d8d8d);
    v.screwHead  = juce::Colour (0xff8a8781);
    return v;
}

juce::Font Vintage::serif (float h)
{
    static const juce::String face = []
    {
        auto names = juce::Font::findAllTypefaceNames();
        for (auto* n : { "Didot", "Bodoni 72", "Baskerville", "Georgia", "Times New Roman" })
            if (names.contains (n)) return juce::String (n);
        return juce::String();
    }();
    if (face.isEmpty()) return Fonts::regular (h);
    return juce::Font (juce::FontOptions (face, h, juce::Font::plain));
}

juce::Image Vintage::renderPanel (int w, int h, const Vintage& v, int seed)
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), true);
    juce::Graphics g (img);
    juce::Random r (seed);
    auto b = img.getBounds().toFloat();

    juce::ColourGradient base (v.panel.brighter (0.04f), 0, 0, v.panel.darker (0.06f), 0, b.getHeight(), false);
    g.setGradientFill (base);
    g.fillRect (b);

    // fine speckle
    for (int i = 0; i < w * h / 60; ++i)
    {
        auto light = r.nextBool();
        g.setColour ((light ? juce::Colours::white : v.grime).withAlpha (0.03f + r.nextFloat() * 0.06f));
        g.fillRect (r.nextFloat() * b.getWidth(), r.nextFloat() * b.getHeight(), 1.0f, 1.0f);
    }
    // worn blotches, mostly near the edges and corners
    for (int i = 0; i < 70; ++i)
    {
        auto edge = r.nextInt (4);
        auto t = r.nextFloat();
        auto d = std::pow (r.nextFloat(), 2.5f) * 40.0f;
        juce::Point<float> c = edge == 0 ? juce::Point<float> (t * b.getWidth(), d)
                             : edge == 1 ? juce::Point<float> (t * b.getWidth(), b.getHeight() - d)
                             : edge == 2 ? juce::Point<float> (d, t * b.getHeight())
                                         : juce::Point<float> (b.getWidth() - d, t * b.getHeight());
        auto rad = 2.0f + r.nextFloat() * 14.0f;
        g.setColour (v.grime.withAlpha (v.night ? 0.25f : 0.05f + r.nextFloat() * 0.12f));
        g.fillEllipse (juce::Rectangle<float> (rad * (0.6f + r.nextFloat()), rad).withCentre (c));
        for (int k = 0; k < 6; ++k)
        {
            auto dot = c + juce::Point<float> ((r.nextFloat() - 0.5f) * rad * 3.0f, (r.nextFloat() - 0.5f) * rad * 3.0f);
            g.fillEllipse (dot.x, dot.y, 1.0f + r.nextFloat() * 2.0f, 1.0f + r.nextFloat() * 2.0f);
        }
    }
    // a few hairline scratches
    for (int i = 0; i < 14; ++i)
    {
        auto p0 = juce::Point<float> (r.nextFloat() * b.getWidth(), r.nextFloat() * b.getHeight());
        auto p1 = p0 + juce::Point<float> ((r.nextFloat() - 0.5f) * 60.0f, (r.nextFloat() - 0.5f) * 14.0f);
        g.setColour ((v.night ? juce::Colours::white : v.grime).withAlpha (0.06f));
        g.drawLine ({ p0, p1 }, 0.6f);
    }
    // edge darkening + bevel
    juce::ColourGradient vig (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                              juce::Colours::black.withAlpha (v.night ? 0.35f : 0.12f), 0, 0, true);
    g.setGradientFill (vig);
    g.fillRect (b);
    g.setColour (juce::Colours::white.withAlpha (v.night ? 0.08f : 0.55f));
    g.drawLine (1, 1.5f, b.getWidth() - 1, 1.5f, 1.5f);
    g.drawLine (1.5f, 1, 1.5f, b.getHeight() - 1, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawLine (1, b.getHeight() - 1.0f, b.getWidth(), b.getHeight() - 1.0f, 2.0f);
    g.drawLine (b.getWidth() - 1.0f, 1, b.getWidth() - 1.0f, b.getHeight(), 1.5f);
    return img;
}

juce::Image Vintage::renderWood (int w, int h, const Vintage& v, int seed)
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), true);
    juce::Graphics g (img);
    juce::Random r (seed);
    juce::ColourGradient base (v.wood.brighter (0.1f), 0, 0, v.woodDark, (float) w, 0, false);
    base.addColour (0.45, v.wood);
    g.setGradientFill (base);
    g.fillAll();
    for (int i = 0; i < w * 3; ++i)
    {
        auto x0 = r.nextFloat() * (float) w;
        auto amp = 0.5f + r.nextFloat() * 2.5f, freq = 0.004f + r.nextFloat() * 0.02f, ph = r.nextFloat() * twoPi;
        juce::Path grain;
        for (int y = 0; y <= h; y += 6)
        {
            auto x = x0 + amp * std::sin ((float) y * freq + ph);
            if (y == 0) grain.startNewSubPath (x, (float) y); else grain.lineTo (x, (float) y);
        }
        auto dark = r.nextFloat() < 0.65f;
        g.setColour ((dark ? v.woodDark.darker (0.4f) : v.wood.brighter (0.5f)).withAlpha (0.05f + r.nextFloat() * 0.12f));
        g.strokePath (grain, juce::PathStrokeType (0.5f + r.nextFloat() * 1.2f));
    }
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRect (img.getBounds(), 1);
    return img;
}

void Vintage::drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle, const Vintage& v)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (r * 2.3f, r * 2.3f).withCentre (c.translated (0.6f, 1.0f)));
    juce::ColourGradient head (v.screwHead.brighter (0.6f), c.x - r * 0.6f, c.y - r * 0.7f, v.screwHead.darker (0.7f), c.x + r, c.y + r, true);
    g.setGradientFill (head);
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 0.7f);
    auto d = juce::Point<float> (std::cos (angle), std::sin (angle)) * (r * 0.75f);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawLine ({ c - d, c + d }, r * 0.28f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawLine ({ c - d + juce::Point<float> (0.0f, r * 0.2f), c + d + juce::Point<float> (0.0f, r * 0.2f) }, r * 0.1f);
}

void Vintage::drawKeycap (juce::Graphics& g, juce::Rectangle<float> b, const Vintage& v, bool selected, bool hover, bool down)
{
    auto pressed = selected || down;
    auto body = b.reduced (2.0f).withTrimmedBottom (4.0f).translated (0.0f, pressed ? 2.5f : 0.0f);
    if (selected) Glass::drawRectGlow (g, body.translated (0.0f, 4.0f), 7.0f, v.accent, 10.0f, 0.55f);
    g.setColour (juce::Colours::black.withAlpha (pressed ? 0.25f : 0.4f));
    g.fillRoundedRectangle (body.translated (0.0f, pressed ? 1.5f : 4.0f).expanded (0.5f), 7.0f);
    g.setColour (v.keyShade.darker (0.25f));
    g.fillRoundedRectangle (body.translated (0.0f, pressed ? 0.5f : 2.0f), 7.0f);
    juce::ColourGradient face (v.key.brighter (hover ? 0.06f : 0.02f), 0, body.getY(), v.key.interpolatedWith (v.keyShade, 0.55f), 0, body.getBottom(), false);
    g.setGradientFill (face);
    g.fillRoundedRectangle (body, 7.0f);
    juce::ColourGradient top (v.key.brighter (0.1f), 0, body.getY(), v.key, 0, body.getBottom(), false);
    g.setGradientFill (top);
    g.fillRoundedRectangle (body.reduced (4.0f, 3.0f).withTrimmedBottom (2.0f), 5.0f);
    g.setColour (juce::Colours::black.withAlpha (v.night ? 0.5f : 0.22f));
    g.drawRoundedRectangle (body, 7.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (v.night ? 0.08f : 0.6f));
    g.drawLine (body.getX() + 7.0f, body.getY() + 1.2f, body.getRight() - 7.0f, body.getY() + 1.2f, 1.0f);
}

// ================================================================== keys

void KeyButton::paintButton (juce::Graphics& g, bool hover, bool down)
{
    if (look == nullptr) return;
    auto& v = *look;
    auto b = getLocalBounds().toFloat();
    auto selected = getToggleState();
    Vintage::drawKeycap (g, b, v, selected, hover, down);
    if (square) return;
    auto text = b.reduced (6.0f).withTrimmedBottom (4.0f).translated (0.0f, selected || down ? 2.5f : 0.0f);
    if (selected && ! v.night)
    {
        g.setColour (v.accentGlow.withAlpha (0.35f));
        g.setFont (Fonts::regular (18.0f));
        for (auto d : { -1.0f, 1.0f }) g.drawText (getButtonText(), text.translated (d, 0.0f), juce::Justification::centred);
    }
    g.setColour (selected ? (v.night ? v.accentGlow.brighter (0.3f) : v.accent.darker (0.25f)) : v.keyText);
    g.setFont (Fonts::regular (18.0f));
    g.drawText (getButtonText(), text, juce::Justification::centred);
}

// ================================================================== knobs

VintageKnob::VintageKnob (const juce::String& l, Style s) : label (l), style (s)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    setMouseDragSensitivity (s == Style::age ? 240 : 190);
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<float> VintageKnob::knobArea() const
{
    auto b = getLocalBounds().toFloat();
    if (style == Style::age)
        return juce::Rectangle<float> (150.0f, 150.0f).withCentre ({ b.getCentreX(), b.getY() + 150.0f });
    if (style == Style::mini)
        return juce::Rectangle<float> (46.0f, 46.0f).withCentre ({ b.getX() + 34.0f, b.getCentreY() });
    return juce::Rectangle<float> (76.0f, 76.0f).withCentre ({ b.getCentreX(), b.getY() + 54.0f });
}

bool VintageKnob::hitTest (int x, int y)
{
    auto k = knobArea();
    return k.getCentre().getDistanceFrom ({ (float) x, (float) y }) < k.getWidth() * 0.62f;
}

void VintageKnob::paint (juce::Graphics& g)
{
    if (look == nullptr) return;
    auto& v = *look;
    auto hot = isMouseOverOrDragging();
    auto prop = (float) valueToProportionOfLength (getValue());
    auto k = knobArea();
    if (style == Style::age) drawAge (g, k, prop, hot); else drawSmall (g, k, prop, hot);

    auto b = getLocalBounds().toFloat();
    if (style == Style::age)
    {
        // value window above the knob
        auto box = juce::Rectangle<float> (64.0f, 34.0f).withCentre ({ b.getCentreX(), b.getY() + 28.0f });
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (box.translated (0.0f, 1.5f), 3.0f);
        juce::ColourGradient lcd (juce::Colour (0xff2a2a2e), 0, box.getY(), juce::Colour (0xff141416), 0, box.getBottom(), false);
        g.setGradientFill (lcd);
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (juce::Colour (0xfff1ebff));
        g.setFont (Fonts::regular (19.0f));
        g.drawText (getTextFromValue (getValue()), box, juce::Justification::centred);
        g.setColour (v.text);
        Fonts::drawSpaced (g, label, { b.getX(), b.getY() + 255.0f, b.getWidth(), 30.0f }, Fonts::regular (24.0f), 0.04f, juce::Justification::centred);
        return;
    }

    if (style == Style::mini)
    {
        auto t = b.withTrimmedLeft (74.0f);
        g.setColour (v.text);
        Fonts::drawSpaced (g, label, t.withTrimmedBottom (t.getHeight() * 0.5f), Fonts::regular (15.0f), 0.04f, juce::Justification::bottomLeft);
        g.setColour (hot ? v.accent.darker (0.2f) : v.textSoft);
        g.setFont (Fonts::regular (14.0f));
        g.drawText (getTextFromValue (getValue()), t.withTrimmedTop (t.getHeight() * 0.5f + 2.0f), juce::Justification::topLeft);
        return;
    }

    auto labelArea = juce::Rectangle<float> (b.getX(), k.getBottom() + 16.0f, b.getWidth(), 24.0f);
    if (hot)
    {
        g.setColour (v.accent.darker (0.2f));
        g.setFont (Fonts::regular (17.0f));
        g.drawText (getTextFromValue (getValue()), labelArea, juce::Justification::centred);
    }
    else
    {
        g.setColour (v.text);
        g.setFont (Fonts::regular (17.5f));
        g.drawText (label, labelArea, juce::Justification::centred);
    }
}

void VintageKnob::drawSmall (juce::Graphics& g, juce::Rectangle<float> k, float prop, bool hot)
{
    auto& v = *look;
    auto c = k.getCentre();
    auto R = k.getWidth() * 0.5f;
    auto start = pi * 1.25f, end = pi * 2.75f;
    auto angle = start + (end - start) * prop;

    // scale dots
    for (int i = 0; i <= 10; ++i)
    {
        auto a = start + (end - start) * (float) i / 10.0f;
        g.setColour (v.text.withAlpha (0.75f));
        g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (c.getPointOnCircumference (R * 1.36f, a)));
    }
    // value arc (always on MIX, on hover for the others)
    if (alwaysShowArc || hot)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, R * 1.2f, R * 1.2f, 0.0f, start, angle, true);
        g.setColour (v.accentGlow.withAlpha (0.35f));
        g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (v.accent);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (R * 2.1f, R * 2.1f).withCentre (c.translated (2.5f, 5.0f)));

    // fluted bakelite skirt
    auto skirt = flutedCircle (c, R, 18, 0.035f, angle);
    juce::ColourGradient brown (juce::Colour (0xff5e4233), c.x - R * 0.6f, c.y - R * 0.7f, juce::Colour (0xff120a06), c.x + R * 0.7f, c.y + R, true);
    brown.addColour (0.45, juce::Colour (0xff35231a));
    g.setGradientFill (brown);
    g.fillPath (skirt);
    for (int i = 0; i < 18; ++i)
    {
        auto a = angle + (float) i / 18.0f * twoPi + pi / 18.0f;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawLine ({ c.getPointOnCircumference (R * 0.66f, a), c.getPointOnCircumference (R * 0.93f, a) }, 1.4f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawLine ({ c.getPointOnCircumference (R * 0.66f, a + 0.12f), c.getPointOnCircumference (R * 0.93f, a + 0.12f) }, 1.0f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (skirt, juce::PathStrokeType (1.0f));

    // brushed chrome cap
    auto capR = R * 0.6f;
    auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);
    juce::ColourGradient chrome (juce::Colour (0xffffffff), c.x - capR * 0.45f, c.y - capR * 0.6f, juce::Colour (0xff5d5d5d), c.x + capR * 0.7f, c.y + capR * 0.9f, true);
    chrome.addColour (0.35, juce::Colour (0xffd8d8d8));
    chrome.addColour (0.75, juce::Colour (0xff8f8f8f));
    g.setGradientFill (chrome);
    g.fillEllipse (cap);
    // brushed-metal sheen: two soft light wedges
    for (auto wedge : { -0.8f, 2.35f })
    {
        juce::Path sheen;
        sheen.addPieSegment (cap, wedge - 0.35f, wedge + 0.35f, 0.0f);
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.fillPath (sheen);
    }
    for (int i = 1; i < 9; ++i)
    {
        g.setColour ((i % 2 ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f));
        g.drawEllipse (cap.reduced (capR * (float) i / 10.0f), 0.8f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (cap, 1.0f);

    // purple pointer on the skirt
    auto p0 = c.getPointOnCircumference (R * 0.66f, angle), p1 = c.getPointOnCircumference (R * 0.97f, angle);
    Glass::drawGlow (g, p1, 10.0f, v.accentGlow, 0.7f);
    g.setColour (v.accentGlow);
    g.drawLine ({ p0, p1 }, 3.2f);
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.drawLine ({ p0, p1 }, 1.0f);
}

void VintageKnob::drawAge (juce::Graphics& g, juce::Rectangle<float> k, float prop, bool hot)
{
    auto& v = *look;
    auto c = k.getCentre();
    auto R = k.getWidth() * 0.5f;
    auto start = pi * 1.25f, end = pi * 2.75f;
    auto angle = start + (end - start) * prop;

    // glowing ring: track + value
    juce::Path track, arc;
    track.addCentredArc (c.x, c.y, R * 1.2f, R * 1.2f, 0.0f, start, end, true);
    arc.addCentredArc (c.x, c.y, R * 1.2f, R * 1.2f, 0.0f, start, angle, true);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.strokePath (track, juce::PathStrokeType (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (v.accentGlow.withAlpha (hot ? 0.45f : 0.3f));
    g.strokePath (arc, juce::PathStrokeType (20.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (v.accent.brighter (0.3f));
    g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (juce::Rectangle<float> (R * 2.08f, R * 2.08f).withCentre (c.translated (4.0f, 8.0f)));

    // scalloped, marbled bakelite
    auto body = flutedCircle (c, R, 10, 0.045f, angle);
    juce::ColourGradient brown (juce::Colour (0xff6e4a33), c.x - R * 0.6f, c.y - R * 0.7f, juce::Colour (0xff160d07), c.x + R * 0.8f, c.y + R, true);
    brown.addColour (0.5, juce::Colour (0xff3c271a));
    g.setGradientFill (brown);
    g.fillPath (body);
    g.saveState();
    g.reduceClipRegion (body);
    juce::Random r (99);
    for (int i = 0; i < 26; ++i)
    {
        juce::Path vein;
        auto a0 = r.nextFloat() * twoPi + angle, rr = R * (0.2f + r.nextFloat() * 0.8f);
        vein.startNewSubPath (c.getPointOnCircumference (rr, a0));
        vein.quadraticTo (c.getPointOnCircumference (rr * (0.6f + r.nextFloat() * 0.6f), a0 + 0.5f),
                          c.getPointOnCircumference (rr * (0.5f + r.nextFloat() * 0.5f), a0 + 0.9f + r.nextFloat()));
        g.setColour (juce::Colour (0xff8a6448).withAlpha (0.04f + r.nextFloat() * 0.06f));
        g.strokePath (vein, juce::PathStrokeType (3.0f + r.nextFloat() * 7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // domed top
    juce::ColourGradient dome (juce::Colours::white.withAlpha (0.16f), c.x - R * 0.4f, c.y - R * 0.6f, juce::Colours::transparentBlack, c.x + R * 0.2f, c.y + R * 0.3f, true);
    g.setGradientFill (dome);
    g.fillEllipse (juce::Rectangle<float> (R * 1.6f, R * 1.6f).withCentre (c));
    g.restoreState();
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.strokePath (body, juce::PathStrokeType (1.4f));
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawEllipse (juce::Rectangle<float> (R * 1.55f, R * 1.55f).withCentre (c), 1.5f);

    // pointer
    auto p0 = c.getPointOnCircumference (R * 0.58f, angle), p1 = c.getPointOnCircumference (R * 0.86f, angle);
    Glass::drawGlow (g, p1, 14.0f, v.accentGlow, 0.8f);
    g.setColour (v.accentGlow);
    g.drawLine ({ p0, p1 }, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawLine ({ p0, p1 }, 1.6f);
}

// ================================================================== VU

namespace
{
    // printed VU scale: dB value -> position 0..1 along the arc
    const float vuDb[]  = { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f };
    const float vuPos[] = { 0.0f, 0.24f, 0.36f, 0.46f, 0.57f, 0.63f, 0.70f, 0.77f, 0.84f, 0.92f, 1.0f };

    float vuPosition (float db)
    {
        if (db <= vuDb[0]) return juce::jmap (juce::jmax (db, -40.0f), -40.0f, -20.0f, -0.08f, 0.0f);
        for (int i = 1; i < 11; ++i)
            if (db <= vuDb[i]) return juce::jmap (db, vuDb[i - 1], vuDb[i], vuPos[i - 1], vuPos[i]);
        return juce::jmin (1.06f, 1.0f + (db - 3.0f) * 0.02f);
    }
}

void VUMeter::setLevel (float peak, float dt)
{
    // VU integrates the level (~300 ms); 0 VU = -18 dBFS
    vu += (peak * 0.707f - vu) * juce::jmin (1.0f, dt / 0.3f);
    auto target = vuPosition (juce::Decibels::gainToDecibels (vu, -60.0f) + 18.0f);
    auto acc = (target - needle) * 160.0f - needleVel * 22.0f;     // slightly under-damped needle
    needleVel += acc * dt;
    needle += needleVel * dt;
    needle = juce::jlimit (-0.1f, 1.08f, needle);
    repaint();
}

void VUMeter::paint (juce::Graphics& g)
{
    if (look == nullptr) return;
    auto& v = *look;
    auto b = getLocalBounds().toFloat();
    auto s = juce::jmin (b.getWidth(), b.getHeight());
    auto c = b.getCentre();

    // bezel
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (juce::Rectangle<float> (s * 0.96f, s * 0.96f).withCentre (c.translated (2.0f, 5.0f)));
    juce::ColourGradient bez (juce::Colour (0xff474747), c.x, c.y - s * 0.5f, juce::Colour (0xff080808), c.x, c.y + s * 0.5f, false);
    g.setGradientFill (bez);
    g.fillEllipse (juce::Rectangle<float> (s * 0.94f, s * 0.94f).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawEllipse (juce::Rectangle<float> (s * 0.92f, s * 0.92f).withCentre (c), 1.2f);
    g.setColour (juce::Colours::black);
    g.fillEllipse (juce::Rectangle<float> (s * 0.82f, s * 0.82f).withCentre (c));

    // glowing face (upper part of the window)
    auto faceR = s * 0.40f;
    juce::Path face;
    face.addEllipse (juce::Rectangle<float> (faceR * 2.0f, faceR * 2.0f).withCentre (c));
    g.saveState();
    g.reduceClipRegion (juce::Rectangle<float> (c.x - faceR, c.y - faceR, faceR * 2.0f, faceR * 1.62f).toNearestInt());
    juce::ColourGradient glow (juce::Colour (0xfffdfaff), c.x, c.y - faceR * 0.15f, juce::Colour (0xffb58ef0), c.x + faceR * 0.95f, c.y + faceR * 0.6f, true);
    glow.addColour (0.55, juce::Colour (0xffe6d8fb));
    g.setGradientFill (glow);
    g.fillPath (face);
    g.setColour (v.accentGlow.withAlpha (0.45f));
    g.strokePath (face, juce::PathStrokeType (5.0f));
    g.restoreState();

    // scale
    auto pivot = juce::Point<float> (c.x, c.y + s * 0.24f);
    auto scaleR = s * 0.44f;
    auto a0 = -0.9f, a1 = 0.9f;                // radians from straight up
    auto posAngle = [&] (float pos) { return a0 + (a1 - a0) * pos; };
    juce::Path scale, red;
    scale.addCentredArc (pivot.x, pivot.y, scaleR, scaleR, 0.0f, a0, posAngle (0.77f), true);
    red.addCentredArc (pivot.x, pivot.y, scaleR, scaleR, 0.0f, posAngle (0.77f), a1, true);
    g.setColour (juce::Colour (0xff2a2430));
    g.strokePath (scale, juce::PathStrokeType (1.2f));
    g.setColour (juce::Colour (0xffc2302f));
    g.strokePath (red, juce::PathStrokeType (3.0f));
    const char* nums[] = { "20", "10", "7", "5", "3", "2", "1", "0", "1", "2", "3" };
    g.setFont (Fonts::regular (s * 0.052f));
    for (int i = 0; i < 11; ++i)
    {
        auto a = posAngle (vuPos[i]);
        g.setColour (i >= 7 ? juce::Colour (0xffc2302f) : juce::Colour (0xff2a2430));
        g.drawLine ({ pivot.getPointOnCircumference (scaleR, a), pivot.getPointOnCircumference (scaleR + s * 0.035f, a) }, 1.2f);
        auto tp = pivot.getPointOnCircumference (scaleR + s * 0.075f, a);
        g.drawText (nums[i], juce::Rectangle<float> (s * 0.1f, s * 0.06f).withCentre (tp), juce::Justification::centred);
    }
    for (int i = 0; i < 20; ++i)
    {
        auto a = posAngle ((float) i / 19.0f);
        g.setColour (juce::Colour (0xff2a2430).withAlpha (0.6f));
        g.drawLine ({ pivot.getPointOnCircumference (scaleR - s * 0.018f, a), pivot.getPointOnCircumference (scaleR, a) }, 0.8f);
    }
    g.setColour (juce::Colour (0xff2a2430));
    g.setFont (Fonts::regular (s * 0.085f));
    g.drawText ("VU", juce::Rectangle<float> (s * 0.3f, s * 0.1f).withCentre ({ c.x, c.y + s * 0.02f }), juce::Justification::centred);

    // needle
    auto na = posAngle (needle);
    auto tip = pivot.getPointOnCircumference (scaleR + s * 0.02f, na);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawLine ({ pivot.translated (2.0f, 2.0f), tip.translated (2.0f, 2.0f) }, 1.6f);
    g.setColour (juce::Colour (0xff141414));
    g.drawLine ({ pivot, tip }, 1.6f);

    // pivot dome and bottom screw
    auto dome = juce::Rectangle<float> (s * 0.26f, s * 0.2f).withCentre (pivot);
    Glass::drawGlow (g, pivot, s * 0.16f, v.accentGlow, 0.45f);
    juce::ColourGradient dg (juce::Colour (0xff6d6870), dome.getCentreX(), dome.getY(), juce::Colour (0xff101010), dome.getCentreX(), dome.getBottom(), false);
    g.setGradientFill (dg);
    g.fillEllipse (dome);
    g.setColour (v.accentGlow.withAlpha (0.4f));
    g.drawEllipse (dome, 1.0f);
    Vintage::drawScrew (g, { c.x, c.y + s * 0.385f }, s * 0.03f, 0.9f, Vintage::nightPanel());
}

// ================================================================== tape screen

void TapeScreen::tick (const TapeState& s)
{
    st = s;
    auto dt = 1.0f / 30.0f;
    time += dt;
    glow = juce::jmax (glow * 0.88f, s.level);
    spin += dt * (0.5f + 0.6f * glow) * juce::jlimit (0.6f, 1.4f, s.speed);
    tilt += ((std::sin ((float) time * 0.21f) * 0.1f * s.drift) - tilt) * 0.05f;
    if (s.tape == 5 && random.nextFloat() < 0.04f + 0.12f * s.dropout) jump = 1.0f;
    jump *= 0.8f;

    auto w = (float) getWidth(), h = (float) getHeight();
    auto head = juce::Point<float> (w * 0.5f, h * 0.79f);
    auto emit = (int) ((0.3f + 6.0f * glow) * (0.4f + 1.2f * s.saturation));
    for (int i = 0; i < emit; ++i)
    {
        auto a = -pi * 0.5f + (random.nextFloat() - 0.5f) * 2.6f;
        auto spd = 30.0f + random.nextFloat() * 90.0f;
        sparks.push_back ({ head + juce::Point<float> ((random.nextFloat() - 0.5f) * 40.0f, 0.0f),
                            { std::cos (a) * spd, std::sin (a) * spd }, 1.0f, 0.8f + random.nextFloat() * 1.6f });
    }
    for (auto& sp : sparks) { sp.pos += sp.vel * dt; sp.vel.y += 60.0f * dt; sp.life -= dt * 1.4f; }
    sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [] (const Spark& sp) { return sp.life <= 0; }), sparks.end());
    if (sparks.size() > 500) sparks.erase (sparks.begin(), sparks.begin() + 150);

    auto want = (int) (10.0f + 120.0f * juce::jmin (1.0f, s.hiss * (0.5f + s.age)));
    while ((int) dust.size() < want)
        dust.push_back ({ { random.nextFloat() * w, random.nextFloat() * h }, random.nextFloat(), 0.6f + random.nextFloat() * 1.2f });
    if ((int) dust.size() > want) dust.resize ((size_t) want);
    for (auto& d : dust)
        if ((d.life -= dt * 0.35f) <= 0) { d.pos = { random.nextFloat() * w, random.nextFloat() * h }; d.life = 1.0f; }
    repaint();
}

void TapeScreen::drawSaturn (juce::Graphics& g, juce::Point<float> c, float r, float ringTilt, float spinPhase, float alpha)
{
    auto sepia = juce::Colour (0xffd9b27a);
    auto ringA = juce::Colour (0xfff1e4d2).interpolatedWith (sepia, st.age * 0.4f);
    auto ringB = juce::Colour (0xffa77de8).interpolatedWith (sepia, st.age * 0.3f);
    auto squash = 0.27f;

    auto ellipsePath = [&] (float radius, float from, float to, float wobble)
    {
        juce::Path p;
        const int seg = 72;
        for (int i = 0; i <= seg; ++i)
        {
            auto a = from + (to - from) * (float) i / (float) seg;
            auto wv = 1.0f + wobble * (0.6f * st.wow * std::sin (a * 2.0f + (float) time * 1.3f)
                                       + 0.25f * st.flutter * std::sin (a * 9.0f + (float) time * 11.0f));
            auto x = radius * wv * std::cos (a), y = radius * wv * squash * std::sin (a);
            juce::Point<float> pt (c.x + x * std::cos (ringTilt) - y * std::sin (ringTilt), c.y + x * std::sin (ringTilt) + y * std::cos (ringTilt));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        return p;
    };

    auto rings = [&] (bool front)
    {
        auto from = front ? 0.0f : pi, to = front ? pi : twoPi;
        struct Band { float r, w; juce::Colour col; float a; };
        const Band bands[] = { { 1.32f, 6.0f, ringB, 0.6f }, { 1.45f, 7.0f, ringA, 0.85f }, { 1.58f, 6.0f, ringB, 0.75f },
                               { 1.70f, 8.0f, ringA, 0.9f }, { 1.84f, 3.0f, ringA, 0.55f }, { 1.95f, 7.0f, ringB, 0.65f } };
        for (auto& bd : bands)
        {
            auto path = ellipsePath (r * bd.r, from, to, 0.02f);
            auto width = juce::jmax (0.8f, bd.w * r / 62.0f);
            if (front)
            {
                g.setColour (juce::Colours::black.withAlpha (0.3f * alpha));
                g.strokePath (path, juce::PathStrokeType (width + 2.0f));
            }
            g.setColour (bd.col.withAlpha (bd.a * alpha * (1.0f - 0.5f * st.dropout)));
            g.strokePath (path, juce::PathStrokeType (width));
        }
        // a few bright glints travelling round: the reel turning
        for (int k = 0; k < 4; ++k)
        {
            auto s0 = std::fmod (spinPhase + (float) k * pi * 0.5f, twoPi);
            if (s0 < 0) s0 += twoPi;
            auto s1 = juce::jmin (to, s0 + 0.3f);
            s0 = juce::jmax (from, s0);
            if (s1 <= s0 + 0.02f) continue;
            g.setColour (juce::Colours::white.withAlpha (0.55f * alpha));
            g.strokePath (ellipsePath (r * (1.45f + 0.25f * (float) (k % 2)), s0, s1, 0.02f),
                          juce::PathStrokeType (juce::jmax (0.8f, 2.0f * r / 62.0f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    };

    Glass::drawGlow (g, c, r * 2.3f, juce::Colour (0xff9b5cff), (0.5f + 0.3f * glow) * alpha);
    rings (false);

    auto disc = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    juce::ColourGradient body (juce::Colour (0xfff5e3cc).interpolatedWith (sepia, st.age * 0.3f), c.x - r * 0.45f, c.y - r * 0.65f,
                               juce::Colour (0xff2c1747), c.x + r * 0.85f, c.y + r * 0.95f, true);
    body.addColour (0.4, juce::Colour (0xffc9a7a0));
    body.addColour (0.7, juce::Colour (0xff7a5a9a));
    g.setGradientFill (body);
    g.fillEllipse (disc);
    g.saveState();
    juce::Path dp;
    dp.addEllipse (disc);
    g.reduceClipRegion (dp);
    auto drift = std::fmod (spinPhase * 0.15f * r, r * 0.34f);      // bands slide slowly as the reel turns
    for (int i = -7; i <= 7; ++i)
    {
        juce::Path band;
        auto y = (float) i * r * 0.17f + drift;
        band.addRectangle (c.x - r * 1.3f, c.y + y, r * 2.6f, r * (0.05f + 0.04f * (float) ((i + 7) % 3)));
        band.applyTransform (juce::AffineTransform::rotation (ringTilt, c.x, c.y));
        g.setColour (((i & 1) != 0 ? juce::Colour (0xff5b3b3a) : juce::Colours::white).withAlpha (0.12f * alpha));
        g.fillPath (band);
    }
    juce::ColourGradient tint (juce::Colours::transparentBlack, c.x - r * 0.4f, c.y - r * 0.5f, juce::Colour (0xff9b6bd6).withAlpha (0.45f), c.x + r * 0.8f, c.y + r * 0.8f, true);
    g.setGradientFill (tint);
    g.fillEllipse (disc);
    juce::ColourGradient shade (juce::Colours::transparentBlack, c.x - r * 0.2f, c.y - r * 0.2f, juce::Colours::black.withAlpha (0.35f), c.x + r, c.y + r, true);
    g.setGradientFill (shade);
    g.fillEllipse (disc);
    g.restoreState();
    g.setColour (juce::Colour (0xffb98aff).withAlpha (0.35f * alpha));
    g.drawEllipse (disc.expanded (1.5f), 5.0f * r / 62.0f + 1.0f);
    g.setColour (juce::Colour (0xffe0ccff).withAlpha (0.9f * alpha));
    g.drawEllipse (disc, juce::jmax (1.0f, 1.8f * r / 62.0f));

    rings (true);
}

void TapeScreen::drawHead (juce::Graphics& g, juce::Rectangle<float> scr)
{
    auto k = scr.getWidth() / 590.0f;
    auto c = juce::Point<float> (scr.getCentreX(), scr.getY() + scr.getHeight() * 0.83f);
    auto chrome = [&] (juce::Rectangle<float> r, float corner)
    {
        juce::ColourGradient cg (juce::Colour (0xfff4f4f4), r.getX(), r.getY(), juce::Colour (0xff7d7d7d), r.getRight(), r.getBottom(), false);
        cg.addColour (0.5, juce::Colour (0xffbdbdbd));
        g.setGradientFill (cg);
        g.fillRoundedRectangle (r, corner);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (r, corner, 1.0f);
    };
    chrome (juce::Rectangle<float> (100.0f * k, 18.0f * k).withCentre (c.translated (0.0f, 36.0f * k)), 3.0f);
    for (auto dx : { -38.0f, 38.0f })
        Vintage::drawScrew (g, c.translated (dx * k, 36.0f * k), 4.0f * k, 0.4f, Vintage::cream());
    for (auto dx : { -50.0f, 50.0f })
    {
        auto rc = c.translated (dx * k, -4.0f * k);
        chrome (juce::Rectangle<float> (22.0f * k, 22.0f * k).withCentre (rc), 11.0f * k);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (juce::Rectangle<float> (7.0f * k, 7.0f * k).withCentre (rc));
    }
    auto headR = juce::Rectangle<float> (56.0f * k, 44.0f * k).withCentre (c.translated (0.0f, 4.0f * k));
    chrome (headR, 4.0f * k);
    for (int i = 0; i < 12; ++i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.05f));
        g.drawHorizontalLine ((int) (headR.getY() + 4.0f + (float) i * headR.getHeight() / 12.0f), headR.getX() + 3.0f, headR.getRight() - 3.0f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (juce::Rectangle<float> (headR.getWidth() * 0.6f, 3.0f).withCentre ({ headR.getCentreX(), headR.getY() + 6.0f }));
}

void TapeScreen::paint (juce::Graphics& g)
{
    if (look == nullptr) return;
    auto b = getLocalBounds().toFloat();
    auto k = b.getWidth() / 636.0f;

    // chrome bezel
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (b.reduced (2.0f).translated (3.0f, 6.0f), 50.0f * k);
    juce::ColourGradient bez (juce::Colour (0xffeeeeee), 0, b.getY(), juce::Colour (0xff6c6c6c), 0, b.getBottom(), false);
    bez.addColour (0.25, juce::Colour (0xff9c9c9c));
    bez.addColour (0.6, juce::Colour (0xffd6d6d6));
    g.setGradientFill (bez);
    g.fillRoundedRectangle (b.reduced (2.0f), 50.0f * k);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (b.reduced (2.0f), 50.0f * k, 1.2f);
    auto inner = b.reduced (14.0f * k);
    g.setColour (juce::Colour (0xff0a0812));
    g.fillRoundedRectangle (inner, 40.0f * k);
    auto scr = inner.reduced (6.0f * k);
    juce::Path screenShape;
    screenShape.addRoundedRectangle (scr, 36.0f * k);

    g.saveState();
    g.reduceClipRegion (screenShape);
    if (! space.isValid() || space.getWidth() != (int) scr.getWidth())
    {
        auto pal = lt::ui::Palette::dark();
        space = lt::ui::NebulaBackground::renderNebula ((int) scr.getWidth(), (int) scr.getHeight(), pal, 31, -0.2f, 1.0f);
        juce::Graphics sg (space);
        juce::Random r (5);
        for (int i = 0; i < 220; ++i)
        {
            auto sz = r.nextFloat() < 0.9f ? 0.8f + r.nextFloat() * 1.3f : 2.0f + r.nextFloat() * 1.4f;
            sg.setColour (juce::Colours::white.withAlpha (0.3f + r.nextFloat() * 0.7f));
            sg.fillEllipse (r.nextFloat() * scr.getWidth(), r.nextFloat() * scr.getHeight(), sz, sz);
        }
    }
    g.drawImageAt (space, (int) scr.getX(), (int) scr.getY());
    if (jump > 0.05f)
        g.addTransform (juce::AffineTransform::translation ((random.nextFloat() - 0.5f) * 6.0f * jump, (random.nextFloat() - 0.5f) * 4.0f * jump));

    auto R = 64.0f * k;
    auto left = juce::Point<float> (b.getX() + 168.0f * k, b.getY() + 190.0f * k);
    auto right = juce::Point<float> (b.getX() + 468.0f * k, b.getY() + 190.0f * k);
    auto headC = juce::Point<float> (scr.getCentreX(), scr.getY() + scr.getHeight() * 0.83f);

    // tape: from each reel to its roller, swaying with WOW/FLUTTER
    for (int side = 0; side < 2; ++side)
    {
        auto reel = side == 0 ? left : right;
        auto from = reel + juce::Point<float> ((side == 0 ? -0.35f : 0.35f) * R, 0.95f * R);
        auto to = headC + juce::Point<float> ((side == 0 ? -61.0f : 61.0f) * k, -4.0f * k);
        auto mid = (from + to) * 0.5f;
        auto sway = (st.wow * 8.0f * std::sin ((float) time * 2.1f + (float) side) + st.flutter * 2.0f * std::sin ((float) time * 17.0f)) * k;
        mid += juce::Point<float> (sway, std::abs (sway) * 0.5f);
        juce::Path tape;
        tape.startNewSubPath (from);
        tape.quadraticTo (mid, to);
        g.setColour (juce::Colour (0xffb98aff).withAlpha (0.35f));
        g.strokePath (tape, juce::PathStrokeType (4.0f));
        g.setColour (juce::Colour (0xffe3d2ff).withAlpha (0.9f));
        g.strokePath (tape, juce::PathStrokeType (1.2f));
    }

    drawSaturn (g, left, R, -0.42f + tilt, spin, 1.0f);
    drawSaturn (g, right, R, 0.42f - tilt, -spin * 1.05f, 1.0f);
    drawSaturn (g, { b.getX() + 318.0f * k, b.getY() + 68.0f * k }, 13.0f * k, -0.3f, spin * 2.0f, 0.9f);
    drawHead (g, scr);

    for (auto& sp : sparks)
    {
        auto col = juce::Colour (0xffffb24d).interpolatedWith (juce::Colour (0xffffe9b0), sp.life * 0.5f);
        g.setColour (col.withAlpha (juce::jmin (1.0f, sp.life * 1.2f)));
        g.fillEllipse (sp.pos.x, sp.pos.y, sp.size * k, sp.size * k);
    }
    for (auto& d : dust)
    {
        g.setColour (juce::Colour (0xfff3e2c0).withAlpha (0.45f * d.life * (0.3f + st.age)));
        g.fillEllipse (d.pos.x, d.pos.y, d.size, d.size);
    }

    // AGE: old picture tube - sepia cast and faint scanlines
    g.setColour (juce::Colour (0xff8a6a3c).withAlpha (0.16f * st.age));
    g.fillRect (scr);
    if (st.age > 0.45f)
        for (float y = scr.getY(); y < scr.getBottom(); y += 3.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.05f * (st.age - 0.45f) * 2.0f));
            g.drawHorizontalLine ((int) y, scr.getX(), scr.getRight());
        }
    g.restoreState();

    g.saveState();
    g.reduceClipRegion (screenShape);
    juce::ColourGradient vig (juce::Colours::transparentBlack, scr.getCentreX(), scr.getCentreY(),
                              juce::Colours::black.withAlpha (0.55f), scr.getX(), scr.getY(), true);
    g.setGradientFill (vig);
    g.fillRect (scr);
    juce::Path refl;
    refl.startNewSubPath (scr.getX(), scr.getY() + scr.getHeight() * 0.45f);
    refl.quadraticTo (scr.getX() + scr.getWidth() * 0.25f, scr.getY() + scr.getHeight() * 0.1f, scr.getX() + scr.getWidth() * 0.62f, scr.getY());
    refl.lineTo (scr.getX(), scr.getY());
    refl.closeSubPath();
    juce::ColourGradient rg (juce::Colours::white.withAlpha (0.13f), scr.getX(), scr.getY(), juce::Colours::white.withAlpha (0.0f),
                             scr.getX() + scr.getWidth() * 0.35f, scr.getY() + scr.getHeight() * 0.3f, false);
    g.setGradientFill (rg);
    g.fillPath (refl);
    g.restoreState();
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (scr, 36.0f * k, 1.2f);
}

} // namespace saturn
