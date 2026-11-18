#include "PluginEditor.h"

namespace vocal
{

using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Logo;

bool Editor::activationShownThisSession = false;

namespace
{
    juce::String utf8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }
    const char* moduleNames[] = { "TUNE", "CLEANUP", "TONE", "DYNAMICS", "SPACE", "FX" };
}

void Editor::BypassButton::paintButton (juce::Graphics& g, bool hover, bool)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    auto pink = juce::Colour (0xffff5a9e);
    if (getToggleState())
    {
        Glass::drawRectGlow (g, r, r.getHeight() * 0.5f, pink, 12.0f, 0.9f);
        g.setColour (pink.withAlpha (0.25f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    else
    {
        Glass::drawPanel (g, r, r.getHeight() * 0.5f, p, hover ? 1.8f : 1.1f);
        Glass::drawRectGlow (g, r, r.getHeight() * 0.5f, pink, 6.0f, 0.35f);
    }
    g.setColour (pink.withAlpha (getToggleState() ? 1.0f : 0.8f));
    g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.6f);
    g.setColour (p.isLight ? p.text : juce::Colours::white);
    g.setFont (Fonts::semiBold (16.0f));
    g.drawText ("BYPASS", r, juce::Justification::centred);
}

// ------------------------------------------------------------------ control factories

SuiteKnob& Editor::knob (const char* id, const juce::String& label, float d, int module)
{
    auto* k = knobs.add (new SuiteKnob (label, d));
    k->palette = &palette;
    if (auto* info = findParam (id)) k->setTooltip (info->help);
    k->setDoubleClickReturnValue (true, findParam (id)->def);
    content.addChildComponent (k);
    moduleOf[k] = module;
    byId[id] = k;
    sliderAttachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (id), *k));
    return *k;
}

ChoicePill& Editor::choice (const char* id, int module, bool large)
{
    auto* c = choices.add (new ChoicePill());
    c->palette = &palette;
    c->large = large;
    if (auto* info = findParam (id)) c->setTooltip (info->help);
    content.addChildComponent (c);
    moduleOf[c] = module;
    byId[id] = c;
    auto names = choicesFor (id);
    auto* att = choiceAttachments.emplace_back (std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (id),
        [c, names] (float v) { c->text = names[juce::roundToInt (v)]; c->repaint(); })).get();
    c->onClick = [this, c, names, att]
    {
        juce::PopupMenu m;
        m.setLookAndFeel (&lnf);
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, names[i] == c->text);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (c), [att] (int r) { if (r > 0) att->setValueAsCompleteGesture ((float) (r - 1)); });
    };
    att->sendInitialUpdate();
    return *c;
}

ModuleSwitch& Editor::toggle (const char* id, const juce::String& label, int module)
{
    auto* s = switches.add (new ModuleSwitch (label));
    s->palette = &palette;
    content.addChildComponent (s);
    moduleOf[s] = module;
    byId[id] = s;
    buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (id), *s));
    return *s;
}

// ------------------------------------------------------------------ construction

