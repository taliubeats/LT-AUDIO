#include "PresetBrowser.h"

namespace nebula
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace { constexpr float pi = juce::MathConstants<float>::pi; const juce::StringArray tags { "ALL", "AMBIENT", "RHYTHMIC", "GLITCH", "DARK", "LO-FI", "VOCAL", "FAVORITES" }; }

PresetBrowser::PresetBrowser (BankLibrary& l) : lib (l)
{
    search.setTextToShowWhenEmpty ("Search presets...", juce::Colours::grey);
    search.setFont (Fonts::regular (16.0f));
    search.setIndents (34, 0);
    search.setJustification (juce::Justification::centredLeft);
    search.onTextChange = [this] { refilter(); };
    addAndMakeVisible (search);

    for (auto& t : tags)
    {
        auto* b = tagButtons.add (new juce::TextButton (t == "FAVORITES" ? juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 FAVORITES")) : t));
        b->onClick = [this, t] { tag = t; refilter(); };
        addAndMakeVisible (b);
    }

    list.setRowHeight (40);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setOutlineThickness (0);
    addAndMakeVisible (list);

    closeButton.setButtonText (juce::CharPointer_UTF8 ("\xc3\x97"));
    closeButton.onClick = [this] { if (onClose) onClose(); };
    newButton.onClick = [this] { newBank(); };
    importButton.onClick = [this] { importBank(); };
    exportButton.onClick = [this] { exportBank(); };
    coverButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a cover image", juce::File(), "*.png;*.jpg;*.jpeg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile()) { auto name = lib.banks()[(size_t) bankIndex].name; lib.setCover (bankIndex, fc.getResult()); selectBank (lib.indexOfBank (name)); }
        });
    };
    for (auto* b : { &closeButton, &newButton, &importButton, &exportButton, &coverButton }) addAndMakeVisible (b);
}

void PresetBrowser::show (const juce::String& currentBank)
{
    lib.rescan();
    selectBank (juce::jmax (0, lib.indexOfBank (currentBank)));
    setVisible (true);
    toFront (true);
}

void PresetBrowser::selectBank (int i)
{
    bankIndex = juce::jlimit (0, (int) lib.banks().size() - 1, i);
    coverButton.setVisible (! lib.banks()[(size_t) bankIndex].factory);
    exportButton.setEnabled (! lib.banks()[(size_t) bankIndex].presets.empty());
    refilter();
    repaint();
}

void PresetBrowser::refilter()
{
    visible.clear();
    auto& bank = lib.banks()[(size_t) bankIndex];
    auto q = search.getText().trim().toLowerCase();
    for (int i = 0; i < (int) bank.presets.size(); ++i)
    {
        auto& p = bank.presets[(size_t) i];
        if (q.isNotEmpty() && ! p.name.toLowerCase().contains (q) && ! p.group.toLowerCase().contains (q)) continue;
        if (tag == "FAVORITES" && ! lib.isFavourite (bank.name, p.name)) continue;
        if (tag != "ALL" && tag != "FAVORITES")
        {
            auto t = p.group.toUpperCase();
            auto match = t.contains (tag)
                      || (tag == "AMBIENT" && (t.contains ("PAD") || t.contains ("DRONE")))
                      || (tag == "RHYTHMIC" && t.contains ("ARP"));
            if (! match) continue;
        }
        visible.push_back (i);
    }
    for (int i = 0; i < tagButtons.size(); ++i) tagButtons[i]->setToggleState (tags[i] == tag, juce::dontSendNotification);
    list.updateContent();
    list.repaint();
}

juce::Rectangle<float> PresetBrowser::bankRow (int i) const
{
    return { panel.getX() + 15.0f, panel.getY() + 90.0f + (float) (i - bankScroll) * 74.0f, 270.0f, 64.0f };
}

void PresetBrowser::resized()
{
    panel = { 42.0f, 38.0f, (float) getWidth() - 84.0f, (float) getHeight() - 76.0f };
    auto x = (int) panel.getX(), y = (int) panel.getY();
    search.setBounds (x + 15, y + 15, 277, 32);
    const int widths[] = { 48, 82, 92, 70, 58, 58, 62, 118 };
    auto tx = x + 315;
    for (int i = 0; i < tagButtons.size(); ++i) { tagButtons[i]->setBounds (tx, y + 16, widths[i], 30); tx += widths[i] + 8; }
    closeButton.setBounds ((int) panel.getRight() - 48, y + 14, 34, 34);
    list.setBounds (x + 315, y + 90, 385, (int) panel.getHeight() - 100);
    coverArea = { panel.getX() + 733.0f, panel.getY() + 90.0f, 267.0f, 270.0f };
    coverButton.setBounds ((int) coverArea.getCentreX() - 62, (int) coverArea.getBottom() - 34, 124, 24);
    newButton.setBounds (x + 15, (int) panel.getBottom() - 46, 108, 30);
    importButton.setBounds (x + 130, (int) panel.getBottom() - 46, 80, 30);
    exportButton.setBounds (x + 216, (int) panel.getBottom() - 46, 78, 30);
}

