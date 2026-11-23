#include "PluginEditor.h"

namespace meteor
{

using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Logo;

bool Editor::activationShownThisSession = false;

namespace
{
    const juce::StringArray audioExtensions { ".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg", ".m4a", ".caf" };
}

MeteorKnob& Editor::addKnob (const char* id, const juce::String& label, float size)
{
    auto* k = knobs.add (new MeteorKnob (label, size));
    k->palette = &palette;
    attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (id), *k));
    content.addAndMakeVisible (k);
    return *k;
}

ChoicePills& Editor::addPills (const char* id, const juce::StringArray& labels, int columns)
{
    auto* pl = pills.add (new ChoicePills (*proc.state.getParameter (id), labels, columns));
    content.addAndMakeVisible (pl);
    return *pl;
}

Editor::Editor (Processor& pr) : AudioProcessorEditor (pr), proc (pr)
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (background);
    content.addAndMakeVisible (decor);

    // ---------------- LIVE
    meteorView.palette = &palette;
    content.addAndMakeVisible (meteorView);
    livePage.push_back (&meteorView);

    const char* names[] = { "REVERSE", "DOUBLE", "CHOP", "GLITCH" };
    const char* powerIds[] = { IDs::revOn, IDs::dblOn, IDs::chopOn, IDs::glitchOn };
    for (int i = 0; i < 4; ++i)
    {
        auto& m = modules[i];
        m.card = { 35 + i * 260, 282, 250, 313 };
        m.power = std::make_unique<PowerPill> (names[i]);
        m.power->palette = &palette;
        m.powerAttachment = std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (powerIds[i]), *m.power);
        content.addAndMakeVisible (*m.power);
        m.power->setBounds (m.card.getX() + 12, m.card.getY() + 12, m.card.getWidth() - 24, 34);
        livePage.push_back (m.power.get());
    }

    auto place = [this] (juce::Component& c, int module, int x, int y, int w, int h)
    {
        auto card = modules[module].card;
        c.setBounds (card.getX() + x, card.getY() + y, w, h);
        livePage.push_back (&c);
    };
    auto caption = [this] (int module, const juce::String& t, int y)
    {
        auto card = modules[module].card;
        modules[module].labels.push_back ({ t, { card.getX() + 14, card.getY() + y, card.getWidth() - 28, 16 } });
    };

    // REVERSE
    caption (0, "SIZE", 60);
    place (addPills (IDs::revSize, Choices::reverseSizes()), 0, 12, 80, 226, 28);
    place (addKnob (IDs::revMix, "MIX", 64.0f), 0, 75, 160, 100, 100);
    // DOUBLE
    caption (1, "VOICES", 60);
    place (addPills (IDs::dblVoices, Choices::voices()), 1, 12, 80, 226, 28);
    place (addKnob (IDs::dblDetune, "DETUNE"), 1, 10, 160, 76, 96);
    place (addKnob (IDs::dblWidth, "WIDTH"), 1, 87, 160, 76, 96);
    place (addKnob (IDs::dblMix, "MIX"), 1, 164, 160, 76, 96);
    // CHOP
    caption (2, "RATE", 60);
    place (addPills (IDs::chopRate, Choices::rates()), 2, 12, 80, 226, 28);
    caption (2, "PATTERN", 118);
    place (addPills (IDs::chopPattern, Choices::patterns(), 2), 2, 12, 138, 226, 58);
    place (addKnob (IDs::chopDepth, "DEPTH"), 2, 75, 208, 100, 96);
    // GLITCH
    caption (3, "RATE", 60);
    place (addPills (IDs::glitchRate, Choices::rates()), 3, 12, 80, 226, 28);
    caption (3, "REPEATS", 118);
    place (addPills (IDs::glitchRepeats, Choices::repeats()), 3, 12, 138, 226, 28);
    place (addKnob (IDs::glitchChance, "CHANCE", 64.0f), 3, 75, 190, 100, 100);

    mixKnob = &addKnob (IDs::mix, "MIX", 60.0f);
    outKnob = &addKnob (IDs::output, "OUTPUT", 60.0f);
    outKnob->setDoubleClickReturnValue (true, 0.0);
    livePage.push_back (mixKnob);
    livePage.push_back (outKnob);

    // ---------------- STROKES
    sourceView.palette = &palette;
    sourceView.onClick = [this] { if (! proc.isCapturing()) chooseFile(); };
    content.addAndMakeVisible (sourceView);
    loadButton.onClick = [this] { chooseFile(); };
    captureButton.onClick = [this] { toggleCapture(); };
    folderButton.onClick = [] { auto f = Processor::outputFolder(); f.createDirectory(); f.startAsProcess(); };
    for (auto* b : { &loadButton, &captureButton, &folderButton }) content.addAndMakeVisible (b);
    strokesPage.insert (strokesPage.end(), { &sourceView, &loadButton, &captureButton, &folderButton });

    auto& len = addKnob (IDs::strokeLength, "LENGTH", 60.0f);
    auto& inten = addKnob (IDs::strokeIntensity, "INTENSITY", 60.0f);
    auto& tone = addKnob (IDs::strokeTone, "TONE", 60.0f);
    tone.setDoubleClickReturnValue (true, 0.0);
    for (auto* k : { &len, &inten, &tone })
    {
        strokesPage.push_back (k);
        k->onValueChange = [this] { proc.requestRender(); };
    }
    len.setBounds (35, 356, 100, 100);
    inten.setBounds (135, 356, 100, 100);
    tone.setBounds (235, 356, 100, 100);

    for (int i = 0; i < Strokes::count; ++i)
    {
        auto* c = cards.add (new StrokeCard (Strokes::names()[i]));
        c->palette = &palette;
        c->onPlay = [this, i] { proc.preview (proc.getPreviewIndex() == i ? -1 : i); };
        c->makeFile = [this, i] { return proc.exportStroke (i); };
        c->onSaved = [this] (const juce::File& f) { setStatus ("Saved " + f.getFileName()); };
        content.addAndMakeVisible (c);
        strokesPage.push_back (c);
    }

    // ---------------- header
    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { savePreset(); };
    themeButton.onClick = [this] { proc.lightTheme = ! proc.lightTheme; applyTheme(); };
    licenseButton.onClick = [this] { showActivation (true); };
    liveTab.onClick = [this] { showPage (0); };
    strokesTab.onClick = [this] { showPage (1); };
    for (auto* b : { &presetButton, &saveButton, &themeButton, &licenseButton, &liveTab, &strokesTab })
        content.addAndMakeVisible (b);

    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->applyTheme(); safe->showPage (safe->proc.page); } });
    };

    applyTheme();
    showPage (proc.page);
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

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png [LT_LIGHT=1] [LT_PAGE=1] [LT_SOURCE=/file.wav]
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
        if (juce::SystemStats::getEnvironmentVariable ("LT_LIGHT", {}).isNotEmpty()) themeButton.triggerClick();
        if (juce::SystemStats::getEnvironmentVariable ("LT_PAGE", {}) == "1") showPage (1);
        auto src = juce::SystemStats::getEnvironmentVariable ("LT_SOURCE", {});
        if (src.isNotEmpty()) { juce::String err; proc.loadSourceFile (juce::File (src), err); }
        if (juce::SystemStats::getEnvironmentVariable ("LT_LIVE_ALL", {}).isNotEmpty())
            for (auto* id : { IDs::revOn, IDs::dblOn, IDs::chopOn, IDs::glitchOn })
                if (auto* prm = proc.state.getParameter (id)) prm->setValueNotifyingHost (1.0f);
        juce::Timer::callAfterDelay (4000, [this, snapshot]
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
    proc.preview (-1);
    setLookAndFeel (nullptr);
}

