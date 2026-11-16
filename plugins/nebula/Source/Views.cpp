#include "Views.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace nebula
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace { constexpr float pi = juce::MathConstants<float>::pi; }

// ================================================================== galaxy

GalaxyView::GalaxyView() { setInterceptsMouseClicks (false, false); }

void GalaxyView::render()
{
    images.clear();
    auto s = juce::jmax (64, juce::jmin (getWidth(), getHeight())) * 2;   // 2x for sharp stars
    renderedLight = palette->isLight;
    auto& p = *palette;
    auto starA = p.isLight ? juce::Colours::white : p.nebulaC;
    auto starB = p.isLight ? juce::Colour (0xfffff3b0) : juce::Colour (0xffffd6f5);
    auto c = (float) s * 0.5f;

    for (int layer = 0; layer < 2; ++layer)
    {
        juce::Image img (juce::Image::ARGB, s, s, true);
        juce::Graphics g (img);
        juce::Random r (layer == 0 ? 11 : 23);
        auto count = layer == 0 ? 1500 : 5200;
        for (int i = 0; i < count; ++i)
        {
            // two logarithmic spiral arms + a halo
            auto arm = r.nextInt (2);
            auto t = std::pow (r.nextFloat(), 0.8f);
            auto radius = t * c * 0.92f;
            auto angle = (float) arm * pi + t * 4.6f + (r.nextFloat() - 0.5f) * (1.3f - 0.6f * t) * (layer == 0 ? 1.0f : 1.5f);
            if (r.nextFloat() < 0.18f) { angle = r.nextFloat() * 2.0f * pi; radius = c * 0.92f * std::sqrt (r.nextFloat()); }
            auto x = c + std::cos (angle) * radius, y = c + std::sin (angle) * radius;
            auto size = (0.8f + r.nextFloat() * 2.6f) * (layer == 0 ? 1.4f : 1.0f) * (1.2f - 0.5f * t);
            auto col = starA.interpolatedWith (starB, r.nextFloat() * 0.6f).withAlpha (0.35f + 0.65f * r.nextFloat());
            if (size > 3.2f) Glass::drawGlow (g, { x, y }, size * 3.0f, col, 0.35f);
            g.setColour (col);
            g.fillEllipse (x - size * 0.5f, y - size * 0.5f, size, size);
        }
        images.push_back (img);
    }

    // bright core with soft arms of light
    juce::Image core (juce::Image::ARGB, s, s, true);
    {
        juce::Graphics g (core);
        auto glow = p.isLight ? juce::Colours::white : p.accent;
        Glass::drawGlow (g, { c, c }, c * 0.9f, glow, 0.35f);
        Glass::drawGlow (g, { c, c }, c * 0.45f, p.isLight ? juce::Colour (0xfffff3b0) : p.nebulaC, 0.7f);
        Glass::drawGlow (g, { c, c }, c * 0.1f, juce::Colours::white, 1.0f);
        for (int arm = 0; arm < 2; ++arm)
            for (int k = 0; k < 40; ++k)
            {
                auto t = (float) k / 40.0f;
                auto a = (float) arm * pi + t * 4.6f;
                auto pt = juce::Point<float> (c + std::cos (a) * t * c * 0.85f, c + std::sin (a) * t * c * 0.85f);
                Glass::drawGlow (g, pt, c * (0.16f - 0.1f * t), glow, 0.12f * (1.0f - t));
            }
    }
    images.push_back (core);
}

void GalaxyView::tick (const GalaxyState& s)
{
    st = s;
    auto dt = 1.0f / 30.0f;
    if (! s.frozen)
        rotation += dt * (0.05f + 0.5f * s.motion) * s.tapeSpeed;
    scale += ((0.8f + 0.25f * s.space) - scale) * 0.05f;
    if (s.stepFlash != lastFlash) pulse = 1.0f;
    lastFlash = s.stepFlash;
    pulse *= 0.9f;

    // every grain becomes a short flash on an arm
    auto newGrains = juce::jmin (12, s.grains - lastGrains);
    lastGrains = s.grains;
    for (int i = 0; i < newGrains; ++i)
    {
        auto t = random.nextFloat();
        flashes.push_back ({ (float) random.nextInt (2) * pi + t * 4.6f + (random.nextFloat() - 0.5f) * 0.6f, t, 1.0f,
                             1.5f + random.nextFloat() * (2.0f + 3.0f * s.level) });
    }
    for (auto& f : flashes) f.life -= s.frozen ? 0.0f : 0.06f;
    flashes.erase (std::remove_if (flashes.begin(), flashes.end(), [] (const Flash& f) { return f.life <= 0; }), flashes.end());
    if (flashes.size() > 300) flashes.erase (flashes.begin(), flashes.begin() + 100);
    repaint();
}