Editor::Editor (Processor& pr) : AudioProcessorEditor (pr), proc (pr)
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (background);
    content.addAndMakeVisible (decor);

    // header
    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { savePreset(); };
    undoButton.onClick = [this] { proc.undo(); };
    redoButton.onClick = [this] { proc.redo(); };
    themeButton.onClick = [this] { proc.lightTheme = ! proc.lightTheme; applyTheme(); };
    licenseButton.onClick = [this] { showActivation (true); };
    for (auto* b : { &presetButton, &saveButton, &undoButton, &redoButton, &themeButton, &licenseButton })
        content.addAndMakeVisible (b);
    for (auto* b : { &undoButton, &redoButton, &themeButton })
        b->getProperties().set ("textOnly", true);
    presetButton.getProperties().set ("chevron", true);
    for (auto* b : { &presetButton, &saveButton, &undoButton, &redoButton, &themeButton })
        b->getProperties().set ("fontSize", 16.5f);
    bypass.palette = &palette;
    content.addAndMakeVisible (bypass);
    buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (IDs::bypass), bypass));

    // assistant
    mascot.palette = &palette;
    mascot.setTooltip ("Assistant settings (Claude API key)");
    mascot.onClick = [this] { showAssistantMenu(); };
    content.addAndMakeVisible (mascot);

    chatInput.getProperties().set ("bare", true);
    chatInput.setFont (Fonts::regular (19.0f));
    chatInput.setBorder ({ 0, 0, 0, 0 });
    chatInput.setIndents (6, 0);
    chatInput.setJustification (juce::Justification::centredLeft);
    chatInput.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    chatInput.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    chatInput.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    chatInput.setColour (juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
    chatInput.onReturnKey = [this] { send(); };
    content.addAndMakeVisible (chatInput);

    sendButton.getProperties().set ("accent", true);
    sendButton.onClick = [this] { send(); };
    content.addAndMakeVisible (sendButton);

    micButton.getProperties().set ("textOnly", true);
    micButton.setTooltip ("Dictate: on Mac press Fn twice (or the dictation key) while the text box is selected");
    micButton.onClick = [this]
    {
        chatInput.grabKeyboardFocus();
        setBubble (utf8 ("Habla con tu voz: en Mac pulsa dos veces la tecla Fn (o la tecla de dictado) y di lo que quieres cambiar."), false);
    };
    content.addAndMakeVisible (micButton);

    proc.assistant.onReply = [this] (const AssistantReply& r)
    {
        mascot.thinking = false;
        sendButton.setEnabled (true);
        if (! r.changes.empty()) { proc.applyValues (r.changes); background.pulse (0.8f); }
        proc.lastAssistantMessage = r.summary;
        setBubble (r.summary, ! r.failed);
    };

    // module tabs
    for (int i = 0; i < numModules; ++i)
    {
        auto* t = tabs.add (new ModuleTab (moduleNames[i], i));
        t->palette = &palette;
        t->onClick = [this, i] { showModule (i); };
        content.addAndMakeVisible (t);
    }

    // TUNE
    toggle (IDs::tuneOn, "TUNE", tuneM);
    keyPill.palette = &palette;
    keyPill.large = true;
    content.addChildComponent (keyPill);
    moduleOf[&keyPill] = tuneM;
    keyAttachment = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::key), [this] (float) { keyPill.text = tune::Scales::display (juce::roundToInt (proc.state.getRawParameterValue (IDs::key)->load()), juce::roundToInt (proc.state.getRawParameterValue (IDs::scale)->load())); keyPill.repaint(); });
    scaleAttachment = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::scale), [this] (float) { keyAttachment->sendInitialUpdate(); });
    keyPill.onClick = [this]
    {
        juce::PopupMenu m;
        m.setLookAndFeel (&lnf);
        auto curKey = juce::roundToInt (proc.state.getRawParameterValue (IDs::key)->load());
        auto curScale = juce::roundToInt (proc.state.getRawParameterValue (IDs::scale)->load());
        for (int k = 0; k < 12; ++k)
        {
            juce::PopupMenu sub;
            for (int s = 0; s < tune::Scales::count; ++s)
                if (s != tune::Scales::chromatic)
                    sub.addItem (1000 + k * 16 + s, tune::Scales::names()[s], true, k == curKey && s == curScale);
            m.addSubMenu (tune::Keys::names()[k], sub, true, nullptr, k == curKey && curScale != tune::Scales::chromatic);
        }
        m.addSeparator();
        m.addItem (999, "Chromatic", true, curScale == tune::Scales::chromatic);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&keyPill), [this] (int r)
        {
            if (r == 999) scaleAttachment->setValueAsCompleteGesture ((float) tune::Scales::chromatic);
            else if (r >= 1000)
            {
                keyAttachment->setValueAsCompleteGesture ((float) ((r - 1000) / 16));
                scaleAttachment->setValueAsCompleteGesture ((float) ((r - 1000) % 16));
            }
        });
    };
    keyAttachment->sendInitialUpdate();
    choice (IDs::voice, tuneM);
    tuneDisplay.palette = &palette;
    content.addChildComponent (tuneDisplay);
    moduleOf[&tuneDisplay] = tuneM;
    knob (IDs::retune, "RETUNE SPEED", 92, tuneM);
    knob (IDs::humanize, "HUMANIZE", 92, tuneM);
    knob (IDs::flex, "FLEX TUNE", 92, tuneM);
    knob (IDs::tuneMix, "MIX", 92, tuneM);

    // CLEANUP
    knob (IDs::lowCut, "LOW CUT", 96, cleanupM);
    knob (IDs::gate, "GATE", 96, cleanupM);
    knob (IDs::deEss, "DE-ESS", 96, cleanupM);
    knob (IDs::deEssFreq, "DE-ESS FREQ", 96, cleanupM);
    essBar.palette = &palette; essBar.label = "DE-ESSING"; essBar.maxDb = 12.0f;
    content.addChildComponent (essBar);
    moduleOf[&essBar] = cleanupM;

    // TONE
    eqCurve.palette = &palette;
    content.addChildComponent (eqCurve);
    moduleOf[&eqCurve] = toneM;
    knob (IDs::body, "BODY", 88, toneM);
    knob (IDs::mud, "MUD CUT", 88, toneM);
    knob (IDs::presence, "PRESENCE", 88, toneM);
    knob (IDs::air, "AIR", 88, toneM);

    // DYNAMICS
    knob (IDs::comp, "COMPRESSION", 120, dynamicsM);
    knob (IDs::attack, "ATTACK", 76, dynamicsM);
    knob (IDs::release, "RELEASE", 76, dynamicsM);
    grBar.palette = &palette; grBar.label = "GAIN REDUCTION"; grBar.maxDb = 18.0f;
    content.addChildComponent (grBar);
    moduleOf[&grBar] = dynamicsM;
    choice (IDs::satType, dynamicsM);
    knob (IDs::satDrive, "SATURATION", 88, dynamicsM);
    knob (IDs::satMix, "SAT MIX", 88, dynamicsM);

    // SPACE
    toggle (IDs::delayOn, "DELAY", spaceM);
    choice (IDs::delayNote, spaceM);
    choice (IDs::delayMode, spaceM);
    knob (IDs::delayFeedback, "FEEDBACK", 80, spaceM);
    knob (IDs::delayMix, "MIX", 80, spaceM);
    toggle (IDs::reverbOn, "REVERB", spaceM);
    choice (IDs::reverbType, spaceM);
    knob (IDs::reverbDecay, "DECAY", 80, spaceM);
    knob (IDs::reverbPre, "PRE-DELAY", 80, spaceM);
    knob (IDs::reverbMix, "MIX", 80, spaceM);

    // FX
    knob (IDs::doubler, "DOUBLER", 100, fxM);
    knob (IDs::phone, "PHONE", 100, fxM);
    knob (IDs::width, "WIDTH", 100, fxM);
    knob (IDs::output, "OUTPUT", 100, fxM);

    inMeter.palette = outMeter.palette = &palette;
    inMeter.label = "INPUT"; outMeter.label = "OUTPUT";
    content.addAndMakeVisible (inMeter);
    content.addAndMakeVisible (outMeter);

    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->applyTheme(); });
    };

    bubbleText = proc.lastAssistantMessage.isNotEmpty() ? proc.lastAssistantMessage
               : utf8 ("Hola, soy tu asistente vocal. Dime qué sonido buscas: \"vocal chain estilo trap melódico\", \"más brillo\", \"autotune más natural\"...");
    bubbleHighlight = proc.lastAssistantMessage.isNotEmpty();

    applyTheme();
    showModule (tuneM);
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

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png [LT_LIGHT=1] [LT_MODULE=0..5] [LT_ASK="text"]
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
        if (juce::SystemStats::getEnvironmentVariable ("LT_LIGHT", {}).isNotEmpty()) themeButton.triggerClick();
        showModule (juce::SystemStats::getEnvironmentVariable ("LT_MODULE", "0").getIntValue());
        auto ask = juce::SystemStats::getEnvironmentVariable ("LT_ASK", {});
        if (ask.isNotEmpty()) { chatInput.setText (ask); send(); }
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
    proc.assistant.onReply = nullptr;
    sliderAttachments.clear();
    buttonAttachments.clear();
    choiceAttachments.clear();
    keyAttachment.reset();
    scaleAttachment.reset();
    proc.onStateRestored = nullptr;
    setLookAndFeel (nullptr);
}

