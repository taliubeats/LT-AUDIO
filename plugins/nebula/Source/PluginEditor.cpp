#include "PluginEditor.h"

namespace nebula
{

using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Logo;

bool Editor::activationShownThisSession = false;

// ================================================================== step bars

StepBars::StepBars (juce::AudioProcessorValueTreeState& s, NebulaEngine& e) : state (s), engine (e) { startTimerHz (15); }

void StepBars::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto custom = juce::roundToInt (state.getRawParameterValue (IDs::stepMode)->load()) == 5;
    auto w = (float) getWidth() / 8.0f;
    auto barsH = (float) getHeight() - 20.0f;
    for (int i = 0; i < 8; ++i)
    {
        auto v = juce::roundToInt (state.getRawParameterValue (IDs::step (i))->load());
        auto col = juce::Rectangle<float> ((float) i * w + 5.0f, 0.0f, w - 10.0f, barsH);
        Glass::drawPanel (g, col, 3.0f, p, 0.8f);
        auto h = barsH * (0.12f + 0.88f * ((float) v + 12.0f) / 24.0f);
        auto bar = col.withTop (col.getBottom() - h);
        auto playing = custom && engine.getStepIndex() == i;
        auto top = p.isLight ? juce::Colours::white : juce::Colour (0xff7fe7ff);
        juce::ColourGradient grad (top, 0, bar.getY(), p.accent, 0, bar.getBottom(), false);
        g.setGradientFill (grad);
        g.setOpacity (playing ? 1.0f : 0.75f);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setOpacity (1.0f);
        if (playing) Glass::drawRectGlow (g, bar, 3.0f, top, 6.0f, 0.8f);
        g.setColour (p.text);
        g.setFont (Fonts::regular (13.0f));
        g.drawText ((v > 0 ? "+" : "") + juce::String (v), juce::Rectangle<float> ((float) i * w, barsH + 2.0f, w, 18.0f), juce::Justification::centred);
    }
}

void StepBars::setFrom (juce::Point<float> pos)
{
    auto i = juce::jlimit (0, 7, (int) (pos.x / ((float) getWidth() / 8.0f)));
    auto barsH = (float) getHeight() - 20.0f;
    auto frac = juce::jlimit (0.0f, 1.0f, (barsH - pos.y) / barsH);
    auto v = juce::roundToInt ((frac - 0.12f) / 0.88f * 24.0f - 12.0f);
    if (auto* prm = state.getParameter (IDs::step (i)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) juce::jlimit (-12, 12, v)));
        prm->endChangeGesture();
    }
    if (auto* mode = state.getParameter (IDs::stepMode))   // editing the bars = CUSTOM pattern
        if (juce::roundToInt (mode->convertFrom0to1 (mode->getValue())) != 5)
            mode->setValueNotifyingHost (mode->convertTo0to1 (5.0f));
    repaint();
}

void StepBars::mouseDown (const juce::MouseEvent& e) { setFrom (e.position); }
void StepBars::mouseDrag (const juce::MouseEvent& e) { setFrom (e.position); }

// ================================================================== tape curve

void TapeCurveView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    auto curve = state.getRawParameterValue (IDs::tapeCurve)->load() * 0.01f;
    juce::Path path, fill;
    for (int i = 0; i <= 80; ++i)
    {
        auto x = (float) i / 80.0f;
        auto speed = std::pow (1.0f - x, 0.4f + curve * 2.6f);
        auto pt = juce::Point<float> (r.getX() + x * r.getWidth(), r.getBottom() - speed * r.getHeight());
        if (i == 0) { path.startNewSubPath (pt); fill.startNewSubPath (r.getX(), r.getBottom()); }
        path.lineTo (pt); fill.lineTo (pt);
    }
    fill.lineTo (r.getRight(), r.getBottom());
    fill.closeSubPath();
    auto col = p.isLight ? juce::Colours::white : juce::Colour (0xffe58cff);
    juce::ColourGradient grad (col.withAlpha (0.35f), 0, r.getY(), col.withAlpha (0.02f), 0, r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (path, juce::PathStrokeType (2.2f));
    g.setColour (p.text.withAlpha (0.4f));
    g.setFont (Fonts::regular (12.0f));
    g.drawText ("drag: CURVE", r.reduced (4.0f), juce::Justification::topRight);
}

