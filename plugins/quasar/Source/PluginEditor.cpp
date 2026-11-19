#include "PluginEditor.h"

namespace quasar
{

using namespace ui;

bool Editor::activationShownThisSession = false;

void Editor::Header::paint (juce::Graphics& g)
{
    // header boxes (the pages draw their own panels)
    drawPanel (g, { 248, 22, 270, 50 }, 8.0f);
    drawPanel (g, { 532, 22, 272, 50 }, 8.0f);
    drawPanel (g, { 814, 22, 104, 50 }, 8.0f);
    lt::ui::Logo::draw (g, { 18, 26, 214, 40 }, lt::ui::Logo::specFor ("QUASAR"), lt::ui::Palette::dark(), juce::Justification::centredLeft);
    g.setColour (Colours::dim); g.setFont (Fonts::regular (10.5f));
    g.drawText ("L", 1138, 76, 12, 12, juce::Justification::centred);
    g.drawText ("R", 1160, 76, 12, 12, juce::Justification::centred);
    // save / heart glyphs
    auto save = juce::Rectangle<float> (543, 37, 18, 18);
    g.setColour (Colours::text);
    g.drawRoundedRectangle (save, 2.5f, 1.3f);
    g.fillRect (save.getX() + 4, save.getY() + 2, 10.0f, 5.0f);
    juce::Path heart;
    auto hx = 582.0f, hy = 46.0f;
    heart.startNewSubPath (hx, hy + 7);
    heart.cubicTo (hx - 11, hy - 1, hx - 5, hy - 10, hx, hy - 4);
    heart.cubicTo (hx + 5, hy - 10, hx + 11, hy - 1, hx, hy + 7);
    auto* cur = owner.proc.presets.find (owner.proc.presetBank, owner.proc.presetName);
    if (cur != nullptr && owner.proc.presets.isFavourite (*cur)) { g.setColour (juce::Colour (0xffb98cff)); g.fillPath (heart); }
    else g.strokePath (heart, juce::PathStrokeType (1.3f));
}

Editor::Editor (Processor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (background);
    background.setPalette (lt::ui::Palette::dark());
    content.addAndMakeVisible (header);
    header.setBounds (0, 0, baseWidth, 96);

    // pages first, then the header controls (controls before attachments)
    for (juce::Component* c : { (juce::Component*) &oscPage, (juce::Component*) &bottom, (juce::Component*) &voicing, (juce::Component*) &mixPage,
                                (juce::Component*) &fxPage, (juce::Component*) &matrixPage, (juce::Component*) &globalPage, (juce::Component*) &browser,
                                (juce::Component*) &simplePage, (juce::Component*) &arp })
    {
        content.addChildComponent (c);
        c->setBounds (0, 0, baseWidth, baseHeight);
    }
    arp.setVisible (true);
    content.addAndMakeVisible (keyboard);
    keyboard.setBounds (242, 747, 940, 135);
    mixPage.navigate = [this] (const juce::String& pg, int bus) { showPage (pg); if (pg == "FX") fxPage.setBus (bus); };
    simplePage.stepPreset = [this] (int d) { browser.step (d); };

    const char* names[] = { "OSC", "MIX", "FX", "MATRIX", "GLOBAL" };
    for (int i = 0; i < 5; ++i)
    {
        auto* b = tabs.add (new juce::TextButton (names[i]));
        b->getProperties().set ("textOnly", true);
        b->getProperties().set ("fontSize", 14.0);
        b->setBounds (254 + i * 52, 28, i == 3 ? 60 : 50, 38);
        if (i == 4) b->setBounds (254 + 3 * 52 + 62, 28, 58, 38);
        b->onClick = [this, n = juce::String (names[i])] { showPage (n); };
        content.addAndMakeVisible (b);
    }
    saveButton.getProperties().set ("textOnly", true);
    favButton.getProperties().set ("textOnly", true);
    saveButton.setBounds (536, 30, 32, 32); favButton.setBounds (568, 30, 30, 32);
    saveButton.setTooltip ("Save preset");
    saveButton.onClick = [this] { browser.savePresetDialog(); };
    favButton.onClick = [this] { if (auto* c = proc.presets.find (proc.presetBank, proc.presetName)) { proc.presets.toggleFavourite (*c); browser.refresh(); header.repaint(); } };
    presetButton.getProperties().set ("textOnly", true);
    presetButton.getProperties().set ("fontSize", 16.0);
    presetButton.setBounds (604, 28, 130, 36);
    presetButton.onClick = [this] { showPage (page == "BROWSER" ? pageBeforeBrowser : "BROWSER"); };
    for (auto* b : { &prevButton, &nextButton }) { b->getProperties().set ("textOnly", true); b->getProperties().set ("fontSize", 18.0); }
    prevButton.setBounds (738, 30, 30, 32); nextButton.setBounds (768, 30, 30, 32);
    prevButton.onClick = [this] { browser.step (-1); };
    nextButton.onClick = [this] { browser.step (1); };
    for (auto* b : { &undoButton, &redoButton }) b->getProperties().set ("fontSize", 11.5);
    undoButton.setBounds (820, 35, 46, 24); redoButton.setBounds (868, 35, 46, 24);
    undoButton.onClick = [this] { proc.undo.undo(); };
    redoButton.onClick = [this] { proc.undo.redo(); };
    browserButton.getProperties().set ("fontSize", 12.0);
    browserButton.setBounds (934, 24, 116, 26);
    browserButton.onClick = [this] { showPage (page == "BROWSER" ? pageBeforeBrowser : "BROWSER"); };
    viewTabs.separators = true; viewTabs.fontSize = 11.5f;
    viewTabs.setBounds (928, 54, 128, 18);
    viewTabs.onChange = [this] (int i) { setSimple (i == 0); };
    licenseButton.getProperties().set ("fontSize", 10.5);
    licenseButton.setBounds (24, 74, 150, 18);
    licenseButton.onClick = [this] { showActivation (true); };
    for (juce::Component* c : { (juce::Component*) &saveButton, (juce::Component*) &favButton, (juce::Component*) &presetButton, (juce::Component*) &prevButton,
                                (juce::Component*) &nextButton, (juce::Component*) &undoButton, (juce::Component*) &redoButton, (juce::Component*) &browserButton,
                                (juce::Component*) &viewTabs, (juce::Component*) &licenseButton })
        content.addAndMakeVisible (c);

    masterKnob = std::make_unique<Knob> (proc, IDs::master, "MASTER", 32.0f);
    masterKnob->labelSize = 11.0f;
    masterKnob->setBounds (1056, 22, 64, 60);
    content.addAndMakeVisible (*masterKnob);
    outMeter = std::make_unique<Meter> ([this] (int c) { return proc.getOutputLevel (c); }, 2, true);
    outMeter->setBounds (1136, 14, 38, 60);
    content.addAndMakeVisible (*outMeter);
    masterKnob->bind (bindings);   // attachments after the controls

    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->browser.refresh(); safe->content.repaint(); } });
    };

    setResizable (true, true);
    setResizeLimits (baseWidth * 5 / 10, baseHeight * 5 / 10, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (baseWidth * 9 / 10, baseHeight * 9 / 10);

    showPage (proc.lastPage.isNotEmpty() ? proc.lastPage : juce::String ("OSC"));
    setSimple (proc.simpleView);

    if (! proc.license.isLicensed() && (! activationShownThisSession || ! proc.license.isUsable()))
    {
        activationShownThisSession = true;
        showActivation (true);
    }
    startTimerHz (15);

    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty()) runSnapshot (snapshot);
}

