#include "Browser.h"

namespace quasar::ui
{

juce::Image makeCover (const juce::String& name, juce::Colour tint, int size)
{
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Graphics g (img);
    juce::Random r ((juce::int64) name.hashCode());
    g.fillAll (juce::Colour (0xff05030b));
    auto nebula = lt::ui::NebulaBackground::renderNebula (size, size, Palette::dark(), (int) (name.hashCode() & 0x7fff), r.nextFloat() * 0.6f - 0.3f, 0.8f);
    g.setOpacity (0.9f);
    g.drawImageAt (nebula, 0, 0);
    g.setOpacity (1.0f);
    g.setColour (tint.withAlpha (0.35f));
    g.fillAll();
    auto c = juce::Point<float> ((float) size * (0.3f + 0.4f * r.nextFloat()), (float) size * (0.3f + 0.4f * r.nextFloat()));
    auto kind = r.nextInt (3);
    for (int i = 0; i < 160; ++i)
    {
        auto x = r.nextFloat() * (float) size, y = r.nextFloat() * (float) size, s = 0.5f + r.nextFloat() * 1.6f;
        g.setColour (juce::Colours::white.withAlpha (0.3f + 0.6f * r.nextFloat()));
        g.fillEllipse (x, y, s, s);
    }
    auto R = (float) size * (0.18f + 0.1f * r.nextFloat());
    if (kind == 0)   // quasar: disk + jets
    {
        for (int k = 0; k < 2; ++k)
        {
            juce::ColourGradient jet (juce::Colours::white.withAlpha (0.8f), c.x, c.y, juce::Colours::transparentWhite, c.x + (k ? 1.0f : -1.0f) * R * 0.8f, c.y + (k ? 1.0f : -1.0f) * R * 2.4f, false);
            g.setGradientFill (jet);
            juce::Path p; p.addTriangle (c.x - 3, c.y, c.x + 3, c.y, c.x + (k ? 1.0f : -1.0f) * R * 0.8f, c.y + (k ? 1.0f : -1.0f) * R * 2.4f);
            g.fillPath (p);
        }
        for (int i = 0; i < 500; ++i)
        {
            auto rad = R * (0.3f + std::pow (r.nextFloat(), 0.8f) * 1.3f), a = r.nextFloat() * juce::MathConstants<float>::twoPi;
            g.setColour (tint.brighter (0.8f).withAlpha (0.25f + 0.5f * r.nextFloat()));
            g.fillEllipse (c.x + std::cos (a) * rad - 0.8f, c.y + std::sin (a) * rad * 0.3f - 0.8f, 1.6f, 1.6f);
        }
    }
    else if (kind == 1)   // ringed planet
    {
        juce::ColourGradient body (tint.brighter (0.9f), c.x - R * 0.5f, c.y - R * 0.5f, tint.darker (0.8f), c.x + R, c.y + R, true);
        g.setGradientFill (body); g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawEllipse (c.x - R * 1.8f, c.y - R * 0.45f, R * 3.6f, R * 0.9f, 2.0f);
    }
    else   // spiral galaxy
        for (int i = 0; i < 900; ++i)
        {
            auto t = r.nextFloat(), arm = (float) r.nextInt (2) * juce::MathConstants<float>::pi;
            auto a = arm + t * 5.0f + (r.nextFloat() - 0.5f) * 0.6f, rad = t * R * 1.8f;
            g.setColour (tint.brighter (1.0f).withAlpha (0.2f + 0.6f * r.nextFloat()));
            g.fillEllipse (c.x + std::cos (a) * rad - 0.7f, c.y + std::sin (a) * rad * 0.7f - 0.7f, 1.4f, 1.4f);
        }
    Glass::drawGlow (g, c, R * 0.9f, tint.brighter (0.6f), 0.8f);
    Glass::drawGlow (g, c, R * 0.25f, juce::Colours::white, 1.0f);
    return img;
}

BrowserPage::BrowserPage (Processor& p) : Page (p)
{
    search.setTextToShowWhenEmpty ("Buscar presets...", Colours::dim);
    search.setFont (Fonts::regular (16.0f));
    search.setIndents (36, 8);
    search.onTextChange = [this] { scroll = 0; filter(); };
    addAndMakeVisible (search);
    search.setBounds (274, 112, 650, 34);
    for (auto* b : { &importButton, &exportBankButton })
    {
        b->getProperties().set ("fontSize", 11.0);
        addAndMakeVisible (b);
    }
    importButton.setBounds (26, 698, 104, 26);
    exportBankButton.setBounds (136, 698, 104, 26);
    for (auto* b : { &saveButton, &newBankButton, &exportButton })
    {
        b->getProperties().set ("fontSize", 14.0);
        addAndMakeVisible (b);
    }
    saveButton.setBounds (962, 602, 208, 32);
    newBankButton.setBounds (962, 642, 208, 32);
    exportButton.setBounds (962, 682, 208, 32);

    importButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import a QUASAR bank or preset", juce::File(), "*.ltqbank;*.ltq");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& c)
        {
            auto f = c.getResult();
            if (f == juce::File()) return;
            juce::String err, name;
            auto ok = f.hasFileExtension ("ltqbank") ? proc.presets.importBank (f, err, &name) : proc.presets.importPreset (f, err);
            if (! ok) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "LT QUASAR", err);
            else if (name.isNotEmpty()) { section = 4; bankFilter = name; }
            refresh();
        });
    };
    exportBankButton.onClick = [this]
    {
        auto bank = section == 4 ? bankFilter : (section == 3 ? juce::String ("USER") : proc.presetBank);
        if (bank.isEmpty()) return;
        chooser = std::make_unique<juce::FileChooser> ("Export bank", juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (bank + ".ltqbank"), "*.ltqbank");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this, bank] (const juce::FileChooser& c)
        {
            auto f = c.getResult();
            if (f == juce::File()) return;
            juce::String err;
            if (! proc.presets.exportBank (bank, f.withFileExtension ("ltqbank"), err))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "LT QUASAR", err);
        });
    };
    exportButton.onClick = [this]
    {
        auto p = proc.capturePreset (proc.presetName);
        if (auto* cur = proc.presets.find (proc.presetBank, proc.presetName)) { p.category = cur->category; p.style = cur->style; p.author = cur->author; }
        chooser = std::make_unique<juce::FileChooser> ("Export preset", juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (proc.presetName + ".ltq"), "*.ltq");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this, p] (const juce::FileChooser& c)
        {
            if (c.getResult() != juce::File()) proc.presets.exportPreset (p, c.getResult().withFileExtension ("ltq"));
        });
    };
    saveButton.onClick = [this] { savePresetDialog(); };
    newBankButton.onClick = [this]
    {
        auto* w = new juce::AlertWindow ("New bank", "A bank is a folder of presets you can export and share (.ltqbank).", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", "MY BANK", "Name");
        w->addTextEditor ("author", "", "Author");
        w->addTextEditor ("desc", "", "Description");
        w->addButton ("CREATE", 1); w->addButton ("CANCEL", 0);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
        {
            if (r != 1) return;
            auto name = w->getTextEditorContents ("name").trim();
            if (name.isEmpty() || ! proc.presets.createBank (name, w->getTextEditorContents ("author"), w->getTextEditorContents ("desc")))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "LT QUASAR", "That bank already exists.");
            section = 4; bankFilter = name.toUpperCase();
            refresh();
        }), true);
    };
    setClickAreas ({ { 18, 140, 230, 552 }, { 262, 150, 674, 582 } });   // sections, banks, pills, preset list
    refresh();
}