void Editor::showPage (int page)
{
    proc.page = juce::jlimit (0, 1, page);
    for (auto* c : livePage) c->setVisible (proc.page == 0);
    for (auto* c : strokesPage) c->setVisible (proc.page == 1);
    liveTab.setToggleState (proc.page == 0, juce::dontSendNotification);
    strokesTab.setToggleState (proc.page == 1, juce::dontSendNotification);
    if (proc.page == 0) proc.preview (-1);
    decor.repaint();
}

void Editor::applyTheme()
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    lnf.setPalette (palette);
    background.setPalette (palette);
    if (activation) activation->setPalette (palette);
    content.repaint();
}

void Editor::setStatus (const juce::String& s, bool error)
{
    status = s;
    statusError = error;
    statusTime = juce::Time::getMillisecondCounter();
    decor.repaint();
}

bool Editor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (audioExtensions.contains (juce::File (f).getFileExtension().toLowerCase())) return true;
    return false;
}

void Editor::filesDropped (const juce::StringArray& files, int, int)
{
    sourceView.dragHover = false;
    for (auto& f : files)
        if (audioExtensions.contains (juce::File (f).getFileExtension().toLowerCase()))
        {
            juce::String err;
            if (proc.loadSourceFile (juce::File (f), err)) { setStatus ("Loaded " + juce::File (f).getFileName()); showPage (1); }
            else setStatus (err, true);
            return;
        }
}

