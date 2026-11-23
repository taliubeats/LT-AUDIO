#include "PluginEditor.h"

namespace corona
{

bool Editor::activationShownThisSession = false;

namespace
{
const juce::Colour barBg (0xff121114), barEdge (0xff3b383f), barText (0xffc4bfca), purple (0xff5b2bb0), purpleEdge (0xff7d45e6);
juce::Font barFont (float h) { return lt::ui::Fonts::regular (h); }
}

// ------------------------------------------------------------------ look
void Editor::BarLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const bool plain = b.getProperties()["plain"];
    if (plain && ! on) { if (over) { g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (r, 3); } return; }
    g.setColour (on ? purple : juce::Colour (0xff1e1d21).brighter (over ? 0.15f : 0.0f).darker (down ? 0.2f : 0.0f));
    g.fillRoundedRectangle (r, plain ? 3.0f : 4.0f);
    if (! plain) { g.setColour (on ? purpleEdge : barEdge); g.drawRoundedRectangle (r, 4, 1); }
}

void Editor::BarLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (barFont ((float) b.getProperties().getWithDefault ("fontSize", 15.0)));
    g.setColour (b.getToggleState() ? juce::Colours::white : barText);
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

juce::Font Editor::BarLook::getPopupMenuFont() { return barFont (16); }

// ------------------------------------------------------------------ content
void Editor::Content::paint (juce::Graphics& g)
{
    const auto& art = ui::Art::get();
    g.drawImageAt (art.face, 0, ui::FACE_Y);
    // top bar
    g.setColour (barBg); g.fillRect (0, 0, ui::W, 53);
    g.setColour (juce::Colours::black); g.drawHorizontalLine (53, 0, (float) ui::W);
    g.setFont (lt::ui::Fonts::semiBold (31)); g.setColour (juce::Colour (0xff9a5cff)); g.drawText ("LT", 16, 6, 60, 40, juce::Justification::centredLeft);
    g.setFont (lt::ui::Fonts::light (31)); g.setColour (barText); g.drawText ("CORONA", 60, 6, 200, 40, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff1a191c)); g.fillRoundedRectangle (452, 8, 326, 37, 5);
    g.setColour (barEdge); g.drawRoundedRectangle (452.5f, 8.5f, 325, 36, 5, 1);
    for (auto* seg : { &owner.modeSeg, &owner.ovsSeg, &owner.sizeSeg })
    {
        g.setFont (barFont (14)); g.setColour (barText);
        int x0 = seg->buttons.front()->getX(), x1 = seg->buttons.back()->getRight();
        g.drawText (seg->caption, x0, 3, x1 - x0, 18, juce::Justification::centred);
    }
    g.setColour (juce::Colour (0xff2b292e)); g.fillRect (1108, 8, 1, 38); g.fillRect (1218, 8, 1, 38);
}

