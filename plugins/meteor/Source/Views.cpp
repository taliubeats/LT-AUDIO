#include "Views.h"

namespace meteor
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace { constexpr float pi = juce::MathConstants<float>::pi; }

// ================================================================== knob

MeteorKnob::MeteorKnob (const juce::String& l, float s) : label (l), size (s)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    setMouseDragSensitivity (180);
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<float> MeteorKnob::knobArea() const
{
    auto b = getLocalBounds().toFloat();
    return juce::Rectangle<float> (size, size).withCentre ({ b.getCentreX(), b.getY() + size * 0.5f + 4.0f });
}

bool MeteorKnob::hitTest (int x, int y)
{
    auto k = knobArea();
    return k.getCentre().getDistanceFrom ({ (float) x, (float) y }) < k.getWidth() * 0.55f;
}

void MeteorKnob::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto k = knobArea();
    lt::ui::GlassLookAndFeel::drawGlassKnob (g, k, (float) valueToProportionOfLength (getValue()), p, 0.75f);
    auto show = isMouseOverOrDragging();
    g.setColour (show ? p.value : p.text);
    g.setFont (Fonts::regular (13.5f));
    g.drawFittedText (show ? getTextFromValue (getValue()) : label, getLocalBounds().withTop ((int) k.getBottom() + 5),
                      juce::Justification::centredTop, 1);
}

// ================================================================== pills

ChoicePills::ChoicePills (juce::RangedAudioParameter& param, const juce::StringArray& labels, int cols) : columns (cols)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->onClick = [this, i] { attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    attachment = std::make_unique<juce::ParameterAttachment> (param, [this] (float v)
    {
        for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == juce::roundToInt (v), juce::dontSendNotification);
    });
    attachment->sendInitialUpdate();
}

void ChoicePills::resized()
{
    auto n = buttons.size();
    auto cols = columns > 0 ? columns : n;
    auto rows = (n + cols - 1) / cols;
    auto gap = 5;
    auto w = (getWidth() - gap * (cols - 1)) / cols;
    auto h = (getHeight() - gap * (rows - 1)) / rows;
    for (int i = 0; i < n; ++i)
        buttons[i]->setBounds ((i % cols) * (w + gap), (i / cols) * (h + gap), w, h);
}

// ================================================================== power pill

void PowerPill::paintButton (juce::Graphics& g, bool hover, bool down)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    auto on = getToggleState();
    Glass::drawPill (g, b, p, on, hover, down);
    auto dot = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ b.getX() + 20.0f, b.getCentreY() });
    if (on) Glass::drawGlow (g, dot.getCentre(), 12.0f, p.isLight ? juce::Colours::white : p.nebulaC, 0.8f);
    g.setColour (on ? p.accentText : p.textDim);
    g.fillEllipse (dot);
    g.setColour (on ? p.accentText : p.text);
    Fonts::drawSpaced (g, getButtonText(), b.withTrimmedLeft (18.0f), Fonts::semiBold (15.0f), 0.12f, juce::Justification::centred);
}

// ================================================================== meteor view

void MeteorView::tick (const MeteorState& s)
{
    st = s;
    auto dt = 1.0f / 30.0f;
    time += dt;
    glow = juce::jmax (glow * 0.88f, s.level);
    auto targetDir = s.reverse ? -1.0f : 1.0f;
    dir += (targetDir - dir) * 0.08f;
    x += dir * dt * (0.06f + 0.1f * glow);
    if (x > 1.12f) x = -0.12f;
    if (x < -0.12f) x = 1.12f;

    if (s.glitches != lastGlitches)
    {
        lastGlitches = s.glitches;
        if (s.glitch) { x += (random.nextFloat() - 0.5f) * 0.06f; jumpFlash = 1.0f; }
    }
    jumpFlash *= 0.82f;

    history.insert (history.begin(), s.chop ? s.gate : 1.0f);
    if (history.size() > 60) history.pop_back();

    auto w = (float) getWidth(), h = (float) getHeight();
    auto head = juce::Point<float> (x * w, h * (0.62f - 0.3f * x));
    for (int i = 0; i < (int) (2 + 10 * glow); ++i)
        sparks.push_back ({ head, { -dir * (20.0f + random.nextFloat() * 60.0f), (random.nextFloat() - 0.3f) * 18.0f },
                            1.0f, 1.0f + random.nextFloat() * 2.0f });
    for (auto& sp : sparks) { sp.pos += sp.vel * dt; sp.life -= dt * 0.9f; }
    sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [] (const Spark& sp) { return sp.life <= 0; }), sparks.end());
    if (sparks.size() > 400) sparks.erase (sparks.begin(), sparks.begin() + 100);
    repaint();
}