void Editor::applyTheme()
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    lnf.setPalette (palette);
    background.setPalette (palette);
    chatInput.setTextToShowWhenEmpty ("What should I change?", palette.textDim);
    chatInput.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    chatInput.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    chatInput.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    chatInput.setColour (juce::TextEditor::textColourId, palette.isLight ? palette.text : juce::Colours::white);
    chatInput.applyColourToAllText (palette.isLight ? palette.text : juce::Colours::white);
    if (activation) activation->setPalette (palette);
    content.repaint();
}

void Editor::showModule (int m)
{
    currentModule = juce::jlimit (0, numModules - 1, m);
    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setToggleState (i == currentModule, juce::dontSendNotification);
    for (auto& [comp, mod] : moduleOf)
        comp->setVisible (mod == currentModule && activation == nullptr);
    layout();
    decor.repaint();
}

void Editor::setBubble (const juce::String& t, bool highlight)
{
    bubbleText = t;
    bubbleHighlight = highlight;
    decor.repaint();
}

void Editor::send()
{
    auto text = chatInput.getText().trim();
    if (text.isEmpty() || proc.assistant.isBusy()) return;
    chatInput.setText ({}, false);
    mascot.thinking = true;
    sendButton.setEnabled (false);
    setBubble (utf8 ("Escuchando tu pedido: \"") + text + utf8 ("\"..."), false);
    proc.assistant.ask (text, proc.currentValues());
}

