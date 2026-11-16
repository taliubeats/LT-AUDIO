#include "PluginEditor.h"

namespace pulsar
{

using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Logo;

bool Editor::activationShownThisSession = false;

Editor::Editor (Processor& p)
    : AudioProcessorEditor (p), proc (p), focusView (p.state, p.engine.spectrum, "PULSAR"), curveView (p.engine)
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    setLookAndFeel (&lnf);

    addAndMakeVisible (content);
    content.addAndMakeVisible (background);
    content.addAndMakeVisible (page);
    content.addChildComponent (focusView);
    focusView.onClose = [this] { showFocus (false); };

    // ---- header
    eqButton.setTooltip ("Frequency Focus: duck only part of the spectrum");
    eqButton.onClick = [this] { showFocus (true); };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { savePreset(); };
    themeButton.onClick = [this]
    {
        proc.lightTheme = ! proc.lightTheme;
        palette = proc.lightTheme ? Palette::light() : Palette::dark();
        applyTheme();
    };
    licenseButton.onClick = [this] { showActivation (true); };
    for (auto* b : { &eqButton, &prevButton, &nextButton, &presetButton, &saveButton, &themeButton, &licenseButton })
        page.addAndMakeVisible (b);
    prevButton.getProperties().set ("flat", true);
    nextButton.getProperties().set ("flat", true);