void PresetBrowser::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    g.fillAll ((p.isLight ? juce::Colour (0xff1c6f99) : juce::Colours::black).withAlpha (0.45f));
    Glass::drawPanel (g, panel, 16.0f, p, 2.2f);
    g.setColour (p.glassEdge);
    g.drawRoundedRectangle (panel, 16.0f, 1.2f);

    // search icon
    auto s = search.getBounds().toFloat();
    g.setColour (p.textDim);
    g.drawEllipse (s.getX() + 11.0f, s.getCentreY() - 7.0f, 11.0f, 11.0f, 1.5f);
    g.drawLine (s.getX() + 20.0f, s.getCentreY() + 2.5f, s.getX() + 25.0f, s.getCentreY() + 7.5f, 1.5f);

    g.setColour (p.textDim);
    g.setFont (Fonts::regular (13.0f));
    g.drawText ("BANKS", juce::Rectangle<float> (panel.getX() + 15.0f, panel.getY() + 62.0f, 200.0f, 20.0f), juce::Justification::centredLeft);
    g.drawText ("PRESETS", juce::Rectangle<float> (panel.getX() + 318.0f, panel.getY() + 62.0f, 200.0f, 20.0f), juce::Justification::centredLeft);

    // banks
    auto& banks = lib.banks();
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (juce::Rectangle<int> ((int) panel.getX(), (int) panel.getY() + 86, 300, (int) panel.getHeight() - 150));
        for (int i = 0; i < (int) banks.size(); ++i)
        {
            auto r = bankRow (i);
            if (i == bankIndex) Glass::drawPanel (g, r.expanded (4.0f, 3.0f), 8.0f, p, 1.6f, true);
            auto cover = r.withWidth (62.0f).withSizeKeepingCentre (62.0f, 62.0f);
            juce::Path clip; clip.addRoundedRectangle (cover, 6.0f);
            {
                juce::Graphics::ScopedSaveState s2 (g);
                g.reduceClipRegion (clip);
                g.drawImage (banks[(size_t) i].cover, cover, juce::RectanglePlacement::fillDestination);
            }
            g.setColour (p.glassEdge);
            g.drawRoundedRectangle (cover, 6.0f, 1.0f);
            g.setColour (p.isLight ? p.text : juce::Colours::white);
            g.setFont (Fonts::regular (22.0f));
            auto text = r.withTrimmedLeft (76.0f);
            g.drawText (banks[(size_t) i].name, text.withTrimmedBottom (banks[(size_t) i].factory ? 22.0f : 0.0f), juce::Justification::centredLeft);
            if (banks[(size_t) i].factory)
            {
                g.setColour (p.textDim);
                g.setFont (Fonts::regular (14.0f));
                g.drawText ("(Factory)", text.withTrimmedTop (30.0f), juce::Justification::topLeft);
            }
        }
    }
    g.setColour (p.glassEdge);
    g.drawHorizontalLine ((int) panel.getBottom() - 58, panel.getX() + 15.0f, panel.getX() + 295.0f);

    // selected bank: cover + name + count
    auto& b = banks[(size_t) bankIndex];
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addRoundedRectangle (coverArea, 10.0f);
        g.reduceClipRegion (clip);
        g.setOpacity (1.0f);
        g.drawImageWithin (b.cover, coverArea.getSmallestIntegerContainer().getX(), coverArea.getSmallestIntegerContainer().getY(),
                           (int) coverArea.getWidth(), (int) coverArea.getHeight(), juce::RectanglePlacement::fillDestination);
    }
    g.setColour (p.glassEdge);
    g.drawRoundedRectangle (coverArea, 10.0f, 1.2f);
    g.setColour (p.isLight ? p.text : juce::Colours::white);
    Fonts::drawSpaced (g, b.name, { coverArea.getX() - 20.0f, coverArea.getBottom() + 22.0f, coverArea.getWidth() + 40.0f, 48.0f },
                       Fonts::regular (b.name.length() > 10 ? 32.0f : 42.0f), 0.02f, juce::Justification::centred);
    g.setColour (p.textDim);
    g.setFont (Fonts::regular (20.0f));
    g.drawText ("by " + b.author, juce::Rectangle<float> (coverArea.getX(), coverArea.getBottom() + 72.0f, coverArea.getWidth(), 26.0f), juce::Justification::centred);
    g.drawText (juce::String ((int) b.presets.size()) + " PRESETS", juce::Rectangle<float> (coverArea.getX(), coverArea.getBottom() + 104.0f, coverArea.getWidth(), 26.0f),
                juce::Justification::centred);
}

