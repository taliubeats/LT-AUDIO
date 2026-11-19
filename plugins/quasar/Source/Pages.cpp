#include "Pages.h"

namespace quasar::ui
{

namespace
{
    const int oscX[3] = { 135, 385, 634 };

    juce::String pctText (float v) { return juce::String (juce::roundToInt (v)) + "%"; }

    void setGestureParam (Processor& p, const juce::String& id, float realValue) { p.setParam (id, realValue); }

    /** dB marks of the level faders (75 % = 0 dB). */
    float levelForDb (float db) { return std::pow (dsp::dbToGain (db), 1.0f / 2.40942f) * 75.0f; }
}

Knob& Page::knob (const juce::String& id, const juce::String& label, juce::Point<int> c, float d, float labelSize)
{
    auto& k = make<Knob> (proc, id, label, d);
    k.labelSize = labelSize;
    auto w = (int) juce::jmax (d + 26.0f, 64.0f), h = (int) d + 24;
    k.setBounds (c.x - w / 2, c.y - (int) (d * 0.5f) - 2, w, h);
    k.bind (bindings);
    return k;
}

// ================================================================== power

PowerButton::PowerButton (juce::RangedAudioParameter& p) : param (p)
{
    att = std::make_unique<juce::ParameterAttachment> (param, [this] (float v) { on = v > 0.5f; repaint(); });
    att->sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}
PowerButton::~PowerButton() { att.reset(); }

void PowerButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    auto c = r.getCentre();
    if (on) Glass::drawGlow (g, c, r.getWidth(), Colours::accent, 0.7f);
    juce::Path p;
    auto rad = r.getWidth() * 0.4f;
    p.addCentredArc (c.x, c.y, rad, rad, 0, 0.7f, juce::MathConstants<float>::twoPi - 0.7f, true);
    p.startNewSubPath (c.x, c.y - rad - 1); p.lineTo (c.x, c.y - 1);
    g.setColour (on ? juce::Colour (0xffd9c4ff) : Colours::faint);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void PowerButton::mouseDown (const juce::MouseEvent&) { att->setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

// ================================================================== OSC page

OscPage::OscPage (Processor& p) : Page (p)
{
    // SUB
    make<WaveView> (p, 3).setBounds (28, 135, 84, 140);
    make<PowerButton> (*p.param (IDs::subOn)).setBounds (94, 106, 18, 18);
    knob (IDs::subOct, "OCT", { 70, 330 }, 40);
    knob (IDs::subLevel, "LEVEL", { 70, 410 }, 40);

    for (int o = 0; o < numOscs; ++o)
    {
        auto x = oscX[o];
        make<PowerButton> (*p.param (IDs::osc (o, "on"))).setBounds (x + 210, 106, 18, 18);
        auto& warp = make<Dropdown> (*p.param (IDs::osc (o, "warpmode")), Choices::warpModes());
        warp.setBounds (x + 120, 104, 84, 21); warp.fontSize = 11.5f;
        make<TextTabs> (Choices::oscTypes(), p.param (IDs::osc (o, "type"))).setBounds (x + 12, 138, 212, 20);
        make<WaveView> (p, o).setBounds (x + 15, 164, 207, 104);
        const char* boxes[] = { "oct", "semi", "fine", "phase", "rand" };
        const char* labels[] = { "OCT", "SEM", "FIN", "PHS", "RND" };
        const float steps[] = { 1, 1, 1, 15, 5 };
        for (int b = 0; b < 5; ++b)
        {
            auto& vb = make<ValueBox> (*p.param (IDs::osc (o, boxes[b])), labels[b], steps[b]);
            vb.setBounds (x + 15 + b * 42, 273, 37, 34);
            if (b == 3) vb.format = [] (float v) { return juce::String (juce::roundToInt (v)); };
            if (b == 4) vb.format = pctText;
        }
        const int kx[] = { 35, 90, 146, 201 };
        knob (IDs::osc (o, "wtpos"), "WT POS", { x + kx[0], 338 }, 36);
        knob (IDs::osc (o, "unison"), "UNISON", { x + kx[1], 338 }, 36);
        knob (IDs::osc (o, "detune"), "DETUNE", { x + kx[2], 338 }, 36);
        knob (IDs::osc (o, "warp"), "WARP", { x + kx[3], 338 }, 36);
        knob (IDs::osc (o, "blend"), "BLEND", { x + kx[0], 410 }, 36);
        knob (IDs::osc (o, "spread"), "SPREAD", { x + kx[1], 410 }, 36);
        knob (IDs::osc (o, "pan"), "PAN", { x + kx[2], 410 }, 36).bipolar = true;
        knob (IDs::osc (o, "level"), "LEVEL", { x + kx[3], 410 }, 36);
    }

    // NOISE
    make<PowerButton> (*p.param (IDs::noiseOn)).setBounds (952, 106, 18, 18);
    make<WaveView> (p, 4).setBounds (894, 135, 70, 140);
    knob (IDs::noiseColor, "COLOR", { 929, 330 }, 38).bipolar = true;
    knob (IDs::noiseLevel, "LEVEL", { 929, 410 }, 38);

    // FILTERS
    for (int f = 0; f < numFilters; ++f)
    {
        auto y = f == 0 ? 0 : 185;
        make<PowerButton> (*p.param (IDs::flt (f, "on"))).setBounds (995, 136 + y, 18, 18);
        auto& dd = make<Dropdown> (*p.param (IDs::flt (f, "type")), Choices::filterTypes());
        dd.setBounds (1082, 135 + y, 92, 21); dd.fontSize = 11.5f;
        make<FilterView> (p, f).setBounds (997, 162 + y, 177, 78);
        const char* ids[] = { "cutoff", "reso", "drive", "key", "mix" };
        const char* names[] = { "CUTOFF", "RES", "DRIVE", "KEY", "MIX" };
        for (int k = 0; k < 5; ++k) knob (IDs::flt (f, ids[k]), names[k], { 1013 + k * 36, 270 + y }, 27, 10.0f);
    }
    startTimerHz (24);
}

void OscPage::paintPanels (juce::Graphics& g)
{
    drawPanel (g, { 18, 98, 104, 360 });
    drawTitle (g, "SUB", { 28, 104, 70, 24 });
    for (int o = 0; o < numOscs; ++o)
    {
        auto x = (float) oscX[o];
        drawPanel (g, { x, 98, 237, 360 });
        drawTitle (g, juce::String ("OSC ") + "ABC"[o], { x + 11, 104, 100, 24 });
        auto on = proc.state.getRawParameterValue (IDs::osc (o, "on"))->load() > 0.5f;
        if (! on) { g.setColour (juce::Colour (0x66000000)); g.fillRoundedRectangle ({ x + 1, 130, 235, 327 }, 8.0f); }
    }
    drawPanel (g, { 884, 98, 90, 360 });
    drawTitle (g, "NOISE", { 894, 104, 60, 24 });
    drawPanel (g, { 987, 98, 195, 399 });
    drawTitle (g, "FILTERS", { 997, 104, 120, 24 });
    for (int f = 0; f < numFilters; ++f)
        drawTitle (g, "FILTER " + juce::String (f + 1), { 1016, 133.0f + (float) f * 185.0f, 70, 24 }, 13.5f);
}

// ================================================================== bottom panels

BottomPanels::BottomPanels (Processor& p) : Page (p)
{
    // MACROS: knob + draggable name
    for (int m = 0; m < numMacros; ++m)
    {
        auto col = m / 4, row = m % 4;
        juce::Point<int> c { col == 0 ? 75 : 172, 524 + row * 52 };
        auto& k = knob (IDs::macro (m), "", c, 28);
        k.showLabel = false;
        auto* tab = macroLabels.add (new ModSourceTab (p.macroNames[m], srcMacro1 + m));
        tab->underline = false; tab->fontSize = 11.0f;
        tab->setBounds (c.x - 46, c.y + 16, 92, 16);
        tab->onDoubleClick = [this, m, tab]
        {
            auto* w = new juce::AlertWindow ("Rename macro", "", juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("name", proc.macroNames[m]);
            w->addButton ("OK", 1); w->addButton ("Cancel", 0);
            w->enterModalState (true, juce::ModalCallbackFunction::create ([this, m, w, tab] (int r)
            {
                if (r == 1) { proc.macroNames.set (m, w->getTextEditorContents ("name").toUpperCase().substring (0, 14)); tab->text = proc.macroNames[m]; tab->repaint(); }
            }), true);
        };
        addAndMakeVisible (tab);
    }

    // ENVELOPES
    const char* envNames[] = { "ENV 1", "ENV 2", "ENV 3", "PITCH ENV" };
    for (int e = 0; e < numEnvs; ++e)
    {
        auto* t = envTabs.add (new ModSourceTab (envNames[e], srcEnv1 + e));
        t->setBounds (258 + e * 88, 480, e == 3 ? 100 : 84, 26);
        t->onClick = [this, e] { showEnv (e); };
        addAndMakeVisible (t);
    }
    envView = &make<EnvView> (p);
    envView->setBounds (258, 512, 352, 142);
    const int ex[] = { 293, 362, 432, 501, 571 };
    for (int e = 0; e < numEnvs; ++e)
    {
        envKnobs[(size_t) e][0] = &knob (IDs::env (e, "att"), "ATK", { ex[0], 690 }, 32);
        envKnobs[(size_t) e][1] = e == 3 ? &knob (IDs::envPAmount, "AMOUNT", { ex[1], 690 }, 32) : &knob (IDs::env (e, "hold"), "HOLD", { ex[1], 690 }, 32);
        envKnobs[(size_t) e][2] = &knob (IDs::env (e, "dec"), "DEC", { ex[2], 690 }, 32);
        envKnobs[(size_t) e][3] = &knob (IDs::env (e, "sus"), "SUS", { ex[3], 690 }, 32);
        envKnobs[(size_t) e][4] = &knob (IDs::env (e, "rel"), "REL", { ex[4], 690 }, 32);
        if (e == 3) envKnobs[3][1]->bipolar = true;
    }

    // LFOs
    for (int l = 0; l < numLfos; ++l)
    {
        auto* t = lfoTabs.add (new ModSourceTab ("LFO " + juce::String (l + 1), srcLfo1 + l));
        t->setBounds (646 + l * 82, 480, 76, 26);
        t->onClick = [this, l] { showLfo (l); };
        addAndMakeVisible (t);

        auto& mode = make<TextTabs> (Choices::lfoModes(), p.param (IDs::lfo (l, "mode")));
        mode.boxed = true; mode.separators = false; mode.fontSize = 11.5f;
        mode.setBounds (650, 515, 186, 24);
        auto& sync = make<juce::TextButton> ("SYNC");
        sync.setClickingTogglesState (true);
        sync.getProperties().set ("fontSize", 11.5);
        sync.setBounds (842, 515, 52, 24);
        bindings.buttons.push_back (std::make_unique<juce::ButtonParameterAttachment> (*p.param (IDs::lfo (l, "sync")), sync, &p.undo));
        auto& div = make<Dropdown> (*p.param (IDs::lfo (l, "div")), Choices::divisions());
        div.setBounds (898, 515, 64, 24); div.fontSize = 11.5f;
        lfoButtons[(size_t) l] = { &mode, &sync, &div, nullptr, nullptr };
        const int lx[] = { 682, 748, 814, 880, 946 };
        lfoKnobs[(size_t) l][0] = &knob (IDs::lfo (l, "rate"), "RATE", { lx[0], 690 }, 32);
        lfoKnobs[(size_t) l][1] = &knob (IDs::lfo (l, "rise"), "RISE", { lx[1], 690 }, 32);
        lfoKnobs[(size_t) l][2] = &knob (IDs::lfo (l, "delay"), "DELAY", { lx[2], 690 }, 32);
        lfoKnobs[(size_t) l][3] = &knob (IDs::lfo (l, "smooth"), "SMOOTH", { lx[3], 690 }, 32);
        lfoKnobs[(size_t) l][4] = &knob (IDs::lfo (l, "phase"), "PHASE", { lx[4], 690 }, 32);
    }
    lfoView = &make<LfoView> (p);
    lfoView->setBounds (650, 545, 312, 110);
    showEnv (0); showLfo (0);
    startTimerHz (24);
}

void BottomPanels::showEnv (int e)
{
    env = e;
    for (int i = 0; i < numEnvs; ++i)
    {
        envTabs[i]->selected = i == e; envTabs[i]->repaint();
        for (auto* k : envKnobs[(size_t) i]) k->setVisible (i == e);
    }
    envView->setEnv (e);
}

void BottomPanels::showLfo (int l)
{
    lfo = l;
    for (int i = 0; i < numLfos; ++i)
    {
        lfoTabs[i]->selected = i == l; lfoTabs[i]->repaint();
        for (auto* k : lfoKnobs[(size_t) i]) k->setVisible (i == l);
        for (auto* c : lfoButtons[(size_t) i]) if (c != nullptr) c->setVisible (i == l);
    }
    lfoView->setLfo (l);
}

void BottomPanels::timerCallback()
{
    auto sync = proc.state.getRawParameterValue (IDs::lfo (lfo, "sync"))->load() > 0.5f;
    lfoKnobs[(size_t) lfo][0]->setEnabled (! sync);
    for (int m = 0; m < numMacros; ++m)
        if (macroLabels[m]->text != proc.macroNames[m]) { macroLabels[m]->text = proc.macroNames[m]; macroLabels[m]->repaint(); }
    repaint();
}

void BottomPanels::paintPanels (juce::Graphics& g)
{
    drawPanel (g, { 18, 474, 212, 258 });
    drawTitle (g, "MACROS", { 28, 480, 150, 24 });
    drawPanel (g, { 247, 474, 375, 258 });
    drawPanel (g, { 634, 474, 338, 258 });
    g.setColour (Colours::faint);
    g.drawHorizontalLine (507, 258, 610);
    g.drawHorizontalLine (507, 646, 962);
}

// ================================================================== voicing

VoicingPanel::VoicingPanel (Processor& p) : Page (p)
{
    auto& poly = make<ValueBox> (*p.param (IDs::poly), "", 1.0f);
    poly.labelBelow = false;
    poly.setBounds (1057, 558, 54, 28);
    for (int i = 0; i < 3; ++i)
    {
        auto* b = modes.add (new juce::TextButton (i == 0 ? "POLY 16" : Choices::voiceModes()[i]));
        b->getProperties().set ("fontSize", 12.0);
        b->setBounds (999 + i * 58, 600, 57, 27);
        b->onClick = [this, i] { proc.setParam (IDs::voiceMode, (float) i); };
        addAndMakeVisible (b);
    }
    knob (IDs::glide, "GLIDE", { 1084, 676 }, 34);
    startTimerHz (10);
}

void VoicingPanel::timerCallback()
{
    auto mode = (int) proc.state.getRawParameterValue (IDs::voiceMode)->load();
    auto poly = (int) proc.state.getRawParameterValue (IDs::poly)->load();
    for (int i = 0; i < 3; ++i) modes[i]->setToggleState (i == mode, juce::dontSendNotification);
    modes[0]->setButtonText ("POLY " + juce::String (poly));
}

void VoicingPanel::paintPanels (juce::Graphics& g)
{
    drawPanel (g, { 987, 513, 195, 219 });
    drawTitle (g, "VOICING", { 997, 519, 150, 24 });
}

// ================================================================== MIX

namespace
{
    const int stripX[10] = { 18, 142, 266, 390, 514, 638, 762, 886, 989, 1092 };
    int stripW (int i) { return i < 7 ? 104 : 90; }
}

MixPage::MixPage (Processor& p) : Page (p)
{
    for (int ch = 0; ch < numMixerChannels; ++ch)
    {
        auto x = stripX[ch], w = stripW (ch);
        auto names = ch == chFlt1 ? Choices::filterDests() : ch == chFlt2 ? juce::StringArray { "MAIN", "BUS 1", "BUS 2" } : Choices::sourceDests();
        auto& d = make<Dropdown> (*p.param (IDs::mixer (ch, "dest")), names);
        d.setBounds (x + 10, 166, w - 20, 21); d.fontSize = 11.0f;
        knob (IDs::mixer (ch, "send1"), "BUS 1", { x + 28, 226 }, 26, 10.0f);
        knob (IDs::mixer (ch, "send2"), "BUS 2", { x + 76, 226 }, 26, 10.0f);
        auto panId = ch == chSub ? juce::String (IDs::subPan) : ch == chNoise ? juce::String (IDs::noisePan) : IDs::mixer (ch, "pan");
        knob (panId, "PAN", { x + 52, 318 }, 34).bipolar = true;
        auto levelId = ch == chSub ? juce::String (IDs::subLevel) : ch == chNoise ? juce::String (IDs::noiseLevel) : IDs::mixer (ch, "level");
        auto& f = make<Fader>();
        f.setBounds (x + 28, 390, 34, 286);
        bindings.sliders.push_back (std::make_unique<juce::SliderParameterAttachment> (*p.param (levelId), f, &p.undo));
        f.setDoubleClickReturnValue (true, 75.0);
        make<Meter> ([this, ch] (int) { return proc.engine.getChannelLevel (ch); }, 1).setBounds (x + 70, 398, 12, 270);
        for (int b = 0; b < 2; ++b)
        {
            auto& btn = make<juce::TextButton> (b == 0 ? "M" : "S");
            btn.setClickingTogglesState (true);
            btn.setBounds (x + 12 + b * 44, 690, 36, 26);
            bindings.buttons.push_back (std::make_unique<juce::ButtonParameterAttachment> (*p.param (IDs::mixer (ch, b == 0 ? "mute" : "solo")), btn, &p.undo));
        }
    }
    const int busOrder[3] = { 1, 2, 0 };
    for (int k = 0; k < 3; ++k)
    {
        auto b = busOrder[k];
        auto x = stripX[7 + k], w = stripW (7 + k);
        for (int i = 0; i < 3; ++i)
        {
            auto* btn = insertButtons.add (new juce::TextButton());
            btn->getProperties().set ("fontSize", 10.0);
            btn->setBounds (x + 9, 192 + i * 28, w - 18, 23);
            btn->onClick = [this, b] { if (navigate) navigate ("FX", b); };
            addAndMakeVisible (btn);
        }
        knob (IDs::bus (b, "pan"), "PAN", { x + w / 2, 318 }, 34).bipolar = true;
        auto& f = make<Fader>();
        f.setBounds (x + 22, 390, 34, 286);
        bindings.sliders.push_back (std::make_unique<juce::SliderParameterAttachment> (*p.param (IDs::bus (b, "level")), f, &p.undo));
        f.setDoubleClickReturnValue (true, 75.0);
        make<Meter> ([this, b] (int c) { return proc.getBusLevel (b, c); }, 2).setBounds (x + 60, 398, 22, 270);
        for (int i = 0; i < 2; ++i)
        {
            auto& btn = make<juce::TextButton> (i == 0 ? "M" : "S");
            btn.setClickingTogglesState (true);
            btn.setBounds (x + 9 + i * 38, 690, 34, 26);
            bindings.buttons.push_back (std::make_unique<juce::ButtonParameterAttachment> (*p.param (IDs::bus (b, i == 0 ? "mute" : "solo")), btn, &p.undo));
        }
    }
    startTimerHz (8);
}

void MixPage::timerCallback()
{
    const int busOrder[3] = { 1, 2, 0 };
    for (int k = 0; k < 3; ++k)
    {
        juce::StringArray names;
        for (int s = 0; s < numFxSlots; ++s)
        {
            auto t = proc.chains[(size_t) busOrder[k]]->getType (s);
            if (t > 0) names.add (fxNames()[t]);
        }
        for (int i = 0; i < 3; ++i)
        {
            auto* b = insertButtons[k * 3 + i];
            b->setButtonText (i < names.size() ? (i == 2 && names.size() > 3 ? names[i] + " +" + juce::String (names.size() - 3) : names[i]) : juce::String());
            b->setAlpha (i < names.size() ? 1.0f : 0.35f);
        }
    }
    // sends only make sense for channels that do not feed a filter
    for (auto* c : owned)
        if (auto* k = dynamic_cast<Knob*> (c))
            if (k->paramId.endsWith ("_send1") || k->paramId.endsWith ("_send2"))
            {
                auto pre = k->paramId.upToLastOccurrenceOf ("_", false, false);
                auto dest = (int) proc.state.getRawParameterValue (pre + "_dest")->load();
                auto isFilter = pre.startsWith ("flt");
                k->setEnabled (isFilter ? (pre == "flt2" || dest != 0) : dest >= 3);
            }
    repaint();
}

void MixPage::paintPanels (juce::Graphics& g)
{
    const juce::StringArray names { "SUB", "OSC A", "OSC B", "OSC C", "NOISE", "FILTER 1", "FILTER 2", "BUS 1", "BUS 2", "MAIN" };
    for (int i = 0; i < 10; ++i)
    {
        auto x = (float) stripX[i], w = (float) stripW (i);
        auto r = juce::Rectangle<float> (x, 98, w, 634);
        if (i >= 5) { auto tint = i >= 7 ? juce::Colour (0x26b08cff) : juce::Colour (0x1a6d8cff); g.setColour (tint); g.fillRoundedRectangle (r, 10.0f); }
        drawPanel (g, r, 10.0f);
        drawTitle (g, names[i], { x, 104, w, 26 }, 16.0f, juce::Justification::centred);
        drawDisplay (g, { x + 6, 135, w - 12, 142 }, 6.0f);
        g.setColour (Colours::dim); g.setFont (Fonts::regular (10.5f));
        g.drawText (i < 7 ? "DEST" : "FX INSERT", juce::Rectangle<float> (x, 145, w, 16).toNearestInt(), juce::Justification::centred);
        // fader scale (dB)
        g.setFont (Fonts::regular (10.0f));
        for (float db : { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
        {
            auto pct = levelForDb (db);
            auto y = 390.0f + 9.0f + (286.0f - 18.0f) * (1.0f - pct / 100.0f);
            g.setColour (Colours::dim);
            g.drawText (db > 0 ? "+" + juce::String ((int) db) : juce::String ((int) db), juce::Rectangle<float> (x + 2, y - 6, 24, 12).toNearestInt(), juce::Justification::centredRight);
        }
    }
}

// ================================================================== FX module

FxModule::FxModule (Processor& p, int b, int s) : bus (b), slot (s), proc (p)
{
    power = std::make_unique<Switch> (*p.param (IDs::fx (b, s, "on")));
    addAndMakeVisible (*power);
    fader = std::make_unique<Fader>();
    addAndMakeVisible (*fader);
    bindings.sliders.push_back (std::make_unique<juce::SliderParameterAttachment> (*p.param (IDs::fx (b, s, "out")), *fader, &p.undo));
    fader->setDoubleClickReturnValue (true, 0.0);
    for (auto* btn : { &up, &down })
    {
        btn->getProperties().set ("textOnly", true);
        addAndMakeVisible (btn);
    }
    up.onClick = [this] { if (slot > 0) proc.chains[(size_t) bus]->move (slot, slot - 1); };
    down.onClick = [this] { if (slot < numFxSlots - 1 && proc.chains[(size_t) bus]->getType (slot + 1) > 0) proc.chains[(size_t) bus]->move (slot, slot + 1); };
    rebuild();
    startTimerHz (20);
}

FxModule::~FxModule() { bindings.clear(); }

void FxModule::rebuild()
{
    bindings.sliders.resize (1);   // keep the fader
    bindings.others.clear();
    bindings.buttons.clear();
    controls.clear();
    type = proc.chains[(size_t) bus]->getType (slot);
    auto& info = fxInfo (type);
    for (int i = 0; i < info.numParams; ++i)
    {
        auto& spec = info.params[(size_t) i];
        auto id = IDs::fxParam (bus, slot, i);
        if (spec.kind == FxParamSpec::choice)
        {
            auto* d = new Dropdown (*proc.param (id), spec.choices);
            d->fontSize = 11.5f;
            // the generic 0..1 parameter maps to the choices
            d->toIndex = [spec] (float v) { return (int) spec.toReal (v); };
            d->fromIndex = [spec] (int i) { return spec.toNorm ((float) i); };
            d->sendUpdate();
            controls.add (d); addAndMakeVisible (d);
        }
        else if (spec.kind == FxParamSpec::toggle)
        {
            auto* b = new juce::TextButton (spec.name);
            b->setClickingTogglesState (true);
            b->getProperties().set ("fontSize", 11.0);
            bindings.buttons.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.param (id), *b, &proc.undo));
            controls.add (b); addAndMakeVisible (b);
        }
        else
        {
            auto* k = new Knob (proc, id, spec.name, 34.0f);
            k->labelSize = 11.0f;
            k->bind (bindings);
            k->setDoubleClickReturnValue (true, spec.def);
            controls.add (k); addAndMakeVisible (k);
        }
    }
    auto* mix = new Knob (proc, IDs::fx (bus, slot, "mix"), "MIX", 40.0f);
    mix->bind (bindings);
    controls.add (mix); addAndMakeVisible (mix);
    resized();
}

void FxModule::resized()
{
    auto w = getWidth(), h = getHeight();
    power->setBounds (12, 14, 34, 18);
    up.setBounds (14, 44, 30, 26); down.setBounds (14, h - 74, 30, 26);
    fader->setBounds (w - 42, 16, 30, h - 32);
    if (controls.isEmpty()) return;
    // choices / toggles on the top line, knobs below, MIX knob on the right
    auto& info = fxInfo (type);
    int topX = 195, knobCount = 0;
    for (int i = 0; i < info.numParams; ++i) if (info.params[(size_t) i].kind == FxParamSpec::linear || info.params[(size_t) i].kind == FxParamSpec::log) ++knobCount;
    auto rows = knobCount > 5 ? 2 : 1;
    auto perRow = (knobCount + rows - 1) / juce::jmax (1, rows);
    auto knobArea = juce::Rectangle<int> (330, rows == 1 ? h / 2 - 30 : 26, w - 330 - 120, rows == 1 ? 74 : h - 40);
    int k = 0;
    for (int i = 0; i < info.numParams; ++i)
    {
        auto* c = controls[i];
        auto kind = info.params[(size_t) i].kind;
        if (kind == FxParamSpec::choice || kind == FxParamSpec::toggle)
        {
            auto bw = kind == FxParamSpec::choice ? 104 : 74;
            c->setBounds (topX + 140, 12, bw, 24);
            topX += bw + 8;
            if (topX + 140 + bw > w - 130) topX = 195;
        }
        else
        {
            auto row = k / juce::jmax (1, perRow), col = k % juce::jmax (1, perRow);
            auto cw = knobArea.getWidth() / juce::jmax (1, perRow);
            auto rh = knobArea.getHeight() / rows;
            c->setBounds (knobArea.getX() + col * cw, knobArea.getY() + row * rh + (rows == 1 ? 0 : 6), cw, juce::jmin (rh, 70));
            ++k;
        }
    }
    controls.getLast()->setBounds (w - 118, h / 2 - 32, 70, 70);
}

void FxModule::timerCallback()
{
    if (proc.chains[(size_t) bus]->getType (slot) != type) rebuild();
    repaint();
}

void FxModule::drawIcon (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto c = r.getCentre();
    auto col = juce::Colour (0xffc9a8ff);
    Glass::drawGlow (g, c, r.getWidth() * 0.6f, Colours::accent, 0.45f);
    g.setColour (col);
    juce::Path p;
    switch ((FxType) type)
    {
        case FxType::saturation: case FxType::distortion:
            p.addStar (c, 5, r.getWidth() * 0.18f, r.getWidth() * 0.42f, 0); g.strokePath (p, juce::PathStrokeType (1.6f)); break;
        case FxType::compressor:
            g.drawEllipse (r.withSizeKeepingCentre (r.getWidth() * 0.5f, r.getWidth() * 0.5f), 1.6f);
            p.addCentredArc (c.x, c.y, r.getWidth() * 0.48f, r.getWidth() * 0.16f, -0.35f, 0, juce::MathConstants<float>::twoPi, true);
            g.strokePath (p, juce::PathStrokeType (1.4f)); break;
        case FxType::delay:
            for (int i = 0; i < 4; ++i) { auto rr = 3.0f + (float) i * 0.8f; g.fillEllipse (c.x - r.getWidth() * 0.3f + (float) i * r.getWidth() * 0.18f - rr, c.y - rr, rr * 2, rr * 2); }
            break;
        case FxType::reverb:
            g.drawEllipse (r.withSizeKeepingCentre (r.getWidth() * 0.8f, r.getWidth() * 0.45f), 1.4f);
            g.drawEllipse (r.withSizeKeepingCentre (r.getWidth() * 0.4f, r.getWidth() * 0.4f), 1.6f);
            break;
        default:
        {
            for (int i = 0; i < 3; ++i)
            {
                juce::Path w;
                for (int k = 0; k <= 30; ++k)
                {
                    auto x = r.getX() + r.getWidth() * (float) k / 30.0f;
                    auto y = c.y + (float) (i - 1) * 7.0f + std::sin ((float) k * 0.42f + (float) i) * 4.0f;
                    if (k == 0) w.startNewSubPath (x, y); else w.lineTo (x, y);
                }
                g.strokePath (w, juce::PathStrokeType (1.4f));
            }
        }
    }
}

void FxModule::drawDisplay (juce::Graphics& g, juce::Rectangle<float> r)
{
    ui::drawDisplay (g, r, 6.0f);
    auto in = r.reduced (8.0f);
    auto P = [this] (int i) { return proc.state.getRawParameterValue (IDs::fxParam (bus, slot, i))->load(); };
    auto& info = fxInfo (type);
    auto R = [&] (int i) { return info.params[(size_t) i].toReal (P (i)); };
    juce::Path p;
    auto line = juce::Colour (0xffc9a8ff);
    switch ((FxType) type)
    {
        case FxType::saturation: case FxType::distortion: case FxType::compressor:
        {
            for (int i = 0; i <= 60; ++i)
            {
                auto x = (float) i / 30.0f - 1.0f;
                float y;
                if ((FxType) type == FxType::compressor)
                {
                    auto db = -60.0f + 60.0f * (float) i / 60.0f, thr = R (0), ratio = R (1);
                    auto out = db < thr ? db : thr + (db - thr) / ratio;
                    y = (out + 60.0f) / 60.0f * 2.0f - 1.0f; x = (db + 60.0f) / 60.0f * 2.0f - 1.0f;
                }
                else { auto drv = 1.0f + R (1) * 0.1f; y = dsp::softClip (x * drv * 2.0f) * 0.9f; }
                auto px = in.getCentreX() + x * in.getWidth() * 0.5f, py = in.getCentreY() - y * in.getHeight() * 0.5f;
                if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
            }
            break;
        }
        case FxType::delay:
        {
            auto fb = R (4) * 0.01f;
            float a = 1.0f;
            for (int i = 0; i < 12; ++i)
            {
                auto x = in.getX() + in.getWidth() * (0.12f + (float) i * 0.075f);
                auto bar = juce::Rectangle<float> (x, in.getBottom() - a * in.getHeight() * 0.85f, in.getWidth() * 0.05f, a * in.getHeight() * 0.85f);
                g.setColour (Colours::accent.withAlpha (0.35f + 0.5f * a)); g.fillRect (bar);
                a *= juce::jmax (0.05f, fb);
            }
            g.setColour (juce::Colours::white); g.drawVerticalLine ((int) (in.getX() + in.getWidth() * 0.12f), in.getY(), in.getBottom());
            return;
        }
        case FxType::reverb:
        {
            auto decay = R (3), pre = R (2);
            for (int i = 0; i <= 80; ++i)
            {
                auto t = (float) i / 80.0f * 4.0f;   // 4 seconds window
                auto y = t * 1000.0f < pre ? 0.0f : std::pow (10.0f, -3.0f * (t - pre * 0.001f) / juce::jmax (0.2f, decay) * 0.5f);
                auto px = in.getX() + in.getWidth() * (float) i / 80.0f, py = in.getBottom() - y * in.getHeight();
                if (i == 0) p.startNewSubPath (px, in.getBottom()); p.lineTo (px, py);
            }
            break;
        }
        case FxType::equalizer: case FxType::filter:
        {
            for (int i = 0; i <= 80; ++i)
            {
                auto f = 20.0f * std::pow (1000.0f, (float) i / 80.0f);
                float db;
                if ((FxType) type == FxType::equalizer)
                {
                    auto bell = [] (float f0, float fc, float g, float q) { auto x = std::log2 (f0 / fc) * q * 1.5f; return g / (1.0f + x * x); };
                    db = R (1) / (1.0f + std::pow (f / R (0), 2.0f)) + bell (f, R (2), R (3), R (4)) + R (6) / (1.0f + std::pow (R (5) / f, 2.0f));
                    if (R (7) > 20.5f) db -= 12.0f * juce::jmax (0.0f, std::log2 (R (7) / f));
                }
                else
                {
                    auto x = f / R (1); auto t = (int) R (0);
                    auto lp = -10.0f * std::log10 (1.0f + std::pow (x, t == 1 ? 8.0f : 4.0f));
                    db = t <= 1 ? lp : t <= 3 ? -10.0f * std::log10 (1.0f + std::pow (1.0f / x, t == 3 ? 8.0f : 4.0f)) : -6.0f * std::abs (std::log2 (x));
                }
                auto px = in.getX() + in.getWidth() * (float) i / 80.0f, py = in.getCentreY() - juce::jlimit (-24.0f, 24.0f, db) / 24.0f * in.getHeight() * 0.5f;
                if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
            }
            break;
        }
        default:
        {
            auto ph = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
            for (int i = 0; i <= 80; ++i)
            {
                auto t = (float) i / 80.0f;
                auto y = std::sin (t * 12.0f + ph * 2.0f) * 0.4f * (0.4f + 0.6f * std::sin (t * 3.0f + ph));
                if ((FxType) type == FxType::bitcrush) y = std::round (y * 6.0f) / 6.0f;
                auto px = in.getX() + in.getWidth() * t, py = in.getCentreY() - y * in.getHeight();
                if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
            }
        }
    }
    g.setColour (line.withAlpha (0.35f)); g.strokePath (p, juce::PathStrokeType (4.0f));
    g.setColour (line); g.strokePath (p, juce::PathStrokeType (1.6f));
}

void FxModule::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPanel (g, r, 10.0f, proc.chains[(size_t) bus]->getActivity (slot) > 0.5f && false);
    g.setColour (Colours::faint);
    g.drawVerticalLine (58, 10, r.getBottom() - 10);
    g.drawVerticalLine ((int) r.getRight() - 52, 10, r.getBottom() - 10);
    drawIcon (g, { 92, r.getCentreY() - 46, 44, 44 });
    g.setColour (Colours::text); g.setFont (Fonts::regular (17.0f));
    g.drawFittedText (fxInfo (type).name, juce::Rectangle<int> (62, (int) r.getCentreY() + 4, 104, 24), juce::Justification::centred, 1);
    drawDisplay (g, { 182, 44, 140, r.getHeight() - 60 });
}

// ================================================================== FX page

FxPage::FxPage (Processor& p) : Page (p)
{
    busTabs.boxed = true; busTabs.separators = false; busTabs.fontSize = 15.0f;
    busTabs.onChange = [this] (int b) { setBus (b); };
    addAndMakeVisible (busTabs);
    busTabs.setBounds (30, 110, 394, 36);
    addButton.getProperties().set ("fontSize", 16.0);
    addButton.onClick = [this] { showAddMenu (count()); };
    addAndMakeVisible (addButton);
    addButton.setBounds (30, 164, 120, 38);
    chainButton.getProperties().set ("fontSize", 15.0);
    chainButton.onClick = [this]
    {
        juce::PopupMenu m;
        static const char* names[] = { "Clean (empty)", "Warm Space", "Trap Lead", "Lo-Fi Tape", "Wide Pad", "Dark Pluck", "808 Grit", "Perreo Bounce" };
        for (int i = 0; i < 8; ++i) m.addItem (i + 1, names[i]);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&chainButton), [this] (int id)
        {
            if (id <= 0) return;
            using T = FxType;
            static const std::vector<std::vector<T>> chains {
                {}, { T::saturation, T::chorus, T::delay, T::reverb }, { T::distortion, T::equalizer, T::delay, T::reverb },
                { T::tape, T::bitcrush, T::equalizer, T::reverb }, { T::chorus, T::stereoWidth, T::reverb },
                { T::saturation, T::filter, T::delay, T::reverb }, { T::distortion, T::compressor, T::equalizer, T::utility },
                { T::saturation, T::compressor, T::delay, T::stereoWidth } };
            auto& c = *proc.chains[(size_t) bus];
            proc.undo.beginNewTransaction();
            auto& list = chains[(size_t) id - 1];
            for (int s = 0; s < numFxSlots; ++s)
                if (s < (int) list.size()) c.setType (s, (int) list[(size_t) s]);
                else proc.setParam (IDs::fx (bus, s, "type"), 0);
            c.service();
            chainButton.setButtonText (id == 1 ? juce::String ("Custom") : juce::String (names[id - 1]));
        });
    };
    addAndMakeVisible (chainButton);
    chainButton.setBounds (158, 164, 266, 38);
    viewport.setViewedComponent (&holder, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    for (int i = 0; i < numFxSlots; ++i)
    {
        auto* s = rowSwitches.add (new Switch (*p.param (IDs::fx (0, i, "on"))));
        addAndMakeVisible (s);
    }
    setBus (0);
    startTimerHz (10);
}

FxPage::~FxPage() { modules.clear(); rowSwitches.clear(); }

void FxPage::resized() { viewport.setBounds (455, 98, 727, 634); layoutModules(); }

int FxPage::count() const
{
    int n = 0;
    for (int s = 0; s < numFxSlots; ++s) if (proc.chains[(size_t) bus]->getType (s) > 0) n = s + 1;
    return n;
}

void FxPage::setBus (int b)
{
    bus = b;
    busTabs.setSelected (b);
    rowSwitches.clear();
    for (int i = 0; i < numFxSlots; ++i)
    {
        auto* s = rowSwitches.add (new Switch (*proc.param (IDs::fx (bus, i, "on"))));
        addAndMakeVisible (s);
    }
    lastSignature = -1;
    timerCallback();
}

void FxPage::layoutModules()
{
    auto n = count();
    modules.clear();
    for (int s = 0; s < n; ++s)
    {
        if (proc.chains[(size_t) bus]->getType (s) == 0) continue;
        auto* m = modules.add (new FxModule (proc, bus, s));
        holder.addAndMakeVisible (m);
    }
    auto w = viewport.getWidth() - (modules.size() > 3 ? 10 : 0);
    int y = 0;
    for (auto* m : modules) { m->setBounds (0, y, w, 205); y += 214; }
    holder.setSize (w, juce::jmax (viewport.getHeight(), y));
}

void FxPage::timerCallback()
{
    // rebuild the modules when the chain changes (types / order)
    int sig = bus * 7919;
    for (int s = 0; s < numFxSlots; ++s) sig = sig * 31 + proc.chains[(size_t) bus]->getType (s);
    if (sig != lastSignature) { lastSignature = sig; layoutModules(); }
    auto n = count();
    for (int i = 0; i < numFxSlots; ++i)
    {
        auto r = rowArea (i);
        rowSwitches[i]->setBounds (r.getX() + 12, r.getCentreY() - 10, 38, 20);
        rowSwitches[i]->setVisible (i < n && proc.chains[(size_t) bus]->getType (i) > 0);
    }
    repaint (18, 98, 430, 634);
}

void FxPage::paintPanels (juce::Graphics& g)
{
    drawPanel (g, { 18, 98, 420, 634 });
    auto n = count();
    for (int i = 0; i < n; ++i)
    {
        auto t = proc.chains[(size_t) bus]->getType (i);
        if (t == 0) continue;
        auto r = rowArea (i).toFloat();
        auto on = proc.state.getRawParameterValue (IDs::fx (bus, i, "on"))->load() > 0.5f;
        drawPanel (g, r, 6.0f, i == dragRow);
        if (i == 0 && dragRow < 0) { g.setColour (juce::Colour (0x187b2cff)); g.fillRoundedRectangle (r, 6.0f); }
        // drag handle
        g.setColour (Colours::dim);
        for (int k = 0; k < 3; ++k) for (int j = 0; j < 2; ++j) g.fillEllipse (r.getX() + 62 + (float) j * 6, r.getCentreY() - 7 + (float) k * 6, 2.6f, 2.6f);
        g.setColour (on ? juce::Colours::white : Colours::dim);
        g.setFont (Fonts::regular (16.0f));
        g.drawText (fxNames()[t], r.withTrimmedLeft (84).toNearestInt(), juce::Justification::centredLeft);
        // remove
        auto x = r.getRight() - 24, y = r.getCentreY();
        g.setColour (Colours::dim);
        g.drawLine (x - 5, y - 5, x + 5, y + 5, 1.4f); g.drawLine (x - 5, y + 5, x + 5, y - 5, 1.4f);
    }
    if (dropRow >= 0 && dragRow >= 0)
    {
        auto r = rowArea (dropRow);
        g.setColour (Colours::arc); g.fillRect (r.getX(), r.getY() - 4, r.getWidth(), 2);
    }
    if (n == 0)
    {
        g.setColour (Colours::dim); g.setFont (Fonts::regular (14.0f));
        g.drawFittedText ("No effects on this bus yet.\nClick + FX to add one.", juce::Rectangle<int> (30, 240, 394, 60), juce::Justification::centred, 2);
    }
}

void FxPage::showAddMenu (int slot)
{
    if (slot >= numFxSlots) return;
    juce::PopupMenu m;
    for (int t = 1; t < (int) FxType::count; ++t) m.addItem (t, juce::String (fxNames()[t]).substring (0, 1) + juce::String (fxNames()[t]).substring (1).toLowerCase());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addButton), [this, slot] (int t)
    {
        if (t > 0) { proc.undo.beginNewTransaction(); proc.chains[(size_t) bus]->setType (slot, t); }
    });
}