void TapeCurveView::mouseDown (const juce::MouseEvent&)
{
    startCurve = state.getRawParameterValue (IDs::tapeCurve)->load();
    if (auto* prm = state.getParameter (IDs::tapeCurve)) prm->beginChangeGesture();
}

void TapeCurveView::mouseDrag (const juce::MouseEvent& e)
{
    if (auto* prm = state.getParameter (IDs::tapeCurve))
        prm->setValueNotifyingHost (prm->convertTo0to1 (juce::jlimit (0.0f, 100.0f, startCurve + (float) e.getDistanceFromDragStartY() * 0.6f)));
    repaint();
}

void TapeCurveView::mouseUp (const juce::MouseEvent&)
{
    if (auto* prm = state.getParameter (IDs::tapeCurve)) prm->endChangeGesture();
}

// ================================================================== editor

MacroKnob& Editor::knob (Page& page, const char* id, const juce::String& label, float diameter)
{
    auto* k = knobs.add (new MacroKnob (label, diameter));
    k->palette = &palette;
    sliderAttachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (id), *k));
    k->getProperties().set ("id", id);
    page.addAndMakeVisible (k);
    setupModulation (*k, id);
    return *k;
}

juce::TextButton& Editor::toggle (Page& page, const char* id, const juce::String& text)
{
    auto* b = toggles.add (new juce::TextButton (text));
    b->setClickingTogglesState (true);
    buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (id), *b));
    b->getProperties().set ("id", juce::String (id) + (&page == &simple ? "_s" : "_a"));
    page.addAndMakeVisible (b);
    return *b;
}

void Editor::setupModulation (MacroKnob& k, const char* id)
{
    auto target = Modulation::targetIndex (id);
    if (target < 0) return;
    k.modDepth = [this, target] (int src) { return proc.mod.getDepth (src, target); };
    k.modOffset = [this, target] { return proc.mod.getOffset (target); };
    k.onModDrop = [this, target, &k] (int src) { proc.mod.addRoute (src, target, 0.5f); k.repaint(); };
    k.onModMenu = [this, target, &k]
    {
        juce::PopupMenu m;
        m.setLookAndFeel (&lnf);
        bool any = false;
        for (int src = 0; src < Modulation::numSources; ++src)
        {
            auto d = proc.mod.getDepth (src, target);
            if (std::abs (d) < 0.001f) continue;
            any = true;
            juce::PopupMenu sub;
            for (auto amt : { 0.1f, 0.25f, 0.5f, 0.75f, 1.0f, -0.25f, -0.5f, -1.0f })
                sub.addItem (src * 100 + 10 + (int) std::round ((amt + 1.0f) * 20.0f),
                             (amt > 0 ? "+" : "") + juce::String (juce::roundToInt (amt * 100.0f)) + "%", true, std::abs (d - amt) < 0.01f);
            sub.addSeparator();
            sub.addItem (src * 100 + 1, "Remove");
            m.addSubMenu (Modulation::sourceNames()[src] + "  (" + juce::String (juce::roundToInt (d * 100.0f)) + "%)", sub);
        }
        if (! any) m.addItem (-1, "Drag MOD 1, MOD 2, ENV or RND here to modulate this knob", false);
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, target, &k] (int r)
        {
            if (r <= 0) return;
            auto src = r / 100, code = r % 100;
            if (code == 1) proc.mod.removeRoute (src, target);
            else proc.mod.setDepth (src, target, (float) (code - 10) / 20.0f - 1.0f);
            k.repaint();
        });
    };
}

