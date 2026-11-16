#include "PluginEditor.h"

namespace knock
{

using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Logo;

bool Editor::activationShownThisSession = false;

Editor::Editor (Processor& pr)
    : AudioProcessorEditor (pr), proc (pr), focusView (pr.state, pr.engine.spectrum, "KNOCK")
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (background);
    content.addAndMakeVisible (decor);

    planet.palette = &palette;
    attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (IDs::knock), planet));
    content.addAndMakeVisible (planet);

    std::pair<KnockKnob*, const char*> knobs[] = {
        { &punch, IDs::punch }, { &saturation, IDs::saturation }, { &distortion, IDs::distortion }, { &bright, IDs::bright },
        { &low, IDs::low }, { &glue, IDs::glue }, { &width, IDs::width }, { &crush, IDs::crush }, { &lofi, IDs::lofi },
        { &stutter, IDs::stutter }, { &filter, IDs::filter }, { &ring, IDs::ring }, { &reverse, IDs::reverse },
        { &reverb, IDs::reverb }, { &delay, IDs::delay }, { &input, IDs::input }, { &output, IDs::output } };
    for (auto& [k, id] : knobs)
    {
        k->palette = &palette;
        attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (id), *k));
        content.addAndMakeVisible (k);
    }
    filter.setDoubleClickReturnValue (true, 0.0);

    distList.palette = &palette;
    distAttachment = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::distType), [this] (float v)
    {
        distList.selected = juce::roundToInt (v);
        distList.repaint();
    });
    distList.onChange = [this] (int i) { distAttachment->setValueAsCompleteGesture ((float) i); };
    content.addAndMakeVisible (distList);
    distAttachment->sendInitialUpdate();

    delayTimeAttachment = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::delayTime), [this] (float v)
    {
        delayTimeButton.setButtonText (DelayTimes::names()[juce::roundToInt (v)]);
    });
    delayTimeButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.setLookAndFeel (&lnf);
        for (int i = 0; i < DelayTimes::names().size(); ++i)
            m.addItem (i + 1, DelayTimes::names()[i], true, delayTimeButton.getButtonText() == DelayTimes::names()[i]);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&delayTimeButton),
                         [this] (int r) { if (r > 0) delayTimeAttachment->setValueAsCompleteGesture ((float) (r - 1)); });
    };
    content.addAndMakeVisible (delayTimeButton);
    delayTimeAttachment->sendInitialUpdate();

    for (auto* l : { &lockTone, &lockFx, &lockSpace })
    {
        l->palette = &palette;
        content.addAndMakeVisible (l);
    }
    lockTone.setToggleState (proc.lockTone, juce::dontSendNotification);
    lockFx.setToggleState (proc.lockFx, juce::dontSendNotification);
    lockSpace.setToggleState (proc.lockSpace, juce::dontSendNotification);
    lockTone.onClick = [this] { proc.lockTone = lockTone.getToggleState(); };
    lockFx.onClick = [this] { proc.lockFx = lockFx.getToggleState(); };
    lockSpace.onClick = [this] { proc.lockSpace = lockSpace.getToggleState(); };

    inMeter.palette = outMeter.palette = &palette;
    content.addAndMakeVisible (inMeter);
    content.addAndMakeVisible (outMeter);

    modeAttachment = std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (IDs::mode), modeToggle);
    content.addAndMakeVisible (modeToggle);

    chaosButton.onClick = [this] { chaos(); };
    undoButton.onClick = [this] { undoChaos(); };
    undoButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x86\xb6"));
    undoButton.setTooltip ("Undo CHAOS");
    undoButton.getProperties().set ("flat", true);
    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { savePreset(); };
    eqButton.onClick = [this] { showFocus (true); };
    themeButton.onClick = [this] { proc.lightTheme = ! proc.lightTheme; applyTheme(); };
    licenseButton.onClick = [this] { showActivation (true); };
    for (auto* b : { &chaosButton, &undoButton, &presetButton, &saveButton, &eqButton, &themeButton, &licenseButton })
        content.addAndMakeVisible (b);

    focusView.setGlassStyle (true);
    focusView.onClose = [this] { showFocus (false); };
    content.addChildComponent (focusView);

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

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png [LT_LIGHT=1] [LT_SHOW_EQ=1] [LT_KNOCK=value]
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
        if (juce::SystemStats::getEnvironmentVariable ("LT_LIGHT", {}).isNotEmpty()) themeButton.triggerClick();
        auto kv = juce::SystemStats::getEnvironmentVariable ("LT_KNOCK", {});
        if (kv.isNotEmpty()) planet.setValue (kv.getDoubleValue(), juce::sendNotificationSync);
        if (juce::SystemStats::getEnvironmentVariable ("LT_SHOW_EQ", {}).isNotEmpty())
        {
            if (auto* b = proc.state.getParameter (lt::dsp::FocusParams::band)) b->setValueNotifyingHost (b->convertTo0to1 (1.0f));
            showFocus (true);
        }
        juce::Timer::callAfterDelay (800, [this]
        {
            // feed a short synthetic beat so the meters and the spectrum move
            proc.engine.prepare (48000.0, 512);
            juce::AudioBuffer<float> b (2, 512);
            juce::Random r (3);
            for (int blk = 0; blk < 120; ++blk)
            {
                for (int i = 0; i < 512; ++i)
                {
                    auto t = (blk * 512 + i) / 48000.0, bt = std::fmod (t, 0.5);
                    auto v = (float) (0.7 * std::exp (-bt * 14) * std::sin (6.2832 * (50 + 120 * std::exp (-bt * 30)) * bt)
                                      + 0.15 * (r.nextFloat() - 0.5f) * std::exp (-std::fmod (t, 0.25) * 40) + 0.12 * std::sin (t * 2764.0));
                    b.setSample (0, i, v); b.setSample (1, i, v);
                }
                KnockSettings s; Transport tr; lt::dsp::FocusSettings f;
                proc.engine.process (b, s, tr, f);
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
    focusView.setPalette (palette);
    lockTone.setToggleState (proc.lockTone, juce::dontSendNotification);
    lockFx.setToggleState (proc.lockFx, juce::dontSendNotification);
    lockSpace.setToggleState (proc.lockSpace, juce::dontSendNotification);
    if (activation) activation->setPalette (palette);
    content.repaint();
}

// ------------------------------------------------------------------ CHAOS

void Editor::chaos()
{
    std::map<juce::String, float> snapshot;
    for (auto* prm : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            snapshot[r->getParameterID()] = r->getValue();
    undoStack.push_back (snapshot);
    if (undoStack.size() > 20) undoStack.erase (undoStack.begin());

    juce::Random rnd;
    auto apply = [&] (const std::vector<ChaosRange>& ranges)
    {
        for (auto& c : ranges)
            if (auto* prm = proc.state.getParameter (c.id))
            {
                // most knobs land low-ish: musical, not a mess
                auto t = std::pow (rnd.nextFloat(), 1.4f);
                auto v = c.lo + (c.hi - c.lo) * t;
                if (juce::String (c.id) == IDs::filter) v = (rnd.nextFloat() < 0.5f ? 0.0f : c.lo + (c.hi - c.lo) * rnd.nextFloat());
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 (v));
                prm->endChangeGesture();
            }
    };
    if (! proc.lockTone) apply (chaosTone());
    if (! proc.lockFx) apply (chaosFx());
    if (! proc.lockSpace) apply (chaosSpace());
    planet.burst();
    background.pulse (1.0f);
}

void Editor::undoChaos()
{
    if (undoStack.empty()) return;
    auto snap = undoStack.back();
    undoStack.pop_back();
    for (auto& [id, norm] : snap)
        if (auto* prm = proc.state.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (norm);
            prm->endChangeGesture();
        }
}

// ------------------------------------------------------------------ timer / layout

void Editor::timerCallback()
{
    auto& e = proc.engine;
    auto level = juce::jlimit (0.0f, 1.0f, e.getEnergy() * 1.5f);
    planet.tick (level, e.getHitCount());
    background.setEnergy (level);
    background.setParticleAmount (0.3f + 0.7f * proc.state.getRawParameterValue (IDs::knock)->load() * 0.01f);
    inMeter.setLevels (e.getInputLevel (0), e.getInputLevel (1));
    outMeter.setLevels (e.getOutputLevel (0), e.getOutputLevel (1));

    auto name = proc.presetName + (proc.isPresetModified() ? " *" : "");
    if (presetButton.getButtonText() != name) presetButton.setButtonText (name);
    undoButton.setEnabled (! undoStack.empty());

    auto focusOn = juce::roundToInt (proc.state.getRawParameterValue (lt::dsp::FocusParams::band)->load()) != 0;
    eqButton.setToggleState (focusOn, juce::dontSendNotification);

    auto chip = proc.license.isLicensed() ? juce::String() : proc.license.isUsable()
                  ? juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (proc.license.getTrialDaysLeft()) + " DAYS"
                  : juce::String ("ACTIVATE");
    if (licenseButton.getButtonText() != chip) licenseButton.setButtonText (chip);
    if (! focusView.isVisible()) licenseButton.setVisible (chip.isNotEmpty());
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
    focusView.setBounds (0, 0, baseWidth, baseHeight);

    presetButton.setBounds (55, 41, 215, 30);
    saveButton.setBounds (283, 41, 66, 30);
    eqButton.setBounds (361, 41, 66, 30);
    modeToggle.setBounds (770, 41, 190, 30);
    themeButton.setBounds (968, 41, 78, 30);
    licenseButton.setBounds (480, 562, 140, 22);

    auto knob = [] (KnockKnob& k, int cx, int cy, int w, int h) { k.setBounds (cx - w / 2, cy - h / 2, w, h); };
    knob (punch, 125, 210, 110, 112);       knob (saturation, 262, 210, 110, 112);
    knob (distortion, 194, 304, 100, 94);   distList.setBounds (248, 266, 70, 64);
    knob (bright, 125, 404, 110, 112);      knob (low, 262, 404, 110, 112);
    knob (glue, 125, 518, 110, 112);        knob (width, 262, 518, 110, 112);

    planet.setBounds (550 - 260, 300 - 160, 520, 320);
    chaosButton.setBounds (491, 480, 118, 36);
    undoButton.setBounds (535, 526, 30, 26);

    knob (crush, 828, 175, 90, 86);   knob (lofi, 915, 175, 90, 86);   knob (stutter, 1002, 175, 90, 86);
    knob (filter, 828, 248, 90, 86);  knob (ring, 915, 248, 90, 86);   knob (reverse, 1002, 248, 90, 86);
    knob (reverb, 828, 366, 90, 86);  knob (delay, 915, 366, 90, 86);
    delayTimeButton.setBounds (976, 345, 54, 24);

    inMeter.setBounds (813, 430, 26, 122);
    outMeter.setBounds (873, 430, 26, 122);
    knob (input, 1002, 464, 90, 76);
    knob (output, 1002, 540, 90, 76);

    lockTone.setBounds (238, 107, 18, 22);
    lockFx.setBounds (944, 107, 18, 22);
    lockSpace.setBounds (960, 305, 18, 22);

    if (activation) activation->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::Decor::paint (juce::Graphics& g)
{
    auto& e = owner;
    auto& p = e.palette;
    auto glow = p.isLight ? juce::Colours::white : p.accent;

    // one glass panel for everything, no boxes
    auto main = juce::Rectangle<float> (42.0f, 30.0f, (float) baseWidth - 84.0f, (float) baseHeight - 46.0f);
    Glass::drawPanel (g, main, 14.0f, p, 0.3f);
    g.setColour (glow.withAlpha (0.35f));
    g.drawRoundedRectangle (main.reduced (0.5f), 14.0f, 1.2f);
    Glass::drawPanel (g, { 46.0f, 34.0f, (float) baseWidth - 92.0f, 44.0f }, 12.0f, p, 0.8f);

    Logo::draw (g, { 400.0f, 36.0f, 300.0f, 38.0f }, Logo::specFor ("KNOCK"), p, juce::Justification::centred);

    auto header = [&] (const juce::String& t, float cx, float y)
    {
        g.setColour (p.isLight ? juce::Colours::white : p.nebulaC.brighter (0.3f));
        Fonts::drawSpaced (g, t, { cx - 80.0f, y, 160.0f, 26.0f }, Fonts::light (23.0f), 0.04f, juce::Justification::centred);
    };
    header ("TONE", 194.0f, 105.0f);
    header ("FX", 915.0f, 105.0f);
    header ("SPACE", 915.0f, 303.0f);

    g.setColour (p.text);
    g.setFont (Fonts::regular (14.0f));
    g.drawText ("IN", juce::Rectangle<float> (806.0f, 556.0f, 40.0f, 18.0f), juce::Justification::centred);
    g.drawText ("OUT", juce::Rectangle<float> (866.0f, 556.0f, 40.0f, 18.0f), juce::Justification::centred);
}

// ------------------------------------------------------------------ presets / pages

void Editor::showPresetMenu()
{
    proc.presets.rescan();
    auto& all = proc.presets.all();
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    for (auto group : { "DRUMS", "MELODY" })
    {
        m.addSectionHeader (group);
        for (int i = 0; i < (int) all.size(); ++i)
            if (all[(size_t) i].factory && all[(size_t) i].group == group)
                m.addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.presetName);
        m.addSeparator();
    }
    bool anyUser = false;
    for (int i = 0; i < (int) all.size(); ++i)
        if (! all[(size_t) i].factory)
        {
            if (! anyUser) { m.addSectionHeader ("MY PRESETS"); anyUser = true; }
            m.addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.presetName);
        }
    if (anyUser) m.addSeparator();
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
        auto melody = proc.state.getRawParameterValue (IDs::mode)->load() > 0.5f;
        if (proc.presets.save (lt::presets::ParamPreset::capture (proc.state, name, melody ? "MELODY" : "DRUMS")))
            proc.presetName = name;
    }), false);
}

void Editor::showFocus (bool show)
{
    // the EQ window floats over the nebula only: hide the main page while it is open
    for (auto* c : content.getChildren())
        if (c != &background && c != &focusView && c != activation.get())
            c->setVisible (! show);
    if (! show) licenseButton.setVisible (licenseButton.getButtonText().isNotEmpty());
    focusView.setVisible (show);
    if (show) focusView.toFront (false);
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

} // namespace knock