void FxPage::mouseDown (const juce::MouseEvent& e)
{
    dragRow = dropRow = -1;
    auto n = count();
    for (int i = 0; i < n; ++i)
    {
        auto r = rowArea (i);
        if (! r.contains (e.getPosition())) continue;
        if (e.x > r.getRight() - 40) { proc.undo.beginNewTransaction(); proc.chains[(size_t) bus]->remove (i); return; }
        if (e.x > r.getX() + 55) dragRow = i;
        if (e.mods.isPopupMenu()) { dragRow = -1; showAddMenu (i); }
    }
}

void FxPage::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow < 0) return;
    dropRow = juce::jlimit (0, juce::jmax (0, count() - 1), (e.y - 222 + 24) / 48);
    repaint();
}

void FxPage::mouseUp (const juce::MouseEvent&)
{
    if (dragRow >= 0 && dropRow >= 0 && dropRow != dragRow) { proc.undo.beginNewTransaction(); proc.chains[(size_t) bus]->move (dragRow, dropRow); }
    dragRow = dropRow = -1;
    repaint();
}

// ================================================================== MATRIX page

MatrixPage::MatrixPage (Processor& p) : Page (p)
{
    for (auto* b : { &addButton, &clearButton })
    {
        b->getProperties().set ("fontSize", 13.0);
        addAndMakeVisible (b);
    }
    addButton.setBounds (30, 108, 76, 26);
    clearButton.setBounds (116, 108, 76, 26);
    addButton.onClick = [this]
    {
        for (int s = 0; s < numModSlots; ++s)
            if ((int) get (s, "src") == 0)
            {
                proc.undo.beginNewTransaction();
                set (s, "src", srcLfo1); set (s, "dst", 0); set (s, "amt", 50); set (s, "on", 1); set (s, "out", 100);
                selected = s; scroll = juce::jlimit (0, numModSlots - 15, s - 7);
                repaint();
                return;
            }
    };
    clearButton.onClick = [this]
    {
        proc.undo.beginNewTransaction();
        for (int s = 0; s < numModSlots; ++s) { set (s, "src", 0); set (s, "dst", 0); set (s, "amt", 0); set (s, "aux", 0); set (s, "curve", 0); set (s, "bip", 0); set (s, "out", 100); set (s, "on", 1); }
        repaint();
    };
    startTimerHz (20);
}

