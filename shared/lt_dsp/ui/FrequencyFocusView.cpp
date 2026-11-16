namespace lt::ui
{

namespace
{
    constexpr float logSpan = 6.907755f; // ln (20000 / 20)

    juce::String hzText (float hz)
    {
        if (hz >= 1000.0f)
        {
            auto k = hz / 1000.0f;
            auto whole = hz >= 10000.0f || std::abs (k - std::round (k)) < 0.05f;
            return (whole ? juce::String (juce::roundToInt (k)) : juce::String (k, 1)) + " kHz";
        }
        return juce::String (juce::roundToInt (hz)) + " Hz";
    }
}

FrequencyFocusView::FrequencyFocusView (juce::AudioProcessorValueTreeState& s, dsp::SpectrumAnalyser& a, const juce::String& id)
    : state (s), analyser (a), productId (id)
{
    setLookAndFeel (&lnf);
    bandParam = s.getParameter (dsp::FocusParams::band);
    lowParam = s.getParameter (dsp::FocusParams::lowCut);
    highParam = s.getParameter (dsp::FocusParams::highCut);
    jassert (bandParam != nullptr && lowParam != nullptr && highParam != nullptr);

    for (int i = 0; i < 4; ++i)
    {
        bandButtons[i].setClickingTogglesState (false);
        bandButtons[i].onClick = [this, i] { setBand (i); };
        addAndMakeVisible (bandButtons[i]);
    }

    invertAttachment = std::make_unique<juce::ButtonParameterAttachment> (*s.getParameter (dsp::FocusParams::invert), invertButton);
    soloAttachment = std::make_unique<juce::ButtonParameterAttachment> (*s.getParameter (dsp::FocusParams::solo), soloButton);
    addAndMakeVisible (invertButton);
    addAndMakeVisible (soloButton);

    backButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (backButton);

    startTimerHz (30);
}

FrequencyFocusView::~FrequencyFocusView()
{
    setLookAndFeel (nullptr);
}

void FrequencyFocusView::setPalette (const Palette& p)
{
    palette = p;
    lnf.setPalette (p);
    nebula = {};
    repaint();
}

dsp::FocusSettings FrequencyFocusView::current() const
{
    dsp::FocusSettings f;
    f.band = juce::roundToInt (bandParam->convertFrom0to1 (bandParam->getValue()));
    auto r = dsp::FocusSettings::rangeFor (f.band);
    f.lowHz = f.band == dsp::FocusSettings::custom ? lowParam->convertFrom0to1 (lowParam->getValue()) : r.getStart();
    f.highHz = f.band == dsp::FocusSettings::custom ? highParam->convertFrom0to1 (highParam->getValue()) : r.getEnd();
    f.invert = invertButton.getToggleState();
    f.solo = soloButton.getToggleState();
    return f;
}

void FrequencyFocusView::setBand (int band)
{
    auto set = [] (juce::RangedAudioParameter* p, float v)
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    };

    if (band != dsp::FocusSettings::full && band != dsp::FocusSettings::custom)
    {
        auto r = dsp::FocusSettings::rangeFor (band);
        set (lowParam, r.getStart());
        set (highParam, r.getEnd());
    }
    set (bandParam, (float) band);
    repaint();
}

void FrequencyFocusView::setCutoffs (float lo, float hi)
{
    lo = juce::jlimit (dsp::FocusSettings::minHz, dsp::FocusSettings::maxHz / 1.25f, lo);
    hi = juce::jlimit (lo * 1.25f, dsp::FocusSettings::maxHz, hi);
    lowParam->setValueNotifyingHost (lowParam->convertTo0to1 (lo));
    highParam->setValueNotifyingHost (highParam->convertTo0to1 (hi));
    if (juce::roundToInt (bandParam->convertFrom0to1 (bandParam->getValue())) != dsp::FocusSettings::custom)
        bandParam->setValueNotifyingHost (bandParam->convertTo0to1 ((float) dsp::FocusSettings::custom));
    repaint();
}

float FrequencyFocusView::xForHz (float hz) const noexcept
{
    return graph.getX() + graph.getWidth() * std::log (juce::jmax (1.0f, hz) / 20.0f) / logSpan;
}

float FrequencyFocusView::hzForX (float x) const noexcept
{
    return 20.0f * std::exp (juce::jlimit (0.0f, 1.0f, (x - graph.getX()) / graph.getWidth()) * logSpan);
}

// ------------------------------------------------------------------ layout

