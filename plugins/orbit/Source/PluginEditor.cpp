#include "PluginEditor.h"

namespace orbit
{

using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Logo;

bool Editor::activationShownThisSession = false;

Editor::Editor (Processor& pr) : AudioProcessorEditor (pr), proc (pr)
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (background);
    content.addAndMakeVisible (decor);
    orbitView.palette = &palette;
    content.addAndMakeVisible (orbitView);

    std::pair<OrbitKnob*, const char*> knobs[] = {
        { &feedback, IDs::feedback }, { &mix, IDs::mix }, { &lowCut, IDs::lowCut }, { &highCut, IDs::highCut },
        { &width, IDs::width }, { &duck, IDs::duck }, { &wobble, IDs::wobble }, { &drive, IDs::drive } };
    for (auto& [k, id] : knobs)
    {
        k->palette = &palette;
        attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (id), *k));
        content.addAndMakeVisible (k);
    }
    time.palette = &palette;
    content.addAndMakeVisible (time);

    // modes
    modeAttachment = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::mode), [this] (float v)
    {
        for (int i = 0; i < Modes::count; ++i) modeButtons[i].setToggleState (i == juce::roundToInt (v), juce::dontSendNotification);
    });
    for (int i = 0; i < Modes::count; ++i)
    {
        modeButtons[i].setButtonText (Modes::names()[i]);
        modeButtons[i].onClick = [this, i] { modeAttachment->setValueAsCompleteGesture ((float) i); };
        content.addAndMakeVisible (modeButtons[i]);
    }

    // NORMAL / DOTTED / TRIPLET
    feelAttachment = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::feel), [this] (float v)
    {
        for (int i = 0; i < 3; ++i) feelButtons[i].setToggleState (i == juce::roundToInt (v), juce::dontSendNotification);
    });
    const char* feels[] = { "NORMAL", "DOTTED", "TRIPLET" };
    for (int i = 0; i < 3; ++i)
    {
        feelButtons[i].setButtonText (feels[i]);
        feelButtons[i].onClick = [this, i] { feelAttachment->setValueAsCompleteGesture ((float) i); };
        content.addAndMakeVisible (feelButtons[i]);
    }

    // SYNC | MS
    syncAttachment = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::sync), [this] (float) { attachTime(); });
    syncButton.onClick = [this] { syncAttachment->setValueAsCompleteGesture (1.0f); };
    msButton.onClick = [this] { syncAttachment->setValueAsCompleteGesture (0.0f); };
    for (auto* b : { &syncButton, &msButton })
    {
        b->getProperties().set ("flat", true);
        content.addAndMakeVisible (b);
    }

    freeze.palette = &palette;
    freezeAttachment = std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (IDs::freeze), freeze);
    content.addAndMakeVisible (freeze);
    inMeter.palette = outMeter.palette = &palette;
    content.addAndMakeVisible (inMeter);
    content.addAndMakeVisible (outMeter);

    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { savePreset(); };
    themeButton.onClick = [this] { proc.lightTheme = ! proc.lightTheme; applyTheme(); };
    licenseButton.onClick = [this] { showActivation (true); };
    for (auto* b : { &presetButton, &saveButton, &themeButton, &licenseButton })
        content.addAndMakeVisible (b);

    modeAttachment->sendInitialUpdate();
    feelAttachment->sendInitialUpdate();
    syncAttachment->sendInitialUpdate();

    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->attachTime(); safe->applyTheme(); } });
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

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png [LT_LIGHT=1]
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
        if (juce::SystemStats::getEnvironmentVariable ("LT_LIGHT", {}).isNotEmpty()) themeButton.triggerClick();
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

void Editor::attachTime()
{
    auto sync = proc.state.getRawParameterValue (IDs::sync)->load() > 0.5f;
    syncButton.setToggleState (sync, juce::dontSendNotification);
    msButton.setToggleState (! sync, juce::dontSendNotification);
    if (timeAttached && sync == timeIsSync) return;
    timeIsSync = sync;
    timeAttached = true;
    timeAttachment.reset();
    timeAttachment = std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (sync ? IDs::note : IDs::ms), time);
    if (sync) time.formatValue = [] (double v) { return Notes::names()[juce::roundToInt (v)]; };
    else time.formatValue = [] (double v) { return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms"; };
    time.repaint();
}

void Editor::applyTheme()
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    lnf.setPalette (palette);
    background.setPalette (palette);
    if (activation) activation->setPalette (palette);
    content.repaint();
}