Editor::~Editor()
{
    stopTimer();
    bindings.clear();            // attachments first
    proc.onStateRestored = nullptr;
    masterKnob.reset();
    activation.reset();
    setLookAndFeel (nullptr);
}

void Editor::showPage (const juce::String& p)
{
    if (p == "BROWSER" && page != "BROWSER") pageBeforeBrowser = page;
    page = p;
    if (p != "BROWSER") proc.lastPage = p;
    auto simple = proc.simpleView && p != "BROWSER";
    oscPage.setVisible (! simple && p == "OSC");
    bottom.setVisible (! simple && (p == "OSC" || p == "GLOBAL"));
    voicing.setVisible (! simple && (p == "OSC" || p == "GLOBAL" || p == "MATRIX"));
    mixPage.setVisible (! simple && p == "MIX");
    fxPage.setVisible (! simple && p == "FX");
    matrixPage.setVisible (! simple && p == "MATRIX");
    globalPage.setVisible (! simple && p == "GLOBAL");
    browser.setVisible (p == "BROWSER");
    simplePage.setVisible (simple);
    if (p == "BROWSER") browser.refresh();
    for (auto* t : tabs) t->setToggleState (! simple && t->getButtonText() == p, juce::dontSendNotification);
    browserButton.setToggleState (p == "BROWSER", juce::dontSendNotification);
    if (activation) activation->toFront (false);
}