void FrequencyFocusView::resized()
{
    auto b = getLocalBounds().toFloat();
    backButton.setBounds ((int) b.getRight() - 30 - 110, 26, 110, 36);

    panel = b.withTrimmedTop (92).withTrimmedBottom (92).reduced (16.0f, 0.0f);
    graph = panel.withTrimmedTop (70.0f).withTrimmedBottom (46.0f).reduced (40.0f, 0.0f);

    auto row = getLocalBounds().withTop (getHeight() - 92).reduced (30, 20);
    for (auto& bb : bandButtons)
    {
        bb.setBounds (row.removeFromLeft (128));
        row.removeFromLeft (12);
    }
    soloButton.setBounds (row.removeFromRight (110));
    row.removeFromRight (20);
    invertButton.setBounds (row.removeFromRight (124));
    nebula = {};
}

// ------------------------------------------------------------------ animation

void FrequencyFocusView::timerCallback()
{
    analyser.update();
    time += 1.0 / 30.0;

    auto f = current();
    for (int i = 0; i < 4; ++i)
        bandButtons[i].setToggleState (f.band == i, juce::dontSendNotification);

    // sparks are born inside the focused band, more when it is loud
    auto lo = f.isActive() ? f.lowHz : dsp::FocusSettings::minHz;
    auto hi = f.isActive() ? f.highHz : dsp::FocusSettings::maxHz;
    auto u0 = std::log (lo / 20.0f) / logSpan, u1 = std::log (hi / 20.0f) / logSpan;
    auto energy = analyser.getLevel (std::sqrt (lo * hi));

    auto births = (int) (1.0f + energy * 6.0f);
    for (int i = 0; i < births && sparks.size() < 160; ++i)
    {
        float u;
        if (! f.invert || ! f.isActive())
            u = juce::jmap (random.nextFloat(), u0, u1);
        else
            u = random.nextBool() ? random.nextFloat() * u0 : juce::jmap (random.nextFloat(), u1, 1.0f);
        sparks.push_back ({ u, 0.15f + random.nextFloat() * 0.85f,
                            (random.nextFloat() - 0.5f) * 0.002f, -0.002f - random.nextFloat() * 0.006f * (0.4f + energy),
                            1.0f, 0.8f + random.nextFloat() * 1.8f });
    }

    for (auto& s : sparks)
    {
        s.x += s.vx;
        s.y += s.vy;
        s.life -= 0.012f + 0.01f * (1.0f - energy);
    }
    sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [] (const Spark& s) { return s.life <= 0 || s.y < 0; }), sparks.end());

    repaint (panel.toNearestInt().expanded (4));
}

// ------------------------------------------------------------------ painting

