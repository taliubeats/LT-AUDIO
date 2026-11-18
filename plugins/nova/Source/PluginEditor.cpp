#include "PluginEditor.h"

namespace nova
{

using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Logo;

bool Editor::activationShownThisSession = false;

Editor::Editor (Processor& pr)
    : AudioProcessorEditor (pr), proc (pr), focusView (pr.state, pr.spectrum, "NOVA")
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (background);
    content.addAndMakeVisible (decor);

    star.palette = &palette;
    content.addAndMakeVisible (star);

    typeBar.palette = &palette;
    content.addAndMakeVisible (typeBar);

    character.leftTag = "EVEN";
    character.rightTag = "ODD";
    std::pair<NovaKnob*, const char*> knobs[] = { { &tone, IDs::tone }, { &warmth, IDs::warmth }, { &air, IDs::air },
                                                  { &character, IDs::character }, { &input, IDs::input }, { &mix, IDs::mix },
                                                  { &output, IDs::output } };
    for (auto& [k, id] : knobs)
    {
        k->palette = &palette;
        content.addAndMakeVisible (k);
    }
    tone.setDoubleClickReturnValue (true, 0.0);
    input.setDoubleClickReturnValue (true, 0.0);
    output.setDoubleClickReturnValue (true, 0.0);
    mix.setDoubleClickReturnValue (true, 100.0);

    autoGain.palette = &palette;
    autoGain.setTooltip ("Keeps the level the same while you add drive, so you only hear the colour");
    content.addAndMakeVisible (autoGain);

    inMeter.palette = outMeter.palette = &palette;
    outMeter.showScale = false;
    inMeter.showScale = false;
    content.addAndMakeVisible (inMeter);
    content.addAndMakeVisible (outMeter);

    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { savePreset(); };
    eqButton.onClick = [this] { showFocus (true); };
    themeButton.onClick = [this] { proc.lightTheme = ! proc.lightTheme; applyTheme(); };
    licenseButton.onClick = [this] { showActivation (true); };
    for (auto* b : { &presetButton, &saveButton, &eqButton, &themeButton, &licenseButton })
        content.addAndMakeVisible (b);
    for (auto* b : { &presetButton, &saveButton, &eqButton, &themeButton })
        b->getProperties().set ("fontSize", 17.0f);

    focusView.setGlassStyle (true);
    focusView.onClose = [this] { showFocus (false); };
    content.addChildComponent (focusView);