BrowserPage::~BrowserPage() = default;

void BrowserPage::refresh() { filter(); repaint(); }

juce::Rectangle<int> BrowserPage::catPill (int i) const
{
    auto& c = PresetLibrary::categories();
    // first row: ALL + 8 categories, second row: the rest
    int row = i < 9 ? 0 : 1, idx = i < 9 ? i : i - 9;
    int x = 274;
    for (int k = 0; k < idx; ++k)
    {
        auto label = (row == 0 ? (k == 0 ? juce::String ("ALL") : c[k - 1]) : c[8 + k]);
        x += juce::GlyphArrangement::getStringWidthInt (Fonts::regular (14.0f), label) + 30 + 8;
    }
    auto label = row == 0 ? (idx == 0 ? juce::String ("ALL") : c[idx - 1]) : c[8 + idx];
    return { x, 160 + row * 34, juce::GlyphArrangement::getStringWidthInt (Fonts::regular (14.0f), label) + 30, 26 };
}

juce::Rectangle<int> BrowserPage::stylePill (int i) const
{
    auto& s = PresetLibrary::styles();
    int x = 274;
    for (int k = 0; k < i; ++k) x += juce::GlyphArrangement::getStringWidthInt (Fonts::regular (14.0f), s[k]) + 30 + 8;
    return { x, 234, juce::GlyphArrangement::getStringWidthInt (Fonts::regular (14.0f), s[i]) + 30, 26 };
}

