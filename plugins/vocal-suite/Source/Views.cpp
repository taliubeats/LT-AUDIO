#include "Views.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include "VocalSuiteData.h"
#include "Scales.h"

namespace vocal
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float startAngle = pi * 1.25f, endAngle = pi * 2.75f;
}

// ================================================================== knob

SuiteKnob::SuiteKnob (const juce::String& l, float d) : label (l), diameter (d)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (startAngle, endAngle, true);
    setMouseDragSensitivity (220);
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<float> SuiteKnob::knobArea() const
{
    auto b = getLocalBounds().toFloat();
    return juce::Rectangle<float> (diameter, diameter).withCentre ({ b.getCentreX(), b.getY() + diameter * 0.75f });
}

bool SuiteKnob::hitTest (int x, int y)
{
    return knobArea().getCentre().getDistanceFrom ({ (float) x, (float) y }) < diameter * 0.58f;
}

void SuiteKnob::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto k = knobArea();
    // tick ring outside the arc, like hardware scale markings
    {
        auto c = k.getCentre();
        auto r0 = k.getWidth() * 0.5f + 4.0f, r1 = r0 + 5.0f;
        auto pos = (float) valueToProportionOfLength (getValue());
        for (int i = 0; i <= 20; ++i)
        {
            auto t = (float) i / 20.0f;
            auto a = startAngle + t * (endAngle - startAngle) - juce::MathConstants<float>::halfPi;
            auto lit = t <= pos + 0.001f;
            g.setColour (lit ? p.accent.withAlpha (0.75f) : p.text.withAlpha (p.isLight ? 0.22f : 0.16f));
            g.drawLine (c.x + std::cos (a) * r0, c.y + std::sin (a) * r0, c.x + std::cos (a) * r1, c.y + std::sin (a) * r1, i % 5 == 0 ? 1.4f : 0.9f);
        }
    }
    lt::ui::GlassLookAndFeel::drawGlassKnob (g, k, (float) valueToProportionOfLength (getValue()), p, 0.8f, startAngle, endAngle);
    auto show = isMouseOverOrDragging();
    g.setColour (show ? p.value : p.text);
    g.setFont (Fonts::regular (diameter > 85 ? 17.5f : 15.5f));
    g.drawFittedText (show ? getTextFromValue (getValue()) : label, getLocalBounds().withTop ((int) k.getBottom() + 8),
                      juce::Justification::centredTop, 1);
}

// ================================================================== mascot

Mascot::Mascot()
{
    image = juce::ImageCache::getFromMemory (VocalSuiteData::mascot_png, VocalSuiteData::mascot_pngSize);
    lightImage = juce::ImageCache::getFromMemory (VocalSuiteData::mascotlight_png, VocalSuiteData::mascotlight_pngSize);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Mascot::tick()
{
    time += 1.0 / 30.0;
    repaint();
}

void Mascot::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto t = (float) time;
    auto bob = std::sin (t * 1.6f) * 5.0f;
    auto tilt = std::sin (t * 0.9f) * 0.03f;
    auto c = b.getCentre().translated (0, bob);

    if (palette != nullptr)
        Glass::drawGlow (g, c, b.getWidth() * (0.42f + (thinking ? 0.06f * (0.5f + 0.5f * std::sin (t * 6.0f)) : 0.0f)),
                         palette->accent, thinking ? 0.45f : 0.22f);

    auto& img = palette != nullptr && palette->isLight && lightImage.isValid() ? lightImage : image;
    if (img.isValid())
    {
        auto size = juce::jmin (b.getWidth(), b.getHeight());
        auto tr = juce::AffineTransform::scale (size / (float) img.getWidth())
                    .rotated (tilt, size * 0.5f, size * 0.5f)
                    .translated (c.x - size * 0.5f, c.y - size * 0.5f);
        g.drawImageTransformed (img, tr);
    }

    if (thinking)
        for (int i = 0; i < 3; ++i)
        {
            auto a = 0.3f + 0.7f * (0.5f + 0.5f * std::sin (t * 7.0f - (float) i * 0.9f));
            g.setColour (juce::Colours::white.withAlpha (a));
            g.fillEllipse (b.getRight() - 46.0f + (float) i * 12.0f, b.getY() + 22.0f, 7.0f, 7.0f);
        }
}

// ================================================================== tabs