// ------------------------------------------------------------------ editor
Editor::Editor (Processor& p) : AudioProcessorEditor (&p), proc (p)
{
    setOpaque (true);
    addAndMakeVisible (content);
    content.setBounds (0, 0, ui::W, ui::H);
    content.addAndMakeVisible (chassis);

    // knobs
    for (auto& s : ui::knobSpecs())
    {
        auto k = std::make_unique<ui::Knob> (s);
        auto* param = proc.state.getParameter (s.id);
        sliderAttachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*param, *k, nullptr));
        k->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        content.addAndMakeVisible (*k);
        knobs.push_back (std::move (k));
    }
    // toggles + lamp
    powerLever.setBounds (1154, 446, 88, 88);
    autoLever.setBounds (274, 657, 80, 80);
    jewel.setBounds (1080 - 36, 490 - 36, 72, 72);
    for (auto* b : { (juce::Button*) &powerLever, (juce::Button*) &autoLever, (juce::Button*) &jewel })
    {
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        content.addAndMakeVisible (*b);
    }
    buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (IDs::power), powerLever, nullptr));
    buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (IDs::power), jewel, nullptr));
    buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (IDs::autoGain), autoLever, nullptr));

    // top bar
    for (auto* b : { &prevButton, &nextButton, &presetButton, &saveButton, &aButton, &bButton, &licenseButton })
    {
        b->setLookAndFeel (&barLook);
        content.addAndMakeVisible (*b);
    }
    for (auto* b : { &prevButton, &nextButton, &presetButton }) b->getProperties().set ("plain", true);
    prevButton.getProperties().set ("fontSize", 22); nextButton.getProperties().set ("fontSize", 22);
    presetButton.getProperties().set ("fontSize", 17);
    prevButton.setBounds (454, 10, 30, 33); nextButton.setBounds (746, 10, 30, 33); presetButton.setBounds (486, 10, 258, 33);
    saveButton.setBounds (787, 8, 58, 37); aButton.setBounds (853, 8, 32, 37); bButton.setBounds (885, 8, 32, 37);
    licenseButton.setBounds (260, 10, 170, 33);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { savePreset(); };
    aButton.onClick = [this] { switchAB (0); };
    bButton.onClick = [this] { switchAB (1); };
    licenseButton.onClick = [this] { showActivation (true); };
    aButton.setToggleState (true, juce::dontSendNotification);

    makeSegment (modeSeg, "MODE", { "STEREO", "M/S", "MONO" }, 938, [this] (int i) { modeAttachment->setValueAsCompleteGesture ((float) i); });
    makeSegment (ovsSeg, "OVS", { "1x", "2x", "4x" }, 1122, [this] (int i) { ovsAttachment->setValueAsCompleteGesture ((float) i); });
    makeSegment (sizeSeg, "SIZE", { "80%", "100%", "125%" }, 1232, [this] (int i)
    {
        static constexpr float scales[] = { 0.64f, 0.8f, 1.0f };
        proc.uiScale = scales[i];
        applyScale();
    });
    auto pick = [] (Segment& seg, int i) { for (int k = 0; k < (int) seg.buttons.size(); ++k) seg.buttons[(size_t) k]->setToggleState (k == i, juce::dontSendNotification); };
    modeAttachment = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::mode), [this, pick] (float v) { pick (modeSeg, juce::roundToInt (v)); });
    ovsAttachment = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (IDs::ovs), [this, pick] (float v) { pick (ovsSeg, juce::roundToInt (v)); });
    modeAttachment->sendInitialUpdate(); ovsAttachment->sendInitialUpdate();

    proc.onStateRestored = [sp = juce::Component::SafePointer<Editor> (this)] { juce::MessageManager::callAsync ([sp] { if (sp) sp->applyScale(); }); };
    applyScale();
    lastTime = juce::Time::getMillisecondCounterHiRes();
    timerCallback();
    startTimerHz (60);

    if (! proc.license.isLicensed() && (! activationShownThisSession || ! proc.license.isUsable()))
    {
        activationShownThisSession = true;
        showActivation (true);
    }
}

Editor::~Editor()
{
    proc.onStateRestored = nullptr;
    for (auto* b : { &prevButton, &nextButton, &presetButton, &saveButton, &aButton, &bButton, &licenseButton }) b->setLookAndFeel (nullptr);
    for (auto* seg : { &modeSeg, &ovsSeg, &sizeSeg }) for (auto& b : seg->buttons) b->setLookAndFeel (nullptr);
}

void Editor::makeSegment (Segment& seg, const juce::String& caption, const juce::StringArray& items, int x, std::function<void (int)> onPick)
{
    seg.caption = caption; seg.x = x;
    int cx = x;
    for (int i = 0; i < items.size(); ++i)
    {
        auto b = std::make_unique<juce::TextButton> (items[i]);
        b->setLookAndFeel (&barLook);
        b->getProperties().set ("plain", true);
        b->getProperties().set ("fontSize", 14.5);
        const int w = juce::roundToInt (barFont (14.5f).getStringWidthFloat (items[i])) + 10;
        b->setBounds (cx, 24, w, 22);
        cx += w + 2;
        b->onClick = [onPick, i] { onPick (i); };
        content.addAndMakeVisible (*b);
        seg.buttons.push_back (std::move (b));
    }
}

void Editor::applyScale()
{
    const float s = proc.uiScale;
    content.setTransform (juce::AffineTransform::scale (s));
    setSize (juce::roundToInt (ui::W * s), juce::roundToInt (ui::H * s));
    const int idx = s < 0.7f ? 0 : s < 0.9f ? 1 : 2;
    for (int k = 0; k < 3; ++k) sizeSeg.buttons[(size_t) k]->setToggleState (k == idx, juce::dontSendNotification);
}