MatrixPage::~MatrixPage() = default;

float MatrixPage::get (int s, const char* w) const { return proc.state.getRawParameterValue (IDs::mod (s, w))->load(); }
void MatrixPage::set (int s, const char* w, float v) { proc.setParam (IDs::mod (s, w), v); }

int MatrixPage::rowAt (juce::Point<float> p) const
{
    for (int r = 0; r < 15; ++r) if (rowRect (r).contains (p)) return r + scroll;
    return -1;
}

void MatrixPage::paintPanels (juce::Graphics& g)
{
    drawPanel (g, { 18, 98, 955, 634 });
    g.setColour (Colours::dim); g.setFont (Fonts::regular (12.5f));
    g.drawText ("Arrastra una fuente sobre cualquier perilla para modular", juce::Rectangle<int> (560, 108, 400, 26), juce::Justification::centredRight);
    g.setFont (Fonts::regular (14.0f)); g.setColour (Colours::text);
    struct Col { const char* name; int x, w; };
    const Col cols[] = { { "SOURCE", 66, 156 }, { "AMOUNT", 232, 228 }, { "DESTINATION", 470, 162 }, { "CURVE", 640, 70 }, { "AUX SOURCE", 716, 94 }, { "OUTPUT", 812, 60 } };
    for (auto& c : cols) g.drawText (c.name, juce::Rectangle<int> (c.x, 142, c.w, 22), juce::Justification::centred);
    juce::Path pw; pw.addCentredArc (900, 153, 6, 6, 0, 0.7f, juce::MathConstants<float>::twoPi - 0.7f, true); pw.startNewSubPath (900, 146); pw.lineTo (900, 152);
    g.strokePath (pw, juce::PathStrokeType (1.4f));

    auto& dests = ModDefs::destinations();
    for (int vr = 0; vr < 15; ++vr)
    {
        auto s = vr + scroll;
        if (s >= numModSlots) break;
        auto r = rowRect (vr);
        auto src = (int) get (s, "src"), dst = (int) get (s, "dst"), aux = (int) get (s, "aux"), curve = (int) get (s, "curve");
        auto amt = get (s, "amt"), out = get (s, "out");
        auto on = get (s, "on") > 0.5f, bip = get (s, "bip") > 0.5f;
        auto used = src > 0;
        auto alpha = used ? 1.0f : 0.35f;
        if (s == selected && used) { drawPanel (g, r.expanded (2, 1), 6.0f, true); }
        else { g.setColour (juce::Colour (0x0affffff)); g.fillRoundedRectangle (r, 5.0f); }
        // icon / number
        g.setColour (used ? ModDefs::sourceColour (src) : Colours::dim);
        if (used) { g.drawRoundedRectangle ({ r.getX() + 4, r.getY() + 5, 24, 23 }, 5.0f, 1.2f); g.fillEllipse (r.getX() + 13, r.getY() + 13, 6, 6); }
        else { g.setFont (Fonts::regular (13.0f)); g.drawText (juce::String (s + 1), juce::Rectangle<float> (r.getX(), r.getY(), 32, r.getHeight()).toNearestInt(), juce::Justification::centred); }
        auto box = [&] (float x, float w, const juce::String& t)
        {
            auto b = juce::Rectangle<float> (x, r.getY() + 5, w, 23);
            g.setColour (juce::Colour (0x18ffffff).withMultipliedAlpha (alpha)); g.fillRoundedRectangle (b, 5.0f);
            g.setColour (Colours::edge.withMultipliedAlpha (alpha * 1.5f)); g.drawRoundedRectangle (b, 5.0f, 1.0f);
            g.setColour (Colours::text.withMultipliedAlpha (alpha)); g.setFont (Fonts::regular (12.5f));
            g.drawFittedText (t, b.reduced (8, 0).withTrimmedRight (10).toNearestInt(), juce::Justification::centredLeft, 1);
            juce::Path chev; auto cx = b.getRight() - 10, cy = b.getCentreY();
            chev.startNewSubPath (cx - 3, cy - 2); chev.lineTo (cx, cy + 2); chev.lineTo (cx + 3, cy - 2);
            g.strokePath (chev, juce::PathStrokeType (1.2f));
        };
        box (66, 156, used ? ModDefs::sourceNames()[src] : "---");
        // amount bar (bipolar, from the centre)
        auto bar = juce::Rectangle<float> (232, r.getY() + 10, 172, 13);
        g.setColour (juce::Colour (0x22ffffff).withMultipliedAlpha (alpha)); g.fillRoundedRectangle (bar, 6.0f);
        if (used)
        {
            auto mid = bar.getCentreX(), end = mid + amt * 0.01f * bar.getWidth() * 0.5f;
            auto fill = juce::Rectangle<float> (juce::jmin (mid, end), bar.getY(), std::abs (end - mid), bar.getHeight());
            Glass::drawRectGlow (g, fill, 6.0f, Colours::accent, 6.0f, 0.6f);
            juce::ColourGradient vio (juce::Colour (0xff6b2cdc), fill.getX(), 0, juce::Colour (0xffb98cff), fill.getRight(), 0, false);
            g.setGradientFill (vio); g.fillRoundedRectangle (fill, 6.0f);
        }
        g.setColour (Colours::dim.withMultipliedAlpha (alpha)); g.fillEllipse (bar.getCentreX() - 2, bar.getCentreY() - 2, 4, 4);
        g.setColour (Colours::text.withMultipliedAlpha (alpha)); g.setFont (Fonts::regular (13.0f));
        g.drawText (used ? (amt > 0 ? "+" : "") + juce::String (juce::roundToInt (amt)) + "%" : juce::String ("---"), juce::Rectangle<float> (408, r.getY(), 52, r.getHeight()).toNearestInt(), juce::Justification::centredRight);
        box (470, 162, used && dst > 0 ? dests[(size_t) dst].name : "---");
        // curve and polarity glyphs
        for (int k = 0; k < 2; ++k)
        {
            auto b = juce::Rectangle<float> (640.0f + (float) k * 37.0f, r.getY() + 5, 31, 23);
            g.setColour (juce::Colour (0x18ffffff).withMultipliedAlpha (alpha)); g.fillRoundedRectangle (b, 5.0f);
            g.setColour (Colours::edge.withMultipliedAlpha (alpha * 1.5f)); g.drawRoundedRectangle (b, 5.0f, 1.0f);
            juce::Path c;
            auto in = b.reduced (7, 5);
            for (int i = 0; i <= 16; ++i)
            {
                auto t = (float) i / 16.0f;
                float y;
                if (k == 0) { y = curve == 1 ? t * t : curve == 2 ? std::sqrt (t) : curve == 3 ? t * t * (3 - 2 * t) : t; }
                else y = bip ? 0.5f + 0.5f * std::sin (t * juce::MathConstants<float>::twoPi) : t;
                auto pt = juce::Point<float> (in.getX() + t * in.getWidth(), in.getBottom() - y * in.getHeight());
                if (i == 0) c.startNewSubPath (pt); else c.lineTo (pt);
            }
            g.setColour (juce::Colour (0xffc9a8ff).withMultipliedAlpha (alpha)); g.strokePath (c, juce::PathStrokeType (1.3f));
        }
        box (716, 92, used && aux > 0 ? ModDefs::sourceNames()[aux] : (used ? juce::String ("-") : juce::String ("---")));
        g.setColour (Colours::text.withMultipliedAlpha (alpha)); g.setFont (Fonts::regular (13.0f));
        g.drawText (used ? juce::String (juce::roundToInt (out)) + "%" : juce::String ("-"), juce::Rectangle<float> (812, r.getY(), 60, r.getHeight()).toNearestInt(), juce::Justification::centred);
        // on switch + remove
        auto sw = juce::Rectangle<float> (880, r.getY() + 8, 38, 18);
        g.setColour (on && used ? Colours::accent : juce::Colour (0x30ffffff)); g.fillRoundedRectangle (sw, 9.0f);
        g.setColour (juce::Colours::white.withMultipliedAlpha (used ? 1.0f : 0.5f));
        g.fillEllipse (on ? sw.getRight() - 16 : sw.getX() + 2, sw.getY() + 2, 14, 14);
        auto xb = juce::Rectangle<float> (928, r.getY() + 6, 22, 21);
        g.setColour (Colours::edge); g.drawRoundedRectangle (xb, 4.0f, 1.0f);
        g.setColour (Colours::dim.withMultipliedAlpha (alpha));
        g.drawLine (xb.getCentreX() - 4, xb.getCentreY() - 4, xb.getCentreX() + 4, xb.getCentreY() + 4, 1.2f);
        g.drawLine (xb.getCentreX() - 4, xb.getCentreY() + 4, xb.getCentreX() + 4, xb.getCentreY() - 4, 1.2f);
    }
    // scroll bar
    auto track = juce::Rectangle<float> (962, 172, 4, 560);
    g.setColour (juce::Colour (0x18ffffff)); g.fillRoundedRectangle (track, 2);
    auto th = track.getHeight() * 15.0f / numModSlots;
    g.setColour (Colours::arc.withAlpha (0.6f)); g.fillRoundedRectangle (track.getX(), track.getY() + (track.getHeight() - th) * (float) scroll / (float) (numModSlots - 15), 4, th, 2);

    // MODULATION PREVIEW
    drawPanel (g, { 987, 98, 195, 399 });
    drawTitle (g, "MODULATION PREVIEW", { 997, 106, 180, 24 }, 14.5f);
    auto src = (int) get (selected, "src"), dst = (int) get (selected, "dst");
    if (src > 0 && dst > 0)
    {
        auto* p = proc.param (dests[(size_t) dst].id);
        drawTitle (g, dests[(size_t) dst].name, { 990, 160, 189, 24 }, 15.0f, juce::Justification::centred);
        auto k = juce::Rectangle<float> (1034, 210, 100, 100);
        auto base = p != nullptr ? p->getValue() : 0.0f;
        drawKnob (g, k, base, false, 1.0f, true);
        auto amt = get (selected, "amt") * 0.01f;
        juce::Path ring;
        auto lo = juce::jlimit (0.0f, 1.0f, juce::jmin (base, base + amt)), hi = juce::jlimit (0.0f, 1.0f, juce::jmax (base, base + amt));
        auto a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
        ring.addCentredArc (k.getCentreX(), k.getCentreY(), 56, 56, 0, a0 + lo * (a1 - a0), a0 + juce::jmax (lo + 0.01f, hi) * (a1 - a0), true);
        g.setColour (ModDefs::sourceColour (src)); g.strokePath (ring, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        auto now = juce::jlimit (0.0f, 1.0f, base + proc.engine.getModOffset (dst));
        auto pt = k.getCentre().getPointOnCircumference (56, a0 + now * (a1 - a0));
        Glass::drawGlow (g, pt, 8.0f, juce::Colours::white, 0.7f);
        g.setColour (juce::Colours::white); g.fillEllipse (pt.x - 3, pt.y - 3, 6, 6);
        g.setColour (juce::Colours::white); g.setFont (Fonts::regular (17.0f));
        g.drawText ((amt > 0 ? "+" : "") + juce::String (juce::roundToInt (amt * 100)) + "%", juce::Rectangle<int> (990, 330, 189, 24), juce::Justification::centred);
        g.setColour (Colours::dim); g.setFont (Fonts::regular (13.0f));
        g.drawText (ModDefs::sourceNames()[src], juce::Rectangle<int> (990, 354, 189, 20), juce::Justification::centred);
    }
    else
    {
        g.setColour (Colours::dim); g.setFont (Fonts::regular (13.0f));
        g.drawFittedText ("Select a row, or drag a source (ENV, LFO, MACRO) onto any knob.", juce::Rectangle<int> (1000, 200, 170, 80), juce::Justification::centred, 4);
    }
}

void MatrixPage::sourceMenu (int s, bool aux, juce::Rectangle<int> at)
{
    juce::PopupMenu m;
    auto cur = (int) get (s, aux ? "aux" : "src");
    for (int i = aux ? 0 : 1; i < numModSources; ++i) m.addItem (i + 1, ModDefs::sourceNames()[i], true, i == cur);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (at)), [this, s, aux] (int id)
    {
        if (id <= 0) return;
        proc.undo.beginNewTransaction();
        set (s, aux ? "aux" : "src", (float) (id - 1));
        if (! aux && get (s, "amt") == 0.0f) set (s, "amt", 50);
        repaint();
    });
}

