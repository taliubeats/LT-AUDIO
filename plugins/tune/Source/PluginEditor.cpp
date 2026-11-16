#include "PluginEditor.h"

namespace tune
{

using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Logo;

bool Editor::activationShownThisSession = false;

namespace
{
    juce::String signedInt (double v)
    {
        auto r = juce::roundToInt (v);
        return r == 0 ? juce::String ("0") : (r > 0 ? "+" : "") + juce::String (r);
    }
}

Editor::Editor (Processor& pr)
    : AudioProcessorEditor (pr), proc (pr), planet (*pr.state.getParameter (IDs::retune))
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (background);
    content.addAndMakeVisible (decor);

    tuner.palette = &palette;
    content.addAndMakeVisible (tuner);

    orbs.palette = &palette;
    orbs.onToggle = [this] (int pc) { proc.undo.beginNewTransaction(); toggleNote (proc.state, pc); };
    content.addAndMakeVisible (orbs);

    planet.palette = &palette;
    content.addAndMakeVisible (planet);

    std::pair<TuneKnob*, const char*> knobs[] = {
        { &humanize, IDs::humanize }, { &flex, IDs::flexTune }, { &vibrato, IDs::vibrato },
        { &formant, IDs::formant }, { &transpose, IDs::transpose }, { &mix, IDs::mix }, { &detune, IDs::detune } };
    for (auto& [k, id] : knobs)
    {
        k->palette = &palette;
        attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (id), *k));
        content.addAndMakeVisible (k);
    }
    humanize.formatValue = flex.formatValue = [] (double v) { return juce::String (juce::roundToInt (v)); };
    vibrato.formatValue = transpose.formatValue = signedInt;
    formant.formatValue = [] (double v)
    {
        auto r = std::round (v * 2.0) / 2.0;
        return std::abs (r - std::round (r)) > 0.01 ? (r > 0 ? "+" : "") + juce::String (r, 1) : signedInt (r);
    };
    mix.formatValue = [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; };
    vibrato.bipolar = formant.bipolar = transpose.bipolar = true;
    detune.compact = true;

    preserve.palette = &palette;
    preserveAttachment = std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (IDs::preserve), preserve);
    content.addAndMakeVisible (preserve);

    inMeter.palette = outMeter.palette = &palette;
    content.addAndMakeVisible (inMeter);
    content.addAndMakeVisible (outMeter);

    // top bar
    for (auto* pill : { &voicePill, &keyPill, &presetPill })
    {
        pill->palette = &palette;
        content.addAndMakeVisible (pill);
    }
    voicePill.onClick = [this] { showVoiceMenu(); };
    keyPill.onClick = [this] { showKeyMenu(); };
    presetPill.onClick = [this] { showPresetMenu(); };
    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { savePreset(); };
    matchButton.onClick = [this] { showMatch (true); };
    themeButton.onClick = [this] { proc.lightTheme = ! proc.lightTheme; applyTheme(); };
    licenseButton.onClick = [this] { showActivation (true); };
    for (auto* b : { &saveButton, &matchButton, &themeButton, &licenseButton })
        content.addAndMakeVisible (b);

    resetNotesButton.onClick = [this] { proc.undo.beginNewTransaction(); resetNotes (proc.state); };
    undoButton.onClick = [this] { proc.undo.undo(); };
    redoButton.onClick = [this] { proc.undo.redo(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };
    powerButton.onClick = [this]
    {
        auto bypassed = proc.state.getRawParameterValue (IDs::bypass)->load() > 0.5f;
        setParam (proc.state, IDs::bypass, bypassed ? 0.0f : 1.0f);
    };
    for (auto* b : { &prevPreset, &nextPreset, &resetNotesButton, &undoButton, &redoButton, &settingsButton, &powerButton })
    {
        b->palette = &palette;
        content.addAndMakeVisible (b);
    }

    // LOW LATENCY | HQ
    modeAttachment = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::mode), [this] (float v)
    {
        lowLatencyButton.setToggleState (v < 0.5f, juce::dontSendNotification);
        hqButton.setToggleState (v >= 0.5f, juce::dontSendNotification);
    });
    lowLatencyButton.onClick = [this] { modeAttachment->setValueAsCompleteGesture (0.0f); };
    hqButton.onClick = [this] { modeAttachment->setValueAsCompleteGesture (1.0f); };
    for (auto* b : { &lowLatencyButton, &hqButton })
    {
        b->getProperties().set ("flat", true);
        content.addAndMakeVisible (b);
    }
    modeAttachment->sendInitialUpdate();

    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->applyTheme(); });
    };

    applyTheme();
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

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png [LT_LIGHT=1] [LT_MATCH_FILE=/song.wav]
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
        if (juce::SystemStats::getEnvironmentVariable ("LT_LIGHT", {}).isNotEmpty()) themeButton.triggerClick();
        auto matchFile = juce::SystemStats::getEnvironmentVariable ("LT_MATCH_FILE", {});
        if (matchFile.isNotEmpty())
        {
            showMatch (true);
            match->analyse (juce::File (matchFile));
        }
        juce::Timer::callAfterDelay (matchFile.isNotEmpty() ? 6000 : 3000, [this, snapshot]
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
    match.reset();
    setLookAndFeel (nullptr);
}