void ModuleTab::paintButton (juce::Graphics& g, bool hover, bool down)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    auto on = getToggleState();
    if (on)
    {
        Glass::drawRectGlow (g, r, r.getHeight() * 0.5f, p.isLight ? juce::Colours::white : p.nebulaC, 10.0f, 0.6f);
        Glass::drawPanel (g, r, r.getHeight() * 0.5f, p, 3.0f, false);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.6f);
    }
    else
        Glass::drawPanel (g, r, r.getHeight() * 0.5f, p, hover ? 1.8f : 1.1f);
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.1f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f); }

    // icon + label, centred together
    auto font = Fonts::regular (17.0f);
    auto textW = juce::GlyphArrangement::getStringWidth (font, getName());
    auto iconW = 20.0f, gap = 10.0f;
    auto x0 = r.getCentreX() - (iconW + gap + textW) * 0.5f;
    auto ic = juce::Rectangle<float> (x0, r.getCentreY() - 10.0f, iconW, 20.0f);
    auto col = on ? (p.isLight ? p.text : juce::Colours::white) : p.text.withAlpha (0.85f);
    g.setColour (col);
    juce::Path path;
    auto cx = ic.getCentreX(), cy = ic.getCentreY();
    switch (icon)
    {
        case 0: // microphone
            path.addRoundedRectangle (cx - 3.5f, cy - 9.0f, 7.0f, 12.0f, 3.5f);
            path.addCentredArc (cx, cy - 1.0f, 6.5f, 6.5f, 0.0f, pi * 0.5f, pi * 1.5f, true);
            path.startNewSubPath (cx, cy + 5.5f); path.lineTo (cx, cy + 9.0f);
            break;
        case 1: // wand
            path.startNewSubPath (cx - 8.0f, cy + 8.0f); path.lineTo (cx + 5.0f, cy - 5.0f);
            for (auto [dx, dy] : { std::pair<float, float> { 8.0f, -8.0f }, { 3.0f, -10.0f }, { 10.0f, -3.0f } })
            { path.startNewSubPath (cx + dx - 1.5f, cy + dy); path.lineTo (cx + dx + 1.5f, cy + dy); path.startNewSubPath (cx + dx, cy + dy - 1.5f); path.lineTo (cx + dx, cy + dy + 1.5f); }
            break;
        case 2: // EQ sliders
            for (int i = 0; i < 3; ++i)
            {
                auto y = cy - 6.0f + (float) i * 6.0f;
                path.startNewSubPath (cx - 9.0f, y); path.lineTo (cx + 9.0f, y);
                auto kx = cx + (i == 1 ? 4.0f : -3.0f + (float) i);
                path.addEllipse (kx - 2.0f, y - 2.0f, 4.0f, 4.0f);
            }
            break;
        case 3: // waveform
            for (int i = 0; i < 7; ++i)
            {
                auto hgt = std::abs (std::sin ((float) i * 1.3f + 0.4f)) * 9.0f + 2.0f;
                auto x = cx - 9.0f + (float) i * 3.0f;
                path.startNewSubPath (x, cy - hgt); path.lineTo (x, cy + hgt);
            }
            break;
        case 4: // dome (space)
            path.addCentredArc (cx, cy + 4.0f, 8.0f, 8.0f, 0.0f, -pi * 0.5f, pi * 0.5f, true);
            path.startNewSubPath (cx - 10.0f, cy + 4.0f); path.lineTo (cx + 10.0f, cy + 4.0f);
            path.startNewSubPath (cx - 7.0f, cy + 8.0f); path.lineTo (cx + 7.0f, cy + 8.0f);
            break;
        default: // FX sliders (horizontal)
            for (int i = 0; i < 3; ++i)
            {
                auto y = cy - 6.0f + (float) i * 6.0f;
                path.startNewSubPath (cx - 9.0f, y); path.lineTo (cx + 9.0f, y);
                path.addEllipse (cx + (float) (i - 1) * 5.0f - 2.0f, y - 2.0f, 4.0f, 4.0f);
            }
            break;
    }
    g.strokePath (path, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setFont (font);
    g.drawText (getName(), juce::Rectangle<float> (ic.getRight() + gap, r.getY(), textW + 4.0f, r.getHeight()), juce::Justification::centredLeft);
}

// ================================================================== tune display

void TuneDisplay::set (bool v, float m, int t)
{
    voiced = v;
    if (v) midi = m;
    target = t;
    auto cents = voiced && target >= 0 ? juce::jlimit (-50.0f, 50.0f, (midi - (float) target) * 100.0f) : 0.0f;
    shownCents += (cents - shownCents) * 0.35f;
    repaint();
}