void MatrixPage::destMenu (int s, juce::Rectangle<int> at)
{
    // grouped by section: OSC A, OSC B, ..., FX of each bus
    juce::PopupMenu m;
    std::map<juce::String, juce::PopupMenu> groups;
    juce::StringArray order;
    auto& d = ModDefs::destinations();
    auto cur = (int) get (s, "dst");
    for (int i = 1; i < (int) d.size(); ++i)
    {
        auto words = juce::StringArray::fromTokens (d[(size_t) i].name, " ", "");
        auto group = words.size() > 2 && (words[0] == "OSC" || words[0] == "FILTER" || words[0] == "LFO" || words[0] == "ENV" || words[0] == "BUS")
                       ? words[0] + " " + words[1] : (words[0] == "MAIN" ? juce::String ("MAIN") : words[0]);
        if (d[(size_t) i].name.contains (" FX")) group = d[(size_t) i].name.upToFirstOccurrenceOf (" P", false, false).upToLastOccurrenceOf (" MIX", false, false);
        if (! order.contains (group)) order.add (group);
        groups[group].addItem (i, d[(size_t) i].name, true, i == cur);
    }
    for (auto& g : order) m.addSubMenu (g, groups[g]);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (at)), [this, s] (int id)
    {
        if (id > 0) { proc.undo.beginNewTransaction(); set (s, "dst", (float) id); repaint(); }
    });
}