void PresetBrowser::mouseUp (const juce::MouseEvent& e)
{
    auto& banks = lib.banks();
    for (int i = 0; i < (int) banks.size(); ++i)
        if (bankRow (i).contains (e.position) && e.position.y < panel.getBottom() - 60.0f)
        {
            if (e.mods.isPopupMenu() && ! banks[(size_t) i].factory && i >= 2)
            {
                juce::PopupMenu m;
                m.addItem (1, "Delete bank \"" + banks[(size_t) i].name + "\" (moves it to the trash)");
                m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, i] (int r) { if (r == 1) { lib.deleteBank (i); selectBank (1); } });
                return;
            }
            selectBank (i);
            return;
        }
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    if (palette == nullptr || row >= (int) visible.size()) return;
    auto& p = *palette;
    auto& bank = lib.banks()[(size_t) bankIndex];
    auto& preset = bank.presets[(size_t) visible[(size_t) row]];
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (2.0f, 2.0f);
    auto current = getCurrentPreset && getCurrentPreset() == preset.name;
    if (current)
    {
        Glass::drawRectGlow (g, r, 8.0f, p.accent, 6.0f, 0.8f);
        Glass::drawPanel (g, r, 8.0f, p, 1.6f, true);
    }
    else
    {
        g.setColour (p.glassEdge.withMultipliedAlpha (0.5f));
        g.drawHorizontalLine (h - 1, 6.0f, (float) w - 6.0f);
    }
    g.setColour (p.isLight ? p.text : juce::Colours::white);
    g.setFont (Fonts::regular (20.0f));
    g.drawText (preset.name, r.withTrimmedLeft (10.0f), juce::Justification::centredLeft);

    // tag pills (up to two) and the star
    auto star = starArea (w, h);
    auto x = star.getX() - 6.0f;
    auto tagList = juce::StringArray::fromTokens (preset.group, ",", "");
    for (int t = juce::jmin (1, tagList.size() - 1); t >= 0; --t)
    {
        auto text = tagList[t].trim();
        if (text.isEmpty()) continue;
        auto tw = juce::GlyphArrangement::getStringWidth (Fonts::regular (13.0f), text) + 18.0f;
        auto pill = juce::Rectangle<float> (x - tw, (float) h * 0.5f - 11.0f, tw, 22.0f);
        g.setColour (p.glassEdge);
        g.drawRoundedRectangle (pill, 11.0f, 1.0f);
        g.setColour (p.text);
        g.setFont (Fonts::regular (13.0f));
        g.drawText (text, pill, juce::Justification::centred);
        x -= tw + 6.0f;
    }
    auto fav = lib.isFavourite (bank.name, preset.name);
    juce::Path starPath;
    starPath.addStar (star.getCentre(), 5, 4.5f, 10.0f, 0.0f);
    juce::ignoreUnused (pi);
    if (fav) { g.setColour (p.isLight ? juce::Colours::white : juce::Colours::white); g.fillPath (starPath); }
    else { g.setColour (p.textDim); g.strokePath (starPath, juce::PathStrokeType (1.3f)); }
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (row >= (int) visible.size()) return;
    auto& bank = lib.banks()[(size_t) bankIndex];
    auto& preset = bank.presets[(size_t) visible[(size_t) row]];
    if (starArea (list.getWidth(), list.getRowHeight()).contains (e.position.x, (float) list.getRowHeight() * 0.5f))
    {
        lib.toggleFavourite (bank.name, preset.name);
        auto keep = bank.name;
        refilter();
        juce::ignoreUnused (keep);
        return;
    }
    if (e.mods.isPopupMenu() && ! bank.factory)
    {
        juce::PopupMenu m;
        m.addItem (1, "Delete \"" + preset.name + "\"");
        auto name = preset.name;
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, name] (int r)
        {
            if (r == 1) { auto b = lib.banks()[(size_t) bankIndex].name; lib.deletePreset (bankIndex, name); selectBank (lib.indexOfBank (b)); }
        });
        return;
    }
    if (onLoad) onLoad (preset, bank.name);
    list.repaint();
}

void PresetBrowser::newBank()
{
    dialog = std::make_unique<juce::AlertWindow> ("New bank", "Name of the bank:", juce::MessageBoxIconType::NoIcon);
    dialog->setLookAndFeel (&getLookAndFeel());
    dialog->addTextEditor ("name", "");
    dialog->addButton ("CREATE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        auto name = dialog->getTextEditorContents ("name").trim();
        dialog.reset();
        if (result == 1 && name.isNotEmpty())
        {
            auto i = lib.createBank (name);
            if (i >= 0) selectBank (i);
        }
    }), false);
}

void PresetBrowser::importBank()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a bank (.ltbank) or a preset (.ltpreset)", juce::File(), "*.ltbank;*.ltpreset");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult().existsAsFile())
        {
            auto i = lib.importFile (fc.getResult());
            if (i >= 0) selectBank (i);
        }
    });
}

void PresetBrowser::exportBank()
{
    auto& b = lib.banks()[(size_t) bankIndex];
    auto suggested = juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (juce::File::createLegalFileName (b.name) + ".ltbank");
    chooser = std::make_unique<juce::FileChooser> ("Export bank", suggested, "*.ltbank");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult() != juce::File()) lib.exportBank (bankIndex, fc.getResult().withFileExtension ("ltbank"));
    });
}

} // namespace nebula