void FrequencyFocusView::drawSpectrum (juce::Graphics& g, juce::Rectangle<float> r, bool focused)
{
    juce::Path p;
    p.startNewSubPath (r.getX(), r.getBottom());
    for (float x = r.getX(); x <= r.getRight(); x += 2.0f)
    {
        auto hz = hzForX (x);
        auto db = analyser.getLevel (hz) * 90.0f - 90.0f + 4.5f * std::log2 (hz / 1000.0f);   // pink-noise tilt
        auto level = juce::jlimit (0.0f, 1.0f, (db + 78.0f) / 72.0f);
        p.lineTo (x, r.getBottom() - level * r.getHeight() * 0.92f);
    }
    p.lineTo (r.getRight(), r.getBottom());
    p.closeSubPath();

    if (focused)
    {
        auto a = palette.isLight ? palette.value : palette.accent;
        juce::ColourGradient grad (a.brighter (0.5f).withAlpha (0.95f), 0, r.getY() + r.getHeight() * 0.3f,
                                   a.withAlpha (0.55f), 0, r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (p);
        g.setColour (a.brighter (0.7f));
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }
    else
    {
        g.setColour (palette.isLight ? juce::Colours::white.withAlpha (0.45f) : juce::Colour (0xff8a8794).withAlpha (0.42f));
        g.fillPath (p);
    }
}

void FrequencyFocusView::drawHandle (juce::Graphics& g, float x, const juce::String& title, float hz, bool hot)
{
    auto bar = juce::Rectangle<float> (x - 5.0f, graph.getY(), 10.0f, graph.getHeight());
    if (hot)
        Glass::drawRectGlow (g, bar, 5.0f, palette.accent, 8.0f, 0.8f);
    g.setColour ((palette.isLight ? juce::Colours::white : juce::Colour (0xffb9b4c9)).withAlpha (hot ? 0.95f : 0.7f));
    g.fillRoundedRectangle (bar, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawRoundedRectangle (bar, 5.0f, 1.0f);

    auto label = juce::Rectangle<float> (x - 70.0f, graph.getY() - 62.0f, 140.0f, 26.0f);
    g.setColour (palette.isLight ? palette.text : palette.textDim);
    g.setFont (Fonts::regular (14.0f));
    g.drawText (title, label, juce::Justification::centred);
    g.setColour (palette.isLight ? palette.text : palette.value);
    g.setFont (Fonts::semiBold (15.0f));
    g.drawText (hzText (hz), label.translated (0, 22.0f), juce::Justification::centred);
}

void FrequencyFocusView::paint (juce::Graphics& g)
{
    auto& p = palette;

    // header
    auto titleArea = juce::Rectangle<float> (36.0f, 22.0f, 700.0f, 44.0f);
    auto used = Logo::draw (g, titleArea, Logo::specFor (productId), p, juce::Justification::centredLeft);
    g.setColour (p.isLight ? juce::Colours::white : p.value);
    Fonts::drawSpaced (g, "FREQUENCY FOCUS", titleArea.withTrimmedLeft (used + 34.0f), Fonts::regular (30.0f), 0.08f,
                       juce::Justification::centredLeft);

    Glass::drawPanel (g, panel, 14.0f, p, 1.0f);
    g.setColour (p.glassEdge.withMultipliedAlpha (0.8f));
    g.drawHorizontalLine (getHeight() - 92, 16.0f, (float) getWidth() - 16.0f);

    // graph background + grid
    g.setColour (p.isLight ? juce::Colours::white.withAlpha (0.08f) : juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (graph, 6.0f);
    for (int decade = 10; decade <= 10000; decade *= 10)
        for (int m = 1; m <= 9; ++m)
        {
            auto hz = (float) (decade * m);
            if (hz < 20.0f || hz > 20000.0f) continue;
            g.setColour (p.text.withAlpha (m == 1 ? 0.16f : 0.07f));
            g.drawVerticalLine ((int) xForHz (hz), graph.getY(), graph.getBottom());
        }
    for (int i = 1; i < 4; ++i)
    {
        g.setColour (p.text.withAlpha (0.07f));
        g.drawHorizontalLine ((int) (graph.getY() + graph.getHeight() * (float) i / 4.0f), graph.getX(), graph.getRight());
    }
    g.setColour (p.glassEdge);
    g.drawRoundedRectangle (graph, 6.0f, 1.0f);

    // frequency labels
    g.setFont (Fonts::regular (14.0f));
    g.setColour (p.isLight ? p.text : p.textDim);
    for (auto [hz, text] : std::initializer_list<std::pair<float, const char*>> {
             { 20.0f, "20" }, { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" }, { 500.0f, "500" }, { 1000.0f, "1k" },
             { 2000.0f, "2k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" }, { 20000.0f, "20k Hz" } })
    {
        auto x = xForHz (hz);
        auto w = hz >= 20000.0f ? 70.0f : 50.0f;
        auto area = juce::Rectangle<float> (x - w * 0.5f, graph.getBottom() + 10.0f, w, 20.0f);
        if (hz >= 20000.0f) area = area.withX (x - 30.0f);
        g.drawText (text, area, juce::Justification::centred);
    }

    auto f = current();
    auto lo = f.isActive() ? f.lowHz : dsp::FocusSettings::minHz;
    auto hi = f.isActive() ? f.highHz : dsp::FocusSettings::maxHz;
    auto xl = xForHz (lo), xh = xForHz (hi);

    // whole spectrum, dim
    drawSpectrum (g, graph, false);

    // focused regions glow
    juce::RectangleList<float> focus;
    if (! f.isActive() || ! f.invert)
        focus.add ({ xl, graph.getY(), xh - xl, graph.getHeight() });
    else
    {
        focus.add ({ graph.getX(), graph.getY(), xl - graph.getX(), graph.getHeight() });
        focus.add ({ xh, graph.getY(), graph.getRight() - xh, graph.getHeight() });
    }

    if (nebula.isNull() && graph.getWidth() > 10)
        nebula = NebulaBackground::renderNebula ((int) graph.getWidth() / 3, (int) graph.getHeight() / 3, p, 41, -0.35f, 1.8f);

    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        for (auto& r : focus)
            clip.addRectangle (r);
        g.reduceClipRegion (clip);

        auto tint = p.isLight ? juce::Colours::white : p.accent;
        g.setColour (tint.withAlpha (p.isLight ? 0.18f : 0.14f));
        g.fillRect (graph);

        drawSpectrum (g, graph, true);

        // the nebula floats over the band, in front of the spectrum
        auto drift = (float) std::sin (time * 0.25) * 24.0f;
        g.setOpacity (0.85f + 0.15f * (float) std::sin (time * 0.7));
        g.drawImage (nebula, graph.translated (drift, 0).expanded (30.0f, 0), juce::RectanglePlacement::stretchToFit);
        g.setOpacity (1.0f);
        for (auto& r : focus)
        {
            auto glowC = juce::Point<float> (r.getCentreX(), graph.getY() + graph.getHeight() * 0.55f);
            Glass::drawGlow (g, glowC, juce::jmax (r.getWidth(), 80.0f) * 0.7f, p.isLight ? juce::Colours::white : p.nebulaC,
                             0.18f + 0.25f * analyser.getLevel (hzForX (r.getCentreX())));
        }

        for (auto& s : sparks)
        {
            auto x = graph.getX() + s.x * graph.getWidth();
            auto y = graph.getY() + s.y * graph.getHeight();
            g.setColour ((p.isLight ? juce::Colours::white : p.nebulaC).withAlpha (juce::jlimit (0.0f, 1.0f, s.life) * 0.9f));
            g.fillEllipse (x - s.size * 0.5f, y - s.size * 0.5f, s.size, s.size);
        }

        for (auto& r : focus)
            Glass::drawRectGlow (g, r, 2.0f, tint, 4.0f, 0.4f);
    }

    if (f.isActive() || hover != Drag::none || dragging != Drag::none)
    {
        auto hotLow = hover == Drag::low || dragging == Drag::low || dragging == Drag::both;
        auto hotHigh = hover == Drag::high || dragging == Drag::high || dragging == Drag::both;
        drawHandle (g, xl, "LOW CUT", lo, hotLow);
        drawHandle (g, xh, "HIGH CUT", hi, hotHigh);
    }
    else
    {
        g.setColour (p.isLight ? p.text : p.textDim);
        g.setFont (Fonts::regular (14.0f));
        g.drawText ("FULL: the effect works on the whole sound. Drag from the edges or pick LOW / MID / HIGH.",
                    panel.withHeight (60.0f).translated (0, 6.0f), juce::Justification::centred);
    }
}

// ------------------------------------------------------------------ mouse

FrequencyFocusView::Drag FrequencyFocusView::dragTargetAt (juce::Point<float> pos) const
{
    if (! graph.expanded (12.0f, 40.0f).contains (pos))
        return Drag::none;

    auto f = current();
    auto lo = f.isActive() ? f.lowHz : dsp::FocusSettings::minHz;
    auto hi = f.isActive() ? f.highHz : dsp::FocusSettings::maxHz;
    auto xl = xForHz (lo), xh = xForHz (hi);
    auto dl = std::abs (pos.x - xl), dh = std::abs (pos.x - xh);

    if (juce::jmin (dl, dh) < 12.0f)
        return dl <= dh ? Drag::low : Drag::high;
    if (f.isActive() && pos.x > xl && pos.x < xh)
        return Drag::both;
    return Drag::none;
}

void FrequencyFocusView::mouseMove (const juce::MouseEvent& e)
{
    auto h = dragTargetAt (e.position);
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h == Drag::both ? juce::MouseCursor::DraggingHandCursor
                        : h != Drag::none ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void FrequencyFocusView::mouseDown (const juce::MouseEvent& e)
{
    dragging = dragTargetAt (e.position);
    if (dragging == Drag::none)
        return;

    auto f = current();
    dragStartLo = f.isActive() ? f.lowHz : dsp::FocusSettings::minHz;
    dragStartHi = f.isActive() ? f.highHz : dsp::FocusSettings::maxHz;
    dragStartX = e.position.x;
    for (auto* p : { bandParam, lowParam, highParam })
        p->beginChangeGesture();
}

void FrequencyFocusView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == Drag::low)
        setCutoffs (hzForX (e.position.x), dragStartHi);
    else if (dragging == Drag::high)
        setCutoffs (dragStartLo, hzForX (e.position.x));
    else if (dragging == Drag::both)
    {
        // slide the band, keeping its width (in octaves)
        auto shift = (e.position.x - dragStartX) / graph.getWidth() * logSpan;
        auto lo = dragStartLo * std::exp (shift), hi = dragStartHi * std::exp (shift);
        if (lo < dsp::FocusSettings::minHz) { hi *= dsp::FocusSettings::minHz / lo; lo = dsp::FocusSettings::minHz; }
        if (hi > dsp::FocusSettings::maxHz) { lo *= dsp::FocusSettings::maxHz / hi; hi = dsp::FocusSettings::maxHz; }
        setCutoffs (lo, hi);
    }
}

void FrequencyFocusView::mouseUp (const juce::MouseEvent&)
{
    if (dragging != Drag::none)
        for (auto* p : { bandParam, lowParam, highParam })
            p->endChangeGesture();
    dragging = Drag::none;
    repaint();
}

void FrequencyFocusView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (graph.contains (e.position))
        setBand (dsp::FocusSettings::full);
}

} // namespace lt::ui