void MatrixPage::mouseDown (const juce::MouseEvent& e)
{
    auto s = rowAt (e.position);
    dragSlot = -1;
    if (s < 0) return;
    selected = s;
    auto x = e.position.x;
    auto r = rowRect (s - scroll).toNearestInt();
    if (x >= 66 && x < 222) sourceMenu (s, false, r.withX (66).withWidth (156));
    else if (x >= 232 && x < 460) { dragSlot = s; dragColumn = 0; dragStart = get (s, "amt"); proc.undo.beginNewTransaction(); if (e.getNumberOfClicks() > 1) set (s, "amt", 0); }
    else if (x >= 470 && x < 632) destMenu (s, r.withX (470).withWidth (162));
    else if (x >= 640 && x < 672) set (s, "curve", (float) (((int) get (s, "curve") + 1) % 4));
    else if (x >= 677 && x < 710) set (s, "bip", get (s, "bip") > 0.5f ? 0.0f : 1.0f);
    else if (x >= 716 && x < 808) sourceMenu (s, true, r.withX (716).withWidth (92));
    else if (x >= 812 && x < 872) { dragSlot = s; dragColumn = 1; dragStart = get (s, "out"); proc.undo.beginNewTransaction(); }
    else if (x >= 880 && x < 920) set (s, "on", get (s, "on") > 0.5f ? 0.0f : 1.0f);
    else if (x >= 926 && x < 952) proc.removeModulation (s);
    repaint();
}