void Editor::applyTheme()
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    lnf.setPalette (palette);
    background.setPalette (palette);
    if (activation) activation->setPalette (palette);
    if (match) match->setPalette (palette);
    content.repaint();
}

void Editor::timerCallback()
{
    auto& e = proc.engine;
    auto voiced = e.isVoiced();
    auto target = e.getTargetNote();
    tuner.update (voiced, target, e.getInputMidi());
    orbs.update (effectiveMask (proc.state), voiced && target >= 0 ? target % 12 : -1);

    auto retunes = e.getRetuneCount();
    if (retunes != lastRetunes && voiced) { planet.flash(); background.pulse (0.5f); }
    lastRetunes = retunes;
    planet.tick();

    inMeter.setLevel (e.getInputLevel());
    outMeter.setLevel (e.getOutputLevel());
    background.setEnergy (juce::jlimit (0.0f, 1.0f, e.getOutputLevel() * 1.6f));

    auto& s = proc.state;
    auto key = juce::roundToInt (s.getRawParameterValue (IDs::key)->load());
    auto scale = juce::roundToInt (s.getRawParameterValue (IDs::scale)->load());
    auto edited = s.getRawParameterValue (IDs::edited)->load() > 0.5f && scale != Scales::custom;
    keyPill.setValue (Scales::display (key, scale) + (edited ? "*" : ""));
    voicePill.setValue (Voices::names()[juce::roundToInt (s.getRawParameterValue (IDs::voice)->load())]);
    presetPill.setValue (proc.presetName + (proc.isPresetModified() ? " *" : ""));

    auto bypassed = s.getRawParameterValue (IDs::bypass)->load() > 0.5f;
    if (powerButton.active == bypassed) { powerButton.active = ! bypassed; powerButton.repaint(); }
    undoButton.setEnabled (proc.undo.canUndo());
    redoButton.setEnabled (proc.undo.canRedo());
    if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        proc.undo.beginNewTransaction();

    auto chip = proc.license.isLicensed() ? juce::String() : proc.license.isUsable()
                  ? juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (proc.license.getTrialDaysLeft()) + " DAYS"
                  : juce::String ("ACTIVATE");
    if (licenseButton.getButtonText() != chip) licenseButton.setButtonText (chip);
    licenseButton.setVisible (chip.isNotEmpty());
    decor.repaint (0, 540, 300, 74);   // DETUNE value
}

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
    decor.setBounds (0, 0, baseWidth, baseHeight);

    // top bar
    voicePill.setBounds (196, 16, 170, 40);
    keyPill.setBounds (374, 16, 206, 40);
    prevPreset.setBounds (588, 22, 22, 28);
    presetPill.setBounds (610, 16, 150, 40);
    nextPreset.setBounds (760, 22, 22, 28);
    saveButton.setBounds (790, 21, 64, 30);
    matchButton.setBounds (862, 21, 112, 30);
    themeButton.setBounds (982, 21, 92, 30);

    // tuner + notes
    tuner.setBounds (14, 72, 728, 240);
    resetNotesButton.setBounds (702, 332, 28, 28);
    orbs.setBounds (40, 356, 676, 170);

    // right column
    planet.setBounds (766, 80, 310, 222);
    const int cx[] = { 818, 922, 1026 };
    humanize.setBounds (cx[0] - 42, 312, 84, 100);
    flex.setBounds (cx[1] - 42, 312, 84, 100);
    vibrato.setBounds (cx[2] - 42, 312, 84, 100);
    formant.setBounds (cx[0] - 42, 424, 84, 100);
    transpose.setBounds (cx[1] - 42, 424, 84, 100);
    mix.setBounds (cx[2] - 42, 424, 84, 100);
    preserve.setBounds (cx[0] - 46, 520, 26, 16);

    // bottom bar
    detune.setBounds (150, 553, 48, 48);
    lowLatencyButton.setBounds (325, 560, 124, 34);
    hqButton.setBounds (451, 560, 100, 34);
    licenseButton.setBounds (566, 562, 112, 30);
    inMeter.setBounds (722, 567, 150, 9);
    outMeter.setBounds (722, 586, 150, 9);
    undoButton.setBounds (900, 556, 40, 40);
    redoButton.setBounds (946, 556, 40, 40);
    settingsButton.setBounds (992, 556, 40, 40);
    powerButton.setBounds (1038, 556, 40, 40);

    if (activation) activation->setBounds (0, 0, baseWidth, baseHeight);
    if (match) match->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::Decor::paint (juce::Graphics& g)
{
    auto& e = owner;
    auto& p = e.palette;
    auto smallText = p.isLight ? juce::Colour (0xff111111) : p.text;

    Logo::draw (g, { 22.0f, 14.0f, 166.0f, 42.0f }, Logo::specFor ("TUNE"), p, juce::Justification::centredLeft);

    Glass::drawPanel (g, { 14.0f, 72.0f, 728.0f, 240.0f }, 12.0f, p, 0.5f);
    Glass::drawPanel (g, { 14.0f, 322.0f, 728.0f, 212.0f }, 12.0f, p, 0.5f);
    Glass::drawPanel (g, { 756.0f, 72.0f, 330.0f, 470.0f }, 12.0f, p, 0.5f);

    g.setColour (smallText);
    Fonts::drawSpaced (g, "EDIT NOTES", { 14.0f, 332.0f, 728.0f, 26.0f }, Fonts::regular (17.0f), 0.04f, juce::Justification::centred);

    g.setColour (p.text.withAlpha (0.8f));
    g.setFont (Fonts::regular (10.0f));
    g.drawText ("PRESERVE", juce::Rectangle<float> (802.0f, 521.0f, 70.0f, 14.0f), juce::Justification::centredLeft);

    // bottom bar
    g.setColour (smallText);
    Fonts::drawSpaced (g, "DETUNE", { 30.0f, 552.0f, 110.0f, 22.0f }, Fonts::regular (16.0f), 0.04f, juce::Justification::centredLeft);
    g.setFont (Fonts::regular (20.0f));
    auto detuneText = juce::String (e.proc.state.getRawParameterValue (IDs::detune)->load(), 1) + " Hz";
    if (p.isLight)
    {
        g.setColour (juce::Colour (0x55000000));
        g.drawText (detuneText, juce::Rectangle<float> (30.0f, 575.2f, 120.0f, 26.0f), juce::Justification::centredLeft);
    }
    g.setColour (p.isLight ? juce::Colour (0xffffc300) : p.value);
    g.drawText (detuneText, juce::Rectangle<float> (30.0f, 574.0f, 120.0f, 26.0f), juce::Justification::centredLeft);
    g.setColour (smallText);
    Fonts::drawSpaced (g, "MODE", { 262.0f, 566.0f, 60.0f, 22.0f }, Fonts::regular (16.0f), 0.04f, juce::Justification::centredLeft);
    Glass::drawPanel (g, { 321.0f, 556.0f, 234.0f, 42.0f }, 21.0f, p, 0.8f);
    g.setFont (Fonts::regular (13.0f));
    g.drawText ("IN", juce::Rectangle<float> (690.0f, 563.0f, 30.0f, 16.0f), juce::Justification::centredLeft);
    g.drawText ("OUT", juce::Rectangle<float> (690.0f, 582.0f, 30.0f, 16.0f), juce::Justification::centredLeft);
}

