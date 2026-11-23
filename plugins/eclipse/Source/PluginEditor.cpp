#include "PluginEditor.h"
#include "Decor.h"

namespace eclipse
{

namespace Colours = juce::Colours;
using lt::ui::Fonts;

bool Editor::activationShownThisSession = false;

// ------------------------------------------------------------------ content (chassis + top bar)
void Editor::Content::paint (juce::Graphics& g)
{
    auto& e = owner;
    const auto& s = e.proc.skin();
    g.fillAll (s.room);
    look::chassis (g, s, getLocalBounds().toFloat());
    decor::drawChassis (g, s, getLocalBounds());

    // logo
    {
        auto lt = juce::Rectangle<float> (28, 14, 64, 46);
        juce::ColourGradient cg (s.acc.interpolatedWith (Colours::white, 0.45f), 0, lt.getY(), s.acc.darker (0.3f), 0, lt.getBottom(), false);
        cg.addColour (0.55, s.acc);
        g.setColour (s.acc.withAlpha (0.25f));
        g.setFont (Fonts::bold (44));
        g.drawText ("LT", lt.translated (0, 1).toNearestInt(), juce::Justification::centredLeft, false);
        g.setGradientFill (cg);
        g.drawText ("LT", lt.toNearestInt(), juce::Justification::centredLeft, false);
        g.setColour (s.tx);
        look::spaced (g, "ECLIPSE", { 96, 14, 260, 46 }, Fonts::regular (30), 9.0f, juce::Justification::centredLeft);
        g.setColour (s.dim); g.fillRect (338.0f, 24.0f, 1.0f, 26.0f);
        look::label (g, s, "Drum", { 350, 22, 120, 14 }, juce::Justification::centredLeft, 8.5f, 3.0f);
        look::label (g, s, "Cloner", { 350, 37, 120, 14 }, juce::Justification::centredLeft, 8.5f, 3.0f);
        // V2 badge
        auto v2 = juce::Rectangle<float> (420, 25, 30, 22);
        g.setColour (s.acc); g.fillRoundedRectangle (v2, 5);
        g.setColour (s.acc.getPerceivedBrightness() > 0.75f ? juce::Colours::black : juce::Colours::white);
        look::spaced (g, "V3", v2, Fonts::bold (11), 1.0f, juce::Justification::centred);
    }
    // kit display
    look::lcd (g, s, e.kitBox, false);
    g.setColour (s.acc); g.setFont (Fonts::bold (9.5f));
    const auto name = e.proc.kitName;
    auto f = Fonts::bold (15.5f);
    const float tw = f.getStringWidthFloat (name) + 34;
    auto mid = e.kitBox.withSizeKeepingCentre (juce::jmin (tw, e.kitBox.getWidth() - 90), e.kitBox.getHeight());
    look::spaced (g, "KIT", mid.withWidth (30), Fonts::bold (9.5f), 2.0f, juce::Justification::centredLeft);
    g.setColour (Colours::white); g.setFont (f);
    g.drawText (name, mid.withTrimmedLeft (34).toNearestInt(), juce::Justification::centredLeft, true);
    look::lcdGlass (g, e.kitBox);

    look::label (g, s, "Out", { 1130, 24, 40, 26 }, juce::Justification::centredLeft, 10, 2.0f);
    look::meter (g, s, { 1296, 20, 20, 34 }, e.proc.getOutputLevel (0), e.proc.getOutputLevel (1));
}

// ------------------------------------------------------------------ editor
Editor::Editor (Processor& p)
    : AudioProcessorEditor (&p), proc (p), source (p), pads (p), padEditor (p), patternEditor (p), seq (p), skins (p), similarPanel (p),
      prevKit (p, juce::CharPointer_UTF8 ("\xe2\x80\xb9")), nextKit (p, juce::CharPointer_UTF8 ("\xe2\x80\xba")), saveKey (p, "SAVE"),
      skinsKey (p, "SKINS"), menuKey (p), outFader (p)
{
    setOpaque (true);
    lnf.setSkin (proc.skin());
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setBounds (0, 0, ui::W, ui::H);

    content.addAndMakeVisible (source);   source.setBounds (ui::sourceArea.toNearestInt());
    content.addAndMakeVisible (pads);     pads.setBounds (ui::padArea.toNearestInt());
    content.addChildComponent (patternEditor); patternEditor.setBounds (ui::padArea.toNearestInt());
    content.addAndMakeVisible (padEditor); padEditor.setBounds (ui::editArea.toNearestInt());
    content.addChildComponent (similarPanel); similarPanel.setBounds (ui::padArea.toNearestInt());
    padEditor.onFindSimilar = [this] { showSimilar (true); };
    similarPanel.onDone = [this] { showSimilar (false); };
    content.addAndMakeVisible (seq);      seq.setBounds (ui::seqArea.toNearestInt());

    for (auto* k : { &prevKit, &nextKit, &saveKey, &skinsKey, &menuKey }) content.addAndMakeVisible (*k);
    prevKit.fontSize = nextKit.fontSize = 15;
    prevKit.setBounds (518, 21, 32, 33);
    nextKit.setBounds (874, 21, 32, 33);
    saveKey.setBounds (930, 19, 76, 37);
    skinsKey.setBounds (1014, 19, 106, 37);
    menuKey.setBounds (1330, 19, 34, 37);
    skinsKey.icon = [this] (juce::Graphics& g, juce::Rectangle<float> r)
    {   // a little colour wheel + the word
        auto c = juce::Rectangle<float> (r.getX() + 16, r.getCentreY() - 7, 14, 14);
        const juce::Colour cols[] = { juce::Colour (0xff39ff14), juce::Colour (0xffffd400), juce::Colour (0xffff5fa2), juce::Colour (0xff2d6bff), juce::Colour (0xffff1e2d) };
        for (int i = 0; i < 5; ++i)
        {
            juce::Path p; p.addPieSegment (c, juce::MathConstants<float>::twoPi * (float) i / 5, juce::MathConstants<float>::twoPi * (float) (i + 1) / 5, 0);
            g.setColour (cols[i]); g.fillPath (p);
        }
        g.setColour (skinsKey.getToggleState() ? Colours::white : proc.skin().keyTx);
        look::spaced (g, "SKINS", r.withTrimmedLeft (38), Fonts::bold (11.5f), 1.0f, juce::Justification::centredLeft);
    };
    menuKey.icon = [] (juce::Graphics& g, juce::Rectangle<float> r)
    {
        for (int i = -1; i <= 1; ++i) g.fillEllipse (r.getCentreX() + (float) i * 6 - 1.6f, r.getCentreY() - 1.6f, 3.2f, 3.2f);
    };
    content.addAndMakeVisible (outFader);
    outFader.setBounds (1166, 22, 122, 30);
    outAttach = std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (IDs::output), outFader, nullptr);
    outFader.setDoubleClickReturnValue (true, 0.0);