    midiAttachment = std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (IDs::midi), midiToggle);
    midiToggle.setTooltip ("Duck every time a MIDI note arrives (route a MIDI track to LT PULSAR)");
    page.addAndMakeVisible (midiToggle);

    // ---- knob
    knob.palette = &palette;
    mixAttachment = std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (IDs::mix), knob);
    page.addAndMakeVisible (knob);

    // ---- visualiser
    curveView.palette = &palette;
    curveView.getCurve = [this] () -> const Curve& { return proc.getCurve(); };
    curveView.onCurveChanged = [this] (const Curve& c) { proc.setCurve (c); };
    page.addAndMakeVisible (curveView);

    waveToggle.setToggleState (proc.showWave, juce::dontSendNotification);
    waveToggle.onClick = [this] { proc.showWave = waveToggle.getToggleState(); curveView.showWave = proc.showWave; };
    curveView.showWave = proc.showWave;
    page.addAndMakeVisible (waveToggle);

    // ---- shapes and rates
    for (int i = 0; i < Curve::numShapes; ++i)
    {
        auto b = std::make_unique<ShapeButton> (i);
        b->palette = &palette;
        b->onClick = [this, i]
        {
            auto s = Curve::shape (i);
            s.setOffset (proc.getCurve().getOffset());
            proc.setCurve (s);
        };
        page.addAndMakeVisible (*b);
        shapeButtons.push_back (std::move (b));
    }

    auto* rateParam = proc.state.getParameter (IDs::rate);
    rateAttachment = std::make_unique<juce::ParameterAttachment> (*rateParam, [this] (float v)
    {
        auto idx = juce::roundToInt (v);
        for (int i = 0; i < Rates::count; ++i)
            rateButtons[i].setToggleState (i == idx, juce::dontSendNotification);
        rateTabs.setRate (idx);
    });
    rateTabs.palette = &palette;
    rateTabs.onChange = [this] (int r) { rateAttachment->setValueAsCompleteGesture ((float) r); };
    page.addAndMakeVisible (rateTabs);
    for (int i = 0; i < Rates::count; ++i)
    {
        rateButtons[i].setButtonText (Rates::names()[i]);
        rateButtons[i].onClick = [this, i] { rateAttachment->setValueAsCompleteGesture ((float) i); };
        page.addAndMakeVisible (rateButtons[i]);
    }
    rateAttachment->sendInitialUpdate();

    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->syncFromProcessor(); });
    };

    applyTheme();
    syncFromProcessor();

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

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png [LT_LIGHT=1] [LT_SHOW_EQ=1] [LT_DEMO=1]
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
        if (juce::SystemStats::getEnvironmentVariable ("LT_LIGHT", {}).isNotEmpty()) themeButton.triggerClick();
        auto presetName = juce::SystemStats::getEnvironmentVariable ("LT_PRESET", {});
        if (presetName.isNotEmpty() && proc.presets.indexOf (presetName) >= 0)
            proc.applyPreset (proc.presets.all()[(size_t) proc.presets.indexOf (presetName)]);
        if (juce::SystemStats::getEnvironmentVariable ("LT_SHOW_EQ", {}).isNotEmpty())
        {
            if (auto* b = proc.state.getParameter (lt::dsp::FocusParams::band)) b->setValueNotifyingHost (b->convertTo0to1 (1.0f));
            if (auto* s = proc.state.getParameter (lt::dsp::FocusParams::solo)) s->setValueNotifyingHost (1.0f);
            showFocus (true);
        }
        // feed a little synthetic beat through the engine so the visualiser has real audio
        juce::Timer::callAfterDelay (800, [this]
        {
            auto sr = 48000.0;
            proc.engine.prepare (sr, 2);
            proc.engine.setCurve (proc.getCurve());
            DuckEngine::Transport t;
            t.playing = t.hasPosition = true;
            t.bpm = 120.0;
            DuckEngine::Settings s;
            s.mix = proc.state.getRawParameterValue (IDs::mix)->load() * 0.01f;
            s.rate = juce::roundToInt (proc.state.getRawParameterValue (IDs::rate)->load());
            juce::Random rnd (5);
            juce::AudioBuffer<float> b (2, 512);
            for (int blk = 0; blk < 200; ++blk)
            {
                for (int i = 0; i < 512; ++i)
                {
                    auto time = (blk * 512 + i) / sr;
                    auto beatT = std::fmod (time, 0.5);
                    auto kick = (float) (std::exp (-beatT * 18.0) * std::sin (juce::MathConstants<double>::twoPi * (50.0 + 90.0 * std::exp (-beatT * 30.0)) * beatT));
                    auto pad = 0.25f * (float) (std::sin (time * 1382.0) + std::sin (time * 1745.0) + 0.6 * std::sin (time * 2074.0)) * (0.7f + 0.3f * rnd.nextFloat());
                    b.setSample (0, i, 0.6f * kick + pad);
                    b.setSample (1, i, 0.6f * kick + pad);
                }
                t.ppq = (blk * 512) / (sr * 0.5);
                proc.engine.process (b, {}, s, t, {});
            }
        });

        juce::Timer::callAfterDelay (3500, [this, snapshot]
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
    lnf.setPalette (palette);
    background.setPalette (palette);
    focusView.setPalette (palette);
    if (activation) activation->setPalette (palette);
    content.repaint();
    for (auto* c : page.getChildren())
        c->repaint();
}

void Editor::syncFromProcessor()
{
    if (proc.lightTheme != palette.isLight)
    {
        palette = proc.lightTheme ? Palette::light() : Palette::dark();
        applyTheme();
    }
    waveToggle.setToggleState (proc.showWave, juce::dontSendNotification);
    curveView.showWave = proc.showWave;
    content.repaint();
}

// ------------------------------------------------------------------ timer