// ------------------------------------------------------------------ menus

void Editor::showVoiceMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    m.addSectionHeader ("VOICE (detection range)");
    auto cur = juce::roundToInt (proc.state.getRawParameterValue (IDs::voice)->load());
    for (int i = 0; i < Voices::count; ++i)
        m.addItem (i + 1, Voices::names()[i] + "   (" + juce::String (juce::roundToInt (Voices::minHz (i))) + "-"
                           + juce::String (juce::roundToInt (Voices::maxHz (i))) + " Hz)", true, i == cur);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&voicePill), [this] (int r)
    {
        if (r > 0) { proc.undo.beginNewTransaction(); setParam (proc.state, IDs::voice, (float) (r - 1)); }
    });
}

void Editor::showKeyMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    auto key = juce::roundToInt (proc.state.getRawParameterValue (IDs::key)->load());
    auto scale = juce::roundToInt (proc.state.getRawParameterValue (IDs::scale)->load());
    m.addSectionHeader ("KEY");
    for (int i = 0; i < 12; ++i) m.addItem (100 + i, Keys::noteName (i), true, i == key);
    m.addSeparator();
    m.addSectionHeader ("SCALE");
    for (int i = 0; i < Scales::count; ++i) m.addItem (200 + i, Scales::names()[i], true, i == scale);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&keyPill), [this, key, scale] (int r)
    {
        if (r <= 0) return;
        proc.undo.beginNewTransaction();
        if (r >= 200) setKeyScale (proc.state, key, r - 200);
        else          setKeyScale (proc.state, r - 100, scale);
    });
}