void MatrixPage::mouseDrag (const juce::MouseEvent& e)
{
    if (dragSlot < 0) return;
    if (dragColumn == 0)
    {
        // absolute position on the bar
        auto v = juce::jlimit (-100.0f, 100.0f, (e.position.x - 318.0f) / 86.0f * 100.0f);
        if (e.mods.isShiftDown()) v = dragStart + (float) e.getDistanceFromDragStartX() * 0.2f;
        set (dragSlot, "amt", std::round (juce::jlimit (-100.0f, 100.0f, v)));
    }
    else set (dragSlot, "out", juce::jlimit (0.0f, 100.0f, dragStart - (float) e.getDistanceFromDragStartY() * 0.5f));
    repaint();
}

void MatrixPage::mouseUp (const juce::MouseEvent&) { dragSlot = -1; }

void MatrixPage::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    scroll = juce::jlimit (0, numModSlots - 15, scroll - (w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0) * 2);
    repaint();
}

// ================================================================== GLOBAL page

GlobalPage::GlobalPage (Processor& p) : Page (p)
{
    for (int i = 0; i < 3; ++i)
    {
        auto* b = qualityButtons.add (new juce::TextButton (Choices::qualities()[i]));
        b->getProperties().set ("fontSize", 14.0);
        b->setBounds (130 + i * 78, 148, 72, 32);
        b->onClick = [this, i] { proc.setParam (IDs::quality, (float) i); };
        addAndMakeVisible (b);
    }
    for (int i = 0; i < 2; ++i)
    {
        auto* b = osButtons.add (new juce::TextButton (Choices::oversamplings()[i]));
        b->getProperties().set ("fontSize", 15.0);
        b->setBounds (138 + i * 113, 232, 106, 32);
        b->onClick = [this, i] { proc.setParam (IDs::oversampling, (float) i); };
        addAndMakeVisible (b);
    }
    knob (IDs::masterTune, "MASTER TUNE", { 449, 196 }, 50, 13.0f).bipolar = true;
    knob (IDs::transpose, "TRANSPOSE", { 556, 196 }, 50, 13.0f).bipolar = true;
    auto& up = make<ValueBox> (*p.param (IDs::pbUp), "", 1.0f); up.labelBelow = false; up.setBounds (416, 334, 78, 30);
    auto& dn = make<ValueBox> (*p.param (IDs::pbDown), "", 1.0f); dn.labelBelow = false; dn.setBounds (511, 334, 78, 30);
    auto& scale = make<Dropdown> (*p.param (IDs::scale), Choices::scales());
    scale.centred = false; scale.setBounds (403, 410, 198, 30);
    auto& poly = make<ValueBox> (*p.param (IDs::poly), "", 1.0f); poly.labelBelow = false; poly.setBounds (646, 166, 130, 30);
    auto& stack = make<ValueBox> (*p.param (IDs::stack), "", 1.0f); stack.labelBelow = false; stack.setBounds (646, 234, 130, 30);
    auto& steal = make<Dropdown> (*p.param (IDs::steal), Choices::stealModes()); steal.centred = false; steal.setBounds (646, 302, 130, 30);
    auto& prio = make<Dropdown> (*p.param (IDs::priority), Choices::priorities()); prio.centred = false; prio.setBounds (646, 368, 130, 30);
    make<Switch> (*p.param (IDs::mpe)).setBounds (692, 418, 48, 24);
    knob (IDs::drift, "DRIFT", { 887, 172 }, 44);
    knob (IDs::warmth, "WARMTH", { 887, 262 }, 44);
    make<Switch> (*p.param (IDs::limiter)).setBounds (999, 176, 44, 22);
    knob (IDs::master, "OUTPUT", { 1084, 262 }, 54, 14.0f);
    meter = &make<Meter> ([this] (int c) { return c < 2 ? proc.getBusLevel (0, c) : proc.getOutputLevel (c - 2); }, 2, true);
    meter->setBounds (1022, 330, 56, 150);
    make<Meter> ([this] (int c) { return proc.getOutputLevel (c); }, 2, true).setBounds (1092, 330, 56, 150);
    startTimerHz (8);
}