void Editor::timerCallback()
{
    auto& e = proc.engine;
    auto cycles = e.getCycleCount();
    auto beat = cycles != lastCycle ? 1.0f : 0.0f;
    lastCycle = cycles;

    auto mix = (float) knob.valueToProportionOfLength (knob.getValue());
    auto level = juce::jlimit (0.0f, 1.0f, e.getInputLevel() * 1.4f);
    knob.tick (beat * juce::jmin (1.0f, 0.3f + level * 2.0f), level, e.getCurrentGain());

    background.setParticleAmount (0.25f + 0.75f * mix);
    background.setEnergy (level * (1.0f - (1.0f - e.getCurrentGain()) * mix));
    if (beat > 0.0f) background.pulse (0.4f + 0.6f * mix);

    if (page.isVisible())
        curveView.tick();

    auto shape = proc.getMatchingShape();
    for (int i = 0; i < (int) shapeButtons.size(); ++i)
        if (shapeButtons[(size_t) i]->getToggleState() != (i == shape))
            shapeButtons[(size_t) i]->setToggleState (i == shape, juce::dontSendNotification);

    auto name = proc.getPresetName() + (proc.isPresetModified() ? " *" : "");
    if (presetButton.getButtonText() != name)
        presetButton.setButtonText (name);

    auto focusOn = juce::roundToInt (proc.state.getRawParameterValue (lt::dsp::FocusParams::band)->load()) != lt::dsp::FocusSettings::full;
    if (eqButton.getToggleState() != focusOn)
        eqButton.setToggleState (focusOn, juce::dontSendNotification);

    auto chipText = proc.license.isLicensed() ? juce::String() : proc.license.isUsable()
                      ? juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (proc.license.getTrialDaysLeft()) + " DAYS"
                      : juce::String ("ACTIVATE");
    if (licenseButton.getButtonText() != chipText) licenseButton.setButtonText (chipText);
    licenseButton.setVisible (chipText.isNotEmpty());
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
    page.setBounds (0, 0, baseWidth, baseHeight);
    focusView.setBounds (0, 0, baseWidth, baseHeight);

    header = { 0, 0, baseWidth, 84 };
    auto y = 24, h = 38;
    eqButton.setBounds (300, y, 84, h);
    presetBox = { 396, y, 280, h };
    prevButton.setBounds (presetBox.withWidth (38));
    nextButton.setBounds (presetBox.withLeft (presetBox.getRight() - 38));
    presetButton.setBounds (presetBox.reduced (38, 0));
    saveButton.setBounds (688, y, 84, h);
    midiToggle.setBounds (786, y, 170, h);
    themeButton.setBounds (baseWidth - 30 - 104, y, 104, h);

    knobPanel = { 16, 90, baseWidth - 32, 280 };
    knob.setBounds (knobPanel.reduced (2));
    licenseButton.setBounds (knobPanel.getX() + 14, knobPanel.getY() + 12, 150, 30);

    wavePanel = { 16, 380, baseWidth - 32, 150 };
    curveView.setBounds (wavePanel.reduced (14, 10));
    waveToggle.setBounds (wavePanel.getRight() - 14 - 100, wavePanel.getY() + 18, 92, 30);
    rateTabs.setBounds (wavePanel.getX() + 20, wavePanel.getY() + 16, 200, 24);

    bottomBar = { 16, 540, baseWidth - 32, 70 };
    auto row = bottomBar.reduced (14, 8);
    for (auto& b : shapeButtons)
    {
        b->setBounds (row.removeFromLeft (64));
        row.removeFromLeft (9);
    }
    auto rates = bottomBar.reduced (14, 8);
    rates = rates.removeFromRight (Rates::count * 80);
    for (auto& b : rateButtons)
        b.setBounds (rates.removeFromLeft (80).reduced (2, 2));

    if (activation)
        activation->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::MainPage::paint (juce::Graphics& g)
{
    auto& e = owner;
    auto& p = e.palette;

    Logo::draw (g, { 36.0f, 21.0f, 250.0f, 44.0f }, Logo::specFor ("PULSAR"), p, juce::Justification::centredLeft);

    // preset box behind < name >
    Glass::drawPanel (g, e.presetBox.toFloat().reduced (1.5f), (float) e.presetBox.getHeight() * 0.5f, p, 1.0f);

    for (auto r : { e.knobPanel, e.wavePanel, e.bottomBar })
    {
        auto rf = r.toFloat();
        Glass::drawPanel (g, rf, 12.0f, p, 0.55f);
    }

    g.setColour (p.isLight ? p.text : p.value);
    Fonts::drawSpaced (g, "MIX", { (float) e.knobPanel.getCentreX() - 60.0f, (float) e.knobPanel.getBottom() - 42.0f, 120.0f, 24.0f },
                       Fonts::regular (17.0f), 0.12f, juce::Justification::centred);

    // tiny tempo readout under MIX
    auto info = e.proc.state.getRawParameterValue (IDs::midi)->load() > 0.5f
                    ? juce::String ("MIDI TRIGGER")
                    : juce::String (juce::roundToInt (e.proc.getHostBpm())) + " BPM" + (e.proc.isHostPlaying() ? "" : "  (free run)");
    g.setColour (p.isLight ? p.text.withAlpha (0.6f) : p.textDim.withAlpha (0.6f));
    g.setFont (Fonts::regular (11.0f));
    g.drawText (info, e.knobPanel.withTop (e.knobPanel.getBottom() - 20).toFloat(), juce::Justification::centred);
}

// ------------------------------------------------------------------ presets

void Editor::showPresetMenu()
{
    proc.presets.rescan();
    auto& all = proc.presets.all();

    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    m.addSectionHeader ("FACTORY");
    for (int i = 0; i < (int) all.size(); ++i)
        if (all[(size_t) i].factory)
            m.addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.getPresetName());

    bool anyUser = false;
    for (int i = 0; i < (int) all.size(); ++i)
        if (! all[(size_t) i].factory)
        {
            if (! anyUser) { m.addSeparator(); m.addSectionHeader ("MY PRESETS"); anyUser = true; }
            m.addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.getPresetName());
        }

    m.addSeparator();
    m.addItem (10001, "Save as...");
    auto current = proc.presets.indexOf (proc.getPresetName());
    m.addItem (10002, "Delete \"" + proc.getPresetName() + "\"", current >= 0 && ! all[(size_t) current].factory);
    m.addItem (10003, "Open presets folder");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton), [this] (int r)
    {
        auto& list = proc.presets.all();
        if (r >= 1 && r <= (int) list.size()) proc.applyPreset (list[(size_t) r - 1]);
        else if (r == 10001) savePreset();
        else if (r == 10002) proc.presets.remove (proc.getPresetName());
        else if (r == 10003)
        {
            PresetLibrary::userFolder().createDirectory();
            PresetLibrary::userFolder().startAsProcess();
        }
    });
}