Editor::Editor (Processor& pr)
    : AudioProcessorEditor (pr), proc (pr), stepBars (pr.state, pr.engine), tapeCurve (pr.state),
      fxChain (pr.state), mod1View (pr.state, pr.mod), mod2View (pr.state, pr.mod)
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (background);
    content.addAndMakeVisible (simple);
    content.addChildComponent (advanced);

    // ---- header (shared)
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { savePreset(); };
    themeButton.onClick = [this] { proc.lightTheme = ! proc.lightTheme; applyTheme(); };
    simpleButton.onClick = [this] { setAdvanced (false); };
    advancedButton.onClick = [this] { setAdvanced (true); };
    licenseButton.onClick = [this] { showActivation (true); };
    for (auto* b : { &prevButton, &nextButton, &presetButton, &saveButton, &themeButton, &simpleButton, &advancedButton, &licenseButton })
        content.addAndMakeVisible (b);

    // ---- SIMPLE
    galaxy.palette = &palette;
    simple.addAndMakeVisible (galaxy);
    auto& s = proc.state;
    stepRow = std::make_unique<ChoiceRow> (*s.getParameter (IDs::stepMode), std::vector<int> { 0, 1, 2, 3, 4 }, juce::StringArray { "OFF", "+5", "+7", "+12", "-12" });
    everyRow = std::make_unique<ChoiceRow> (*s.getParameter (IDs::stepRate), std::vector<int> { 2, 3, 4, 6 }, juce::StringArray { "1/4", "1/2", "1 BAR", "4 BAR" });
    tapeRow = std::make_unique<ChoiceRow> (*s.getParameter (IDs::tapeEvery), std::vector<int> { 0, 1, 2, 3 }, juce::StringArray { "OFF", "1 BAR", "2 BAR", "4 BAR" });
    for (auto* r : { stepRow.get(), everyRow.get(), tapeRow.get() }) simple.addAndMakeVisible (r);
    toggle (simple, IDs::particles, "PARTICLES");
    toggle (simple, IDs::cloud, "GRAIN CLOUD");
    toggle (simple, IDs::reverseFx, "REVERSE");
    toggle (simple, IDs::freeze, "FREEZE");
    toggle (simple, IDs::shimmer, "SHIMMER");
    knob (simple, IDs::motion, "MOTION", 100); knob (simple, IDs::texture, "TEXTURE", 100); knob (simple, IDs::pitch, "PITCH", 100);
    knob (simple, IDs::space, "SPACE", 100);   knob (simple, IDs::dirt, "DIRT", 100);       knob (simple, IDs::mix, "MIX", 100);

    // ---- ADVANCED
    for (auto [id, label] : std::initializer_list<std::pair<const char*, const char*>> {
             { IDs::size, "SIZE" }, { IDs::density, "DENSITY" }, { IDs::position, "POSITION" }, { IDs::spray, "SPRAY" },
             { IDs::grainPitch, "PITCH" }, { IDs::stereo, "STEREO" }, { IDs::shape, "SHAPE" }, { IDs::reverse, "REVERSE" },
             { IDs::gdTime, "TIME" }, { IDs::gdFeedback, "FEEDBACK" }, { IDs::gdHighPass, "HP" }, { IDs::gdLowPass, "LP" },
             { IDs::glide, "GLIDE" }, { IDs::chance, "CHANCE" }, { IDs::tapeLength, "LENGTH" }, { IDs::tapeStart, "START" },
             { IDs::tapeChance, "CHANCE " }, { IDs::input, "INPUT" }, { IDs::output, "OUTPUT" } })
        knob (advanced, id, label, 44);
    toggle (advanced, IDs::freeze, "FREEZE");

    // FX CHAIN + MODULATION
    fxChain.palette = &palette;
    advanced.addAndMakeVisible (fxChain);
    mod1View.palette = mod2View.palette = &palette;
    advanced.addAndMakeVisible (mod1View);
    advanced.addAndMakeVisible (mod2View);
    for (int src = 0; src < Modulation::numSources; ++src)
    {
        auto* c = chips.add (new ModChip (src));
        c->palette = &palette;
        c->mod = &proc.mod;
        c->setTooltip ("Drag onto any knob to modulate it (right-click the knob to set the amount)");
        advanced.addAndMakeVisible (c);
    }
    mod1Random.onClick = [this]
    {
        juce::Random r;
        for (int i = 0; i < 8; ++i)
            if (auto* prm = proc.state.getParameter (IDs::mod1Step (i)))
                prm->setValueNotifyingHost (r.nextFloat());
    };
    mod1Clear.onClick = [this]
    {
        for (int i = 0; i < 8; ++i)
            if (auto* prm = proc.state.getParameter (IDs::mod1Step (i))) prm->setValueNotifyingHost (0.0f);
    };
    advanced.addAndMakeVisible (mod1Random);
    advanced.addAndMakeVisible (mod1Clear);
    mod1Rate = std::make_unique<ChoiceRow> (*s.getParameter (IDs::mod1Rate), std::vector<int> { 0, 1, 2, 3, 4 }, juce::StringArray { "1/16", "1/8", "1/4", "1/2", "1 BAR" });
    mod2Rate = std::make_unique<ChoiceRow> (*s.getParameter (IDs::mod2Rate), std::vector<int> { 2, 3, 4, 5, 6 }, juce::StringArray { "1/4", "1/2", "1 BAR", "2 BAR", "4 BAR" });
    rndRate = std::make_unique<ChoiceRow> (*s.getParameter (IDs::rndRate), std::vector<int> { 1, 2, 3 }, juce::StringArray { "1/8", "1/4", "1/2" });
    for (auto* r : { mod1Rate.get(), mod2Rate.get(), rndRate.get() }) advanced.addAndMakeVisible (r);
    knob (advanced, IDs::mod1Swing, "SWING", 36);
    knob (advanced, IDs::mod2Swing, "SWING ", 36);

    gdMixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gdMixSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    sliderAttachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*s.getParameter (IDs::gdMix), gdMixSlider));
    advanced.addAndMakeVisible (gdMixSlider);

    stepBars.palette = &palette;
    tapeCurve.palette = &palette;
    advanced.addAndMakeVisible (stepBars);
    advanced.addAndMakeVisible (tapeCurve);
    advStepRate = std::make_unique<ChoiceRow> (*s.getParameter (IDs::stepRate), std::vector<int> { 0, 1, 2, 3, 4 }, juce::StringArray { "1/16", "1/8", "1/4", "1/2", "1 BAR" });
    advScale = std::make_unique<ChoiceRow> (*s.getParameter (IDs::scaleLock), std::vector<int> { 0, 1, 2, 3 }, juce::StringArray { "FREE", "MAJ", "MIN", "PENTA" });
    advEvery = std::make_unique<ChoiceRow> (*s.getParameter (IDs::tapeEvery), std::vector<int> { 0, 1, 2, 3 }, juce::StringArray { "OFF", "1 BAR", "2 BAR", "4 BAR" });
    tapeTrigger = std::make_unique<ChoiceRow> (*s.getParameter (IDs::tapeTrigger), std::vector<int> { 0, 1, 2 }, juce::StringArray { "AUTO", "MANUAL", "MIDI" });
    for (auto* r : { advStepRate.get(), advScale.get(), advEvery.get(), tapeTrigger.get() }) advanced.addAndMakeVisible (r);
    tapeNow.onClick = [this] { proc.engine.triggerTapeStop(); };
    tapeNow.setTooltip ("MANUAL: stop the tape now");
    advanced.addAndMakeVisible (tapeNow);
    miniGalaxy.palette = &palette;
    miniGalaxy.framed = false;
    advanced.addAndMakeVisible (miniGalaxy);

    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->applyTheme(); safe->setAdvanced (safe->proc.advanced); } });
    };

    applyTheme();
    setAdvanced (proc.advanced);
    setResizable (true, true);
    setResizeLimits (baseWidth * 7 / 10, baseHeight * 7 / 10, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (baseWidth, baseHeight);

    if (! proc.license.isLicensed() && (! activationShownThisSession || ! proc.license.isUsable()))
    {
        activationShownThisSession = true;
        showActivation (true);
    }
    startTimerHz (30);

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png [LT_LIGHT=1] [LT_ADVANCED=1]
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
        if (juce::SystemStats::getEnvironmentVariable ("LT_LIGHT", {}).isNotEmpty()) themeButton.triggerClick();
        if (juce::SystemStats::getEnvironmentVariable ("LT_ADVANCED", {}).isNotEmpty()) setAdvanced (true);
        juce::Timer::callAfterDelay (500, [this]
        {
            proc.engine.prepare (48000.0, 512);
            juce::AudioBuffer<float> b (2, 512);
            Transport t; t.playing = t.hasPosition = true;
            for (int blk = 0; blk < 90; ++blk)
            {
                for (int i = 0; i < 512; ++i)
                {
                    auto tt = (blk * 512 + i) / 48000.0;
                    auto v = (float) (0.3 * std::sin (tt * 1382.0) + 0.2 * std::sin (tt * 2074.0));
                    b.setSample (0, i, v); b.setSample (1, i, v);
                }
                NebulaSettings demo; demo.particles = true;
                proc.engine.process (b, demo, t);
                t.ppq += 512 / 24000.0;
            }
        });
        juce::Timer::callAfterDelay (3000, [this, snapshot]
        {
            auto img = content.createComponentSnapshot (content.getLocalBounds(), true, 1.0f);
            juce::File (snapshot).deleteFile();
            juce::FileOutputStream out { juce::File (snapshot) };
            juce::PNGImageFormat().writeImageToStream (img, out);
            out.flush();
            juce::JUCEApplicationBase::quit();
        });
    }
}

