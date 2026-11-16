#pragma once

#include "StudioData.h"

namespace studio
{

/** Sidebar entry: product (icon + name) or a section (CUSTOMERS / SETTINGS). */
class SidebarItem : public juce::Button
{
public:
    SidebarItem (juce::String id, juce::String label, bool isProduct, const lt::ui::Palette&);
    void paintButton (juce::Graphics&, bool hover, bool down) override;
    const juce::String id;

private:
    juce::String label;
    bool isProduct;
    const lt::ui::Palette& palette;
};

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Renders the window into a PNG (used for previews / tests). */
    void saveScreenshot (const juce::File&);

private:
    enum class View { newLicense, customers, settings };

    void showView (View);
    void selectProduct (const juce::String& id);
    void generate();
    void showResult (const LicenseRecord&, bool justCreated);
    void saveLicenseFile();
    void refreshLists();
    void layoutNewLicense (juce::Rectangle<int>);
    void layoutSettings (juce::Rectangle<int>);
    void layoutCustomers (juce::Rectangle<int>);
    void setStatus (const juce::String&, bool error);

    struct RecentModel;
    struct CustomerModel;

    /** Draws headers, labels and glass columns above the nebula but below the controls. */
    struct Overlay : public juce::Component
    {
        explicit Overlay (MainComponent& o) : owner (o) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override { owner.paintOverlay (g); }
        MainComponent& owner;
    };

    void paintOverlay (juce::Graphics&);

    lt::ui::Palette palette = lt::ui::Palette::dark();
    lt::ui::GlassLookAndFeel lnf;
    lt::ui::NebulaBackground background { 11 };
    Overlay overlay { *this };

    SigningKey key;
    LicenseStore store;

    juce::OwnedArray<SidebarItem> productItems;
    std::unique_ptr<SidebarItem> customersItem, settingsItem;
    View view = View::newLicense;
    juce::String selectedProduct = lt::licensing::Products::voidVerb;

    // new license
    juce::TextEditor nameEditor, emailEditor, machineEditor, resultEditor;
    juce::TextButton pasteMachineButton { "PASTE" }, generateButton { "GENERATE LICENSE" },
                     copyButton { "COPY" }, saveButton { "SAVE .lic FILE" };
    std::optional<LicenseRecord> current;
    bool currentVerified = false;
    juce::String status;
    bool statusIsError = false;

    // recent licenses
    juce::TextEditor searchEditor;
    juce::ListBox recentList;
    std::unique_ptr<RecentModel> recentModel;
    juce::TextButton removeButton { "REMOVE" };

    // customers
    juce::ListBox customerList;
    std::unique_ptr<CustomerModel> customerModel;

    // settings
    juce::TextEditor publicKeyEditor;
    juce::TextButton copyKeyButton { "COPY PUBLIC KEY" }, exportKeyButton { "EXPORT KEY BACKUP" },
                     importKeyButton { "IMPORT KEY BACKUP" }, openFolderButton { "OPEN DATA FOLDER" };

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> sidebarArea, centreArea, rightArea, resultBox, computersRow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace studio