void TuneDisplay::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();

    juce::String note = "--";
    if (target >= 0)
        note = tune::Keys::noteName (target % 12) + juce::String (target / 12 - 1);
    else if (voiced)
        note = tune::Keys::noteName (juce::roundToInt (midi) % 12) + juce::String (juce::roundToInt (midi) / 12 - 1);

    auto noteArea = b.withHeight (b.getHeight() * 0.62f);
    if (voiced) Glass::drawGlow (g, noteArea.getCentre(), 90.0f, p.accent, 0.25f);
    g.setColour (voiced ? p.value : p.value.withAlpha (0.45f));
    g.setFont (Fonts::light (noteArea.getHeight() * 0.85f));
    g.drawText (note, noteArea, juce::Justification::centred);

    auto meter = b.withTop (noteArea.getBottom() + 8.0f).reduced (b.getWidth() * 0.12f, 0);
    g.setColour (p.textDim);
    g.setFont (Fonts::regular (11.5f));
    g.drawText ("CENTS", meter.withHeight (16.0f), juce::Justification::centred);
    auto line = meter.withTop (meter.getY() + 24.0f).withHeight (16.0f);
    for (int i = 0; i <= 10; ++i)
    {
        auto x = line.getX() + line.getWidth() * (float) i / 10.0f;
        g.setColour (p.text.withAlpha (i == 5 ? 0.8f : 0.35f));
        g.fillRect (x - 0.75f, line.getY() + (i % 5 == 0 ? 0.0f : 4.0f), 1.5f, i % 5 == 0 ? 14.0f : 8.0f);
    }
    auto mx = line.getCentreX() + line.getWidth() * 0.5f * shownCents / 50.0f;
    auto inTune = std::abs (shownCents) < 8.0f;
    Glass::drawGlow (g, { mx, line.getCentreY() }, 14.0f, inTune ? juce::Colours::white : p.accent, voiced ? 0.8f : 0.2f);
    g.setColour (voiced ? juce::Colours::white : p.textDim);
    g.fillRoundedRectangle (mx - 2.0f, line.getY() - 2.0f, 4.0f, 20.0f, 2.0f);
    g.setColour (p.textDim);
    g.setFont (Fonts::regular (12.0f));
    auto labels = line.translated (0, 20.0f).withHeight (16.0f);
    g.drawText ("-50", labels.withWidth (40.0f).translated (-20.0f, 0), juce::Justification::centred);
    g.drawText ("0", labels.withSizeKeepingCentre (40.0f, 16.0f), juce::Justification::centred);
    g.drawText ("+50", labels.withX (labels.getRight() - 20.0f).withWidth (40.0f), juce::Justification::centred);
}

// ================================================================== choice pill / switch

void ChoicePill::paintButton (juce::Graphics& g, bool hover, bool)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    Glass::drawPanel (g, r, r.getHeight() * 0.5f, p, hover ? 2.0f : 1.4f);
    g.setColour (p.text);
    g.setFont (Fonts::regular (large ? 21.0f : 14.5f));
    g.drawFittedText (text, r.reduced (16.0f, 0).withTrimmedRight (18.0f).toNearestInt(), juce::Justification::centred, 1);
    juce::Path arrow;
    auto ax = r.getRight() - 24.0f, ay = r.getCentreY();
    arrow.startNewSubPath (ax - 5.0f, ay - 2.5f); arrow.lineTo (ax, ay + 2.5f); arrow.lineTo (ax + 5.0f, ay - 2.5f);
    g.setColour (p.text.withAlpha (0.8f));
    g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ModuleSwitch::paintButton (juce::Graphics& g, bool hover, bool)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto r = getLocalBounds().toFloat();
    auto sw = juce::Rectangle<float> (40.0f, 20.0f).withCentre ({ r.getX() + 22.0f, r.getCentreY() });
    auto on = getToggleState();
    Glass::drawPill (g, sw, p, on, hover, false);
    auto knob = on ? sw.withLeft (sw.getRight() - 20.0f).reduced (3.0f) : sw.withWidth (20.0f).reduced (3.0f);
    g.setColour (on ? p.accentText : p.text.withAlpha (0.6f));
    g.fillEllipse (knob);
    g.setColour (p.text);
    g.setFont (Fonts::semiBold (13.0f));
    g.drawText (getName(), r.withTrimmedLeft (50.0f), juce::Justification::centredLeft);
}

// ================================================================== meters

void DashMeter::setLevel (float v)
{
    level = juce::jmax (v, level * 0.86f);
    peak = juce::jmax (v, peak * 0.99f);
    repaint();
}