Editor::~Editor()
{
    proc.onStateRestored = nullptr;
    setLookAndFeel (nullptr);
}

void Editor::applyTheme()
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    lnf.setPalette (palette);
    background.setPalette (palette);
    if (activation) activation->setPalette (palette);
    content.repaint();
    galaxy.repaint();
}

void Editor::setAdvanced (bool adv)
{
    proc.advanced = adv;
    simple.setVisible (! adv);
    advanced.setVisible (adv);
    simpleButton.setToggleState (! adv, juce::dontSendNotification);
    advancedButton.setToggleState (adv, juce::dontSendNotification);
}

void Editor::timerCallback()
{
    auto& e = proc.engine;
    auto& s = proc.state;
    GalaxyState g;
    g.motion = s.getRawParameterValue (IDs::motion)->load() * 0.01f;
    g.texture = s.getRawParameterValue (IDs::texture)->load() * 0.01f;
    g.space = s.getRawParameterValue (IDs::space)->load() * 0.01f;
    g.dirt = s.getRawParameterValue (IDs::dirt)->load() * 0.01f;
    g.frozen = s.getRawParameterValue (IDs::freeze)->load() > 0.5f;
    g.tapeSpeed = e.getTapeSpeed();
    g.level = juce::jlimit (0.0f, 1.0f, e.getWetLevel() * 2.0f);
    g.grains = e.getGrainCount();
    g.stepFlash = e.getStepFlash();
    (proc.advanced ? miniGalaxy : galaxy).tick (g);
    background.setEnergy (g.level);
    background.setFrozen (g.frozen);
    background.setDriftSpeed ((0.3f + g.motion) * g.tapeSpeed);

    auto fall = [] (float& v, float target) { v = target > v ? target : v * 0.9f + target * 0.1f; };
    fall (inL, e.getInputLevel (0)); fall (inR, e.getInputLevel (1));
    fall (outL, e.getOutputLevel (0)); fall (outR, e.getOutputLevel (1));
    if (proc.advanced) advanced.repaint (970, 360, 110, 240);

    auto name = proc.presetName + (proc.isPresetModified() ? " *" : "");
    if (presetButton.getButtonText() != name) presetButton.setButtonText (name);

    auto chip = proc.license.isLicensed() ? juce::String() : proc.license.isUsable()
                  ? juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (proc.license.getTrialDaysLeft()) + " DAYS"
                  : juce::String ("ACTIVATE");
    if (licenseButton.getButtonText() != chip) licenseButton.setButtonText (chip);
    licenseButton.setVisible (chip.isNotEmpty());
}

