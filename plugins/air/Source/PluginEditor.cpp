#include "PluginEditor.h"

namespace air
{

using lt::ui::Fonts;
using lt::ui::Glass;

bool Editor::activationShownThisSession = false;

namespace
{
    // layout (base 1200 x 896, see design/air/main-dark.webp)
    const juce::Rectangle<float> topBar { 72, 68, 1056, 74 }, display { 72, 160, 1056, 258 }, bottomBar { 72, 778, 1056, 67 };
    const juce::Rectangle<float> modules[3] { { 72, 437, 340, 323 }, { 430, 437, 340, 323 }, { 788, 437, 340, 323 } };

    juce::String dbText (float v) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }

    /** Developer preview only (LT_EDITOR_SNAPSHOT): a vocal-like spectrum so the display isn't empty without audio. */
    float demoLevel (float hz)
    {
        auto o = std::log2 (hz / 100.0f);
        auto shape = 0.86f - 0.035f * o * o * 0.5f - (hz < 120.0f ? (120.0f - hz) / 200.0f : 0.0f);
        shape += 0.05f * std::sin (o * 2.6f) + 0.035f * std::sin (o * 6.1f + 1.0f);
        return juce::jlimit (0.0f, 1.0f, shape);
    }
}

Editor::Editor (Processor& pr) : AudioProcessorEditor (pr), proc (pr)
{
    lnf.setPalette (palette);
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    background.setPalette (palette);
    background.setParticleAmount (0.7f);
    content.addAndMakeVisible (background);
    content.addAndMakeVisible (decor);
    content.addAndMakeVisible (atmosphere);

    for (int i = 0; i < 3; ++i)
    {
        auto& k = knobs[(size_t) i];
        auto* prm = proc.state.getParameter (IDs::amount[(size_t) i]);
        attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*prm, k, &proc.undo));
        k.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        lastValue[(size_t) i] = (float) k.getValue();
        k.onValueChange = [this, i] { linkMoved (i); };
        k.onDragStart = [this, i]
        {
            dragging[(size_t) i] = true;
            if (i > 0 && linkButton.getToggleState()) proc.state.getParameter (IDs::amount[(size_t) (3 - i)])->beginChangeGesture();
        };
        k.onDragEnd = [this, i]
        {
            if (i > 0 && dragging[(size_t) i] && linkButton.getToggleState()) proc.state.getParameter (IDs::amount[(size_t) (3 - i)])->endChangeGesture();
            dragging[(size_t) i] = false;
        };
        content.addAndMakeVisible (k);

        pills[(size_t) i] = std::make_unique<ModelPills> (Models::names (i));
        modelAttachments[(size_t) i] = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::model[(size_t) i]),
            [this, i] (float v) { pills[(size_t) i]->setSelected (juce::roundToInt (v)); decor.repaint(); }, &proc.undo);
        pills[(size_t) i]->onSelect = [this, i] (int m)
        {
            proc.undo.beginNewTransaction();
            modelAttachments[(size_t) i]->setValueAsCompleteGesture ((float) m);
        };
        modelAttachments[(size_t) i]->sendInitialUpdate();
        content.addAndMakeVisible (*pills[(size_t) i]);
    }

    std::pair<SmallKnob*, const char*> small[] = { { &trim, IDs::trim }, { &mix, IDs::mix }, { &output, IDs::output } };
    for (auto& [k, id] : small)
    {
        auto* prm = proc.state.getParameter (id);
        attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*prm, *k, &proc.undo));
        k->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        k->onValueChange = [this] { decor.repaint (bottomBar.toNearestInt()); };
        content.addAndMakeVisible (k);
    }
    trim.bipolar = output.bipolar = true;
    output.setDoubleClickReturnValue (true, 0.0);
    trim.setDoubleClickReturnValue (true, 0.0);
    content.addAndMakeVisible (inMeter);
    content.addAndMakeVisible (outMeter);

    linkButton.setClickingTogglesState (true);
    linkAttachment = std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (IDs::link), linkButton, &proc.undo);
    linkButton.setTooltip ("Moves MID AIR and HIGH AIR together");
    saveButton.fontSize = 10.5f;

    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { savePreset(); };
    undoButton.onClick = [this] { proc.undo.undo(); };
    redoButton.onClick = [this] { proc.undo.redo(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };
    powerButton.onClick = [this] { toggleBypass(); };
    bypassButton.onClick = [this] { toggleBypass(); };
    licenseButton.onClick = [this] { showActivation (true); };
    abButton.onSelect = [this] (int s) { proc.switchAB (s); refresh(); };
    for (juce::Component* c : { (juce::Component*) &prevPreset, (juce::Component*) &nextPreset, (juce::Component*) &presetButton,
                                (juce::Component*) &saveButton, (juce::Component*) &undoButton, (juce::Component*) &redoButton,
                                (juce::Component*) &abButton, (juce::Component*) &settingsButton, (juce::Component*) &powerButton,
                                (juce::Component*) &bypassButton, (juce::Component*) &linkButton, (juce::Component*) &licenseButton })
        content.addAndMakeVisible (c);
    undoButton.setTooltip ("Undo");
    redoButton.setTooltip ("Redo");
    abButton.setTitle ("A | B");
    powerButton.setTooltip ("Bypass");

    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->refresh(); });
    };

    setResizable (true, true);
    setResizeLimits (baseWidth * 55 / 100, baseHeight * 55 / 100, baseWidth * 16 / 10, baseHeight * 16 / 10);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (baseWidth * 8 / 10, baseHeight * 8 / 10);

    if (! proc.license.isLicensed() && (! activationShownThisSession || ! proc.license.isUsable()))
    {
        activationShownThisSession = true;
        showActivation (true);
    }
    refresh();
    startTimerHz (30);

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png (shows a demo spectrum, since no audio is playing)
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
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