void Editor::stepPreset (int delta)
{
    proc.presets.rescan();
    auto& all = proc.presets.all();
    if (all.empty()) return;
    auto i = proc.presets.indexOf (proc.getPresetName());
    auto n = (int) all.size();
    i = ((i < 0 ? 0 : i + delta) % n + n) % n;
    proc.applyPreset (all[(size_t) i]);
}

void Editor::savePreset()
{
    auto current = proc.presets.indexOf (proc.getPresetName());
    auto suggested = current >= 0 && ! proc.presets.all()[(size_t) current].factory ? proc.getPresetName() : juce::String();

    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name for your preset:", juce::MessageBoxIconType::NoIcon);
    saveDialog->setLookAndFeel (&lnf);
    saveDialog->addTextEditor ("name", suggested);
    saveDialog->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        auto name = saveDialog->getTextEditorContents ("name").trim();
        saveDialog.reset();
        if (result != 1 || name.isEmpty())
            return;

        auto p = proc.makePreset (name);
        if (proc.presets.save (p))
            proc.applyPreset (proc.presets.all()[(size_t) juce::jmax (0, proc.presets.indexOf (name))]);
    }), false);
}

// ------------------------------------------------------------------ pages

void Editor::showFocus (bool show)
{
    focusView.setVisible (show);
    page.setVisible (! show);
    background.setDriftSpeed (show ? 0.5f : 1.0f);
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
        if (! proc.license.isUsable())
            return; // trial over and not licensed: keep it up
        content.removeChildComponent (activation.get());
        activation.reset();
    }
}

} // namespace pulsar