// ------------------------------------------------------------------ timer / layout

void Editor::timerCallback()
{
    mascot.tick();
    auto& c = proc.chain;
    inMeter.setLevel (c.getInputLevel());
    outMeter.setLevel (c.getOutputLevel());
    background.setEnergy (juce::jlimit (0.0f, 1.0f, c.getOutputLevel() * 1.5f));

    if (currentModule == tuneM)
    {
        auto& t = c.getTune();
        tuneDisplay.set (t.isVoiced(), t.getInputMidi(), t.getTargetNote());
    }
    if (currentModule == dynamicsM) grBar.setValue (c.getGainReductionDb());
    if (currentModule == cleanupM)  essBar.setValue (c.getDeEssDb());
    if (currentModule == toneM)
    {
        auto val = [this] (const char* id) { return proc.state.getRawParameterValue (id)->load(); };
        eqCurve.lowCut = val (IDs::lowCut); eqCurve.body = val (IDs::body); eqCurve.mud = val (IDs::mud);
        eqCurve.presence = val (IDs::presence); eqCurve.air = val (IDs::air);
        eqCurve.repaint();
    }

    auto name = proc.presetName + (proc.isPresetModified() ? " *" : "");
    if (presetButton.getButtonText() != name) presetButton.setButtonText (name);
    undoButton.setEnabled (proc.canUndo());
    redoButton.setEnabled (proc.canRedo());

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

    presetButton.setBounds (63, 69, 245, 40);
    saveButton.setBounds (318, 69, 76, 40);
    undoButton.setBounds (818, 69, 72, 40);
    redoButton.setBounds (896, 69, 64, 40);
    bypass.setBounds (964, 67, 98, 44);
    themeButton.setBounds (1068, 69, 82, 40);
    licenseButton.setBounds (520, 850, 160, 24);

    mascot.setBounds (140, 165, 255, 255);
    micButton.setBounds (436, 330, 34, 46);
    chatInput.setBounds (484, 330, 476, 46);
    sendButton.setBounds (968, 331, 86, 44);

    auto tabW = (984 - 5 * 12) / 6;
    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setBounds (108 + i * (tabW + 12), 440, tabW, 50);

    inMeter.setBounds (62, 150, 26, 690);
    outMeter.setBounds (1112, 150, 26, 690);

    auto place = [this] (const char* id, int cx, int cy, int w, int h)
    { if (auto* c = byId[id]) c->setBounds (cx - w / 2, cy - h / 2, w, h); };
    auto placeKnob = [this] (const char* id, int cx, int cy)
    {
        if (auto* k = dynamic_cast<SuiteKnob*> (byId[id]))
        {
            auto d = (int) k->getDiameter();
            k->setBounds (cx - d, cy - d * 3 / 4, d * 2, d * 3 / 2 + 34);   // knob centre at (cx, cy)
        }
    };

    // TUNE (panel 108..1092 x 510..832)
    place (IDs::tuneOn, 192, 538, 130, 28);
    keyPill.setBounds (140, 640, 190, 50);
    place (IDs::voice, 235, 716, 150, 34);
    tuneDisplay.setBounds (372, 548, 276, 268);
    placeKnob (IDs::retune, 797, 596);
    placeKnob (IDs::humanize, 977, 596);
    placeKnob (IDs::flex, 797, 744);
    placeKnob (IDs::tuneMix, 977, 744);

    // CLEANUP
    placeKnob (IDs::lowCut, 238, 640);
    placeKnob (IDs::gate, 458, 640);
    placeKnob (IDs::deEss, 678, 640);
    placeKnob (IDs::deEssFreq, 898, 640);
    essBar.setBounds (580, 760, 420, 34);

    // TONE
    eqCurve.setBounds (130, 528, 500, 286);
    placeKnob (IDs::body, 760, 600);
    placeKnob (IDs::mud, 950, 600);
    placeKnob (IDs::presence, 760, 750);
    placeKnob (IDs::air, 950, 750);

    // DYNAMICS
    placeKnob (IDs::comp, 240, 628);
    placeKnob (IDs::attack, 400, 630);
    placeKnob (IDs::release, 540, 630);
    grBar.setBounds (150, 772, 440, 34);
    place (IDs::satType, 850, 580, 220, 40);
    placeKnob (IDs::satDrive, 760, 712);
    placeKnob (IDs::satMix, 940, 712);

    // SPACE
    place (IDs::delayOn, 190, 538, 130, 28);
    place (IDs::delayNote, 210, 600, 140, 36);
    place (IDs::delayMode, 380, 600, 160, 36);
    placeKnob (IDs::delayFeedback, 220, 738);
    placeKnob (IDs::delayMix, 400, 738);
    place (IDs::reverbOn, 660, 538, 130, 28);
    place (IDs::reverbType, 760, 600, 180, 36);
    placeKnob (IDs::reverbDecay, 680, 738);
    placeKnob (IDs::reverbPre, 840, 738);
    placeKnob (IDs::reverbMix, 1000, 738);

    // FX
    placeKnob (IDs::doubler, 250, 668);
    placeKnob (IDs::phone, 450, 668);
    placeKnob (IDs::width, 650, 668);
    placeKnob (IDs::output, 870, 668);

    if (activation) activation->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::Decor::paint (juce::Graphics& g)
{
    auto& e = owner;
    auto& p = e.palette;
    auto edge = (p.isLight ? juce::Colours::white : p.text).withAlpha (0.2f);

    auto main = juce::Rectangle<float> (44.0f, 44.0f, (float) baseWidth - 88.0f, (float) baseHeight - 88.0f);
    Glass::drawPanel (g, main, 18.0f, p, 0.35f);
    g.setColour (edge);
    g.drawRoundedRectangle (main.reduced (0.5f), 18.0f, 1.2f);
    g.drawHorizontalLine (128, main.getX() + 1.0f, main.getRight() - 1.0f);

    Logo::draw (g, { 400.0f, 62.0f, 400.0f, 54.0f }, Logo::specFor ("VOCALSUITE"), p, juce::Justification::centred);

    // assistant screen
    auto screen = juce::Rectangle<float> (108.0f, 143.0f, 984.0f, 277.0f);
    Glass::drawPanel (g, screen, 18.0f, p, 0.9f);
    g.setColour (edge);
    g.drawRoundedRectangle (screen.reduced (0.5f), 18.0f, 1.0f);

    auto bubble = juce::Rectangle<float> (423.0f, 178.0f, 637.0f, 114.0f);
    Glass::drawPanel (g, bubble, 18.0f, p, 1.6f);
    {
        juce::AttributedString as;
        as.setJustification (juce::Justification::centredLeft);
        as.setWordWrap (juce::AttributedString::byWord);
        auto font = Fonts::regular (bubbleTextSize (e.bubbleText));
        auto text = e.bubbleText;
        if (e.bubbleHighlight && text.containsChar (':') && text.indexOfChar (':') < 22)
        {
            auto head = text.upToFirstOccurrenceOf (":", true, false);
            as.append (head, font, p.isLight ? juce::Colour (0xff6a2bd9) : p.nebulaC.brighter (0.2f));
            as.append (text.fromFirstOccurrenceOf (":", false, false), font, p.isLight ? p.text : juce::Colours::white);
        }
        else
            as.append (text, font, p.isLight ? p.text : juce::Colours::white);
        as.draw (g, bubble.reduced (26.0f, 12.0f));
    }

    auto bar = juce::Rectangle<float> (423.0f, 322.0f, 637.0f, 62.0f);
    Glass::drawPanel (g, bar, 31.0f, p, 1.4f, e.chatInput.hasKeyboardFocus (true));
    // mic icon
    {
        auto c = juce::Point<float> (453.0f, 353.0f);
        juce::Path mic;
        mic.addRoundedRectangle (c.x - 5.0f, c.y - 12.0f, 10.0f, 16.0f, 5.0f);
        mic.addCentredArc (c.x, c.y - 2.0f, 9.0f, 9.0f, 0.0f, juce::MathConstants<float>::pi * 0.5f, juce::MathConstants<float>::pi * 1.5f, true);
        mic.startNewSubPath (c.x, c.y + 7.0f); mic.lineTo (c.x, c.y + 12.0f);
        g.setColour (p.textDim);
        g.strokePath (mic, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawVerticalLine (478, 336.0f, 370.0f);
    }

    // module panel
    auto panel = juce::Rectangle<float> (108.0f, 510.0f, 984.0f, 322.0f);
    Glass::drawPanel (g, panel, 18.0f, p, 0.9f);
    g.setColour (edge);
    g.drawRoundedRectangle (panel.reduced (0.5f), 18.0f, 1.0f);

    g.setColour (edge.withMultipliedAlpha (0.9f));
    auto label = [&] (const juce::String& t, float x, float y, float w)
    {
        g.setColour (p.textDim);
        lt::ui::Fonts::drawSpaced (g, t, { x, y, w, 20.0f }, Fonts::regular (15.0f), 0.08f, juce::Justification::centred);
    };
    switch (e.currentModule)
    {
        case tuneM:
            g.setColour (edge); g.drawVerticalLine (357, 540.0f, 800.0f); g.drawVerticalLine (662, 540.0f, 800.0f);
            label ("KEY", 140.0f, 610.0f, 190.0f);
            break;
        case cleanupM:
            label ("REMOVE RUMBLE, NOISE AND HARSH S", 140.0f, 530.0f, 920.0f);
            g.setColour (e.proc.chain.isGateOpen() ? juce::Colour (0xff6dffa8) : p.textDim.withAlpha (0.4f));
            g.fillEllipse (412.0f, 772.0f, 10.0f, 10.0f);
            g.setColour (p.textDim);
            g.setFont (Fonts::regular (12.5f));
            g.drawText ("GATE OPEN", juce::Rectangle<float> (430.0f, 768.0f, 120.0f, 18.0f), juce::Justification::centredLeft);
            break;
        case toneM:     label ("VOCAL EQ", 130.0f, 516.0f, 500.0f); break;
        case dynamicsM:
            g.setColour (edge); g.drawVerticalLine (640, 540.0f, 800.0f);
            label ("COMPRESSOR", 140.0f, 530.0f, 480.0f);
            label ("SATURATION (LT NOVA)", 660.0f, 530.0f, 400.0f);
            break;
        case spaceM:
            g.setColour (edge); g.drawVerticalLine (600, 540.0f, 800.0f);
            label ("ECHOES (LT ORBIT)", 280.0f, 530.0f, 220.0f);
            label ("REVERB (LT VOID)", 780.0f, 530.0f, 240.0f);
            break;
        case fxM:       label ("WIDTH, DOUBLES AND CREATIVE COLOUR", 140.0f, 540.0f, 920.0f); break;
        default: break;
    }
}

float Editor::Decor::bubbleTextSize (const juce::String& t)
{
    return t.length() < 70 ? 28.0f : t.length() < 120 ? 23.0f : t.length() < 200 ? 19.5f : 17.0f;
}

// ------------------------------------------------------------------ menus

void Editor::showAssistantMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    auto has = proc.assistant.hasApiKey();
    m.addSectionHeader (has ? juce::String ("AI: Claude connected") : utf8 ("AI: built-in assistant (no API key)"));
    m.addItem (1, has ? "Change Claude API key..." : "Connect Claude (API key)...");
    m.addItem (2, "Remove API key", has);
    m.addItem (3, "Forget the conversation");
    m.addSeparator();
    m.addItem (4, "Show what the assistant can do");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&mascot), [this] (int r)
    {
        if (r == 1) askForApiKey();
        else if (r == 2) { proc.assistant.setApiKey ({}); setBubble (utf8 ("Clave borrada. Uso el asistente integrado (sin conexión)."), false); }
        else if (r == 3) { proc.assistant.clearHistory(); setBubble (utf8 ("Listo: empezamos de cero."), true); }
        else if (r == 4)
            setBubble (utf8 ("Prueba: \"vocal chain estilo trap melódico\", \"más aire\", \"menos reverb\", \"autotune duro\", "
                             "\"voz de teléfono\", \"más ancha\". Con Claude conectado entiendo cualquier pedido, incluso estilos de artistas."), false);
    });
}