void BrowserPage::filter()
{
    list.clear();
    auto q = search.getText().trim().toLowerCase();
    auto& lib = proc.presets;
    std::vector<const Preset*> source;
    if (section == 2)
    {
        for (auto& key : lib.getRecent())
            if (auto* p = lib.find (key.upToFirstOccurrenceOf ("/", false, false), key.fromFirstOccurrenceOf ("/", false, false))) source.push_back (p);
    }
    else source = lib.allPresets();
    for (auto* p : source)
    {
        if (section == 1 && ! lib.isFavourite (*p)) continue;
        if (section == 3 && p->bank != "USER") continue;
        if (section == 4 && p->bank != bankFilter) continue;
        if (category.isNotEmpty() && p->category != category) continue;
        if (style.isNotEmpty() && ! p->style.contains (style)) continue;
        if (q.isNotEmpty() && ! (p->name.toLowerCase().contains (q) || p->category.toLowerCase().contains (q) || p->bank.toLowerCase().contains (q)
                                 || p->style.toLowerCase().contains (q) || p->author.toLowerCase().contains (q))) continue;
        list.push_back (p);
    }
    scroll = juce::jlimit (0, juce::jmax (0, (int) list.size() - 8), scroll);
}

const juce::Image& BrowserPage::coverFor (const Bank& b)
{
    auto it = covers.find (b.name);
    if (it != covers.end()) return it->second;
    return covers[b.name] = b.cover.isValid() ? b.cover : makeCover (b.name, b.colour, 256);
}