// ------------------------------------------------------------------ layout

void Editor::resized()
{
    auto scale = (float) getWidth() / (float) baseWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, baseWidth, baseHeight);
    layout();
}

void Editor::layout()
{
    background.setBounds (0, 0, baseWidth, baseHeight);
    simple.setBounds (0, 0, baseWidth, baseHeight);
    advanced.setBounds (0, 0, baseWidth, baseHeight);

    prevButton.setBounds (80, 30, 34, 32);
    presetButton.setBounds (114, 30, 150, 32);
    nextButton.setBounds (264, 30, 34, 32);
    saveButton.setBounds (308, 30, 64, 32);
    simpleButton.setBounds (776, 30, 86, 32);
    advancedButton.setBounds (862, 30, 108, 32);
    themeButton.setBounds (985, 30, 80, 32);
    licenseButton.setBounds (480, 586, 140, 22);

    auto find = [this] (const char* id) -> juce::Component*
    {
        for (auto* k : knobs) if (k->getProperties()["id"].toString() == id && k->getParentComponent() == (proc.advanced ? (juce::Component*) &advanced : &simple)) return k;
        return nullptr;
    };
    juce::ignoreUnused (find);

    // SIMPLE
    galaxy.setBounds (550 - 162, 246 - 162, 324, 324);
    stepRow->setBounds (36, 182, 300, 32);
    everyRow->setBounds (36, 298, 300, 32);
    tapeRow->setBounds (780, 182, 286, 32);
    for (auto* b : toggles)
    {
        auto id = b->getProperties()["id"].toString();
        if (id == juce::String (IDs::particles) + "_s") b->setBounds (780, 270, 134, 32);
        if (id == juce::String (IDs::cloud) + "_s")     b->setBounds (920, 270, 144, 32);
        if (id == juce::String (IDs::reverseFx) + "_s") b->setBounds (780, 312, 92, 32);
        if (id == juce::String (IDs::freeze) + "_s")    b->setBounds (878, 312, 84, 32);
        if (id == juce::String (IDs::shimmer) + "_s")   b->setBounds (968, 312, 96, 32);

        if (id == juce::String (IDs::freeze) + "_a")    b->setBounds (668, 112, 96, 28);
    }

    const int mx[] = { 117, 290, 463, 636, 810, 983 };
    int mi = 0;
    for (auto* k : knobs)
        if (k->getParentComponent() == &simple)
            k->setBounds (mx[mi] - 70, 420, 140, 170), ++mi;

    // ADVANCED
    struct Pos { const char* id; int x, y; };
    static const Pos pos[] = {
        { IDs::size, 72, 132 }, { IDs::density, 150, 132 }, { IDs::position, 228, 132 }, { IDs::spray, 306, 132 },
        { IDs::grainPitch, 384, 132 }, { IDs::stereo, 462, 132 }, { IDs::shape, 540, 132 }, { IDs::reverse, 618, 132 },
        { IDs::gdTime, 818, 132 }, { IDs::gdFeedback, 892, 132 }, { IDs::gdHighPass, 966, 132 }, { IDs::gdLowPass, 1040, 132 },
        { IDs::glide, 420, 262 }, { IDs::chance, 492, 262 }, { IDs::tapeLength, 852, 262 }, { IDs::tapeStart, 922, 262 },
        { IDs::tapeChance, 992, 262 }, { IDs::input, 1004, 420 }, { IDs::output, 1004, 520 },
        { IDs::mod1Swing, 360, 418 }, { IDs::mod2Swing, 360, 522 } };
    for (auto* k : knobs)
        if (k->getParentComponent() == &advanced)
            for (auto& p : pos)
                if (k->getProperties()["id"].toString() == p.id)
                    k->setBounds (p.x - 38, p.y - 34, 76, 80);

    gdMixSlider.setBounds (858, 176, 210, 20);
    stepBars.setBounds (44, 232, 320, 92);
    advStepRate->setBounds (390, 304, 170, 22);
    advScale->setBounds (390, 330, 170, 22);
    tapeCurve.setBounds (580, 232, 232, 74);
    advEvery->setBounds (580, 314, 232, 22);
    tapeTrigger->setBounds (838, 304, 160, 22);
    tapeNow.setBounds (1004, 304, 66, 22);
    miniGalaxy.setBounds (550 - 120, 482 - 112, 240, 224);
    fxChain.setBounds (720, 378, 226, 222);
    chips[0]->setBounds (40, 382, 70, 22);
    chips[1]->setBounds (40, 486, 70, 22);
    chips[2]->setBounds (180, 354, 58, 22);
    chips[3]->setBounds (244, 354, 58, 22);
    mod1View.setBounds (40, 408, 280, 50);
    mod1Rate->setBounds (40, 462, 200, 20);
    mod1Random.setBounds (246, 462, 52, 20);
    mod1Clear.setBounds (302, 462, 46, 20);
    mod2View.setBounds (40, 512, 280, 52);
    mod2Rate->setBounds (40, 568, 200, 20);
    rndRate->setBounds (308, 356, 130, 20);

    if (activation) activation->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::Page::paint (juce::Graphics& g)
{
    auto& e = owner;
    auto& p = e.palette;
    Glass::drawPanel (g, { 12.0f, 10.0f, (float) baseWidth - 24.0f, (float) baseHeight - 20.0f }, 14.0f, p, 0.25f);
    Logo::draw (g, { 400.0f, 24.0f, 300.0f, 44.0f }, Logo::specFor ("NEBULA"), p, juce::Justification::centred);

    // bank cover (LT COSMOS)
    auto cover = juce::Rectangle<float> (36.0f, 30.0f, 34.0f, 32.0f);
    juce::ColourGradient cg (p.nebulaC, cover.getCentreX(), cover.getCentreY(), p.background, cover.getRight(), cover.getBottom(), true);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (cover, 4.0f);
    g.setColour (p.glassEdge);
    g.drawRoundedRectangle (cover, 4.0f, 1.0f);

    auto label = [&] (const juce::String& t, juce::Rectangle<float> r, float size, juce::Justification j = juce::Justification::centred)
    {
        g.setColour (p.text);
        Fonts::drawSpaced (g, t, r, Fonts::regular (size), 0.04f, j);
    };

    if (! advanced)
    {
        label ("PITCH STEP", { 36.0f, 150.0f, 300.0f, 24.0f }, 19.0f);
        label ("EVERY", { 36.0f, 266.0f, 300.0f, 24.0f }, 19.0f);
        label ("TAPE STOP", { 780.0f, 150.0f, 286.0f, 24.0f }, 19.0f);
        label ("FX", { 780.0f, 238.0f, 286.0f, 24.0f }, 19.0f);
        return;
    }

    auto header = [&] (const juce::String& t, float x, float y) { label (t, { x, y, 300.0f, 24.0f }, 18.0f, juce::Justification::centredLeft); };
    header ("GRAIN ENGINE", 40.0f, 80.0f);
    header ("GRAIN DELAY", 790.0f, 80.0f);
    header ("PITCH STEP", 40.0f, 202.0f);
    header ("TAPE STOP", 578.0f, 202.0f);
    header ("MODULATION", 40.0f, 352.0f);
    header ("FX CHAIN", 720.0f, 352.0f);
    label ("NEBULA", { 450.0f, 352.0f, 200.0f, 24.0f }, 18.0f);
    label ("DRY/WET", { 790.0f, 174.0f, 70.0f, 24.0f }, 13.0f, juce::Justification::centredLeft);

    g.setColour (p.glassEdge);
    g.drawHorizontalLine (196, 36.0f, (float) baseWidth - 36.0f);
    g.drawHorizontalLine (346, 36.0f, (float) baseWidth - 36.0f);
    g.drawVerticalLine (772, 82.0f, 190.0f);
    g.drawVerticalLine (566, 206.0f, 340.0f);

    g.setColour (p.textDim);
    g.setFont (Fonts::regular (11.0f));
    g.drawText ("drag a chip onto any knob", juce::Rectangle<float> (120.0f, 384.0f, 200.0f, 18.0f), juce::Justification::centredLeft);

    // meters
    auto meter = [&] (float x, float l, float r, const juce::String& name)
    {
        for (int c = 0; c < 2; ++c)
        {
            auto v = c == 0 ? l : r;
            auto db = juce::Decibels::gainToDecibels (v, -60.0f);
            auto f = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
            auto bar = juce::Rectangle<float> (x + (float) c * 9.0f, 380.0f, 5.0f, 196.0f);
            g.setColour (p.text.withAlpha (0.12f));
            g.fillRoundedRectangle (bar, 2.5f);
            auto fill = bar.withTop (bar.getBottom() - bar.getHeight() * f);
            g.setColour (p.isLight ? juce::Colour (0xfffff176) : p.accent.brighter (0.3f));
            g.fillRoundedRectangle (fill, 2.5f);
        }
        g.setColour (p.text);
        g.setFont (Fonts::regular (12.0f));
        g.drawText (name, juce::Rectangle<float> (x - 18.0f, 580.0f, 50.0f, 16.0f), juce::Justification::centred);
    };
    meter (960.0f, e.inL, e.inR, "INPUT");
    meter (1046.0f, e.outL, e.outR, "OUTPUT");
}

// ------------------------------------------------------------------ presets

void Editor::showPresetMenu()
{
    proc.presets.rescan();
    auto& all = proc.presets.all();
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    m.addSectionHeader ("LT COSMOS");
    // the factory bank, grouped by its first tag
    std::map<juce::String, juce::PopupMenu> byTag;
    for (int i = 0; i < (int) all.size(); ++i)
        if (all[(size_t) i].factory)
        {
            auto tag = juce::StringArray::fromTokens (all[(size_t) i].group, ",", "")[0];
            byTag[tag].addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.presetName);
        }
    for (auto& [tag, sub] : byTag) m.addSubMenu (tag, sub);
    bool anyUser = false;
    for (int i = 0; i < (int) all.size(); ++i)
        if (! all[(size_t) i].factory)
        {
            if (! anyUser) { m.addSeparator(); m.addSectionHeader ("MY BANK"); anyUser = true; }
            m.addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.presetName);
        }
    m.addSeparator();
    m.addItem (10001, "Save as...");
    auto ci = proc.presets.indexOf (proc.presetName);
    m.addItem (10002, "Delete \"" + proc.presetName + "\"", ci >= 0 && ! all[(size_t) ci].factory);
    m.addItem (10003, "Open presets folder");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton), [this] (int r)
    {
        auto& list = proc.presets.all();
        if (r >= 1 && r <= (int) list.size()) proc.applyPreset (list[(size_t) r - 1]);
        else if (r == 10001) savePreset();
        else if (r == 10002) proc.presets.remove (proc.presetName);
        else if (r == 10003) { proc.presets.userFolder().createDirectory(); proc.presets.userFolder().startAsProcess(); }
    });
}

