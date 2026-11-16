#include "PluginEditor.h"

namespace lt_void
{

using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Logo;

bool Editor::activationShownThisSession = false;

Palette Editor::paletteFor (int type)
{
    auto c = TypeInfo::colours (type);
    if (c.light)
    {
        auto p = Palette::light();
        p.nebulaA = c.accent;
        p.nebulaB = juce::Colour (0xffb3ecff);
        p.nebulaC = juce::Colour (0xfffff3b0);
        p.accent = c.accent;
        p.accentText = juce::Colour (0xff111111);
        return p;
    }
    auto p = Palette::tinted (c.accent, c.nebulaA, c.nebulaB, c.background);
    p.nebulaC = c.accent.brighter (0.7f);
    return p;
}

Editor::Editor (Processor& pr)
    : AudioProcessorEditor (pr), proc (pr)
{
    auto type = juce::roundToInt (proc.state.getRawParameterValue (IDs::type)->load());
    palette = fromPalette = toPalette = paletteFor (type);
    shownType = type;
    lnf.setPalette (palette);
    setLookAndFeel (&lnf);

    addAndMakeVisible (content);
    backA.setPalette (palette);
    backB.setPalette (palette);
    content.addAndMakeVisible (backA);
    content.addChildComponent (backB);
    content.addAndMakeVisible (decor);

    // ---- knobs
    hole.palette = &palette;
    attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (IDs::decay), hole));
    content.addAndMakeVisible (hole);

    std::pair<VoidKnob*, const char*> knobs[] = {
        { &mix, IDs::mix }, { &size, IDs::size }, { &highFreq, IDs::highFreq }, { &lowFreq, IDs::lowFreq },
        { &attack, IDs::attack }, { &diffusion, IDs::diffusion }, { &modRate, IDs::modRate }, { &modDepth, IDs::modDepth },
        { &lowCut, IDs::lowCut }, { &highCut, IDs::highCut }, { &width, IDs::width }, { &duck, IDs::duck } };
    for (auto& [k, id] : knobs)
    {
        k->palette = &palette;
        attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (id), *k));
        content.addAndMakeVisible (k);
    }
    preDelay.palette = &palette;
    content.addAndMakeVisible (preDelay);
    attachPreDelay();

    syncAttachment = std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (IDs::preSync), syncButton);
    syncButton.setClickingTogglesState (true);
    syncButton.onClick = [this] { attachPreDelay(); };
    content.addAndMakeVisible (syncButton);

    freeze.palette = &palette;
    freezeAttachment = std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (IDs::freeze), freeze);
    content.addAndMakeVisible (freeze);

    inMeter.palette = &palette;
    outMeter.palette = &palette;
    content.addAndMakeVisible (inMeter);
    content.addAndMakeVisible (outMeter);

    // ---- type selector
    typeAttachment = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::type), [this] (float v)
    {
        auto t = juce::roundToInt (v);
        typeName.setButtonText (TypeInfo::names()[t]);
        if (t != shownType) startTransition (t);
    });
    typePrev.onClick = [this] { setType ((shownType + TypeInfo::count - 1) % TypeInfo::count); };
    typeNext.onClick = [this] { setType ((shownType + 1) % TypeInfo::count); };
    typeName.onClick = [this] { showTypeMenu(); };
    typeMenu.onClick = [this] { showTypeMenu(); };
    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { savePreset(); };
    licenseButton.onClick = [this] { showActivation (true); };
    for (auto* b : { &typePrev, &typeNext, &typeName, &typeMenu, &presetButton, &saveButton, &licenseButton })
        content.addAndMakeVisible (b);
    typeAttachment->sendInitialUpdate();

    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->attachPreDelay(); safe->repaintAll(); } });
    };

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

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png [LT_TYPE=n]
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
        auto t = juce::SystemStats::getEnvironmentVariable ("LT_TYPE", {});
        if (t.isNotEmpty())
        {
            auto& all = proc.presets.all();
            for (auto& preset : all)
                if (preset.group == TypeInfo::names()[t.getIntValue()]) { proc.applyPreset (preset); break; }
        }
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