void Editor::resized() {}

void Editor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (0.0, 0.05, (now - lastTime) * 0.001);
    lastTime = now;
    const bool power = proc.state.getRawParameterValue (IDs::power)->load() > 0.5f;
    chassis.update (power ? proc.getGainReductionDb() : 0.0f, power, dt);

    auto name = proc.presetName + (proc.isPresetModified() ? " *" : "");
    if (presetButton.getButtonText() != name) presetButton.setButtonText (name);

    auto chip = proc.license.isLicensed() ? juce::String() : proc.license.isUsable()
                  ? juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (proc.license.getTrialDaysLeft()) + " DAYS"
                  : juce::String ("ACTIVATE");
    if (licenseButton.getButtonText() != chip) licenseButton.setButtonText (chip);
    licenseButton.setVisible (chip.isNotEmpty());
}

void Editor::stepPreset (int delta)
{
    const auto& all = proc.presets.all();
    if (all.empty()) return;
    int i = proc.presets.indexOf (proc.presetName);
    i = ((i < 0 ? 0 : i) + delta + (int) all.size()) % (int) all.size();
    proc.applyPreset (all[(size_t) i]);
}

void Editor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&barLook);
    const auto& all = proc.presets.all();
    std::map<juce::String, juce::PopupMenu> groups;
    juce::StringArray order;
    for (size_t i = 0; i < all.size(); ++i)
    {
        auto g = all[i].factory ? all[i].group : juce::String ("USER");
        if (! order.contains (g)) order.add (g);
        groups[g].addItem ((int) i + 1, all[i].name, true, all[i].name == proc.presetName);
    }
    for (auto& g : order) menu.addSubMenu (g, groups[g]);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton),
                        [sp = juce::Component::SafePointer<Editor> (this)] (int r)
                        {
                            if (sp == nullptr || r <= 0) return;
                            const auto& a = sp->proc.presets.all();
                            if (r - 1 < (int) a.size()) sp->proc.applyPreset (a[(size_t) r - 1]);
                        });
}

void Editor::savePreset()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name of the preset", juce::MessageBoxIconType::NoIcon);
    saveDialog->addTextEditor ("name", proc.presetName == "Default" ? juce::String ("My preset") : proc.presetName);
    saveDialog->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([sp = juce::Component::SafePointer<Editor> (this)] (int r)
    {
        if (sp == nullptr || sp->saveDialog == nullptr) return;
        auto name = sp->saveDialog->getTextEditorContents ("name").trim();
        if (r == 1 && name.isNotEmpty())
        {
            auto preset = lt::presets::ParamPreset::capture (sp->proc.state, name, "USER");
            preset.factory = false;
            if (sp->proc.presets.save (preset)) sp->proc.presetName = name;
        }
        sp->saveDialog.reset();
    }), false);
}

void Editor::switchAB (int slot)
{
    if (slot != abCurrent)
    {
        abSlots[abCurrent] = proc.state.copyState();
        if (abSlots[slot].isValid()) proc.state.replaceState (abSlots[slot].createCopy());
        abCurrent = slot;
    }
    aButton.setToggleState (abCurrent == 0, juce::dontSendNotification);
    bButton.setToggleState (abCurrent == 1, juce::dontSendNotification);
}

void Editor::showActivation (bool show)
{
    if (show && activation == nullptr)
    {
        proc.license.refresh();
        activation = std::make_unique<lt::ui::ActivationComponent> (proc.license, lt::ui::Palette::dark());
        activation->onDismiss = [sp = juce::Component::SafePointer<Editor> (this)] { juce::MessageManager::callAsync ([sp] { if (sp) sp->showActivation (false); }); };
        content.addAndMakeVisible (*activation);
        activation->setBounds (0, 0, ui::W, ui::H);
    }
    else if (! show && activation != nullptr)
    {
        proc.refreshLicense();
        if (! proc.license.isUsable()) return;
        content.removeChildComponent (activation.get());
        activation.reset();
    }
}

} // namespace corona