void Editor::stepPreset (int delta)
{
    proc.presets.rescan();
    auto& all = proc.presets.all();
    if (all.empty()) return;
    auto n = (int) all.size();
    auto i = proc.presets.indexOf (proc.presetName);
    i = ((i < 0 ? 0 : i + delta) % n + n) % n;
    proc.applyPreset (all[(size_t) i]);
}

void Editor::savePreset()
{
    auto ci = proc.presets.indexOf (proc.presetName);
    auto suggested = ci >= 0 && ! proc.presets.all()[(size_t) ci].factory ? proc.presetName : juce::String();
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name for your preset:", juce::MessageBoxIconType::NoIcon);
    saveDialog->setLookAndFeel (&lnf);
    saveDialog->addTextEditor ("name", suggested);
    saveDialog->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        auto name = saveDialog->getTextEditorContents ("name").trim();
        saveDialog.reset();
        if (result != 1 || name.isEmpty()) return;
        if (proc.presets.save (lt::presets::ParamPreset::capture (proc.state, name, "MY BANK")))
            proc.presetName = name;
    }), false);
}

void Editor::showActivation (bool show)
{
    if (show && activation == nullptr)
    {
        proc.license.refresh();
        activation = std::make_unique<lt::ui::ActivationComponent> (proc.license, palette);
        activation->onDismiss = [this] { juce::MessageManager::callAsync ([this] { showActivation (false); }); };
        content.addAndMakeVisible (*activation);
        activation->setBounds (0, 0, baseWidth, baseHeight);
    }
    else if (! show && activation != nullptr)
    {
        proc.refreshLicense();
        if (! proc.license.isUsable()) return;
        content.removeChildComponent (activation.get());
        activation.reset();
    }
}

} // namespace nebula