void MeteorView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto w = (float) getWidth(), h = (float) getHeight();
    auto core = p.isLight ? juce::Colours::white : juce::Colour (0xfff1e9ff);
    auto tailCol = p.isLight ? juce::Colour (0xffffe14d) : p.nebulaC;

    auto drawMeteor = [&] (float mx, float yOff, float scale, float alpha)
    {
        auto head = juce::Point<float> (mx * w, h * (0.62f - 0.3f * mx) + yOff);
        auto slope = -0.3f * h / w;                         // path direction (dy/dx)
        auto len = (120.0f + 160.0f * glow) * scale;
        auto dirv = juce::Point<float> (-dir, -dir * slope);
        dirv = dirv / juce::jmax (0.001f, dirv.getDistanceFromOrigin());

        // tail: segments; CHOP cuts it using the recent gate history
        auto segs = (int) history.size();
        for (int i = 0; i < segs - 1; ++i)
        {
            auto t0 = (float) i / (float) segs, t1 = (float) (i + 1) / (float) segs;
            auto a = alpha * (1.0f - t0) * history[(size_t) i];
            if (a < 0.02f) continue;
            g.setColour (tailCol.withAlpha (juce::jmin (1.0f, a * 0.9f)));
            g.drawLine (juce::Line<float> (head + dirv * (t0 * len), head + dirv * (t1 * len)), (7.0f * (1.0f - t0) + 1.0f) * scale);
        }
        Glass::drawGlow (g, head, 30.0f * scale * (1.0f + glow), p.isLight ? juce::Colours::white : p.accent, 0.6f * alpha + 0.4f * jumpFlash);
        g.setColour (core.withAlpha (alpha));
        g.fillEllipse (juce::Rectangle<float> (12.0f * scale, 12.0f * scale).withCentre (head));
    };

    for (auto& sp : sparks)
    {
        g.setColour (tailCol.withAlpha (0.6f * sp.life));
        g.fillEllipse (sp.pos.x, sp.pos.y, sp.size, sp.size);
    }

    if (st.doubled)
    {
        const float offs[3][2] = { { -0.035f, -18.0f }, { -0.06f, 20.0f }, { -0.085f, -32.0f } };
        for (int v = 0; v < juce::jlimit (1, 3, st.voices); ++v)
            drawMeteor (x + offs[v][0] * dir, offs[v][1], 0.6f, 0.45f);
    }
    drawMeteor (x, 0.0f, 1.0f, 1.0f);

    if (jumpFlash > 0.05f)
    {
        // glitch: a couple of displaced slices of the frame
        juce::Random r ((juce::int64) (time * 1000.0));
        for (int i = 0; i < 3; ++i)
        {
            auto y = r.nextFloat() * h;
            g.setColour (core.withAlpha (0.12f * jumpFlash));
            g.fillRect (juce::Rectangle<float> (0.0f, y, w, 2.0f + r.nextFloat() * 6.0f));
        }
    }
}

// ================================================================== waveform

