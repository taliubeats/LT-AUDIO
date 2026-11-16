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

    setOpaque (true);
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

// ------------------------------------------------------------------ look (dark = black/purple, light = sky blue/yellow)

namespace
{
    struct Look
    {
        juce::Colour bgTop, bgBottom, frame, panel, panelEdge, graphBg, grid, gridMajor, spectrum,
                     focusTop, focusBottom, glow, glowSoft, edge, handle, label, value, title, sparkA, sparkB;
        bool light = false;
    };

    Look lookFor (const Palette& p)
    {
        Look l;
        l.light = p.isLight;
        if (p.isLight)
        {
            l.bgTop = juce::Colour (0xff55c6f8);   l.bgBottom = juce::Colour (0xff47bdf2);
            l.frame = juce::Colours::white.withAlpha (0.75f);
            l.panel = juce::Colour (0xff4fc3f7);   l.panelEdge = juce::Colours::white.withAlpha (0.7f);
            l.graphBg = juce::Colour (0xff4fc3f7);
            l.grid = juce::Colours::white.withAlpha (0.28f);  l.gridMajor = juce::Colours::white.withAlpha (0.5f);
            l.spectrum = juce::Colours::white.withAlpha (0.55f);
            l.focusTop = juce::Colour (0xfffff176); l.focusBottom = juce::Colour (0xffffd60a);
            l.glow = juce::Colour (0xffffdd00);     l.glowSoft = juce::Colour (0xfffffbe0);
            l.edge = juce::Colour (0xffffe55c);
            l.handle = juce::Colours::white.withAlpha (0.9f);
            l.label = juce::Colour (0xff111111);    l.value = juce::Colour (0xff111111);
            l.title = juce::Colour (0xffffd60a);
            l.sparkA = juce::Colours::white;        l.sparkB = juce::Colour (0xfffff9c4);
        }
        else
        {
            l.bgTop = juce::Colour (0xff1d1d22);   l.bgBottom = juce::Colour (0xff121216);
            l.frame = juce::Colour (0xff2e2e35);
            l.panel = juce::Colour (0xff151519);   l.panelEdge = juce::Colour (0xff2b2b32);
            l.graphBg = juce::Colour (0xff0c0c0f);
            l.grid = juce::Colours::white.withAlpha (0.06f);  l.gridMajor = juce::Colours::white.withAlpha (0.12f);
            l.spectrum = juce::Colour (0xff5d5c64);
            l.focusTop = juce::Colour (0xfff2b6ff); l.focusBottom = juce::Colour (0xff9a4dff);
            l.glow = juce::Colour (0xff9442ff);     l.glowSoft = juce::Colour (0xffd88cff);
            l.edge = juce::Colour (0xffb07bff);
            l.handle = juce::Colour (0xff9796a2).withAlpha (0.6f);
            l.label = juce::Colour (0xffa9a7b3);    l.value = juce::Colours::white;
            l.title = juce::Colours::white;
            l.sparkA = juce::Colour (0xffffc9f2);   l.sparkB = juce::Colours::white;
        }
        return l;
    }

    void drawScrew (juce::Graphics& g, juce::Point<float> c, const Look& l)
    {
        auto r = juce::Rectangle<float> (9.0f, 9.0f).withCentre (c);
        g.setColour (l.light ? juce::Colours::white.withAlpha (0.25f) : juce::Colour (0xff23232a));
        g.fillEllipse (r);
        g.setColour (l.light ? juce::Colours::white.withAlpha (0.7f) : juce::Colour (0xff3a3a43));
        g.drawEllipse (r, 1.0f);
        g.drawLine (c.x - 2.5f, c.y - 2.5f, c.x + 2.5f, c.y + 2.5f, 1.0f);
    }
}

// ------------------------------------------------------------------ buttons

