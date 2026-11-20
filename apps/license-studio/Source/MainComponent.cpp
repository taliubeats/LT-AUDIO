#include "MainComponent.h"

namespace studio
{

using namespace lt::licensing;
using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Logo;

namespace
{
    const char* productOrder[] = { Products::typeBeats, Products::pulsar, Products::knock,
                                   Products::nebula, Products::orbit, Products::voidVerb, Products::tune, Products::nova,
                                   Products::vocalSuite, Products::quasar, Products::cosmos, Products::lyra, Products::warp, Products::all };

    juce::Font mono (float h)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain));
    }

    juce::String shortDate (const juce::String& iso)
    {
        auto t = juce::Time::fromISO8601 (iso);
        return t.toMilliseconds() > 0 ? t.formatted ("%b %d") : iso;
    }

    void styleEditor (juce::TextEditor& e, const juce::String& placeholder, const lt::ui::Palette& p, bool monospace = false)
    {
        e.setFont (monospace ? mono (16.0f) : Fonts::regular (15.0f));
        e.setTextToShowWhenEmpty (placeholder, p.textDim);
        e.setBorder ({ 0, 0, 0, 0 });
        e.setIndents (14, 0);
        e.setJustification (juce::Justification::centredLeft);
    }
}

// ------------------------------------------------------------------ SidebarItem

SidebarItem::SidebarItem (juce::String i, juce::String l, bool product, const lt::ui::Palette& p)
    : juce::Button (l), id (std::move (i)), label (std::move (l)), isProduct (product), palette (p)
{
    setClickingTogglesState (false);
}

void SidebarItem::paintButton (juce::Graphics& g, bool hover, bool)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    auto selected = getToggleState();

    if (selected)
        Glass::drawPanel (g, r, 10.0f, palette, 2.0f, true);
    else if (hover)
        Glass::drawPanel (g, r, 10.0f, palette, 1.0f);

    auto iconArea = r.removeFromLeft (r.getHeight()).reduced (6.0f);
    if (isProduct)
    {
        Logo::drawIcon (g, iconArea, id, palette, selected);
    }
    else
    {
        // simple line icons for sections
        auto c = iconArea.getCentre();
        auto s = iconArea.getWidth() * 0.32f;
        g.setColour (palette.textDim);
        if (id == "CUSTOMERS")
        {
            g.drawEllipse (c.x - s * 0.9f, c.y - s * 1.1f, s * 1.1f, s * 1.1f, 1.3f);
            g.drawEllipse (c.x + s * 0.1f, c.y - s * 0.9f, s * 0.9f, s * 0.9f, 1.3f);
            juce::Path body;
            body.addCentredArc (c.x - s * 0.35f, c.y + s * 0.9f, s * 0.9f, s * 0.7f, 0.0f, -1.5f, 1.5f, true);
            g.strokePath (body, juce::PathStrokeType (1.3f));
        }
        else
        {
            g.drawEllipse (c.x - s * 0.5f, c.y - s * 0.5f, s, s, 1.4f);
            for (int i = 0; i < 8; ++i)
            {
                auto a = (float) i * juce::MathConstants<float>::pi * 0.25f;
                g.drawLine ({ c.getPointOnCircumference (s * 0.75f, a), c.getPointOnCircumference (s * 1.1f, a) }, 1.6f);
            }
        }
    }

    g.setColour (selected ? juce::Colours::white : palette.text.withAlpha (isProduct ? 0.95f : 0.7f));
    g.setFont (isProduct ? Fonts::regular (15.0f) : Fonts::regular (13.5f));
    g.drawText (label, r.withTrimmedLeft (8.0f), juce::Justification::centredLeft);
}

// ------------------------------------------------------------------ list models

struct MainComponent::RecentModel : public juce::ListBoxModel
{
    explicit RecentModel (MainComponent& o) : owner (o) {}

    void rebuild()
    {
        rows.clear();
        auto q = owner.searchEditor.getText().trim();
        for (auto& r : owner.store.all())
            if (q.isEmpty() || r.info.name.containsIgnoreCase (q) || r.info.email.containsIgnoreCase (q)
                 || r.info.productId.containsIgnoreCase (q) || r.info.machineId.containsIgnoreCase (q))
                rows.push_back (&r);
    }