    prevKit.onClick = [this] { stepKit (-1); };
    nextKit.onClick = [this] { stepKit (1); };
    saveKey.onClick = [this] { saveKit(); };
    skinsKey.onClick = [this] { showSkins (! skins.isVisible()); };
    menuKey.onClick = [this] { showMainMenu(); };

    content.addChildComponent (skins);
    skins.setBounds (content.getLocalBounds());
    skins.onPick = [this] (const juce::String& id) { applySkin (id); };
    skins.onClose = [this] { showSkins (false); };

    pads.onSelect = [this] { padEditor.padChanged(); pads.refresh(); if (patternEditor.isVisible()) patternEditor.repaint(); };
    seq.onEdit = [this] (bool on) { showPatternEditor (on); };
    seq.onPatternChanged = [this]
    {   // a pattern of kit B shows bank B
        const auto& k = proc.kit;
        if (! juce::isPositiveAndBelow (k.currentPattern, (int) k.patterns.size())) return;
        const int bank = k.patterns[(size_t) k.currentPattern].kit;
        if (bank != proc.selectedPad / padsPerBank)
        {
            proc.selectedPad = bank * padsPerBank + proc.selectedPad % padsPerBank;
            for (int i = 0; i < padsPerBank; ++i) if (! proc.kit.pad (bank * padsPerBank + i).empty()) { proc.selectedPad = bank * padsPerBank + i; break; }
            kitChanged();
        }
    };
    patternEditor.onDone = [this] { showPatternEditor (false); };
    patternEditor.onChange = [this] { seq.repaint(); };

    proc.onKitChanged = [sp = juce::Component::SafePointer<Editor> (this)] { if (sp) sp->kitChanged(); };
    proc.onStateRestored = [sp = juce::Component::SafePointer<Editor> (this)] { if (sp) { sp->applySkin (sp->proc.skinId); sp->applyScale(); } };
    applyScale();
    kitChanged();
    lastTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);

    if (! proc.license.isLicensed() && (! activationShownThisSession || ! proc.license.isUsable()))
    {
        activationShownThisSession = true;
        showActivation (true);
    }
}

Editor::~Editor()
{
    proc.onKitChanged = nullptr;
    proc.onStateRestored = nullptr;
    setLookAndFeel (nullptr);
}

void Editor::applyScale()
{
    const float s = proc.uiScale;
    content.setTransform (juce::AffineTransform::scale (s));
    setSize (juce::roundToInt (ui::W * s), juce::roundToInt (ui::H * s));
}

void Editor::applySkin (const juce::String& id)
{
    proc.setSkin (id);
    lnf.setSkin (proc.skin());
    padEditor.padChanged();
    patternEditor.refresh();
    content.repaint();
    for (auto* c : content.getChildren()) c->repaint();
    pads.refresh();
}

void Editor::showPatternEditor (bool on)
{
    patternEditor.setVisible (on);
    pads.setVisible (! on);
    seq.editKey.setToggleState (on, juce::dontSendNotification);
    if (on) patternEditor.refresh();
}