void FrequencyFocusView::FocusLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                                                 bool hover, bool down)
{
    auto l = lookFor (getPalette());
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    auto on = b.getToggleState();

    if (l.light)
    {
        if (on)
        {
            Glass::drawRectGlow (g, r, 6.0f, juce::Colours::white, 10.0f, 0.9f);
            g.setColour (juce::Colours::white);
            g.fillRoundedRectangle (r, 6.0f);
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (hover ? 0.18f : 0.06f));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (juce::Colour (0xff0d3a52).withAlpha (0.85f));
            g.drawRoundedRectangle (r.reduced (0.7f), 6.0f, 1.4f);
        }
    }
    else
    {
        if (on)
        {
            Glass::drawRectGlow (g, r, 6.0f, l.glow, 12.0f, 0.75f);
            juce::ColourGradient fill (juce::Colour (0xff3d2566), r.getX(), r.getY(), juce::Colour (0xff24163f), r.getX(), r.getBottom(), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (l.edge);
            g.drawRoundedRectangle (r.reduced (0.8f), 6.0f, 1.8f);
        }
        else
        {
            juce::ColourGradient fill (juce::Colour (hover ? 0xff2d2d34 : 0xff26262c), r.getX(), r.getY(),
                                       juce::Colour (0xff1a1a1f), r.getX(), r.getBottom(), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (juce::Colour (0xff36363e));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawHorizontalLine ((int) r.getY() + 2, r.getX() + 6.0f, r.getRight() - 6.0f);
        }
    }

    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (r, 6.0f);
    }
}

void FrequencyFocusView::FocusLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool hover, bool)
{
    auto l = lookFor (getPalette());
    auto on = b.getToggleState();
    g.setFont (Fonts::regular (juce::jmin (20.0f, (float) b.getHeight() * 0.42f)));
    g.setColour (l.light ? juce::Colour (0xff111111)
                         : (on ? juce::Colours::white : juce::Colour (0xff8f8d99).brighter (hover ? 0.3f : 0.0f)));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

void FrequencyFocusView::FocusLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool)
{
    auto l = lookFor (getPalette());
    auto r = b.getLocalBounds().toFloat();
    auto sw = juce::Rectangle<float> (r.getRight() - 58.0f, r.getCentreY() - 15.0f, 56.0f, 30.0f);
    auto on = b.getToggleState();

    if (l.light)
    {
        g.setColour (on ? juce::Colours::white : juce::Colour (0xff111111));
        g.fillRoundedRectangle (sw, 15.0f);
        if (on) Glass::drawRectGlow (g, sw, 15.0f, juce::Colours::white, 6.0f, 0.6f);
        auto knob = (on ? sw.withLeft (sw.getRight() - 30.0f) : sw.withWidth (30.0f)).reduced (4.0f);
        g.setColour (on ? juce::Colours::white : juce::Colour (0xff4fc3f7));
        g.fillEllipse (knob);
        g.setColour (juce::Colour (0xff0d3a52).withAlpha (on ? 0.25f : 0.0f));
        g.drawEllipse (knob, 1.0f);
    }
    else
    {
        if (on)
        {
            Glass::drawRectGlow (g, sw, 15.0f, l.glow, 8.0f, 0.7f);
            juce::ColourGradient fill (juce::Colour (0xff9b5cff), sw.getX(), sw.getY(), juce::Colour (0xff6b2ee0), sw.getX(), sw.getBottom(), false);
            g.setGradientFill (fill);
        }
        else
            g.setColour (juce::Colour (0xff0f0f12));
        g.fillRoundedRectangle (sw, 15.0f);
        g.setColour (on ? l.edge : juce::Colour (0xff2e2e36));
        g.drawRoundedRectangle (sw.reduced (0.5f), 15.0f, 1.0f);

        auto knob = (on ? sw.withLeft (sw.getRight() - 30.0f) : sw.withWidth (30.0f)).reduced (4.0f);
        if (on) Glass::drawGlow (g, knob.getCentre(), 14.0f, juce::Colour (0xffe3ccff), 0.6f);
        g.setColour (on ? juce::Colour (0xffe9dcff) : juce::Colour (0xff34343c));
        g.fillEllipse (knob);
    }

    g.setColour (l.light ? juce::Colour (0xff111111) : juce::Colour (0xff9a98a4).brighter (hover ? 0.3f : 0.0f));
    g.setFont (Fonts::regular (19.0f));
    g.drawText (b.getButtonText(), r.withRight (sw.getX() - 12.0f), juce::Justification::centredRight);
}

// ------------------------------------------------------------------ layout

