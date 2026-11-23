#include "PluginEditor.h"

namespace saturn
{

using lt::ui::Fonts;
using lt::ui::Glass;

bool Editor::activationShownThisSession = false;

namespace
{
    // layout (base 1200 x 896, see design/saturn/main-v2.webp)
    const juce::Rectangle<int> topPanel { 50, 10, 1100, 228 }, midPanel { 50, 242, 1100, 458 }, bottomPanel { 50, 704, 1100, 186 };
    const int knobX[] = { 133, 250, 366, 483, 600, 716, 833, 950, 1063 };
}

Editor::Editor (Processor& pr) : AudioProcessorEditor (pr), proc (pr)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (decor);

    screen.look = &look;
    content.addAndMakeVisible (screen);

    std::pair<VintageKnob*, const char*> knobs[] = {
        { &age, IDs::age }, { &output, IDs::output }, { &wow, IDs::wow }, { &flutter, IDs::flutter }, { &drift, IDs::drift },
        { &saturation, IDs::saturation }, { &tone, IDs::tone }, { &bandwidth, IDs::bandwidth }, { &hiss, IDs::hiss },
        { &ensemble, IDs::ensemble }, { &mix, IDs::mix } };
    for (auto& [k, id] : knobs)
    {
        k->look = &look;
        attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (id), *k));
        content.addAndMakeVisible (k);
    }
    mix.alwaysShowArc = true;
    tone.setDoubleClickReturnValue (true, 0.0);
    output.setDoubleClickReturnValue (true, 0.0);

    for (int i = 0; i < Tapes::count; ++i)
    {
        auto* k = tapeKeys.add (new KeyButton (Tapes::names()[i]));
        k->look = &look;
        k->onClick = [this, i] { tapeAttachment->setValueAsCompleteGesture ((float) i); };
        content.addAndMakeVisible (k);
    }
    tapeAttachment = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::tape), [this] (float v)
    {
        for (int i = 0; i < tapeKeys.size(); ++i) tapeKeys[i]->setToggleState (i == juce::roundToInt (v), juce::dontSendNotification);
    });
    tapeAttachment->sendInitialUpdate();

    inMeter.look = outMeter.look = &look;
    content.addAndMakeVisible (inMeter);
    content.addAndMakeVisible (outMeter);

    presetsKey.onClick = [this] { showPresetMenu(); };
    saveKey.onClick = [this] { savePreset(); };
    themeKey.onClick = [this] { proc.night = ! proc.night; applyTheme(); };
    licenseKey.onClick = [this] { showActivation (true); };
    for (auto* k : { &presetsKey, &saveKey, &themeKey, &licenseKey })
    {
        k->look = &look;
        content.addAndMakeVisible (k);
    }

    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->applyTheme(); });
    };

    applyTheme();
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
    startTimerHz (30);

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png [LT_NIGHT=1]
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
        juce::Timer::callAfterDelay (500, [this]
        {
            proc.night = juce::SystemStats::getEnvironmentVariable ("LT_NIGHT", {}).isNotEmpty();
            applyTheme();
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
    look = proc.night ? Vintage::nightPanel() : Vintage::cream();
    panelTop = Vintage::renderPanel (topPanel.getWidth(), topPanel.getHeight(), look, 11);
    panelMid = Vintage::renderPanel (midPanel.getWidth(), midPanel.getHeight(), look, 12);
    panelBottom = Vintage::renderPanel (bottomPanel.getWidth(), bottomPanel.getHeight(), look, 13);
    woodLeft = Vintage::renderWood (52, baseHeight, look, 3);
    woodRight = Vintage::renderWood (52, baseHeight, look, 4);
    lnf.setPalette (lt::ui::Palette::dark());
    content.repaint();
}

void Editor::timerCallback()
{
    auto now = juce::Time::getMillisecondCounter();
    auto dt = lastTick == 0 ? 1.0f / 30.0f : juce::jlimit (0.005f, 0.2f, (float) (now - lastTick) * 0.001f);
    lastTick = now;

    auto& e = proc.engine;
    auto s = proc.currentSettings();
    TapeState t;
    t.tape = s.tape;
    t.age = s.age; t.wow = s.wow; t.flutter = s.flutter; t.drift = s.drift; t.hiss = s.hiss;
    t.ensemble = s.ensemble; t.saturation = s.saturation;
    t.speed = e.getSpeed();
    t.dropout = e.getDropout();
    t.level = juce::jlimit (0.0f, 1.0f, e.getInputLevel() * 1.5f);
    screen.tick (t);
    inMeter.setLevel (e.getInputLevel(), dt);
    outMeter.setLevel (e.getOutputLevel(), dt);

    auto chip = proc.license.isLicensed() ? juce::String() : proc.license.isUsable()
                  ? juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (proc.license.getTrialDaysLeft()) + " DAYS"
                  : juce::String ("ACTIVATE");
    if (licenseKey.getButtonText() != chip) { licenseKey.setButtonText (chip); decor.repaint(); }
    licenseKey.setVisible (chip.isNotEmpty());

    static juce::String lastPreset;
    auto name = proc.presetName + (proc.isPresetModified() ? " *" : "");
    if (name != lastPreset) { lastPreset = name; decor.repaint(); }
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
    decor.setBounds (0, 0, baseWidth, baseHeight);

    presetsKey.setBounds (956, 70, 46, 46);
    saveKey.setBounds (1010, 70, 46, 46);
    themeKey.setBounds (1064, 70, 46, 46);
    licenseKey.setBounds (78, 66, 200, 46);

    for (int i = 0; i < tapeKeys.size(); ++i)
        tapeKeys[i]->setBounds (101 + i * 167, 158, 161, 60);

    screen.setBounds (282, 253, 636, 437);
    age.setBounds (45, 305, 240, 290);
    output.setBounds (80, 612, 190, 60);
    inMeter.setBounds (947, 280, 180, 180);
    outMeter.setBounds (947, 490, 180, 180);

    VintageKnob* small[] = { &wow, &flutter, &drift, &saturation, &tone, &bandwidth, &hiss, &ensemble, &mix };
    for (int i = 0; i < 9; ++i)
        small[i]->setBounds (knobX[i] - 58, 728, 116, 130);

    if (activation) activation->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::Decor::paint (juce::Graphics& g)
{
    auto& e = owner;
    auto& v = e.look;

    g.fillAll (v.night ? juce::Colour (0xff0d0b0a) : juce::Colour (0xff1d1510));
    g.drawImageAt (e.panelTop, topPanel.getX(), topPanel.getY());
    g.drawImageAt (e.panelMid, midPanel.getX(), midPanel.getY());
    g.drawImageAt (e.panelBottom, bottomPanel.getX(), bottomPanel.getY());
    g.drawImageAt (e.woodLeft, 0, 0);
    g.drawImageAt (e.woodRight, baseWidth - 52, 0);

    auto screws = [&] (juce::Rectangle<int> r, int inset)
    {
        juce::Random rnd (r.getY());
        for (auto pt : { juce::Point<int> (r.getX() + inset, r.getY() + inset), juce::Point<int> (r.getRight() - inset, r.getY() + inset),
                         juce::Point<int> (r.getX() + inset, r.getBottom() - inset), juce::Point<int> (r.getRight() - inset, r.getBottom() - inset) })
            Vintage::drawScrew (g, pt.toFloat(), 8.5f, rnd.nextFloat() * 3.0f, v);
    };
    screws (topPanel, 20);
    screws (midPanel, 20);
    screws (bottomPanel, 20);
    screws ({ 282, 253, 636, 437 }, 20);

    // logo: embossed purple "LT" + serif "SATURN" with a little planet above
    {
        auto ltArea = juce::Rectangle<float> (372.0f, 38.0f, 150.0f, 100.0f);
        auto ltFont = Fonts::bold (104.0f);
        g.setFont (ltFont);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawText ("LT", ltArea.translated (4.0f, 7.0f), juce::Justification::centredRight);
        for (int d = 5; d >= 1; --d)      // extruded, like a metal badge
        {
            g.setColour (v.accent.darker (0.9f - 0.08f * (float) d));
            g.drawText ("LT", ltArea.translated ((float) d * 0.6f, (float) d * 1.0f), juce::Justification::centredRight);
        }
        juce::ColourGradient ltGrad (v.accent.brighter (0.45f), 0, ltArea.getY() + 15.0f, v.accent.darker (0.2f), 0, ltArea.getBottom() - 10.0f, false);
        g.setGradientFill (ltGrad);
        g.drawText ("LT", ltArea, juce::Justification::centredRight);

        g.setColour (juce::Colours::white.withAlpha (v.night ? 0.05f : 0.45f));
        g.setFont (Vintage::serif (82.0f));
        auto nameArea = juce::Rectangle<float> (534.0f, 46.0f, 320.0f, 96.0f);
        g.drawText ("SATURN", nameArea.translated (0.0f, 1.5f), juce::Justification::centredLeft);
        g.setColour (v.text);
        g.drawText ("SATURN", nameArea, juce::Justification::centredLeft);

        // version badge, engraved after the name
        g.setColour (v.accent.darker (v.night ? 0.0f : 0.3f));
        Fonts::drawSpaced (g, "v2", { 818.0f, 104.0f, 40.0f, 22.0f }, Fonts::semiBold (17.0f), 0.04f, juce::Justification::centredLeft);

        auto pc = juce::Point<float> (688.0f, 52.0f);
        g.setColour (v.text.withAlpha (0.55f));
        g.drawLine (548.0f, pc.y, pc.x - 22.0f, pc.y, 1.0f);
        g.drawLine (pc.x + 22.0f, pc.y, 810.0f, pc.y, 1.0f);
        juce::ColourGradient pg (juce::Colour (0xfff2dfc6), pc.x - 6.0f, pc.y - 7.0f, juce::Colour (0xff4b2c66), pc.x + 8.0f, pc.y + 9.0f, true);
        g.setGradientFill (pg);
        g.fillEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (pc));
        juce::Path ring;
        ring.addEllipse (-19.0f, -5.0f, 38.0f, 10.0f);
        ring.applyTransform (juce::AffineTransform::rotation (-0.3f).translated (pc.x, pc.y));
        g.setColour (v.accent.withAlpha (0.85f));
        g.strokePath (ring, juce::PathStrokeType (1.8f));
    }

    auto label = [&] (const juce::String& t, juce::Rectangle<float> r, float size)
    {
        g.setColour (v.text);
        Fonts::drawSpaced (g, t, r, Fonts::regular (size), 0.04f, juce::Justification::centred);
    };
    label ("PRESETS", { 939.0f, 120.0f, 80.0f, 18.0f }, 13.0f);
    label ("SAVE", { 1003.0f, 120.0f, 60.0f, 18.0f }, 13.0f);
    label ("THEME", { 1057.0f, 120.0f, 60.0f, 18.0f }, 13.0f);
    label ("IN", { 987.0f, 456.0f, 100.0f, 24.0f }, 19.0f);
    label ("OUT", { 987.0f, 666.0f, 100.0f, 24.0f }, 19.0f);

    // current preset, engraved under the program keys
    auto preset = e.proc.presetName + (e.proc.isPresetModified() ? " *" : "");
    g.setColour (v.textSoft);
    Fonts::drawSpaced (g, preset.toUpperCase(), { 101.0f, 220.0f, 998.0f, 16.0f }, Fonts::regular (11.0f), 0.18f, juce::Justification::centred);
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

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetsKey), [this] (int r)
    {
        auto& list = proc.presets.all();
        if (r >= 1 && r <= (int) list.size()) proc.applyPreset (list[(size_t) r - 1]);
        else if (r == 10001) savePreset();
        else if (r == 10002) proc.presets.remove (proc.presetName);
        else if (r == 10003) { proc.presets.userFolder().createDirectory(); proc.presets.userFolder().startAsProcess(); }
        decor.repaint();
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
        auto tape = juce::roundToInt (proc.state.getRawParameterValue (IDs::tape)->load());
        if (proc.presets.save (lt::presets::ParamPreset::capture (proc.state, name, Tapes::names()[tape])))
            proc.presetName = name;
        decor.repaint();
    }), false);
}

void Editor::showActivation (bool show)
{
    if (show && activation == nullptr)
    {
        proc.license.refresh();
        activation = std::make_unique<lt::ui::ActivationComponent> (proc.license, lt::ui::Palette::dark());
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

} // namespace saturn