void GlobalPage::timerCallback()
{
    auto q = (int) proc.state.getRawParameterValue (IDs::quality)->load();
    auto o = (int) proc.state.getRawParameterValue (IDs::oversampling)->load();
    for (int i = 0; i < 3; ++i) qualityButtons[i]->setToggleState (i == q, juce::dontSendNotification);
    for (int i = 0; i < 2; ++i) { osButtons[i]->setToggleState (i == o && q > 0, juce::dontSendNotification); osButtons[i]->setEnabled (q > 0); }
    repaint (987, 98, 195, 120);
}

void GlobalPage::paintPanels (juce::Graphics& g)
{
    drawPanel (g, { 18, 98, 355, 360 }); drawTitle (g, "QUALITY", { 28, 104, 200, 24 });
    g.setColour (Colours::text); g.setFont (Fonts::regular (14.0f));
    g.drawText ("OVERSAMPLING", juce::Rectangle<int> (138, 206, 219, 20), juce::Justification::centred);
    g.setColour (Colours::dim); g.setFont (Fonts::regular (12.0f));
    g.drawFittedText ("ECO: lowest CPU, no oversampling.\nHIGH: 2x / 4x for the filters, drive and warmth.\nULTRA: finer modulation (faster envelopes and LFOs).",
                      juce::Rectangle<int> (34, 300, 324, 100), juce::Justification::topLeft, 5);
    drawPanel (g, { 386, 98, 236, 360 }); drawTitle (g, "TUNING", { 396, 104, 200, 24 });
    drawDisplay (g, { 403, 275, 198, 112 }, 8.0f);
    g.setColour (Colours::text); g.setFont (Fonts::regular (14.0f));
    g.drawText ("PITCH BEND", juce::Rectangle<int> (403, 284, 198, 20), juce::Justification::centred);
    g.setFont (Fonts::regular (13.0f));
    g.drawText ("UP", juce::Rectangle<int> (416, 310, 78, 18), juce::Justification::centred);
    g.drawText ("DOWN", juce::Rectangle<int> (511, 310, 78, 18), juce::Justification::centred);
    drawPanel (g, { 634, 98, 155, 360 }); drawTitle (g, "VOICES", { 644, 104, 140, 24 });
    g.setColour (Colours::text); g.setFont (Fonts::regular (13.5f));
    g.drawText ("POLYPHONY", juce::Rectangle<int> (646, 144, 140, 20), juce::Justification::centredLeft);
    g.drawText ("UNISON VOICES", juce::Rectangle<int> (646, 212, 140, 20), juce::Justification::centredLeft);
    g.drawText ("VOICE STEAL:", juce::Rectangle<int> (646, 280, 140, 20), juce::Justification::centredLeft);
    g.drawText ("NOTE PRIORITY:", juce::Rectangle<int> (646, 346, 140, 20), juce::Justification::centredLeft);
    g.drawText ("MPE", juce::Rectangle<int> (646, 420, 44, 20), juce::Justification::centredLeft);
    drawPanel (g, { 802, 98, 170, 360 }); drawTitle (g, "ANALOG", { 812, 104, 150, 24 });
    // circuit illustration: a glowing chip with traces
    {
        auto c = juce::Point<float> (887, 392);
        juce::Random r (5);
        for (int i = 0; i < 26; ++i)
        {
            auto a = r.nextFloat() * juce::MathConstants<float>::twoPi;
            auto len = 30.0f + r.nextFloat() * 40.0f;
            juce::Path t;
            auto s = c.getPointOnCircumference (22, a);
            auto m = juce::Point<float> (s.x + std::cos (a) * len * 0.5f, s.y);
            auto e = juce::Point<float> (m.x + std::cos (a) * len * 0.5f, m.y + std::sin (a) * len * 0.6f);
            t.startNewSubPath (s); t.lineTo (m); t.lineTo (e);
            g.setColour (juce::Colour (0xffe0a040).withAlpha (0.55f)); g.strokePath (t, juce::PathStrokeType (1.0f));
            g.setColour (juce::Colour (0xffffd080)); g.fillEllipse (e.x - 1.6f, e.y - 1.6f, 3.2f, 3.2f);
        }
        auto chip = juce::Rectangle<float> (46, 46).withCentre (c);
        Glass::drawGlow (g, c, 50, juce::Colour (0xffffb050), 0.35f);
        juce::ColourGradient body (juce::Colour (0xff3a2f50), chip.getX(), chip.getY(), juce::Colour (0xff120d1c), chip.getRight(), chip.getBottom(), false);
        g.setGradientFill (body); g.fillRoundedRectangle (chip, 5.0f);
        g.setColour (juce::Colour (0xffffc060)); g.drawRoundedRectangle (chip, 5.0f, 1.2f);
        g.setColour (juce::Colour (0xffffe0a0)); g.drawRoundedRectangle (chip.reduced (12), 2.0f, 1.0f);
    }
    drawPanel (g, { 987, 98, 195, 399 }); drawTitle (g, "OUTPUT", { 997, 104, 150, 24 });
    g.setColour (Colours::text); g.setFont (Fonts::regular (13.5f));
    g.drawFittedText ("SAFETY\nLIMITER", juce::Rectangle<int> (999, 136, 80, 36), juce::Justification::centredLeft, 2);
    drawDisplay (g, { 1085, 138, 82, 60 }, 6.0f);
    auto cpu = juce::jlimit (0.0f, 1.0f, proc.getCpuLoad());
    g.setColour (Colours::accent.withAlpha (0.5f)); g.fillRoundedRectangle (1091, 146, 70.0f * cpu + 1, 18, 3);
    g.setColour (Colours::text); g.setFont (Fonts::regular (13.5f));
    g.drawText ("CPU " + juce::String (juce::roundToInt (cpu * 100)) + "%", juce::Rectangle<int> (1085, 170, 82, 22), juce::Justification::centred);
    g.setColour (Colours::dim); g.setFont (Fonts::regular (11.0f));
    g.drawText ("MAIN", juce::Rectangle<int> (1022, 482, 56, 14), juce::Justification::centred);
    g.drawText ("OUT", juce::Rectangle<int> (1092, 482, 56, 14), juce::Justification::centred);
}