void FrequencyFocusView::resized()
{
    auto b = getLocalBounds().toFloat();
    backButton.setBounds ((int) b.getRight() - 30 - 110, 26, 110, 38);

    panel = { 10.0f, 92.0f, b.getWidth() - 20.0f, b.getHeight() - 92.0f - 90.0f };
    graph = panel.withTrimmedTop (62.0f).withTrimmedBottom (48.0f).reduced (50.0f, 0.0f);
    bottomBar = { 10.0f, b.getHeight() - 82.0f, b.getWidth() - 20.0f, 72.0f };

    auto row = bottomBar.toNearestInt().reduced (24, 11);
    for (auto& bb : bandButtons)
    {
        bb.setBounds (row.removeFromLeft (140));
        row.removeFromLeft (14);
    }
    soloButton.setBounds (row.removeFromRight (124));
    row.removeFromRight (18);
    invertButton.setBounds (row.removeFromRight (140));
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

    auto births = (int) (2.0f + energy * 8.0f + random.nextFloat());
    for (int i = 0; i < births && sparks.size() < 220; ++i)
    {
        float u;
        if (! f.invert || ! f.isActive())
            u = juce::jmap (random.nextFloat(), u0, u1);
        else
            u = random.nextBool() ? random.nextFloat() * u0 : juce::jmap (random.nextFloat(), u1, 1.0f);
        sparks.push_back ({ u, 0.05f + random.nextFloat() * 0.95f,
                            (random.nextFloat() - 0.5f) * 0.0012f, -0.0008f - random.nextFloat() * 0.0035f * (0.4f + energy),
                            0.6f + random.nextFloat() * 0.4f, 0.8f + random.nextFloat() * 1.9f, random.nextFloat() < 0.4f });
    }

    for (auto& s : sparks)
    {
        s.x += s.vx;
        s.y += s.vy;
        s.life -= 0.008f + 0.008f * (1.0f - energy);
    }
    sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [] (const Spark& s) { return s.life <= 0 || s.y < 0; }), sparks.end());

    repaint (panel.toNearestInt().expanded (4));
}

// ------------------------------------------------------------------ painting

juce::Path FrequencyFocusView::spectrumPath (juce::Rectangle<float> r) const
{
    juce::Path p;
    p.startNewSubPath (r.getX(), r.getBottom());
    for (float x = r.getX(); x <= r.getRight(); x += 1.5f)
    {
        auto hz = hzForX (x);
        auto db = analyser.getLevel (hz) * 90.0f - 90.0f + 4.5f * std::log2 (hz / 1000.0f);   // pink-noise tilt
        auto level = juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 70.0f);
        p.lineTo (x, r.getBottom() - level * r.getHeight() * 0.95f);
    }
    p.lineTo (r.getRight(), r.getBottom());
    p.closeSubPath();
    return p;
}

void FrequencyFocusView::drawGlowBlobs (juce::Graphics& g, juce::Rectangle<float> region, float energy)
{
    auto l = lookFor (palette);
    auto w = juce::jmax (region.getWidth(), 70.0f);
    auto t = (float) time;
    auto breathe = 0.85f + 0.15f * std::sin (t * 1.3f) + 0.35f * energy;

    struct Blob { float u, v, size, alpha, speed, phase; };
    static constexpr Blob blobs[] = {
        { 0.35f, 0.45f, 0.62f, 0.55f, 0.37f, 0.0f }, { 0.65f, 0.38f, 0.55f, 0.45f, 0.29f, 1.7f },
        { 0.50f, 0.62f, 0.70f, 0.50f, 0.23f, 3.1f }, { 0.28f, 0.70f, 0.45f, 0.35f, 0.41f, 4.4f },
        { 0.75f, 0.58f, 0.50f, 0.40f, 0.33f, 5.2f }, { 1.20f, 0.55f, 0.42f, 0.28f, 0.27f, 2.4f },   // bleeds out to the right
    };

    for (auto& b : blobs)
    {
        auto cx = region.getX() + w * (b.u + 0.06f * std::sin (t * b.speed + b.phase));
        auto cy = graph.getY() + graph.getHeight() * (b.v + 0.05f * std::cos (t * b.speed * 1.3f + b.phase));
        auto rad = w * b.size;
        auto k = l.light ? 1.25f : 1.05f;
        Glass::drawGlow (g, { cx, cy }, rad, l.glow, juce::jmin (1.0f, b.alpha * k * breathe));
        Glass::drawGlow (g, { cx, cy }, rad * 0.5f, l.glowSoft, juce::jmin (1.0f, b.alpha * (l.light ? 0.9f : 0.45f) * breathe));
    }
}