void Editor::refresh()
{
    for (int i = 0; i < 3; ++i) lastValue[(size_t) i] = (float) knobs[(size_t) i].getValue();
    abButton.setSlot (proc.abSlot());
    content.repaint();
}

void Editor::toggleBypass()
{
    proc.undo.beginNewTransaction();
    setParam (proc.state, IDs::bypass, proc.v (IDs::bypass) > 0.5f ? 0.0f : 1.0f);
}

void Editor::linkMoved (int i)
{
    auto v = (float) knobs[(size_t) i].getValue();
    auto delta = v - lastValue[(size_t) i];
    lastValue[(size_t) i] = v;
    if (i == 0 || linking || ! dragging[(size_t) i] || ! linkButton.getToggleState() || std::abs (delta) < 1.0e-4f) return;

    // LINK: MID AIR and HIGH AIR move together, keeping their difference
    auto other = (size_t) (3 - i);
    auto* prm = proc.state.getParameter (IDs::amount[other]);
    auto cur = prm->convertFrom0to1 (prm->getValue());
    linking = true;
    prm->setValueNotifyingHost (prm->convertTo0to1 (juce::jlimit (0.0f, 100.0f, cur + delta)));
    linking = false;
}

// ------------------------------------------------------------------ timer

void Editor::timerCallback()
{
    auto now = juce::Time::getMillisecondCounter();
    auto dt = lastTick == 0 ? 1.0f / 30.0f : juce::jlimit (0.005f, 0.2f, (float) (now - lastTick) * 0.001f);
    lastTick = now;

    auto& e = proc.engine;
    auto bypassed = proc.v (IDs::bypass) > 0.5f;
    static const bool demo = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {}).isNotEmpty();

    AtmosphereState st;
    for (int i = 0; i < 3; ++i)
    {
        st.amount[i] = proc.v (IDs::amount[(size_t) i]) * 0.01f;
        st.activity[i] = e.getSectionActivity (i);
    }
    st.bypassed = bypassed;
    st.lufs = demo ? -14.2f : e.getInputLufs();
    proc.spectrum.update();
    if (demo) st.level = demoLevel;
    else st.level = [this] (float hz) { return proc.spectrum.getLevel (hz); };
    atmosphere.tick (st);

    auto inPk = e.takeInputPeak(), outPk = e.takeOutputPeak();
    if (demo) { inPk = 0.55f; outPk = 0.7f; }
    inMeter.setLevel (inPk, dt);
    outMeter.setLevel (outPk, dt);

    auto lit = ! bypassed;
    if (powerButton.lit != lit) { powerButton.lit = lit; powerButton.repaint(); }
    if (bypassButton.lit == lit) { bypassButton.lit = ! lit; bypassButton.repaint(); }
    background.setEnergy (bypassed ? 0.0f : juce::jlimit (0.0f, 1.0f, juce::Decibels::gainToDecibels (outPk, -60.0f) / 60.0f + 1.0f) * 0.6f);

    undoButton.setEnabled (proc.undo.canUndo());
    redoButton.setEnabled (proc.undo.canRedo());
    if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        proc.undo.beginNewTransaction();
    abButton.setSlot (proc.abSlot());

    auto name = proc.presetName + (proc.isPresetModified() ? " *" : "");
    presetButton.setText (name);

    auto chip = proc.license.isLicensed() ? juce::String() : proc.license.isUsable()
                  ? juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (proc.license.getTrialDaysLeft()) + " DAYS"
                  : juce::String ("ACTIVATE");
    if (licenseButton.getButtonText() != chip) licenseButton.setButtonText (chip);
    licenseButton.setVisible (chip.isNotEmpty() && ! demo);
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
    decor.setBounds (0, 0, baseWidth, baseHeight);

    // top bar
    prevPreset.setBounds (470, 91, 28, 30);
    presetButton.setBounds (500, 89, 182, 34);
    nextPreset.setBounds (684, 91, 28, 30);
    saveButton.setBounds (715, 93, 56, 25);
    undoButton.setBounds (873, 88, 34, 34);
    redoButton.setBounds (915, 88, 34, 34);
    abButton.setBounds (954, 90, 72, 30);
    settingsButton.setBounds (1032, 88, 34, 34);
    powerButton.setBounds (1076, 88, 34, 34);

    atmosphere.setBounds (display.toNearestInt());

    // modules
    for (int i = 0; i < 3; ++i)
    {
        auto m = modules[i];
        pills[(size_t) i]->setBounds ((int) m.getX() + 10, 491, (int) m.getWidth() - 20, 25);
        knobs[(size_t) i].setBounds ((int) m.getCentreX() - 111, 529, 222, 200);
    }
    linkButton.setBounds (751, 594, 54, 23);

    // bottom bar
    bypassButton.setBounds (90, 790, 42, 42);
    licenseButton.setBounds (236, 798, 150, 26);
    trim.setBounds (432, 789, 46, 46);
    mix.setBounds (573, 786, 52, 52);
    output.setBounds (669, 789, 46, 46);
    inMeter.setBounds (888, 795, 224, 12);
    outMeter.setBounds (888, 814, 224, 12);

    if (activation) activation->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::Decor::paint (juce::Graphics& g)
{
    auto& e = owner;
    auto& p = e.palette;
    const auto white = juce::Colours::white;

    auto panel = [&] (juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.38f));
        g.fillRoundedRectangle (r, 14.0f);
        Glass::drawPanel (g, r, 14.0f, p, 0.9f);
        g.setColour (white.withAlpha (0.16f));
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
    };
    panel (topBar);
    panel (display);
    for (auto& m : modules) panel (m);
    panel (bottomBar);

    // logo: "LT" glowing lavender-purple (as in the design), AIR thin white with its stellar wind
    auto logoPalette = p;
    logoPalette.accent = juce::Colour (0xffa57cff);
    Glass::drawGlow (g, { 122.0f, 106.0f }, 62.0f, juce::Colour (0xff9b5cff), 0.32f);
    lt::ui::Logo::draw (g, { 90.0f, 71.0f, 300.0f, 68.0f }, lt::ui::Logo::specFor ("AIR"), logoPalette, juce::Justification::centredLeft);

    // module titles + model captions
    for (int i = 0; i < 3; ++i)
    {
        auto m = modules[i];
        g.setColour (white.withAlpha (0.95f));
        Fonts::drawSpaced (g, Models::sectionNames()[i], { m.getX(), 452.0f, m.getWidth(), 30.0f }, Fonts::regular (21.0f), 0.03f,
                           juce::Justification::centred);
        auto model = juce::roundToInt (e.proc.v (IDs::model[(size_t) i]));
        g.setColour (white.withAlpha (0.5f));
        g.setFont (Fonts::regular (17.0f));
        g.drawText (Models::caption (i, model), juce::Rectangle<float> (m.getX(), 714.0f, m.getWidth(), 28.0f), juce::Justification::centred);
    }

    // bottom bar labels
    g.setColour (white.withAlpha (0.92f));
    g.setFont (Fonts::regular (13.5f));
    g.drawText ("BYPASS", juce::Rectangle<float> (143.0f, 802.0f, 80.0f, 18.0f), juce::Justification::centredLeft);
    auto pair = [&] (const juce::String& name, const juce::String& value, float x)
    {
        g.setColour (white.withAlpha (0.92f));
        g.setFont (Fonts::regular (13.5f));
        g.drawText (name, juce::Rectangle<float> (x, 795.0f, 90.0f, 17.0f), juce::Justification::centredLeft);
        g.setColour (white.withAlpha (0.6f));
        g.setFont (Fonts::regular (12.5f));
        g.drawText (value, juce::Rectangle<float> (x, 812.0f, 90.0f, 17.0f), juce::Justification::centredLeft);
    };
    pair ("TRIM", dbText (e.proc.v (IDs::trim)), 489.0f);
    pair ("OUTPUT", dbText (e.proc.v (IDs::output)), 727.0f);
    g.setColour (white.withAlpha (0.92f));
    g.setFont (Fonts::regular (13.5f));
    g.drawText ("IN", juce::Rectangle<float> (840.0f, 793.0f, 43.0f, 16.0f), juce::Justification::centredRight);
    g.drawText ("OUT", juce::Rectangle<float> (840.0f, 812.0f, 43.0f, 16.0f), juce::Justification::centredRight);
}