// ================================================================== SIMPLE

SimplePage::SimplePage (Processor& p) : Page (p)
{
    for (int m = 0; m < numMacros; ++m)
    {
        auto& k = knob (IDs::macro (m), p.macroNames[m], { 118 + m * 138, 640 }, 64, 13.0f);
        k.valueInside = true;
    }
    for (auto* b : { &prev, &next })
    {
        b->getProperties().set ("fontSize", 22.0);
        b->getProperties().set ("corner", 22.0);
        addAndMakeVisible (b);
    }
    prev.setBounds (300, 300, 46, 46); next.setBounds (854, 300, 46, 46);
    prev.onClick = [this] { if (stepPreset) stepPreset (-1); };
    next.onClick = [this] { if (stepPreset) stepPreset (1); };
    startTimerHz (30);
}

void SimplePage::timerCallback()
{
    spin += 0.012f;
    energy = energy * 0.85f + 0.15f * juce::jlimit (0.0f, 1.0f, proc.getOutputLevel (0) * 2.0f);
    int i = 0;
    for (auto* c : owned) if (auto* k = dynamic_cast<Knob*> (c)) { if (i < numMacros) k->label = proc.macroNames[i]; ++i; }
    repaint();
}

void SimplePage::paintPanels (juce::Graphics& g)
{
    drawPanel (g, { 18, 98, 1164, 634 });
    auto c = juce::Point<float> (600, 330);
    // accretion disk (rotating particles), jets and the core
    juce::Random r (99);
    for (int i = 0; i < 900; ++i)
    {
        auto rad = 40.0f + std::pow (r.nextFloat(), 0.7f) * 190.0f;
        auto a = r.nextFloat() * juce::MathConstants<float>::twoPi + spin * (300.0f / rad);
        auto x = c.x + std::cos (a) * rad, y = c.y + std::sin (a) * rad * 0.28f;
        auto s = 0.8f + r.nextFloat() * 1.8f;
        auto col = juce::Colour (0xffb98cff).interpolatedWith (juce::Colour (0xffffe6ff), r.nextFloat()).withAlpha (0.25f + 0.6f * r.nextFloat());
        g.setColour (col); g.fillEllipse (x - s * 0.5f, y - s * 0.5f, s, s);
    }
    for (int k = 0; k < 2; ++k)
    {
        auto dir = k == 0 ? -1.0f : 1.0f;
        juce::ColourGradient jet (juce::Colour (0xccd9c4ff).withMultipliedAlpha (0.6f + 0.4f * energy), c.x, c.y, juce::Colours::transparentWhite, c.x, c.y + dir * 230, false);
        g.setGradientFill (jet);
        juce::Path p; p.addTriangle (c.x - 9, c.y, c.x + 9, c.y, c.x, c.y + dir * 230);
        g.fillPath (p);
    }
    Glass::drawGlow (g, c, 150, Colours::accent, 0.45f + 0.3f * energy);
    Glass::drawGlow (g, c, 60, juce::Colour (0xffe0ccff), 0.9f);
    Glass::drawGlow (g, c, 18, juce::Colours::white, 1.0f);

    auto* preset = proc.presets.find (proc.presetBank, proc.presetName);
    drawTitle (g, proc.presetName, { 300, 112, 600, 40 }, 30.0f, juce::Justification::centred);
    g.setColour (Colours::dim); g.setFont (Fonts::regular (14.0f));
    juce::String sub = preset != nullptr ? preset->category + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) + preset->bank : proc.presetBank;
    g.drawText (sub, juce::Rectangle<int> (300, 150, 600, 22), juce::Justification::centred);
    g.setColour (Colours::faint); g.drawHorizontalLine (560, 40, 1160);
}

// ================================================================== ARP

ArpPanel::ArpPanel (Processor& p) : Page (p)
{
    make<Switch> (*p.param (IDs::arpOn)).setBounds (36, 806, 46, 24);
    make<Wheel> (p, true).setBounds (118, 758, 34, 100);
    make<Wheel> (p, false).setBounds (174, 758, 34, 100);
}

void ArpPanel::paintPanels (juce::Graphics& g)
{
    drawPanel (g, { 18, 747, 212, 135 });
    drawTitle (g, "ARP", { 26, 772, 70, 26 }, 18.0f, juce::Justification::centred);
    g.setColour (Colours::dim); g.setFont (Fonts::regular (12.0f));
    g.drawText ("ON", juce::Rectangle<int> (26, 832, 66, 18), juce::Justification::centred);
    g.drawText ("PITCH", juce::Rectangle<int> (105, 860, 60, 16), juce::Justification::centred);
    g.drawText ("MOD", juce::Rectangle<int> (161, 860, 60, 16), juce::Justification::centred);
    g.setColour (Colours::faint); g.drawVerticalLine (101, 760, 870);
    // little gear: the arp settings
    g.setColour (Colours::dim);
    g.drawEllipse (82, 777, 12, 12, 1.2f);
}

void ArpPanel::mouseDown (const juce::MouseEvent& e)
{
    if (juce::Rectangle<int> (24, 768, 76, 34).contains (e.getPosition())) openSettings();
}

void ArpPanel::openSettings()
{
    struct Settings : public juce::Component
    {
        explicit Settings (Processor& p) : proc (p)
        {
            auto add = [this] (juce::Component* c, juce::Rectangle<int> r) { owned.add (c); addAndMakeVisible (c); c->setBounds (r); };
            add (new Dropdown (*p.param (IDs::arpMode), Choices::arpModes()), { 90, 12, 150, 24 });
            add (new Dropdown (*p.param (IDs::arpRate), Choices::divisions()), { 90, 44, 150, 24 });
            add (new Dropdown (*p.param (IDs::arpPattern), Choices::arpPatterns()), { 90, 76, 150, 24 });
            auto* oct = new ValueBox (*p.param (IDs::arpOct), "", 1.0f); oct->labelBelow = false;
            add (oct, { 90, 108, 60, 24 });
            add (new Switch (*p.param (IDs::arpLatch)), { 194, 108, 46, 22 });
            auto* gate = new Knob (p, IDs::arpGate, "GATE", 40); gate->bind (b);
            auto* swing = new Knob (p, IDs::arpSwing, "SWING", 40); swing->bind (b);
            add (gate, { 40, 146, 70, 68 }); add (swing, { 150, 146, 70, 68 });
            setSize (256, 222);
        }
        ~Settings() override { b.clear(); }
        void paint (juce::Graphics& g) override
        {
            g.setColour (Colours::text); g.setFont (Fonts::regular (13.0f));
            const char* l[] = { "MODE", "RATE", "PATTERN", "OCTAVES" };
            for (int i = 0; i < 4; ++i) g.drawText (l[i], 12, 12 + i * 32, 76, 24, juce::Justification::centredLeft);
            g.drawText ("LATCH", 150, 108, 44, 24, juce::Justification::centredLeft);
        }
        Processor& proc; Bindings b; juce::OwnedArray<juce::Component> owned;
    };
    auto content = std::make_unique<Settings> (proc);
    juce::CallOutBox::launchAsynchronously (std::move (content), localAreaToGlobal (juce::Rectangle<int> (24, 768, 76, 34)), nullptr);
}

} // namespace quasar::ui