void DashMeter::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    auto meterArea = b.withTrimmedBottom (78.0f);
    auto dashes = 64;
    auto dh = meterArea.getHeight() / (float) dashes;
    auto db = juce::jlimit (-60.0f, 0.0f, juce::Decibels::gainToDecibels (level, -100.0f));
    auto lit = (int) ((db + 60.0f) / 60.0f * (float) dashes);
    auto pk = (int) ((juce::jlimit (-60.0f, 0.0f, juce::Decibels::gainToDecibels (peak, -100.0f)) + 60.0f) / 60.0f * (float) dashes);
    for (int col = 0; col < 2; ++col)
        for (int i = 0; i < dashes; ++i)
        {
            auto y = meterArea.getBottom() - (float) (i + 1) * dh;
            auto on = i < lit || i == pk;
            auto c = on ? (i > dashes - 6 ? juce::Colour (0xffff7ab0) : (p.isLight ? juce::Colours::white : p.nebulaC)) : p.text.withAlpha (0.12f);
            g.setColour (c);
            g.fillRect (meterArea.getX() + (float) col * 9.0f, y + 0.6f, 6.0f, dh - 1.6f);
        }
    // vertical label at the bottom
    juce::Graphics::ScopedSaveState ss (g);
    g.addTransform (juce::AffineTransform::rotation (-pi * 0.5f, b.getCentreX(), b.getBottom() - 38.0f));
    g.setColour (p.textDim);
    g.setFont (Fonts::regular (14.0f));
    Fonts::drawSpaced (g, label, juce::Rectangle<float> (80.0f, 18.0f).withCentre ({ b.getCentreX(), b.getBottom() - 38.0f }),
                       Fonts::regular (14.0f), 0.12f, juce::Justification::centred);
}

void ReductionBar::setValue (float db)
{
    value = juce::jmax (db, value * 0.9f);
    repaint();
}

void ReductionBar::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat();
    g.setColour (p.textDim);
    g.setFont (Fonts::regular (12.5f));
    g.drawText (label, b.withHeight (16.0f), juce::Justification::centredLeft);
    g.drawText ((value < 0.05f ? juce::String ("0.0") : juce::String (-value, 1)) + " dB", b.withHeight (16.0f), juce::Justification::centredRight);
    auto bar = b.withTop (22.0f).withHeight (6.0f);
    g.setColour (p.text.withAlpha (0.12f));
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (p.isLight ? juce::Colours::white : p.accent.brighter (0.3f));
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, value / maxDb)), 3.0f);
}

void EqCurve::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    Glass::drawPanel (g, b, 10.0f, p, 0.8f);
    auto plot = b.reduced (12.0f, 14.0f);

    // magnitude of the four filters (simple analog-style approximations, enough for a display)
    auto mag = [this] (float f)
    {
        auto shelf = [] (float f0, float fc, float db, bool high)
        { auto x = std::pow (f0 / fc, 2.0f); auto t = high ? x / (1 + x) : 1 / (1 + x); return db * t; };
        auto bell = [] (float f0, float fc, float db, float q)
        { auto o = std::log2 (f0 / fc) * q * 1.4f; return db * std::exp (-o * o); };
        auto hp = 10.0f * std::log10 (1.0f / (1.0f + std::pow (lowCut / f, 8.0f)));
        return hp + shelf (f, 180.0f, body, false) + bell (f, 350.0f, -mud, 1.0f) + bell (f, 3200.0f, presence, 0.9f) + shelf (f, 12000.0f, air, true);
    };
    auto xOf = [&] (float f) { return plot.getX() + plot.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };
    auto yOf = [&] (float db) { return plot.getCentreY() - juce::jlimit (-12.0f, 12.0f, db) / 12.0f * plot.getHeight() * 0.5f; };

    g.setColour (p.text.withAlpha (0.12f));
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
    for (auto f : { 100.0f, 1000.0f, 10000.0f }) g.drawVerticalLine ((int) xOf (f), plot.getY(), plot.getBottom());

    juce::Path curve, fill;
    for (int i = 0; i <= 120; ++i)
    {
        auto f = 20.0f * std::pow (1000.0f, (float) i / 120.0f);
        auto pt = juce::Point<float> (xOf (f), yOf (mag (f)));
        if (i == 0) { curve.startNewSubPath (pt); fill.startNewSubPath (pt.x, plot.getCentreY()); fill.lineTo (pt); }
        else { curve.lineTo (pt); fill.lineTo (pt); }
    }
    fill.lineTo (plot.getRight(), plot.getCentreY());
    fill.closeSubPath();
    g.setColour ((p.isLight ? juce::Colours::white : p.accent).withAlpha (0.18f));
    g.fillPath (fill);
    g.setColour (p.isLight ? juce::Colours::white : p.nebulaC);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (p.textDim);
    g.setFont (Fonts::regular (10.5f));
    for (auto [f, t] : { std::pair<float, const char*> { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } })
        g.drawText (t, juce::Rectangle<float> (xOf (f) - 20.0f, plot.getBottom() - 2.0f, 40.0f, 14.0f), juce::Justification::centred);
}

} // namespace vocal