    int getNumRows() override { return (int) rows.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int) rows.size()) return;
        auto& r = *rows[(size_t) row];
        auto& p = owner.palette;
        auto area = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (2.0f, 1.0f);

        if (selected)
            Glass::drawPanel (g, area, 8.0f, p, 2.0f, true);

        g.setColour (p.glassEdge.withMultipliedAlpha (0.5f));
        g.drawHorizontalLine (h - 1, 4.0f, (float) w - 4.0f);

        auto text = area.reduced (8.0f, 0);
        auto tag = text.removeFromRight (56.0f).withSizeKeepingCentre (56.0f, 18.0f);
        Glass::drawPill (g, tag, p, false, false, false);
        g.setColour (p.accent.brighter (0.5f));
        g.setFont (Fonts::semiBold (9.5f));
        g.drawText ("ACTIVE", tag, juce::Justification::centred);

        g.setColour (p.text);
        g.setFont (Fonts::regular (13.0f));
        g.drawText (r.info.name, text.removeFromTop (text.getHeight() * 0.55f), juce::Justification::bottomLeft, true);
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (11.0f));
        g.drawText (Logo::displayNameFor (r.info.productId) + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) + r.info.machineId.substring (0, 9) + juce::String (juce::CharPointer_UTF8 ("...  \xc2\xb7  "))
                        + shortDate (r.info.issued),
                    text, juce::Justification::topLeft, true);
    }

    void selectedRowsChanged (int row) override
    {
        if (row >= 0 && row < (int) rows.size())
        {
            owner.showView (View::newLicense);
            owner.showResult (*rows[(size_t) row], false);
        }
        owner.removeButton.setEnabled (row >= 0);
    }

    MainComponent& owner;
    std::vector<const LicenseRecord*> rows;
};

struct MainComponent::CustomerModel : public juce::ListBoxModel
{
    explicit CustomerModel (MainComponent& o) : owner (o) {}

    struct Customer { juce::String name, email; juce::StringArray products, machines; };

    void rebuild()
    {
        customers.clear();
        for (auto& r : owner.store.all())
        {
            auto it = std::find_if (customers.begin(), customers.end(),
                                    [&] (const Customer& c) { return c.email.equalsIgnoreCase (r.info.email); });
            if (it == customers.end())
            {
                customers.push_back ({ r.info.name, r.info.email, {}, {} });
                it = customers.end() - 1;
            }
            it->products.addIfNotAlreadyThere (Logo::displayNameFor (r.info.productId));
            it->machines.addIfNotAlreadyThere (normaliseMachineId (r.info.machineId));
        }
    }

    int getNumRows() override { return (int) customers.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int) customers.size()) return;
        auto& c = customers[(size_t) row];
        auto& p = owner.palette;
        auto area = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (2.0f);

        Glass::drawPanel (g, area, 10.0f, p, selected ? 2.0f : 1.0f, selected);

        auto text = area.reduced (16.0f, 8.0f);
        auto right = text.removeFromRight (160.0f);
        g.setColour (p.text);
        g.setFont (Fonts::semiBold (15.0f));
        g.drawText (c.name, text.removeFromTop (22.0f), juce::Justification::centredLeft, true);
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (12.5f));
        g.drawText (c.email, text.removeFromTop (18.0f), juce::Justification::centredLeft, true);
        g.drawText (c.products.joinIntoString (juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "))), text, juce::Justification::centredLeft, true);

        g.setColour (p.text);
        g.setFont (Fonts::regular (13.0f));
        g.drawText (juce::String (c.machines.size()) + (c.machines.size() == 1 ? " computer" : " computers"),
                    right, juce::Justification::centredRight);
    }

    void selectedRowsChanged (int row) override
    {
        if (row >= 0 && row < (int) customers.size())
        {
            owner.searchEditor.setText (customers[(size_t) row].email, true);
        }
    }

    MainComponent& owner;
    std::vector<Customer> customers;
};

// ------------------------------------------------------------------ MainComponent