void Editor::stepPreset (int delta)
{
    proc.presets.rescan();
    auto& all = proc.presets.all();
    if (all.empty()) return;
    auto i = proc.presets.indexOf (proc.presetName);
    i = ((i < 0 ? 0 : i + delta) + (int) all.size()) % (int) all.size();
    proc.undo.beginNewTransaction();
    proc.applyPreset (all[(size_t) i]);
}

void Editor::showPresetMenu()
{
    proc.presets.rescan();
    auto& all = proc.presets.all();
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    juce::String group;
    for (int i = 0; i < (int) all.size(); ++i)
    {
        auto& pr = all[(size_t) i];
        if (! pr.factory) continue;
        if (pr.group != group && i > 0) m.addSeparator();
        if (pr.group != group) { m.addSectionHeader (pr.group); group = pr.group; }
        m.addItem (i + 1, pr.name, true, pr.name == proc.presetName);
    }
    bool anyUser = false;
    for (int i = 0; i < (int) all.size(); ++i)
        if (! all[(size_t) i].factory)
        {
            if (! anyUser) { m.addSeparator(); m.addSectionHeader ("MY PRESETS"); anyUser = true; }
            m.addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.presetName);
        }
    m.addSeparator();
    m.addItem (10001, "Save as...");
    auto ci = proc.presets.indexOf (proc.presetName);
    m.addItem (10002, "Delete \"" + proc.presetName + "\"", ci >= 0 && ! all[(size_t) ci].factory);
    m.addItem (10003, "Open presets folder");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetPill), [this] (int r)
    {
        auto& list = proc.presets.all();
        if (r >= 1 && r <= (int) list.size()) { proc.undo.beginNewTransaction(); proc.applyPreset (list[(size_t) r - 1]); }
        else if (r == 10001) savePreset();
        else if (r == 10002) proc.presets.remove (proc.presetName);
        else if (r == 10003) { proc.presets.userFolder().createDirectory(); proc.presets.userFolder().startAsProcess(); }
    });
}