void GalaxyView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    if (images.size() < 3 || renderedLight != p.isLight) render();

    auto b = getLocalBounds().toFloat();
    auto size = juce::jmin (b.getWidth(), b.getHeight());
    auto circle = juce::Rectangle<float> (size, size).withCentre (b.getCentre());
    auto c = circle.getCentre();
    auto glow = p.isLight ? juce::Colours::white : p.accent;

    if (framed)
    {
        Glass::drawGlow (g, c, size * 0.62f, glow, 0.22f);
        g.setColour ((p.isLight ? juce::Colour (0xff1d7aa8) : juce::Colours::black).withAlpha (p.isLight ? 0.18f : 0.55f));
        g.fillEllipse (circle);
    }

    {
        juce::Graphics::ScopedSaveState ss (g);
        if (framed) { juce::Path clip; clip.addEllipse (circle.reduced (2.0f)); g.reduceClipRegion (clip); }

        auto img = (float) images[0].getWidth();
        auto drawRotated = [&] (const juce::Image& im, float rot, float sc, float alpha)
        {
            auto k = size / img * sc;
            g.setOpacity (juce::jlimit (0.0f, 1.0f, alpha));
            g.drawImageTransformed (im, juce::AffineTransform::translation (-img * 0.5f, -img * 0.5f)
                                            .rotated (rot).scaled (k, k * 0.92f).translated (c.x, c.y));
        };
        auto dirtJitter = st.dirt * 0.02f * (random.nextFloat() - 0.5f);
        drawRotated (images[2], rotation * 1.3f, scale, 0.85f + 0.3f * pulse + 0.2f * st.level);
        drawRotated (images[0], rotation, scale, 0.55f + 0.45f * (1.0f - st.texture));
        drawRotated (images[1], rotation * 1.08f + dirtJitter, scale, 0.25f + 0.75f * st.texture);
        g.setOpacity (1.0f);

        // grain flashes
        auto sparkle = p.isLight ? juce::Colours::white : juce::Colour (0xfffff0ff);
        for (auto& f : flashes)
        {
            auto r = f.radius * size * 0.46f * scale;
            auto a = f.angle + rotation * 1.1f;
            auto pos = juce::Point<float> (c.x + std::cos (a) * r, c.y + std::sin (a) * r * 0.92f);
            Glass::drawGlow (g, pos, f.size * 3.5f, glow, f.life * 0.5f);
            g.setColour (sparkle.withAlpha (f.life));
            g.fillEllipse (pos.x - f.size * 0.5f, pos.y - f.size * 0.5f, f.size, f.size);
        }
        if (st.frozen)
        {
            g.setColour (juce::Colour (0xffbfe9ff).withAlpha (0.08f));
            g.fillEllipse (circle);
        }
    }

    if (framed)
    {
        juce::ColourGradient rim (juce::Colours::white.withAlpha (0.5f), circle.getX(), circle.getY(),
                                  glow.withAlpha (0.3f), circle.getRight(), circle.getBottom(), false);
        g.setGradientFill (rim);
        g.drawEllipse (circle.reduced (1.0f), 1.6f);
    }
}

// ================================================================== knob

MacroKnob::MacroKnob (const juce::String& l, float d) : label (l), diameter (d)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    setMouseDragSensitivity (220);
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<float> MacroKnob::knobArea() const
{
    auto b = getLocalBounds().toFloat();
    return juce::Rectangle<float> (diameter, diameter).withCentre ({ b.getCentreX(), b.getY() + diameter * 0.5f + 10.0f });
}

bool MacroKnob::hitTest (int x, int y)
{
    return knobArea().getCentre().getDistanceFrom ({ (float) x, (float) y }) < diameter * 0.58f;
}

bool MacroKnob::isInterestedInDragSource (const SourceDetails& d)
{
    return onModDrop != nullptr && d.description.toString().startsWith ("mod:");
}

void MacroKnob::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    if (onModDrop) onModDrop (d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue());
    repaint();
}

void MacroKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onModMenu) { onModMenu(); return; }
    juce::Slider::mouseDown (e);
}

void MacroKnob::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto k = knobArea();
    auto base = (float) valueToProportionOfLength (getValue());
    lt::ui::GlassLookAndFeel::drawGlassKnob (g, k, base, p, 1.0f);

    // modulation rings: one coloured arc per routed source, and a dot where the knob really is now
    if (modDepth)
    {
        auto start = pi * 1.25f, end = pi * 2.75f;
        auto c = k.getCentre();
        auto radius = k.getWidth() * 0.5f + 3.0f;
        int ring = 0;
        for (int src = 0; src < 4; ++src)
        {
            auto d = modDepth (src);
            if (std::abs (d) < 0.001f) continue;
            auto rr = radius + (float) ring++ * 4.0f;
            auto bipolar = src == 1 || src == 3;
            auto lo = juce::jlimit (0.0f, 1.0f, bipolar ? base - std::abs (d) * 0.5f : juce::jmin (base, base + d));
            auto hi = juce::jlimit (0.0f, 1.0f, bipolar ? base + std::abs (d) * 0.5f : juce::jmax (base, base + d));
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, rr, rr, 0, start + lo * (end - start), start + juce::jmax (lo + 0.005f, hi) * (end - start), true);
            g.setColour (juce::Colour (src == 0 ? 0xffffd35a : src == 1 ? 0xffff7aa8 : src == 2 ? 0xff5ee6ff : 0xff7dff9b));
            g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        if (ring > 0 && modOffset)
        {
            auto now = juce::jlimit (0.0f, 1.0f, base + modOffset());
            auto pt = c.getPointOnCircumference (radius, start + now * (end - start));
            Glass::drawGlow (g, pt, 7.0f, juce::Colours::white, 0.7f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (pt.x - 2.5f, pt.y - 2.5f, 5.0f, 5.0f);
        }
    }
    if (dropHover)
        Glass::drawGlow (g, k.getCentre(), k.getWidth() * 0.7f, juce::Colours::white, 0.35f);
    auto big = diameter >= 80.0f;
    if (big)
    {
        g.setColour (p.value);
        g.setFont (Fonts::regular (diameter * 0.2f));
        g.drawFittedText (getTextFromValue (getValue()), k.reduced (diameter * 0.18f).toNearestInt(), juce::Justification::centred, 1);
    }
    if (showLabel)
    {
        auto show = ! big && isMouseOverOrDragging();
        g.setColour (show ? p.value : p.text);
        g.setFont (big ? Fonts::regular (22.0f) : Fonts::regular (14.0f));
        g.drawFittedText (show ? getTextFromValue (getValue()) : label,
                          getLocalBounds().withTop ((int) k.getBottom() + (big ? 10 : 5)), juce::Justification::centredTop, 1);
    }
}

// ================================================================== choice row

ChoiceRow::ChoiceRow (juce::RangedAudioParameter& p, std::vector<int> idx, juce::StringArray labels)
    : param (p), indices (std::move (idx))
{
    for (int i = 0; i < (int) indices.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->onClick = [this, i]
        {
            attachment->setValueAsCompleteGesture ((float) indices[(size_t) i]);
            if (onChoose) onChoose (indices[(size_t) i]);
        };
        addAndMakeVisible (b);
    }
    attachment = std::make_unique<juce::ParameterAttachment> (param, [this] (float v)
    {
        auto cur = juce::roundToInt (v);
        for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (indices[(size_t) i] == cur, juce::dontSendNotification);
    });
    attachment->sendInitialUpdate();
}

void ChoiceRow::resized()
{
    auto total = 0.0f;
    std::vector<float> widths;
    for (auto* b : buttons)
    {
        auto w = juce::GlyphArrangement::getStringWidth (Fonts::regular (15.0f), b->getButtonText()) + 30.0f;
        widths.push_back (w); total += w;
    }
    auto gap = 8.0f;
    auto scale = juce::jmin (1.0f, ((float) getWidth() - gap * (float) (buttons.size() - 1)) / total);
    auto x = 0.0f;
    for (int i = 0; i < buttons.size(); ++i)
    {
        auto w = widths[(size_t) i] * scale;
        buttons[i]->setBounds ((int) x, 0, (int) w, getHeight());
        x += w + gap;
    }
}

} // namespace nebula