MainComponent::MainComponent()
{
    setLookAndFeel (&lnf);
    lnf.setPalette (palette);
    background.setPalette (palette);
    background.setParticleAmount (0.25f);
    addAndMakeVisible (background);
    addAndMakeVisible (overlay);

    key.loadOrCreate();
    store.load();
    store.onChange = [this] { refreshLists(); };

    for (auto* id : productOrder)
    {
        auto* item = productItems.add (new SidebarItem (id, Logo::displayNameFor (id), true, palette));
        item->onClick = [this, pid = juce::String (id)] { selectProduct (pid); showView (View::newLicense); };
        addAndMakeVisible (item);
    }

    customersItem = std::make_unique<SidebarItem> ("CUSTOMERS", "CUSTOMERS", false, palette);
    customersItem->onClick = [this] { showView (View::customers); };
    addAndMakeVisible (*customersItem);

    settingsItem = std::make_unique<SidebarItem> ("SETTINGS", "SETTINGS", false, palette);
    settingsItem->onClick = [this] { showView (View::settings); };
    addAndMakeVisible (*settingsItem);

    styleEditor (nameEditor, "Friend name", palette);
    styleEditor (emailEditor, "Email (required)", palette);
    styleEditor (machineEditor, "Machine ID  (XXXX-XXXX-XXXX-XXXX)", palette, true);
    emailEditor.onTextChange = [this] { overlay.repaint(); };
    machineEditor.onTextChange = [this] { overlay.repaint(); };
    nameEditor.onReturnKey = emailEditor.onReturnKey = machineEditor.onReturnKey = [this] { generate(); };

    for (auto* e : { &nameEditor, &emailEditor, &machineEditor })
        addChildComponent (*e);

    resultEditor.setReadOnly (true);
    resultEditor.setMultiLine (true, true);
    resultEditor.setCaretVisible (false);
    resultEditor.setFont (mono (13.0f));
    resultEditor.setBorder ({ 10, 12, 10, 12 });
    addChildComponent (resultEditor);

    pasteMachineButton.onClick = [this] { machineEditor.setText (normaliseMachineId (juce::SystemClipboard::getTextFromClipboard())); };
    generateButton.getProperties().set ("accent", true);
    generateButton.onClick = [this] { generate(); };
    copyButton.onClick = [this]
    {
        if (current)
        {
            juce::SystemClipboard::copyTextToClipboard (current->licenseText);
            setStatus ("License copied to the clipboard.", false);
        }
    };
    saveButton.onClick = [this] { saveLicenseFile(); };

    for (auto* b : { &pasteMachineButton, &generateButton, &copyButton, &saveButton })
        addChildComponent (*b);

    // recent list
    styleEditor (searchEditor, "Search", palette);
    searchEditor.onTextChange = [this] { refreshLists(); };
    addAndMakeVisible (searchEditor);

    recentModel = std::make_unique<RecentModel> (*this);
    recentList.setModel (recentModel.get());
    recentList.setRowHeight (52);
    recentList.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (recentList);

    removeButton.setEnabled (false);
    removeButton.onClick = [this]
    {
        auto row = recentList.getSelectedRow();
        if (row < 0 || row >= (int) recentModel->rows.size()) return;
        auto record = *recentModel->rows[(size_t) row];

        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Remove license record",
            "Remove the record of " + record.info.name + " (" + Logo::displayNameFor (record.info.productId) + ")?\n\n"
            "This only removes it from your list. Licenses already sent keep working.",
            "REMOVE", "CANCEL", this,
            juce::ModalCallbackFunction::create ([this, id = record.info.licenseId] (int result)
            {
                if (result == 1)
                {
                    recentList.deselectAllRows();
                    current.reset();
                    resultEditor.clear();
                    store.remove (id);
                }
            }));
    };
    addAndMakeVisible (removeButton);

    // customers
    customerModel = std::make_unique<CustomerModel> (*this);
    customerList.setModel (customerModel.get());
    customerList.setRowHeight (84);
    customerList.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addChildComponent (customerList);

    // settings
    publicKeyEditor.setReadOnly (true);
    publicKeyEditor.setFont (mono (14.0f));
    publicKeyEditor.setBorder ({ 10, 12, 10, 12 });
    publicKeyEditor.setText (key.publicKeyHex());
    addChildComponent (publicKeyEditor);

    copyKeyButton.getProperties().set ("accent", true);
    copyKeyButton.onClick = [this]
    {
        juce::SystemClipboard::copyTextToClipboard (key.publicKeyHex());
        setStatus ("Public key copied.", false);
    };

    exportKeyButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save signing key backup",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("LT-signing-key-backup.ltkey"), "*.ltkey");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f != juce::File())
                setStatus (key.exportTo (f) ? "Backup saved. Keep it somewhere safe and private." : "Could not save the backup.",
                           ! f.existsAsFile());
        });
    };

    importKeyButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import signing key backup", juce::File(), "*.ltkey;*.json");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;

            juce::String error;
            if (key.importFrom (f, error))
            {
                publicKeyEditor.setText (key.publicKeyHex());
                setStatus ("Signing key imported.", false);
            }
            else
            {
                setStatus (error, true);
            }
        });
    };

    openFolderButton.onClick = [] { getDataDirectory().startAsProcess(); };

    for (auto* b : { &copyKeyButton, &exportKeyButton, &importKeyButton, &openFolderButton })
        addChildComponent (*b);

    selectProduct (selectedProduct);
    refreshLists();
    showView (View::newLicense);
    setSize (1240, 780);
}