void Editor::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a sound for the strokes", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.caf");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        juce::String err;
        if (proc.loadSourceFile (f, err)) setStatus ("Loaded " + f.getFileName());
        else setStatus (err, true);
    });
}

void Editor::toggleCapture()
{
    if (proc.isCapturing())
    {
        proc.stopCapture();
        setStatus (proc.hasSource() ? juce::String ("Recorded from the track") : juce::String ("Nothing was recorded (play the track first)"),
                   ! proc.hasSource());
    }
    else
    {
        proc.preview (-1);
        proc.startCapture();
        setStatus ("Play your track: METEOR records what comes in. Click STOP when done.");
    }
}

void Editor::timerCallback()
{
    auto& e = proc.engine;
    MeteorState s;
    s.level = juce::jlimit (0.0f, 1.0f, e.getInputLevel() * 1.5f);
    s.gate = e.getGate();
    s.reverse = proc.v (IDs::revOn) > 0.5f;
    s.doubled = proc.v (IDs::dblOn) > 0.5f;
    s.chop = proc.v (IDs::chopOn) > 0.5f;
    s.glitch = proc.v (IDs::glitchOn) > 0.5f;
    s.voices = juce::roundToInt (proc.v (IDs::dblVoices)) + 1;
    s.glitches = e.getGlitchCount();
    if (proc.page == 0) meteorView.tick (s);
    background.setEnergy (juce::jlimit (0.0f, 1.0f, e.getOutputLevel() * 1.2f));

    // strokes page
    auto capturing = proc.isCapturing();
    captureButton.setButtonText (capturing ? juce::String ("STOP ") + juce::String (proc.getCaptureSeconds(), 1) + " s" : juce::String ("USE TRACK"));
    captureButton.setToggleState (capturing, juce::dontSendNotification);
    sourceView.capturing = capturing;
    sourceView.captureSeconds = proc.getCaptureSeconds();
    auto src = proc.getSourceForDisplay();
    if (src != sourceView.buffer || capturing) { sourceView.buffer = src; sourceView.name = proc.getSourceName(); sourceView.repaint(); }

    auto gen = proc.getRenderGeneration();
    auto rendering = proc.isRendering();
    auto preview = proc.getPreviewIndex();
    for (int i = 0; i < cards.size(); ++i)
    {
        auto* c = cards[i];
        auto changed = gen != lastGeneration;
        if (changed) c->buffer = proc.getStroke (i);
        c->sampleRate = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        c->waiting = rendering || (src != nullptr && c->buffer == nullptr);
        auto playing = preview == i;
        if (changed || playing || c->playing != playing) { c->playing = playing; c->progress = playing ? proc.getPreviewProgress() : 0.0f; c->repaint(); }
    }
    lastGeneration = gen;

    if (status.isNotEmpty() && juce::Time::getMillisecondCounter() - statusTime > 6000) { status = {}; decor.repaint(); }

    auto name = proc.presetName + (proc.isPresetModified() ? " *" : "");
    if (presetButton.getButtonText() != name) presetButton.setButtonText (name);

    auto chip = proc.license.isLicensed() ? juce::String() : proc.license.isUsable()
                  ? juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (proc.license.getTrialDaysLeft()) + " DAYS"
                  : juce::String ("ACTIVATE");
    if (licenseButton.getButtonText() != chip) licenseButton.setButtonText (chip);
    licenseButton.setVisible (chip.isNotEmpty());
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

    presetButton.setBounds (35, 30, 196, 30);
    saveButton.setBounds (240, 30, 70, 30);
    themeButton.setBounds (982, 30, 86, 30);
    licenseButton.setBounds (836, 30, 136, 30);
    liveTab.setBounds (baseWidth / 2 - 124, 98, 120, 30);
    strokesTab.setBounds (baseWidth / 2 + 4, 98, 120, 30);

    meteorView.setBounds (35, 140, 840, 132);
    mixKnob->setBounds (890, 150, 86, 100);
    outKnob->setBounds (978, 150, 86, 100);

    sourceView.setBounds (35, 140, 300, 150);
    loadButton.setBounds (35, 302, 146, 34);
    captureButton.setBounds (189, 302, 146, 34);
    folderButton.setBounds (35, 561, 300, 34);
    for (int i = 0; i < cards.size(); ++i)
        cards[i]->setBounds (355 + (i % 4) * 178, 140 + (i / 4) * 230, 170, 225);

    if (activation) activation->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::Decor::paint (juce::Graphics& g)
{
    auto& e = owner;
    auto& p = e.palette;
    Glass::drawPanel (g, { 8.0f, 8.0f, (float) baseWidth - 16.0f, (float) baseHeight - 16.0f }, 14.0f, p, 0.2f);
    Logo::draw (g, { 0.0f, 26.0f, (float) baseWidth, 42.0f }, Logo::specFor ("METEOR"), p, juce::Justification::centred);
    g.setColour (p.glassEdge);
    g.drawHorizontalLine (85, 35.0f, (float) baseWidth - 35.0f);

    if (e.proc.page == 0)
    {
        for (auto& m : e.modules)
        {
            auto on = m.power != nullptr && m.power->getToggleState();
            Glass::drawPanel (g, m.card.toFloat(), 14.0f, p, on ? 1.3f : 0.8f, false);
            for (auto& [text, r] : m.labels)
            {
                g.setColour (p.textDim);
                Fonts::drawSpaced (g, text, r.toFloat(), Fonts::regular (11.5f), 0.12f, juce::Justification::centredLeft);
            }
        }
        return;
    }

    auto r = juce::Rectangle<float> (35.0f, 466.0f, 300.0f, 84.0f);
    if (e.status.isNotEmpty())
    {
        g.setColour (e.statusError ? p.danger : p.value);
        g.setFont (Fonts::regular (13.0f));
        g.drawFittedText (e.status, r.toNearestInt(), juce::Justification::centred, 3);
    }
    else
    {
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (12.5f));
        g.drawFittedText ("Drag a stroke onto a track in your DAW.\nClick DRAG to save the WAV in Music/LT METEOR.\n"
                          "LENGTH, INTENSITY and TONE re-render every stroke.",
                          r.toNearestInt(), juce::Justification::centred, 4);
    }
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
        if (pr.group != group && i > 0) { m.addSeparator(); }
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
        if (r >= 1 && r <= (int) list.size()) proc.applyPreset (list[(size_t) r - 1]);
        else if (r == 10001) savePreset();
        else if (r == 10002) proc.presets.remove (proc.presetName);
        else if (r == 10003) { proc.presets.userFolder().createDirectory(); proc.presets.userFolder().startAsProcess(); }
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
        for (auto* id : IDs::strokes) preset.values.erase (id);   // presets are about the LIVE effects
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

} // namespace meteor