void BrowserPage::paintPanels (juce::Graphics& g)
{
    auto& lib = proc.presets;
    // ---- left
    drawPanel (g, { 18, 98, 230, 634 });
    {
        auto icon = juce::Rectangle<float> (30, 112, 20, 18);
        g.setColour (Colours::text); g.drawRoundedRectangle (icon, 3, 1.3f);
        g.drawHorizontalLine (118, 33, 47); g.drawHorizontalLine (123, 33, 47);
        drawTitle (g, "BROWSER", { 58, 106, 170, 30 }, 20.0f);
    }
    const char* sections[] = { "ALL PRESETS", "FAVORITES", "RECENT", "USER" };
    for (int i = 0; i < 4; ++i)
    {
        auto r = sectionRow (i).toFloat();
        if (section == i) drawPanel (g, r, 6.0f, true);
        g.setColour (section == i ? juce::Colours::white : Colours::text); g.setFont (Fonts::regular (15.0f));
        g.drawText (sections[i], r.withTrimmedLeft (10).toNearestInt(), juce::Justification::centredLeft);
    }
    g.setColour (Colours::dim); g.setFont (Fonts::regular (14.0f));
    g.drawText ("BANKS", 30, 294, 200, 22, juce::Justification::centredLeft);
    int vb = 0;
    for (int i = bankScroll; i < (int) lib.banks().size() && vb < 7; ++i, ++vb)
    {
        auto& b = lib.banks()[(size_t) i];
        if (b.name == "USER") { --vb; continue; }
        auto r = bankRow (vb).toFloat();
        auto sel = section == 4 && bankFilter == b.name;
        if (sel) drawPanel (g, r, 6.0f, true);
        g.drawImage (coverFor (b), r.withWidth (40).withSizeKeepingCentre (40, 40).translated (4, 0));
        g.setColour (sel ? juce::Colours::white : Colours::text); g.setFont (Fonts::regular (13.5f));
        g.drawFittedText (b.name, r.withTrimmedLeft (54).toNearestInt(), juce::Justification::centredLeft, 2);
    }

    // ---- centre
    drawPanel (g, { 262, 98, 674, 634 });
    {
        auto c = juce::Point<float> (292, 129);
        g.setColour (Colours::dim); g.drawEllipse (c.x - 7, c.y - 7, 12, 12, 1.4f); g.drawLine (c.x + 4, c.y + 4, c.x + 9, c.y + 9, 1.6f);
    }
    auto pill = [&] (juce::Rectangle<int> r, const juce::String& t, bool sel)
    {
        auto rf = r.toFloat();
        if (sel) { Glass::drawRectGlow (g, rf, 13, Colours::accent, 8, 0.6f); g.setColour (juce::Colour (0xff4a1c9a)); g.fillRoundedRectangle (rf, 13); g.setColour (juce::Colour (0xffb78cff)); }
        else { g.setColour (juce::Colour (0x18ffffff)); g.fillRoundedRectangle (rf, 13); g.setColour (Colours::edge.withMultipliedAlpha (1.6f)); }
        g.drawRoundedRectangle (rf.reduced (0.5f), 13, 1.0f);
        g.setColour (sel ? juce::Colours::white : Colours::text); g.setFont (Fonts::regular (14.0f));
        g.drawText (t, r, juce::Justification::centred);
    };
    auto& cats = PresetLibrary::categories();
    pill (catPill (0), "ALL", category.isEmpty());
    for (int i = 0; i < cats.size(); ++i) pill (catPill (i + 1), cats[i], category == cats[i]);
    auto& sty = PresetLibrary::styles();
    for (int i = 0; i < sty.size(); ++i) pill (stylePill (i), sty[i], style == sty[i]);
    g.setColour (Colours::faint); g.drawHorizontalLine (272, 262, 936);
    // header
    g.setColour (Colours::dim); g.setFont (Fonts::regular (14.0f));
    juce::Path star; star.addStar ({ 293, 297 }, 5, 4.0f, 9.0f, 0);
    g.fillPath (star);
    g.drawText ("NAME", 324, 286, 180, 22, juce::Justification::centredLeft);
    g.drawText ("CATEGORY", 516, 286, 110, 22, juce::Justification::centredLeft);
    g.drawText ("BANK", 634, 286, 160, 22, juce::Justification::centredLeft);
    g.drawText ("AUTHOR", 800, 286, 120, 22, juce::Justification::centredLeft);
    for (int i = 0; i < 8 && i + scroll < (int) list.size(); ++i)
    {
        auto* p = list[(size_t) (i + scroll)];
        auto r = listRow (i).toFloat();
        auto current = p->name == proc.presetName && p->bank == proc.presetBank;
        if (current) drawPanel (g, r, 8.0f, true);
        g.setColour (Colours::faint); g.drawHorizontalLine ((int) r.getBottom() + 3, 274, 924);
        juce::Path s; s.addStar ({ r.getX() + 19, r.getCentreY() }, 5, 4.5f, 10.0f, 0);
        auto fav = proc.presets.isFavourite (*p);
        g.setColour (fav ? juce::Colour (0xffb98cff) : Colours::dim);
        if (fav) g.fillPath (s); else g.strokePath (s, juce::PathStrokeType (1.2f));
        g.setColour (juce::Colours::white); g.setFont (Fonts::regular (16.0f));
        g.drawText (p->name, juce::Rectangle<float> (324, r.getY(), 186, r.getHeight()).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (Colours::text); g.setFont (Fonts::regular (14.0f));
        g.drawText (p->category, juce::Rectangle<float> (516, r.getY(), 110, r.getHeight()).toNearestInt(), juce::Justification::centredLeft);
        g.drawText (p->bank, juce::Rectangle<float> (634, r.getY(), 160, r.getHeight()).toNearestInt(), juce::Justification::centredLeft);
        g.drawText (p->author, juce::Rectangle<float> (800, r.getY(), 120, r.getHeight()).toNearestInt(), juce::Justification::centredLeft);
    }
    if (list.empty())
    {
        g.setColour (Colours::dim); g.setFont (Fonts::regular (15.0f));
        g.drawText ("No presets here yet.", 274, 380, 650, 30, juce::Justification::centred);
    }
    if ((int) list.size() > 8)
    {
        auto track = juce::Rectangle<float> (928, 318, 4, 400);
        g.setColour (juce::Colour (0x18ffffff)); g.fillRoundedRectangle (track, 2);
        auto th = juce::jmax (20.0f, track.getHeight() * 8.0f / (float) list.size());
        g.setColour (Colours::arc.withAlpha (0.6f));
        g.fillRoundedRectangle (track.getX(), track.getY() + (track.getHeight() - th) * (float) scroll / (float) juce::jmax (1, (int) list.size() - 8), 4, th, 2);
    }

    // ---- right: the bank of the current selection
    drawPanel (g, { 950, 98, 232, 634 });
    const Bank* shown = nullptr;
    auto name = section == 4 ? bankFilter : section == 3 ? juce::String ("USER") : proc.presetBank;
    for (auto& b : lib.banks()) if (b.name == name) shown = &b;
    if (shown == nullptr && ! lib.banks().empty()) shown = &lib.banks().front();
    if (shown != nullptr)
    {
        auto img = juce::Rectangle<float> (962, 110, 208, 208);
        g.drawImage (coverFor (*shown), img);
        g.setColour (Colours::edge); g.drawRoundedRectangle (img, 6, 1);
        g.setColour (juce::Colour (0xffb98cff)); g.setFont (Fonts::regular (15.0f));
        g.drawText (juce::String ((int) shown->presets.size()) + " PRESETS", 962, 334, 208, 22, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white); g.setFont (Fonts::bold (22.0f));
        g.drawFittedText (shown->name, juce::Rectangle<int> (962, 358, 208, 30), juce::Justification::centredLeft, 1);
        g.setColour (juce::Colour (0xffb98cff)); g.setFont (Fonts::regular (14.0f));
        g.drawText ("BY " + shown->author.toUpperCase(), 962, 390, 208, 20, juce::Justification::centredLeft);
        g.setColour (Colours::text); g.setFont (Fonts::regular (13.5f));
        g.drawFittedText (shown->description, juce::Rectangle<int> (962, 418, 208, 170), juce::Justification::topLeft, 9);
    }
}

void BrowserPage::load (const Preset& p)
{
    proc.applyPreset (p);
    repaint();
}

void BrowserPage::step (int delta)
{
    if (list.empty()) filter();
    if (list.empty()) return;
    int cur = -1;
    for (int i = 0; i < (int) list.size(); ++i) if (list[(size_t) i]->name == proc.presetName && list[(size_t) i]->bank == proc.presetBank) cur = i;
    auto next = cur < 0 ? 0 : (cur + delta + (int) list.size()) % (int) list.size();
    load (*list[(size_t) next]);
}

void BrowserPage::mouseDown (const juce::MouseEvent& e)
{
    auto pos = e.getPosition();
    for (int i = 0; i < 4; ++i) if (sectionRow (i).contains (pos)) { section = i; bankFilter = {}; scroll = 0; refresh(); return; }
    int vb = 0;
    for (int i = bankScroll; i < (int) proc.presets.banks().size() && vb < 7; ++i, ++vb)
    {
        auto& b = proc.presets.banks()[(size_t) i];
        if (b.name == "USER") { --vb; continue; }
        if (bankRow (vb).contains (pos)) { section = 4; bankFilter = b.name; scroll = 0; refresh(); return; }
    }
    auto& cats = PresetLibrary::categories();
    if (catPill (0).contains (pos)) { category = {}; refresh(); return; }
    for (int i = 0; i < cats.size(); ++i) if (catPill (i + 1).contains (pos)) { category = category == cats[i] ? juce::String() : cats[i]; scroll = 0; refresh(); return; }
    auto& sty = PresetLibrary::styles();
    for (int i = 0; i < sty.size(); ++i) if (stylePill (i).contains (pos)) { style = style == sty[i] ? juce::String() : sty[i]; scroll = 0; refresh(); return; }
    for (int i = 0; i < 8 && i + scroll < (int) list.size(); ++i)
        if (listRow (i).contains (pos))
        {
            auto* p = list[(size_t) (i + scroll)];
            if (e.x < 310) { proc.presets.toggleFavourite (*p); refresh(); return; }
            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu m;
                m.addItem (1, "Load");
                m.addItem (2, proc.presets.isFavourite (*p) ? "Remove from favorites" : "Add to favorites");
                m.addItem (3, "Delete", ! p->factory);
                auto copy = *p;
                m.showMenuAsync (juce::PopupMenu::Options(), [this, copy] (int id)
                {
                    if (id == 1) load (copy);
                    else if (id == 2) proc.presets.toggleFavourite (copy);
                    else if (id == 3) proc.presets.deletePreset (copy);
                    refresh();
                });
                return;
            }
            load (*p);
            return;
        }
}