MainComponent::~MainComponent()
{
    recentList.setModel (nullptr);
    customerList.setModel (nullptr);
    setLookAndFeel (nullptr);
}

void MainComponent::setStatus (const juce::String& s, bool error)
{
    status = s;
    statusIsError = error;
    overlay.repaint();
}

void MainComponent::selectProduct (const juce::String& id)
{
    selectedProduct = id;
    for (auto* item : productItems)
        item->setToggleState (item->id == id, juce::dontSendNotification);
    overlay.repaint();
}

void MainComponent::showView (View v)
{
    view = v;
    customersItem->setToggleState (v == View::customers, juce::dontSendNotification);
    settingsItem->setToggleState (v == View::settings, juce::dontSendNotification);
    for (auto* item : productItems)
        item->setToggleState (v == View::newLicense && item->id == selectedProduct, juce::dontSendNotification);

    auto isNew = v == View::newLicense, isSettings = v == View::settings;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &nameEditor, &emailEditor, &machineEditor, &resultEditor,
                                                                          &pasteMachineButton, &generateButton, &copyButton, &saveButton })
        c->setVisible (isNew);

    for (juce::Component* c : std::initializer_list<juce::Component*> { &publicKeyEditor, &copyKeyButton, &exportKeyButton,
                                                                          &importKeyButton, &openFolderButton })
        c->setVisible (isSettings);

    customerList.setVisible (v == View::customers);
    if (v == View::customers)
        customerList.updateContent();

    status.clear();
    resized();
    repaint();
}

void MainComponent::refreshLists()
{
    recentModel->rebuild();
    recentList.updateContent();
    recentList.repaint();
    customerModel->rebuild();
    customerList.updateContent();
    customerList.repaint();
}

void MainComponent::generate()
{
    auto name = nameEditor.getText().trim();
    auto email = emailEditor.getText().trim();
    auto machine = normaliseMachineId (machineEditor.getText());

    if (name.isEmpty())                     { setStatus ("Write your friend's name.", true); return; }
    if (! isValidEmail (email))             { setStatus ("A valid email is required.", true); return; }
    if (machine.removeCharacters ("-").length() != 16 || ! machine.removeCharacters ("-").containsOnly ("0123456789ABCDEF"))
                                            { setStatus ("The Machine ID must look like XXXX-XXXX-XXXX-XXXX.", true); return; }
    if (! key.isValid())                    { setStatus ("No signing key. Check SETTINGS.", true); return; }

    auto doGenerate = [this, name, email, machine]
    {
        juce::uint8 idBytes[8];
        secureRandomBytes (idBytes, sizeof (idBytes));

        LicenseRecord r;
        r.info.productId = selectedProduct;
        r.info.name = name;
        r.info.email = email.toLowerCase();
        r.info.machineId = machine;
        r.info.issued = juce::Time::getCurrentTime().toISO8601 (true);
        r.info.licenseId = toHex (idBytes, sizeof (idBytes));
        r.licenseText = createLicenseText (r.info, key.secret());

        store.add (r);
        recentList.deselectAllRows();
        showResult (r, true);
    };

    auto machines = store.machinesFor (email, selectedProduct);
    if (! machines.contains (machine) && machines.size() >= 2)
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Computer limit reached",
            name + " already has 2 computers for " + Logo::displayNameFor (selectedProduct) + ":\n\n"
                + machines.joinIntoString ("\n") + "\n\nGenerate a license for a third computer anyway?",
            "GENERATE ANYWAY", "CANCEL", this,
            juce::ModalCallbackFunction::create ([doGenerate] (int result) { if (result == 1) doGenerate(); }));
        return;
    }

    doGenerate();
}