void Editor::showSettingsMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    m.addSectionHeader ("LT TUNE " LT_VERSION);
    auto latencyMs = 1000.0 * proc.getLatencySamples() / juce::jmax (1.0, proc.getSampleRate());
    m.addItem (1, "Latency: " + juce::String (latencyMs, 1) + " ms (" + juce::String (proc.getLatencySamples()) + " samples)", false);
    m.addSeparator();
    m.addItem (2, "License...");
    m.addItem (3, "Open presets folder");
    m.addItem (4, "Reset to default");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton), [this] (int r)
    {
        if (r == 2) showActivation (true);
        else if (r == 3) { proc.presets.userFolder().createDirectory(); proc.presets.userFolder().startAsProcess(); }
        else if (r == 4)
        {
            proc.undo.beginNewTransaction();
            for (auto* prm : proc.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                {
                    rp->beginChangeGesture();
                    rp->setValueNotifyingHost (rp->getDefaultValue());
                    rp->endChangeGesture();
                }
            proc.presetName = "Default";
        }
    });
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
        auto preset = lt::presets::ParamPreset::capture (proc.state, name, "MY PRESETS");
        // VOICE, MODE, bypass and the notes stay with the session, not the preset
        for (auto* id : { IDs::voice, IDs::mode, IDs::bypass }) preset.values.erase (id);
        for (int i = 0; i < 12; ++i) preset.values.erase (IDs::note (i));
        preset.values[IDs::edited] = 0.0f;
        if (proc.presets.save (preset))
            proc.presetName = name;
    }), false);
}

// ------------------------------------------------------------------ overlays

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

void Editor::showMatch (bool show)
{
    if (show && match == nullptr)
    {
        match = std::make_unique<MatchView> (palette, proc.lastMatch);
        auto safe = juce::Component::SafePointer<Editor> (this);
        match->onClose = [safe] { juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->showMatch (false); }); };
        match->onResult = [this] (std::shared_ptr<MatchResult> r) { proc.lastMatch = std::move (r); };
        match->onApply = [this, safe] (const MatchResult& r, std::array<bool, MatchView::rowCount> en, float amount)
        {
            applyMatch (r, en, amount);
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->showMatch (false); });
        };
        content.addAndMakeVisible (*match);
        match->setBounds (0, 0, baseWidth, baseHeight);
        if (activation) activation->toFront (false);
    }
    else if (! show && match != nullptr)
    {
        content.removeChildComponent (match.get());
        match.reset();
    }
}

void Editor::applyMatch (const MatchResult& r, std::array<bool, MatchView::rowCount> en, float amount)
{
    if (! r.ok || amount <= 0.0f) return;
    auto& s = proc.state;
    proc.undo.beginNewTransaction();
    auto blend = [&] (const char* id, float target)
    {
        auto cur = s.getRawParameterValue (id)->load();
        setParam (s, id, cur + amount * (target - cur));
    };
    if (en[MatchView::keyRow])      setKeyScale (s, r.key, r.scale);
    if (en[MatchView::retuneRow])   blend (IDs::retune, r.retuneMs);
    if (en[MatchView::humanizeRow]) blend (IDs::humanize, r.humanize);
    if (en[MatchView::flexRow])     blend (IDs::flexTune, r.flex);
    if (en[MatchView::vibratoRow])  blend (IDs::vibrato, r.vibrato);
    if (en[MatchView::formantRow])  blend (IDs::formant, r.formant);
    proc.undo.beginNewTransaction();
}

} // namespace tune