void FrequencyFocusView::drawHandle (juce::Graphics& g, float x, const juce::String& title, float hz, bool hot)
{
    auto l = lookFor (palette);
    auto bar = juce::Rectangle<float> (x - 6.5f, graph.getY() - 3.0f, 13.0f, graph.getHeight() + 6.0f);
    if (hot)
        Glass::drawRectGlow (g, bar, 6.0f, l.glow, 9.0f, 0.9f);
    juce::ColourGradient fill (l.handle.brighter (hot ? 0.4f : 0.15f), bar.getX(), 0, l.handle, bar.getRight(), 0, false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (bar, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (l.light ? 0.9f : 0.35f));
    g.drawRoundedRectangle (bar.reduced (0.5f), 6.0f, 1.0f);

    auto label = juce::Rectangle<float> (x - 80.0f, graph.getY() - 58.0f, 160.0f, 24.0f);
    g.setColour (l.label);
    g.setFont (Fonts::regular (16.0f));
    g.drawText (title, label, juce::Justification::centred);
    g.setColour (l.value);
    g.setFont (Fonts::regular (17.0f));
    g.drawText (hzText (hz), label.translated (0, 23.0f), juce::Justification::centred);
}

void FrequencyFocusView::paint (juce::Graphics& g)
{
    auto l = lookFor (palette);
    auto bounds = getLocalBounds().toFloat();

    // window: solid, like a hardware unit
    juce::ColourGradient bg (l.bgTop, 0, 0, l.bgBottom, 0, bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillAll();
    g.setColour (l.frame);
    g.drawRoundedRectangle (bounds.reduced (1.5f), 8.0f, 2.0f);
    g.drawRoundedRectangle ({ 10.0f, 8.0f, bounds.getWidth() - 20.0f, 78.0f }, 6.0f, 1.2f);

    // title
    auto titleArea = juce::Rectangle<float> (40.0f, 22.0f, 760.0f, 46.0f);
    auto used = Logo::draw (g, titleArea, Logo::specFor (productId), palette, juce::Justification::centredLeft);
    auto titleRect = titleArea.withTrimmedLeft (used + 32.0f);
    if (l.light)
    {
        g.setColour (juce::Colour (0xff1a7fae).withAlpha (0.35f));
        Fonts::drawSpaced (g, "FREQUENCY FOCUS", titleRect.translated (1.5f, 2.0f), Fonts::semiBold (38.0f), 0.04f, juce::Justification::centredLeft);
    }
    g.setColour (l.title);
    Fonts::drawSpaced (g, "FREQUENCY FOCUS", titleRect, Fonts::semiBold (38.0f), 0.04f, juce::Justification::centredLeft);

    // panels
    for (auto r : { panel, bottomBar })
    {
        g.setColour (l.panel);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (l.panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, l.light ? 1.6f : 1.2f);
    }
    for (auto c : { panel.getTopLeft().translated (14, 14), panel.getTopRight().translated (-14, 14),
                    panel.getBottomLeft().translated (14, -14), panel.getBottomRight().translated (-14, -14) })
        drawScrew (g, c, l);

    // graph background + grid
    g.setColour (l.graphBg);
    g.fillRoundedRectangle (graph, 4.0f);
    for (int decade = 10; decade <= 10000; decade *= 10)
        for (int m = 1; m <= 9; ++m)
        {
            auto hz = (float) (decade * m);
            if (hz <= 20.0f || hz >= 20000.0f) continue;
            g.setColour (m == 1 || m == 2 || m == 5 ? l.gridMajor : l.grid);
            g.drawVerticalLine ((int) xForHz (hz), graph.getY(), graph.getBottom());
        }
    for (int i = 1; i < 4; ++i)
    {
        g.setColour (l.grid);
        g.drawHorizontalLine ((int) (graph.getY() + graph.getHeight() * (float) i / 4.0f), graph.getX(), graph.getRight());
    }

    // frequency labels
    g.setFont (Fonts::regular (17.0f));
    g.setColour (l.label);
    for (auto [hz, text] : std::initializer_list<std::pair<float, const char*>> {
             { 20.0f, "20" }, { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" }, { 500.0f, "500" }, { 1000.0f, "1k" },
             { 2000.0f, "2k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" }, { 20000.0f, "20k Hz" } })
    {
        auto x = xForHz (hz);
        auto area = juce::Rectangle<float> (x - 30.0f, graph.getBottom() + 12.0f, 60.0f, 22.0f);
        if (hz >= 20000.0f) area = juce::Rectangle<float> (x - 40.0f, area.getY(), 90.0f, 22.0f);
        g.drawText (text, area, juce::Justification::centred);
    }

    auto f = current();
    auto lo = f.isActive() ? f.lowHz : dsp::FocusSettings::minHz;
    auto hi = f.isActive() ? f.highHz : dsp::FocusSettings::maxHz;
    auto xl = xForHz (lo), xh = xForHz (hi);

    juce::Array<juce::Rectangle<float>> focus;
    if (! f.isActive() || ! f.invert)
        focus.add ({ xl, graph.getY(), xh - xl, graph.getHeight() });
    else
    {
        focus.add ({ graph.getX(), graph.getY(), xl - graph.getX(), graph.getHeight() });
        focus.add ({ xh, graph.getY(), graph.getRight() - xh, graph.getHeight() });
    }

    auto spectrum = spectrumPath (graph);
    auto energy = analyser.getLevel (std::sqrt (lo * hi));

    {
    juce::Graphics::ScopedSaveState clipToGraph (g);
    g.reduceClipRegion (graph.toNearestInt());

    // the whole spectrum, dim
    g.setColour (l.spectrum);
    g.fillPath (spectrum);

    for (auto& r : focus)
    {
        // a soft tint, and the spectrum lights up just past the edges too
        g.setColour ((l.light ? l.glowSoft : l.glow).withAlpha (l.light ? 0.14f : 0.12f));
        g.fillRect (r);

        for (auto side : { -1, 1 })
        {
            auto edgeX = side < 0 ? r.getX() : r.getRight();
            auto bleed = juce::Rectangle<float> (side < 0 ? edgeX - 60.0f : edgeX, graph.getY(), 60.0f, graph.getHeight());
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (bleed.toNearestInt());
            juce::ColourGradient fade (l.focusBottom.withAlpha (0.8f), edgeX, 0,
                                       l.focusBottom.withAlpha (0.0f), edgeX + side * 60.0f, 0, false);
            g.setGradientFill (fade);
            g.fillPath (spectrum);
        }

        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.toNearestInt());
        juce::ColourGradient grad (l.focusTop, 0, graph.getY() + graph.getHeight() * 0.35f, l.focusBottom, 0, graph.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (spectrum);
    }

    // glowing nebula over the band
    for (auto& r : focus)
        drawGlowBlobs (g, r, energy);

    for (auto& r : focus)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.expanded (24.0f, 0).toNearestInt());
        for (auto& s : sparks)
        {
            auto x = graph.getX() + s.x * graph.getWidth();
            auto y = graph.getY() + s.y * graph.getHeight();
            auto a = juce::jlimit (0.0f, 1.0f, s.life);
            g.setColour ((s.white ? l.sparkB : l.sparkA).withAlpha (a * 0.95f));
            g.fillEllipse (x - s.size * 0.5f, y - s.size * 0.5f, s.size, s.size * (0.7f + 0.6f * (float) (((int) (s.x * 997.0f)) % 2)));
        }

        // bright edges top and bottom
        for (auto y : { r.getY(), r.getBottom() })
        {
            auto line = juce::Rectangle<float> (r.getX(), y - 1.5f, r.getWidth(), 3.0f);
            Glass::drawRectGlow (g, line, 1.5f, l.glow, 7.0f, 0.9f);
            g.setColour (l.edge);
            g.fillRect (line);
        }
    }

    g.setColour (l.light ? juce::Colours::white.withAlpha (0.55f) : juce::Colour (0xff2c2c33));
    g.drawRoundedRectangle (graph, 4.0f, 1.0f);

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
        g.setColour (l.label);
        g.setFont (Fonts::regular (15.0f));
        g.drawText ("FULL: the effect works on the whole sound. Drag from the edges or pick LOW / MID / HIGH.",
                    panel.withHeight (56.0f).translated (0, 4.0f), juce::Justification::centred);
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