void BrowserPage::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    auto d = w.deltaY > 0 ? -1 : w.deltaY < 0 ? 1 : 0;
    if (e.x < 250) bankScroll = juce::jlimit (0, juce::jmax (0, (int) proc.presets.banks().size() - 7), bankScroll + d);
    else scroll = juce::jlimit (0, juce::jmax (0, (int) list.size() - 8), scroll + d);
    repaint();
}

void BrowserPage::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("Save preset", "", juce::MessageBoxIconType::NoIcon);
    auto* cur = proc.presets.find (proc.presetBank, proc.presetName);
    w->addTextEditor ("name", proc.presetName, "Name");
    w->addComboBox ("category", PresetLibrary::categories(), "Category");
    w->addComboBox ("style", PresetLibrary::styles(), "Style");
    juce::StringArray banks { "USER" };
    for (auto& b : proc.presets.banks()) if (! b.factory && b.name != "USER") banks.add (b.name);
    w->addComboBox ("bank", banks, "Bank");
    w->addTextEditor ("author", cur != nullptr && ! cur->factory ? cur->author : juce::String(), "Author");
    if (auto* c = w->getComboBoxComponent ("category")) c->setSelectedItemIndex (juce::jmax (0, PresetLibrary::categories().indexOf (cur != nullptr ? cur->category : "PLUCK")));
    if (auto* c = w->getComboBoxComponent ("style")) c->setSelectedItemIndex (juce::jmax (0, PresetLibrary::styles().indexOf (cur != nullptr ? cur->style.upToFirstOccurrenceOf (" ", false, false) : "TRAP")));
    if (auto* c = w->getComboBoxComponent ("bank")) c->setSelectedItemIndex (juce::jmax (0, banks.indexOf (section == 4 ? bankFilter : "USER")));
    w->addButton ("SAVE", 1); w->addButton ("CANCEL", 0);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
    {
        if (r != 1) return;
        auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty()) return;
        auto p = proc.capturePreset (name);
        p.category = w->getComboBoxComponent ("category")->getText();
        p.style = w->getComboBoxComponent ("style")->getText();
        p.author = w->getTextEditorContents ("author").trim();
        auto bank = w->getComboBoxComponent ("bank")->getText();
        if (proc.presets.saveUserPreset (p, bank))
        {
            if (auto* saved = proc.presets.find (bank, name)) proc.applyPreset (*saved);
        }
        refresh();
    }), true);
}

} // namespace quasar::ui