void Waveform::draw (juce::Graphics& g, juce::Rectangle<float> r, const juce::AudioBuffer<float>* b, juce::Colour col)
{
    if (b == nullptr || b->getNumSamples() == 0 || r.getWidth() < 2) return;
    r = r.toNearestInt().toFloat();
    auto width = (int) r.getWidth();
    if (b != cached || b->getNumSamples() != cachedSamples || width != cachedWidth)
    {
        cached = b; cachedSamples = b->getNumSamples(); cachedWidth = width;
        peaks.assign ((size_t) width, { 0.0f, 0.0f });
        auto per = (double) b->getNumSamples() / width;
        float pk = 1.0e-6f;
        for (int x = 0; x < width; ++x)
        {
            auto s0 = (int) (x * per), s1 = juce::jmin (b->getNumSamples(), (int) ((x + 1) * per) + 1);
            float lo = 0, hi = 0;
            for (int c = 0; c < b->getNumChannels(); ++c)
            {
                auto range = juce::FloatVectorOperations::findMinAndMax (b->getReadPointer (c, s0), juce::jmax (1, s1 - s0));
                lo = juce::jmin (lo, range.getStart()); hi = juce::jmax (hi, range.getEnd());
            }
            peaks[(size_t) x] = { lo, hi };
            pk = juce::jmax (pk, -lo, hi);
        }
        for (auto& pr : peaks) { pr.first /= pk; pr.second /= pk; }
    }
    g.setColour (col);
    auto cy = r.getCentreY(), half = r.getHeight() * 0.5f;
    for (int x = 0; x < width; ++x)
    {
        auto& pr = peaks[(size_t) x];
        g.fillRect (r.getX() + (float) x, cy - pr.second * half, 1.0f, juce::jmax (1.0f, (pr.second - pr.first) * half));
    }
}

// ================================================================== source

void SourceView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    Glass::drawPanel (g, b, 12.0f, p, dragHover ? 1.8f : 1.0f, dragHover || capturing);

    if (capturing)
    {
        auto blink = (juce::Time::getMillisecondCounter() / 400) % 2 == 0;
        g.setColour (p.danger.withAlpha (blink ? 1.0f : 0.4f));
        g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ b.getCentreX() - 70.0f, b.getCentreY() }));
        g.setColour (p.value);
        g.setFont (Fonts::regular (18.0f));
        g.drawText ("RECORDING  " + juce::String (captureSeconds, 1) + " s", b, juce::Justification::centred);
        return;
    }

    if (buffer == nullptr)
    {
        g.setColour (p.text);
        Fonts::drawSpaced (g, "DROP AUDIO HERE", b.withTrimmedBottom (28.0f), Fonts::regular (17.0f), 0.08f, juce::Justification::centred);
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (12.5f));
        g.drawText ("or click to choose a file  \xc2\xb7  or USE TRACK", b.withTrimmedTop (36.0f), juce::Justification::centred);
        return;
    }
    auto top = b.reduced (14.0f, 10.0f);
    g.setColour (p.text);
    g.setFont (Fonts::regular (14.0f));
    g.drawFittedText (name, top.removeFromTop (20.0f).toNearestInt(), juce::Justification::centredLeft, 1);
    wave.draw (g, top.reduced (0.0f, 6.0f), buffer.get(), (p.isLight ? juce::Colours::white : p.nebulaC).withAlpha (0.85f));
}

// ================================================================== stroke card

StrokeCard::StrokeCard (const juce::String& n) : name (n) {}

juce::Rectangle<float> StrokeCard::playArea() const
{
    auto b = getLocalBounds().toFloat().reduced (10.0f);
    return b.removeFromBottom (34.0f).removeFromLeft (34.0f);
}

juce::Rectangle<float> StrokeCard::dragArea() const
{
    auto b = getLocalBounds().toFloat().reduced (10.0f);
    return b.removeFromBottom (34.0f).withTrimmedLeft (42.0f);
}