    // attachments last
    attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (IDs::drive), star));
    for (auto& [k, id] : knobs)
        attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (id), *k));
    autoGainAttachment = std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (IDs::autoGain), autoGain);
    typeAttachment = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::type),
                                                                  [this] (float v) { typeBar.setSelected (juce::roundToInt (v)); });
    typeBar.onChange = [this] (int i) { typeAttachment->setValueAsCompleteGesture ((float) i); background.pulse (0.6f); };
    typeAttachment->sendInitialUpdate();

    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->applyTheme(); });
    };

    applyTheme();
    setResizable (true, true);
    setResizeLimits (baseWidth * 6 / 10, baseHeight * 6 / 10, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (baseWidth * 5 / 6, baseHeight * 5 / 6);

    if (! proc.license.isLicensed() && (! activationShownThisSession || ! proc.license.isUsable()))
    {
        activationShownThisSession = true;
        showActivation (true);
    }
    startTimerHz (30);

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png [LT_LIGHT=1] [LT_SHOW_EQ=1] [LT_DRIVE=0..100]
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
        if (juce::SystemStats::getEnvironmentVariable ("LT_LIGHT", {}).isNotEmpty()) themeButton.triggerClick();
        auto dv = juce::SystemStats::getEnvironmentVariable ("LT_DRIVE", {});
        if (dv.isNotEmpty()) star.setValue (dv.getDoubleValue(), juce::sendNotificationSync);
        if (juce::SystemStats::getEnvironmentVariable ("LT_SHOW_EQ", {}).isNotEmpty())
        {
            if (auto* b = proc.state.getParameter (lt::dsp::FocusParams::band)) b->setValueNotifyingHost (b->convertTo0to1 (1.0f));
            showFocus (true);
        }
        juce::Timer::callAfterDelay (500, [this]
        {
            proc.prepareToPlay (48000.0, 512);
            juce::AudioBuffer<float> b (2, 512);
            juce::MidiBuffer m;
            for (int blk = 0; blk < 120; ++blk)
            {
                for (int i = 0; i < 512; ++i)
                {
                    auto t = (blk * 512 + i) / 48000.0;
                    auto v = (float) (0.4 * std::sin (6.2832 * 110.0 * t) + 0.2 * std::sin (6.2832 * 440.0 * t) * std::exp (-std::fmod (t, 0.5) * 6));
                    b.setSample (0, i, v); b.setSample (1, i, v * 0.9f);
                }
                proc.processBlock (b, m);
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
    attachments.clear();
    autoGainAttachment.reset();
    typeAttachment.reset();
    proc.onStateRestored = nullptr;
    setLookAndFeel (nullptr);
}

void Editor::applyTheme()
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    lnf.setPalette (palette);
    background.setPalette (palette);
    focusView.setPalette (palette);
    if (activation) activation->setPalette (palette);
    content.repaint();
}

void Editor::timerCallback()
{
    auto e = proc.engine.getDriveEnergy();
    auto level = juce::jmax (proc.getOutputLevel (0), proc.getOutputLevel (1));
    star.tick (juce::jlimit (0.0f, 1.0f, level * 1.6f) * (0.3f + 0.7f * e));
    background.setEnergy (juce::jlimit (0.0f, 1.0f, level * e));
    background.setParticleAmount (0.25f + 0.6f * (float) star.valueToProportionOfLength (star.getValue()));
    inMeter.setLevels (proc.getInputLevel (0), proc.getInputLevel (1));
    outMeter.setLevels (proc.getOutputLevel (0), proc.getOutputLevel (1));

    auto name = proc.presetName + (proc.isPresetModified() ? " *" : "");
    if (presetButton.getButtonText() != name) presetButton.setButtonText (name);

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

    presetButton.setBounds (57, 55, 196, 40);
    saveButton.setBounds (266, 55, 88, 40);
    eqButton.setBounds (932, 55, 96, 40);
    themeButton.setBounds (1041, 55, 102, 40);
    licenseButton.setBounds (520, 838, 160, 24);

    typeBar.setBounds (57, 148, 1086, 40);

    star.setBounds (600 - 460, 448 - 460, 920, 920);   // big so the corona fades out without clipping

    // (cx, cy) = centre of the knob in the design; the component is larger so glows fade out freely
    auto knob = [] (NovaKnob& k, int cx, int cy, float d)
    {
        auto w = (int) (d * 2.2f), h = (int) (d * 1.5f + 46.0f);
        k.setBounds (cx - w / 2, cy - (int) d, w, h + (int) (d * 0.5f));
    };
    knob (tone, 158, 333, 96);
    knob (warmth, 158, 530, 96);
    knob (air, 1041, 333, 96);
    knob (character, 1041, 535, 96);
    knob (input, 447, 768, 78);
    knob (mix, 600, 768, 78);
    knob (output, 747, 768, 78);

    autoGain.setBounds (852, 750, 84, 36);
    inMeter.setBounds (1028, 702, 18, 114);
    outMeter.setBounds (1100, 702, 18, 114);

    if (activation) activation->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::Decor::paint (juce::Graphics& g)
{
    auto& e = owner;
    auto& p = e.palette;

    auto main = juce::Rectangle<float> (30.0f, 28.0f, (float) baseWidth - 60.0f, (float) baseHeight - 56.0f);
    Glass::drawPanel (g, main, 16.0f, p, 0.35f);
    g.setColour ((p.isLight ? juce::Colours::white : p.text).withAlpha (0.18f));
    g.drawRoundedRectangle (main.reduced (0.5f), 16.0f, 1.2f);
    g.drawHorizontalLine (124, main.getX() + 1.0f, main.getRight() - 1.0f);

    Logo::draw (g, { 400.0f, 50.0f, 400.0f, 54.0f }, Logo::specFor ("NOVA"), p, juce::Justification::centred);

    g.setColour (p.text);
    g.setFont (Fonts::regular (15.0f));
    g.drawText ("AUTO GAIN", juce::Rectangle<float> (824.0f, 792.0f, 140.0f, 22.0f), juce::Justification::centred);
    g.drawText ("IN", juce::Rectangle<float> (1017.0f, 822.0f, 40.0f, 20.0f), juce::Justification::centred);

    // dB scale between the two meters, like the design
    g.setColour (p.textDim);
    g.setFont (Fonts::regular (11.0f));
    for (auto db : { 0, -6, -18, -24 })
    {
        auto y = 816.0f - ((float) db + 48.0f) / 48.0f * 114.0f;
        g.drawText (juce::String (db), juce::Rectangle<float> (1054.0f, y - 7.0f, 40.0f, 14.0f), juce::Justification::centred);
    }
    g.drawText ("dB", juce::Rectangle<float> (1054.0f, 804.0f, 40.0f, 14.0f), juce::Justification::centred);
    g.drawText ("OUT", juce::Rectangle<float> (1089.0f, 822.0f, 40.0f, 20.0f), juce::Justification::centred);
}

// ------------------------------------------------------------------ presets

void Editor::showPresetMenu()
{
    proc.presets.rescan();
    auto& all = proc.presets.all();
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    juce::StringArray groups;
    for (auto& pr : all) if (pr.factory) groups.addIfNotAlreadyThere (pr.group);
    for (auto& group : groups)
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
        if (r >= 1 && r <= (int) list.size()) { proc.applyPreset (list[(size_t) r - 1]); background.pulse (0.5f); }
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
        auto type = juce::roundToInt (proc.state.getRawParameterValue (IDs::type)->load());
        if (proc.presets.save (lt::presets::ParamPreset::capture (proc.state, name, SatTypes::names()[type])))
            proc.presetName = name;
    }), false);
}

void Editor::showFocus (bool show)
{
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

} // namespace nova