void Editor::attachPreDelay()
{
    auto sync = proc.state.getRawParameterValue (IDs::preSync)->load() > 0.5f;
    if (preDelayAttachment != nullptr && sync == syncShown) return;
    syncShown = sync;
    preDelayAttachment.reset();
    preDelay.formatValue = nullptr;
    preDelayAttachment = std::make_unique<juce::SliderParameterAttachment> (*proc.state.getParameter (sync ? IDs::preNote : IDs::preDelay), preDelay);
    preDelay.repaint();
}

void Editor::setType (int t)
{
    typeAttachment->setValueAsCompleteGesture ((float) t);
}

void Editor::startTransition (int t)
{
    shownType = t;
    fromPalette = palette;
    toPalette = paletteFor (t);
    transition = 0.0f;
    back->setPalette (toPalette);
    back->setAlpha (0.0f);
    back->setVisible (true);
    back->toBehind (&decor);
    front->toBehind (back);
}

void Editor::repaintAll()
{
    lnf.setPalette (palette);
    if (activation) activation->setPalette (palette);
    content.repaint();
}

// ------------------------------------------------------------------ timer

void Editor::timerCallback()
{
    // colour transition between reverb types
    if (transition < 1.0f)
    {
        transition = juce::jmin (1.0f, transition + 1.0f / 18.0f);
        auto t = transition * transition * (3.0f - 2.0f * transition);
        palette = Palette::blend (fromPalette, toPalette, t);
        back->setAlpha (t);
        if (transition >= 1.0f)
        {
            palette = toPalette;
            front->setVisible (false);
            std::swap (front, back);
        }
        repaintAll();
    }

    auto& e = proc.engine;
    auto level = juce::jlimit (0.0f, 1.0f, e.getWetLevel() * 2.0f);
    HoleState hs;
    hs.size = proc.state.getRawParameterValue (IDs::size)->load() * 0.01f;
    hs.decay = proc.state.getRawParameterValue (IDs::decay)->load();
    hs.modRate = proc.state.getRawParameterValue (IDs::modRate)->load();
    hs.modDepth = proc.state.getRawParameterValue (IDs::modDepth)->load() * 0.01f;
    hs.brightness = std::log (proc.state.getRawParameterValue (IDs::highFreq)->load() / 1000.0f) / std::log (20.0f);
    hs.frozen = proc.state.getRawParameterValue (IDs::freeze)->load() > 0.5f;
    hs.level = level;
    hs.hits = e.getHitCount();
    hole.tick (hs);

    front->setEnergy (level);
    front->setFrozen (false);
    front->setDriftSpeed (0.6f + hs.modRate * 0.5f);
    inMeter.setLevel (e.getInputLevel());
    outMeter.setLevel (e.getOutputLevel());

    attachPreDelay();
    if (syncShown)
        preDelay.formatValue = [] (double v) { return Notes::names()[juce::roundToInt (v)]; };

    auto name = proc.presetName + (proc.isPresetModified() ? " *" : "");
    if (presetButton.getButtonText() != name) presetButton.setButtonText (name);

    auto chip = proc.license.isLicensed() ? juce::String() : proc.license.isUsable()
                  ? juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (proc.license.getTrialDaysLeft()) + " DAYS"
                  : juce::String ("ACTIVATE");
    if (licenseButton.getButtonText() != chip) licenseButton.setButtonText (chip);
    licenseButton.setVisible (chip.isNotEmpty());

    decor.repaint (0, 480, baseWidth, 135);
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
    for (auto* b : { &backA, &backB }) b->setBounds (0, 0, baseWidth, baseHeight);
    decor.setBounds (0, 0, baseWidth, baseHeight);

    licenseButton.setBounds (46, 96, 140, 26);

    typeBox = { 444, 50, 214, 36 };
    typePrev.setBounds (typeBox.getX() + 2, typeBox.getY() + 2, 32, 32);
    typeNext.setBounds (typeBox.getRight() - 64, typeBox.getY() + 2, 30, 32);
    typeMenu.setBounds (typeBox.getRight() - 34, typeBox.getY() + 2, 32, 32);
    typeName.setBounds (typeBox.getX() + 36, typeBox.getY() + 2, typeBox.getWidth() - 102, 32);

    presetBox = { 694, 50, 284, 36 };
    saveButton.setBounds (presetBox.getRight() - 66, presetBox.getY() + 4, 62, 28);
    presetButton.setBounds (presetBox.getX() + 2, presetBox.getY() + 2, presetBox.getWidth() - 72, 32);

    inMeter.setBounds (996, 54, 18, 400);
    outMeter.setBounds (1050, 54, 18, 400);

    auto bigKnob = [] (VoidKnob& k, int cx, int cy) { k.setBounds (cx - 75, cy - 70, 150, 154); };
    bigKnob (mix, 166, 177);
    bigKnob (preDelay, 166, 325);
    bigKnob (size, 166, 472);
    syncButton.setBounds (226, 370, 50, 22);

    hole.setBounds (550 - 300, 262 - 262, 600, 524);   // big, so the glow is never clipped (only the ring is clickable)

    auto small = [] (VoidKnob& k, int cx, int cy) { k.setBounds (cx - 34, cy - 24, 68, 66); };
    small (highFreq, 859, 168); small (lowFreq, 943, 168);
    small (attack, 859, 260);   small (diffusion, 943, 260);
    small (modRate, 859, 352);  small (modDepth, 943, 352);
    small (lowCut, 859, 443);   small (highCut, 943, 443);
    small (width, 859, 534);    small (duck, 943, 534);
    freeze.setBounds (998, 508, 74, 40);

    if (activation) activation->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::Decor::paint (juce::Graphics& g)
{
    auto& e = owner;
    auto& p = e.palette;
    auto bounds = juce::Rectangle<float> (0, 0, (float) baseWidth, (float) baseHeight);

    Glass::drawPanel (g, bounds.reduced (12.0f), 14.0f, p, 0.25f);
    Logo::draw (g, { 44.0f, 40.0f, 260.0f, 52.0f }, Logo::specFor ("VOID"), p, juce::Justification::centredLeft);

    auto label = [&] (const juce::String& t, juce::Rectangle<float> r, float size = 13.0f)
    {
        g.setColour (p.isLight ? p.text : p.textDim);
        Fonts::drawSpaced (g, t, r, Fonts::regular (size), 0.06f, juce::Justification::centred);
    };
    label ("REVERB TYPE", { (float) e.typeBox.getX(), 28.0f, (float) e.typeBox.getWidth(), 18.0f });
    label ("PRESETS", { (float) e.presetBox.getX(), 28.0f, (float) e.presetBox.getWidth(), 18.0f });
    Glass::drawPanel (g, e.typeBox.toFloat(), 18.0f, p, 1.2f);
    Glass::drawPanel (g, e.presetBox.toFloat(), 18.0f, p, 1.2f);

    label ("INPUT", { 980.0f, 30.0f, 50.0f, 16.0f }, 11.0f);
    label ("OUTPUT", { 1034.0f, 30.0f, 50.0f, 16.0f }, 11.0f);
    label ("INPUT", { 980.0f, 462.0f, 50.0f, 16.0f }, 10.5f);
    label ("OUTPUT", { 1034.0f, 462.0f, 50.0f, 16.0f }, 10.5f);

    // right column group headers with thin lines
    auto group = [&] (const juce::String& t, float y)
    {
        auto r = juce::Rectangle<float> (828.0f, y, 150.0f, 16.0f);
        g.setColour (p.glassEdge);
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getCentreX() - 34.0f);
        g.drawHorizontalLine ((int) r.getCentreY(), r.getCentreX() + 34.0f, r.getRight());
        label (t, r, 12.5f);
    };
    group ("DAMPING", 124.0f);
    group ("SHAPE", 216.0f);
    group ("MOD", 308.0f);
    group ("EQ", 399.0f);
    g.setColour (p.glassEdge);
    g.drawHorizontalLine (492, 828.0f, 1080.0f);

    // DECAY value
    auto decay = e.proc.state.getRawParameterValue (IDs::decay)->load();
    auto frozen = e.proc.state.getRawParameterValue (IDs::freeze)->load() > 0.5f;
    g.setColour (p.value);
    g.setFont (Fonts::light (42.0f));
    g.drawText (frozen ? juce::String ("FROZEN") : juce::String (decay, decay < 10.0f ? 1 : 0) + " s",
                juce::Rectangle<float> (400.0f, 470.0f, 300.0f, 50.0f), juce::Justification::centred);
    g.setColour (p.text);
    Fonts::drawSpaced (g, "DECAY", { 450.0f, 520.0f, 200.0f, 22.0f }, Fonts::regular (17.0f), 0.08f, juce::Justification::centred);

    g.setColour (p.text);
    g.setFont (Fonts::regular (12.5f));
    g.drawText ("FREEZE", juce::Rectangle<float> (998.0f, 552.0f, 74.0f, 16.0f), juce::Justification::centred);

    // description of the type
    auto t = juce::roundToInt (e.proc.state.getRawParameterValue (IDs::type)->load());
    g.setColour (p.isLight ? p.text.withAlpha (0.75f) : p.textDim);
    g.setFont (Fonts::regular (13.5f));
    g.drawText (TypeInfo::names()[t] + juce::String (juce::CharPointer_UTF8 (" \xe2\x80\x94 ")) + TypeInfo::description (t),
                juce::Rectangle<float> (150.0f, 582.0f, 800.0f, 20.0f), juce::Justification::centred);
}