void Editor::showSimilar (bool on)
{
    if (on) { showPatternEditor (false); similarPanel.search(); }
    similarPanel.setVisible (on);
    pads.setVisible (! on && ! patternEditor.isVisible());
    if (! on) { kitChanged(); }
}

void Editor::showSkins (bool on)
{
    skins.setVisible (on);
    skinsKey.setToggleState (on, juce::dontSendNotification);
    if (on) skins.toFront (false);
}

void Editor::kitChanged()
{
    source.kitChanged();
    pads.bank = proc.selectedPad / padsPerBank;
    pads.refresh();
    padEditor.padChanged();
    seq.refresh();
    if (patternEditor.isVisible()) patternEditor.refresh();
    content.repaint (kitBox.toNearestInt());
}

void Editor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.05, (now - lastTime) * 0.001);
    lastTime = now;
    pads.tick (dt);
    padEditor.tick();
    content.repaint (juce::Rectangle<int> (1290, 14, 32, 46));
    if (proc.isCloning() || wasCloning) source.repaint();
    wasCloning = proc.isCloning();
    if (proc.isPreviewPlaying())
    {
        seq.repaint();
        if (patternEditor.isVisible()) patternEditor.repaint();
    }
}

// ------------------------------------------------------------------ kits
void Editor::stepKit (int d)
{
    auto files = proc.kitsFolder().findChildFiles (juce::File::findFiles, false, "*.ltkit");
    if (files.isEmpty()) return;
    files.sort();
    int i = 0;
    for (int k = 0; k < files.size(); ++k) if (files[k].getFileNameWithoutExtension() == proc.kitName) i = k + d;
    i = (i % files.size() + files.size()) % files.size();
    proc.loadKit (files[i]);
}

void Editor::showKitMenu()
{
    juce::PopupMenu m;
    auto files = proc.kitsFolder().findChildFiles (juce::File::findFiles, false, "*.ltkit");
    files.sort();
    for (int i = 0; i < files.size(); ++i) m.addItem (i + 1, files[i].getFileNameWithoutExtension(), true, files[i].getFileNameWithoutExtension() == proc.kitName);
    if (files.isEmpty()) m.addItem (-1, "No saved kits yet", false, false);
    m.addSeparator();
    m.addItem (1000, "Save kit...");
    m.addItem (1001, "Show kits folder");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (content.localAreaToGlobal (kitBox.toNearestInt())), [this, files] (int r)
    {
        if (r > 0 && r <= files.size()) proc.loadKit (files[r - 1]);
        else if (r == 1000) saveKit();
        else if (r == 1001) proc.kitsFolder().revealToUser();
    });
}

void Editor::saveKit()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save kit", "Name of the kit", juce::MessageBoxIconType::NoIcon);
    saveDialog->addTextEditor ("name", proc.kitName);
    saveDialog->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([sp = juce::Component::SafePointer<Editor> (this)] (int r)
    {
        if (sp == nullptr || sp->saveDialog == nullptr) return;
        const auto name = sp->saveDialog->getTextEditorContents ("name").trim();
        if (r == 1 && name.isNotEmpty()) { sp->proc.saveKit (name); sp->content.repaint(); }
        sp->saveDialog.reset();
    }), false);
}

void Editor::showMainMenu()
{
    juce::PopupMenu m, size;
    const float scales[] = { 0.64f, 0.8f, 1.0f };
    const char* names[] = { "80%", "100%", "125%" };
    for (int i = 0; i < 3; ++i) size.addItem (10 + i, names[i], true, std::abs (proc.uiScale - scales[i]) < 0.01f);
    m.addSubMenu ("Size", size);
    m.addItem (1, "Skins...");
    m.addItem (2, "Kits folder");
    m.addItem (4, "Split a song's kits into banks", true, proc.splitKits);
    m.addSeparator();
    m.addItem (3, proc.license.isLicensed() ? "License" : "Activate...");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuKey), [this] (int r)
    {
        if (r >= 10 && r < 13) { const float sc[] = { 0.64f, 0.8f, 1.0f }; proc.uiScale = sc[r - 10]; applyScale(); }
        else if (r == 1) showSkins (true);
        else if (r == 2) proc.kitsFolder().revealToUser();
        else if (r == 3) showActivation (true);
        else if (r == 4) proc.splitKits = ! proc.splitKits;
    });
}

void Editor::showActivation (bool show)
{
    if (show && activation == nullptr)
    {
        proc.license.refresh();
        activation = std::make_unique<lt::ui::ActivationComponent> (proc.license, lt::ui::Palette::dark());
        activation->onDismiss = [sp = juce::Component::SafePointer<Editor> (this)] { juce::MessageManager::callAsync ([sp] { if (sp) sp->showActivation (false); }); };
        content.addAndMakeVisible (*activation);
        activation->setBounds (content.getLocalBounds());
        activation->toFront (true);
    }
    else if (! show && activation != nullptr)
    {
        activation.reset();
        proc.refreshLicense();
    }
}

} // namespace eclipse