void StrokeCard::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    Glass::drawPanel (g, b, 12.0f, p, playing ? 1.6f : 1.0f, playing);

    auto inner = b.reduced (10.0f);
    g.setColour (p.text);
    Fonts::drawSpaced (g, name, inner.removeFromTop (22.0f), Fonts::semiBold (13.0f), 0.08f, juce::Justification::centredLeft);
    auto waveArea = inner.withTrimmedBottom (44.0f).reduced (0.0f, 4.0f);

    auto accent = p.isLight ? juce::Colours::white : p.nebulaC;
    if (buffer != nullptr && buffer->getNumSamples() > 0)
    {
        wave.draw (g, waveArea, buffer.get(), accent.withAlpha (0.8f));
        if (playing)
        {
            auto x = waveArea.getX() + waveArea.getWidth() * progress;
            g.setColour (p.value);
            g.drawVerticalLine ((int) x, waveArea.getY(), waveArea.getBottom());
        }
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (11.0f));
        g.drawText (juce::String (buffer->getNumSamples() / sampleRate, 1) + " s", waveArea.withHeight (14.0f), juce::Justification::topRight);
    }
    else
    {
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (12.0f));
        g.drawText (waiting ? "rendering..." : "no audio yet", waveArea, juce::Justification::centred);
    }

    auto mouse = getMouseXYRelative().toFloat();
    auto ready = buffer != nullptr && buffer->getNumSamples() > 0;

    // PLAY / STOP
    auto pa = playArea();
    auto hotPlay = isMouseOver() && pa.contains (mouse);
    Glass::drawPill (g, pa, p, playing, hotPlay && ready, false);
    g.setColour (ready ? (playing ? p.accentText : p.text) : p.textDim);
    auto ic = pa.reduced (11.0f);
    if (playing) g.fillRect (ic);
    else
    {
        juce::Path tri;
        tri.addTriangle (ic.getX() + 1.0f, ic.getY(), ic.getX() + 1.0f, ic.getBottom(), ic.getRight() + 1.0f, ic.getCentreY());
        g.fillPath (tri);
    }

    // DRAG
    auto da = dragArea();
    auto hotDrag = (isMouseOver() && da.contains (mouse)) || dragging;
    Glass::drawPanel (g, da, 8.0f, p, hotDrag && ready ? 1.8f : 1.0f, hotDrag && ready);
    g.setColour (ready ? (hotDrag ? p.value : p.text) : p.textDim);
    auto grip = da.removeFromLeft (30.0f).reduced (9.0f, 10.0f);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 2; ++c)
            g.fillEllipse (grip.getX() + (float) c * 6.0f, grip.getY() + (float) r * 6.0f, 2.5f, 2.5f);
    Fonts::drawSpaced (g, "DRAG", da.withTrimmedRight (10.0f), Fonts::regular (13.0f), 0.12f, juce::Justification::centred);
}

void StrokeCard::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    pressedPlay = playArea().contains (e.position);
}

void StrokeCard::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || pressedPlay || e.getDistanceFromDragStart() < 6 || ! makeFile || buffer == nullptr) return;
    dragging = true;
    repaint();
    auto f = makeFile();
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
                                                                    [safe = juce::Component::SafePointer<StrokeCard> (this)]
                                                                    { if (safe) { safe->dragging = false; safe->repaint(); } });
    else
        dragging = false;
}

void StrokeCard::mouseUp (const juce::MouseEvent& e)
{
    auto click = ! dragging && e.getDistanceFromDragStart() < 6;
    if (click && pressedPlay && playArea().contains (e.position) && onPlay) onPlay();
    else if (click && dragArea().contains (e.position) && makeFile && buffer != nullptr)
    {
        auto f = makeFile();
        if (f.existsAsFile() && onSaved) onSaved (f);
    }
    else if (click && ! pressedPlay && ! dragArea().contains (e.position) && onPlay) onPlay();
    dragging = false;
    pressedPlay = false;
    repaint();
}

} // namespace meteor