void MainComponent::showResult (const LicenseRecord& r, bool justCreated)
{
    current = r;
    LicenseInfo check;
    currentVerified = verifyLicenseText (r.licenseText, key.publicKey(), check)
                       && check.machineId == normaliseMachineId (r.info.machineId);
    resultEditor.setText (r.licenseText, false);

    if (! justCreated)
    {
        nameEditor.setText (r.info.name, false);
        emailEditor.setText (r.info.email, false);
        machineEditor.setText (r.info.machineId, false);
        selectProduct (r.info.productId);
    }

    setStatus (justCreated ? "License generated for " + r.info.name + "." : juce::String(), false);
}

void MainComponent::saveLicenseFile()
{
    if (! current) return;

    auto fileName = "LT-" + current->info.productId + "-" + juce::File::createLegalFileName (current->info.name).replaceCharacter (' ', '-') + ".lic";
    chooser = std::make_unique<juce::FileChooser> ("Save license file",
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (fileName), "*.lic");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File() || ! current) return;
        if (f.replaceWithText (current->licenseText))
            setStatus ("Saved " + f.getFileName() + ". Send it to your friend.", false);
        else
            setStatus ("Could not save the file.", true);
    });
}

// ------------------------------------------------------------------ layout

void MainComponent::resized()
{
    background.setBounds (getLocalBounds());
    overlay.setBounds (getLocalBounds());

    auto b = getLocalBounds();
    sidebarArea = b.removeFromLeft (270);
    rightArea = b.removeFromRight (330);
    centreArea = b;

    // sidebar
    auto s = sidebarArea.reduced (16, 0).withTrimmedTop (64);
    for (auto* item : productItems)
        item->setBounds (s.removeFromTop (54).reduced (0, 3));

    auto bottom = sidebarArea.reduced (16, 0).removeFromBottom (110).withTrimmedTop (14);
    customersItem->setBounds (bottom.removeFromTop (44));
    settingsItem->setBounds (bottom.removeFromTop (44));

    // right column
    auto r = rightArea.reduced (20, 0).withTrimmedTop (64);
    searchEditor.setBounds (r.removeFromTop (38));
    r.removeFromTop (12);
    removeButton.setBounds (r.removeFromBottom (60).withSizeKeepingCentre (120, 36));
    recentList.setBounds (r);

    auto c = centreArea.reduced (44, 0);
    if (view == View::newLicense)      layoutNewLicense (c);
    else if (view == View::settings)   layoutSettings (c);
    else                               layoutCustomers (c);
}

void MainComponent::layoutNewLicense (juce::Rectangle<int> c)
{
    c.removeFromTop (150);
    c = c.withWidth (juce::jmin (c.getWidth(), 520));

    c.removeFromTop (22);
    nameEditor.setBounds (c.removeFromTop (44));
    c.removeFromTop (34);
    emailEditor.setBounds (c.removeFromTop (44));
    c.removeFromTop (34);
    auto m = c.removeFromTop (44);
    pasteMachineButton.setBounds (m.removeFromRight (92).reduced (4, 4));
    m.removeFromRight (6);
    machineEditor.setBounds (m);
    computersRow = c.removeFromTop (30);
    c.removeFromTop (8);
    generateButton.setBounds (c.removeFromTop (50));
    c.removeFromTop (26);

    resultBox = c.removeFromTop (150);
    auto inner = resultBox.reduced (1);
    auto buttons = inner.removeFromBottom (46).reduced (12, 6);
    saveButton.setBounds (buttons.removeFromRight (140));
    buttons.removeFromRight (8);
    copyButton.setBounds (buttons.removeFromRight (80));
    resultEditor.setBounds (inner.reduced (4));
}

void MainComponent::layoutSettings (juce::Rectangle<int> c)
{
    c.removeFromTop (180);
    c = c.withWidth (juce::jmin (c.getWidth(), 600));
    publicKeyEditor.setBounds (c.removeFromTop (46));
    c.removeFromTop (12);
    copyKeyButton.setBounds (c.removeFromTop (44).withWidth (220));
    c.removeFromTop (196);
    auto row = c.removeFromTop (42);
    exportKeyButton.setBounds (row.removeFromLeft (200));
    row.removeFromLeft (10);
    importKeyButton.setBounds (row.removeFromLeft (200));
    row.removeFromLeft (10);
    openFolderButton.setBounds (row.removeFromLeft (180));
}