void Editor::setSimple (bool s)
{
    proc.simpleView = s;
    viewTabs.setSelected (s ? 0 : 1);
    showPage (page == "BROWSER" ? juce::String ("BROWSER") : page);
}

void Editor::timerCallback()
{
    auto name = proc.presetName + (proc.isPresetModified() ? " *" : "");
    if (presetButton.getButtonText() != name) { presetButton.setButtonText (name); header.repaint(); }
    undoButton.setEnabled (proc.undo.canUndo());
    redoButton.setEnabled (proc.undo.canRedo());
    auto chip = proc.license.isLicensed() ? juce::String() : proc.license.isUsable()
                  ? juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (proc.license.getTrialDaysLeft()) + " DAYS"
                  : juce::String ("ACTIVATE");
    if (licenseButton.getButtonText() != chip) licenseButton.setButtonText (chip);
    licenseButton.setVisible (chip.isNotEmpty());
    background.setEnergy (juce::jlimit (0.0f, 1.0f, proc.getOutputLevel (0) * 1.5f));
}

void Editor::resized()
{
    auto scale = (float) getWidth() / (float) baseWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, baseWidth, baseHeight);
    background.setBounds (0, 0, baseWidth, baseHeight);
    if (activation) activation->setBounds (0, 0, baseWidth, baseHeight);
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

void Editor::runSnapshot (const juce::String& file)
{
    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png [LT_PAGE=OSC|MIX|FX|MATRIX|GLOBAL|BROWSER|SIMPLE] [LT_PRESET=name]
    activation.reset();
    auto pg = juce::SystemStats::getEnvironmentVariable ("LT_PAGE", "OSC").toUpperCase();
    auto presetName = juce::SystemStats::getEnvironmentVariable ("LT_PRESET", {});
    if (presetName.isNotEmpty()) if (auto* pr = proc.presets.find ({}, presetName)) proc.applyPreset (*pr);
    if (pg == "SIMPLE") { setSimple (true); }
    else { setSimple (false); showPage (pg); }
    setSize (baseWidth, baseHeight);
    juce::Timer::callAfterDelay (300, [this]
    {
        // play a chord through the processor so meters, envelopes and rings show real values
        proc.setRateAndBufferSizeDetails (48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        proc.serviceNow();
        juce::AudioBuffer<float> b (2, 512);
        for (int blk = 0; blk < 60; ++blk)
        {
            juce::MidiBuffer m;
            if (blk == 0) for (int n : { 48, 55, 60, 63 }) m.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
            proc.processBlock (b, m);
        }
    });
    juce::Timer::callAfterDelay (2500, [this, file]
    {
        auto img = content.createComponentSnapshot (content.getLocalBounds(), true, 1.0f);
        juce::File (file).deleteFile();
        juce::FileOutputStream out { juce::File (file) };
        juce::PNGImageFormat().writeImageToStream (img, out);
        out.flush();
        juce::JUCEApplicationBase::quit();
    });
}

} // namespace quasar