// ------------------------------------------------------------------ presets & menus

void Editor::stepPreset (int delta)
{
    proc.presets.rescan();
    auto& all = proc.presets.all();
    if (all.empty()) return;
    auto i = proc.presets.indexOf (proc.presetName);
    i = ((i < 0 ? 0 : i + delta) + (int) all.size()) % (int) all.size();
    proc.undo.beginNewTransaction();
    proc.applyPreset (all[(size_t) i]);
    refresh();
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

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton), [this] (int r)
    {
        auto& list = proc.presets.all();
        if (r >= 1 && r <= (int) list.size()) { proc.undo.beginNewTransaction(); proc.applyPreset (list[(size_t) r - 1]); refresh(); }
        else if (r == 10001) savePreset();
        else if (r == 10002) proc.presets.remove (proc.presetName);
        else if (r == 10003) { proc.presets.userFolder().createDirectory(); proc.presets.userFolder().startAsProcess(); }
    });
}

void Editor::showSettingsMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    m.addSectionHeader ("LT AIR " LT_VERSION);
    auto latencyMs = 1000.0 * proc.getLatencySamples() / juce::jmax (1.0, proc.getSampleRate());
    m.addItem (1, "Latency: " + juce::String (latencyMs, 2) + " ms (" + juce::String (proc.getLatencySamples()) + " samples)", false);
    m.addItem (5, "Auto gain (match loudness for A/B)", true, proc.v (IDs::autoGain) > 0.5f);
    m.addSeparator();
    m.addItem (2, "License...");
    m.addItem (3, "Open presets folder");
    m.addItem (4, "Reset to default");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton), [this] (int r)
    {
        if (r == 5) setParam (proc.state, IDs::autoGain, proc.v (IDs::autoGain) > 0.5f ? 0.0f : 1.0f);
        else if (r == 2) showActivation (true);
        else if (r == 3) { proc.presets.userFolder().createDirectory(); proc.presets.userFolder().startAsProcess(); }
        else if (r == 4)
        {
            proc.undo.beginNewTransaction();
            proc.applyPreset (factoryPresets().front());
            refresh();
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
        preset.values.erase (IDs::bypass);   // bypass and auto gain stay with the session
        preset.values.erase (IDs::autoGain);
        if (proc.presets.save (preset))
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

} // namespace air