void Editor::timerCallback()
{
    auto& e = proc.engine;
    OrbitState s;
    s.delaySeconds = proc.getDelaySeconds();
    s.feedback = proc.state.getRawParameterValue (IDs::feedback)->load() * 0.01f;
    s.mode = juce::roundToInt (proc.state.getRawParameterValue (IDs::mode)->load());
    s.frozen = proc.state.getRawParameterValue (IDs::freeze)->load() > 0.5f;
    s.level = juce::jlimit (0.0f, 1.0f, juce::jmax (e.getInputLevel (0), e.getInputLevel (1)) * 1.5f);
    s.hits = e.getHitCount();
    orbitView.tick (s);
    background.setEnergy (juce::jlimit (0.0f, 1.0f, e.getWetLevel() * 1.5f));

    inMeter.setLevels (e.getInputLevel (0), e.getInputLevel (1));
    outMeter.setLevels (e.getOutputLevel (0), e.getOutputLevel (1));

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

    presetButton.setBounds (35, 30, 186, 30);
    saveButton.setBounds (230, 30, 70, 30);
    themeButton.setBounds (982, 30, 86, 30);
    licenseButton.setBounds (836, 30, 136, 30);

    const int widths[] = { 76, 64, 78, 98, 82, 86, 64, 64 };
    auto x = 221;
    for (int i = 0; i < Modes::count; ++i)
    {
        modeButtons[i].setBounds (x, 106, widths[i], 30);
        x += widths[i] + 8;
    }

    orbitView.setBounds (230, 150, 640, 340);

    time.setBounds (114 - 70, 212 - 70, 140, 140);
    feelButtons[0].setBounds (26, 312, 58, 24);
    feelButtons[1].setBounds (86, 312, 58, 24);
    feelButtons[2].setBounds (146, 312, 60, 24);
    syncButton.setBounds (62, 348, 50, 26);
    msButton.setBounds (118, 348, 42, 26);
    feedback.setBounds (114 - 70, 492 - 70, 140, 140);

    mix.setBounds (997 - 70, 212 - 70, 140, 140);
    freeze.setBounds (948, 334, 98, 32);
    inMeter.setBounds (958, 428, 22, 132);
    outMeter.setBounds (1016, 428, 22, 132);

    const int cx[] = { 271, 383, 496, 608, 720, 832 };
    OrbitKnob* small[] = { &lowCut, &highCut, &width, &duck, &wobble, &drive };
    for (int i = 0; i < 6; ++i)
        small[i]->setBounds (cx[i] - 50, 500, 100, 96);

    if (activation) activation->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::Decor::paint (juce::Graphics& g)
{
    auto& e = owner;
    auto& p = e.palette;
    Glass::drawPanel (g, { 8.0f, 8.0f, (float) baseWidth - 16.0f, (float) baseHeight - 16.0f }, 14.0f, p, 0.2f);
    Logo::draw (g, { 0.0f, 26.0f, (float) baseWidth, 42.0f }, Logo::specFor ("ORBIT"), p, juce::Justification::centred);
    g.setColour (p.glassEdge);
    g.drawHorizontalLine (85, 35.0f, (float) baseWidth - 35.0f);

    auto label = [&] (const juce::String& t, juce::Rectangle<float> r, float size)
    {
        g.setColour (p.text);
        Fonts::drawSpaced (g, t, r, Fonts::regular (size), 0.05f, juce::Justification::centred);
    };
    label ("TIME", { 54.0f, 112.0f, 120.0f, 22.0f }, 18.0f);
    label ("FEEDBACK", { 44.0f, 562.0f, 140.0f, 22.0f }, 18.0f);
    label ("MIX", { 937.0f, 112.0f, 120.0f, 22.0f }, 18.0f);
    label ("FREEZE", { 937.0f, 309.0f, 120.0f, 22.0f }, 17.0f);
    label ("IN", { 949.0f, 402.0f, 40.0f, 18.0f }, 15.0f);
    label ("OUT", { 1007.0f, 402.0f, 40.0f, 18.0f }, 15.0f);
    label ("IN", { 949.0f, 568.0f, 40.0f, 18.0f }, 15.0f);
    label ("OUT", { 1007.0f, 568.0f, 40.0f, 18.0f }, 15.0f);
    g.setColour (p.glassEdge);
    g.drawVerticalLine (115, 352.0f, 370.0f);
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
        if (pr.group != group) { m.addSectionHeader (pr.name == "Default" ? juce::String ("DEFAULT") : pr.group); group = pr.group; }
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
        if (r >= 1 && r <= (int) list.size()) { proc.applyPreset (list[(size_t) r - 1]); attachTime(); }
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
        auto mode = juce::roundToInt (proc.state.getRawParameterValue (IDs::mode)->load());
        if (proc.presets.save (lt::presets::ParamPreset::capture (proc.state, name, Modes::names()[mode])))
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

} // namespace orbit