void MainComponent::layoutCustomers (juce::Rectangle<int> c)
{
    c.removeFromTop (130);
    customerList.setBounds (c.withTrimmedBottom (30));
}

// ------------------------------------------------------------------ paint

void MainComponent::paint (juce::Graphics&) {}

void MainComponent::paintOverlay (juce::Graphics& g)
{
    const auto& p = palette;

    // glass columns
    Glass::drawPanel (g, sidebarArea.toFloat().reduced (0.5f), 0.0f, p, 1.2f);
    Glass::drawPanel (g, rightArea.toFloat().reduced (0.5f), 0.0f, p, 1.2f);

    auto header = [&] (juce::Rectangle<int> area, const juce::String& text)
    {
        g.setColour (p.textDim);
        lt::ui::Fonts::drawSpaced (g, text, area.reduced (24, 0).withHeight (64).toFloat(),
                                   Fonts::regular (13.0f), 0.12f, juce::Justification::centredLeft);
    };

    header (sidebarArea, "PLUGINS");
    header (rightArea, "RECENT LICENSES");

    g.setColour (p.glassEdge.withMultipliedAlpha (0.6f));
    g.drawHorizontalLine (customersItem->getY() - 10, (float) sidebarArea.getX() + 16.0f, (float) sidebarArea.getRight() - 16.0f);

    auto c = centreArea.reduced (44, 0);
    auto title = c.withTrimmedTop (40).withHeight (52).toFloat();
    auto subtitle = title.translated (0, 50).withHeight (30);

    if (view == View::newLicense)
    {
        g.setColour (p.text);
        g.setFont (Fonts::light (40.0f));
        g.drawText ("NEW LICENSE", title, juce::Justification::centredLeft);
        Logo::draw (g, subtitle.withWidth (420.0f), Logo::specFor (selectedProduct), p, juce::Justification::centredLeft);

        auto label = [&] (juce::Component& comp, const juce::String& text)
        {
            g.setColour (p.text);
            g.setFont (Fonts::regular (15.0f));
            g.drawText (text, comp.getBounds().translated (0, -26).withHeight (22), juce::Justification::centredLeft);
        };
        label (nameEditor, "Friend name");
        label (emailEditor, "Email");
        label (machineEditor, "Machine ID");

        // computers used for this email + product
        auto email = emailEditor.getText().trim();
        auto machines = isValidEmail (email) ? store.machinesFor (email, selectedProduct) : juce::StringArray();
        auto machine = normaliseMachineId (machineEditor.getText());
        auto used = machines.size() + (machine.isNotEmpty() && ! machines.contains (machine) ? 1 : 0);

        auto row = computersRow.toFloat();
        g.setColour (used > 2 ? p.danger : p.textDim);
        g.setFont (Fonts::regular (13.0f));
        auto text = "Computers used: " + juce::String (juce::jmin (used, 99)) + " of 2";
        g.drawText (text, row, juce::Justification::centredLeft);
        auto dotX = row.getX() + juce::GlyphArrangement::getStringWidth (Fonts::regular (13.0f), text) + 12.0f;
        for (int i = 0; i < 2; ++i)
        {
            auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ dotX + (float) i * 16.0f, row.getCentreY() });
            if (i < used) { Glass::drawGlow (g, dot.getCentre(), 8.0f, p.accent, 0.8f); g.setColour (p.accent.brighter (0.4f)); g.fillEllipse (dot); }
            else          { g.setColour (p.textDim); g.drawEllipse (dot, 1.0f); }
        }

        // result box
        auto rb = resultBox.toFloat();
        Glass::drawPanel (g, rb, 12.0f, p, 1.6f, current.has_value());
        g.setColour (p.glassEdge.withMultipliedAlpha (0.6f));
        g.drawHorizontalLine (copyButton.getY() - 6, rb.getX() + 1.0f, rb.getRight() - 1.0f);

        auto statusArea = juce::Rectangle<float> (rb.getX() + 16.0f, (float) copyButton.getY(), 260.0f, (float) copyButton.getHeight());
        if (current)
        {
            auto ok = currentVerified;
            auto ic = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ statusArea.getX() + 7.0f, statusArea.getCentreY() });
            g.setColour (ok ? p.success : p.danger);
            g.fillEllipse (ic);
            g.setColour (juce::Colours::white);
            juce::Path tick;
            tick.startNewSubPath (ic.getX() + 3.5f, ic.getCentreY());
            tick.lineTo (ic.getX() + 6.0f, ic.getBottom() - 4.0f);
            tick.lineTo (ic.getRight() - 3.0f, ic.getY() + 4.0f);
            g.strokePath (tick, juce::PathStrokeType (1.6f));
            g.setColour (p.textDim);
            g.setFont (Fonts::regular (13.0f));
            g.drawText (ok ? "Signed & verified" : "Signature check failed", statusArea.withTrimmedLeft (22.0f), juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (p.textDim);
            g.setFont (Fonts::regular (13.0f));
            g.drawText ("The license will appear here", rb.withTrimmedBottom (46.0f), juce::Justification::centred);
        }

        if (! key.matchesEmbeddedKey())
        {
            g.setColour (juce::Colour (0xfffbbf24));
            g.setFont (Fonts::regular (12.5f));
            g.drawFittedText ("Your plugins don't contain this Studio's public key yet, so licenses won't activate. See SETTINGS.",
                              resultBox.translated (0, resultBox.getHeight() + 34).withHeight (36), juce::Justification::topLeft, 2);
        }
    }
    else if (view == View::settings)
    {
        g.setColour (p.text);
        g.setFont (Fonts::light (40.0f));
        g.drawText ("SETTINGS", title, juce::Justification::centredLeft);

        g.setColour (p.text);
        g.setFont (Fonts::semiBold (15.0f));
        g.drawText ("Public key", publicKeyEditor.getBounds().translated (0, -30).withHeight (24), juce::Justification::centredLeft);
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (13.0f));
        g.drawFittedText ("The plugins need this key to accept your licenses. It is safe to share.",
                          publicKeyEditor.getBounds().translated (0, -56).withHeight (24), juce::Justification::centredLeft, 1);

        auto status = juce::Rectangle<int> (copyKeyButton.getRight() + 16, copyKeyButton.getY(), 400, copyKeyButton.getHeight());
        auto installed = key.matchesEmbeddedKey();
        g.setColour (installed ? p.success : juce::Colour (0xfffbbf24));
        g.setFont (Fonts::regular (13.0f));
        g.drawText (installed ? "Installed in the plugins of this build" : "Not installed in the plugins yet",
                    status, juce::Justification::centredLeft);

        auto howTo = juce::Rectangle<int> (publicKeyEditor.getX(), copyKeyButton.getBottom() + 18, 600, 160);
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (13.0f));
        g.drawFittedText ("One-time setup:\n"
                          "1. Copy the public key.\n"
                          "2. On GitHub, open LT-AUDIO > shared/license_public_key.txt, click the pencil, paste it and commit.\n"
                          "3. GitHub rebuilds the plugins with your key. Install that new version.\n\n"
                          "Export a backup of your signing key and keep it private. Without it you can't create new licenses.",
                          howTo, juce::Justification::topLeft, 8);
    }
    else
    {
        g.setColour (p.text);
        g.setFont (Fonts::light (40.0f));
        g.drawText ("CUSTOMERS", title, juce::Justification::centredLeft);
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (13.0f));
        g.drawText (juce::String (customerModel->customers.size()) + juce::String (juce::CharPointer_UTF8 (" friends  \xc2\xb7  click one to see their licenses")),
                    subtitle, juce::Justification::centredLeft);
    }

    if (status.isNotEmpty())
    {
        g.setColour (statusIsError ? p.danger : p.success);
        g.setFont (Fonts::regular (13.5f));
        auto area = view == View::newLicense ? generateButton.getBounds().translated (0, 52).withHeight (20)
                                             : centreArea.reduced (44, 0).removeFromBottom (40);
        g.drawText (status, area, juce::Justification::centredLeft);
    }
}

void MainComponent::saveScreenshot (const juce::File& f)
{
    auto img = createComponentSnapshot (getLocalBounds(), true, 1.0f);
    juce::PNGImageFormat png;
    f.deleteFile();
    juce::FileOutputStream out (f);
    png.writeImageToStream (img, out);
}

} // namespace studio