void Editor::askForApiKey()
{
    dialog = std::make_unique<juce::AlertWindow> ("Connect Claude",
        utf8 ("Pega tu clave de API de Claude (console.anthropic.com > API Keys).\n"
              "Se guarda solo en esta computadora. Cada pedido cuesta muy poco."),
        juce::MessageBoxIconType::NoIcon);
    dialog->setLookAndFeel (&lnf);
    dialog->addTextEditor ("key", {}, "API key");
    if (auto* ed = dialog->getTextEditor ("key")) ed->setPasswordCharacter ((juce::juce_wchar) 0x2022);
    dialog->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        auto key = dialog->getTextEditorContents ("key").trim();
        dialog.reset();
        if (result != 1 || key.isEmpty()) return;
        proc.assistant.setApiKey (key);
        setBubble (utf8 ("Listo: Claude conectado. Pídeme cualquier estilo."), true);
    }), false);
}

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
        if (r >= 1 && r <= (int) list.size()) { proc.applyPreset (list[(size_t) r - 1]); background.pulse (0.6f); }
        else if (r == 10001) savePreset();
        else if (r == 10002) proc.presets.remove (proc.presetName);
        else if (r == 10003) { proc.presets.userFolder().createDirectory(); proc.presets.userFolder().startAsProcess(); }
    });
}

void Editor::savePreset()
{
    auto ci = proc.presets.indexOf (proc.presetName);
    auto suggested = ci >= 0 && ! proc.presets.all()[(size_t) ci].factory ? proc.presetName : juce::String();
    dialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name for your preset:", juce::MessageBoxIconType::NoIcon);
    dialog->setLookAndFeel (&lnf);
    dialog->addTextEditor ("name", suggested);
    dialog->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        auto name = dialog->getTextEditorContents ("name").trim();
        dialog.reset();
        if (result != 1 || name.isEmpty()) return;
        if (proc.presets.save (lt::presets::ParamPreset::capture (proc.state, name, "MY PRESETS")))
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
        showModule (currentModule);
    }
}

} // namespace vocal