// ------------------------------------------------------------------ menus

void Editor::showTypeMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    for (int i = 0; i < TypeInfo::count; ++i)
        m.addItem (i + 1, TypeInfo::names()[i], true, i == shownType);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&typeName), [this] (int r) { if (r > 0) setType (r - 1); });
}

void Editor::showPresetMenu()
{
    proc.presets.rescan();
    auto& all = proc.presets.all();
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);

    // presets of the current type first, the rest in sub-menus
    auto current = TypeInfo::names()[shownType];
    m.addSectionHeader (current);
    for (int i = 0; i < (int) all.size(); ++i)
        if (all[(size_t) i].factory && all[(size_t) i].group == current)
            m.addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.presetName);

    m.addSeparator();
    for (int t = 0; t < TypeInfo::count; ++t)
    {
        if (t == shownType) continue;
        juce::PopupMenu sub;
        for (int i = 0; i < (int) all.size(); ++i)
            if (all[(size_t) i].factory && all[(size_t) i].group == TypeInfo::names()[t])
                sub.addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.presetName);
        m.addSubMenu (TypeInfo::names()[t], sub);
    }

    juce::PopupMenu mine;
    for (int i = 0; i < (int) all.size(); ++i)
        if (! all[(size_t) i].factory)
            mine.addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.presetName);
    m.addSeparator();
    if (mine.getNumItems() > 0) m.addSubMenu ("MY PRESETS", mine);
    m.addItem (10001, "Save as...");
    auto ci = proc.presets.indexOf (proc.presetName);
    m.addItem (10002, "Delete \"" + proc.presetName + "\"", ci >= 0 && ! all[(size_t) ci].factory);
    m.addItem (10003, "Open presets folder");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton), [this] (int r)
    {
        auto& list = proc.presets.all();
        if (r >= 1 && r <= (int) list.size()) { proc.applyPreset (list[(size_t) r - 1]); attachPreDelay(); }
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
        auto preset = lt::presets::ParamPreset::capture (proc.state, name, TypeInfo::names()[shownType]);
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

} // namespace lt_void
