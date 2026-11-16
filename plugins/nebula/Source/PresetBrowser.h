#pragma once

#include "Banks.h"

namespace nebula
{

using lt::ui::Palette;

/** The preset browser (design/nebula/presets-*.webp): banks with covers, search, tags, favourites. */
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel
{
public:
    explicit PresetBrowser (BankLibrary&);

    const Palette* palette = nullptr;
    std::function<void (const lt::presets::ParamPreset&, const juce::String& bank)> onLoad;
    std::function<void()> onClose;
    std::function<juce::String()> getCurrentPreset;

    void show (const juce::String& currentBank);
    int getSelectedBank() const noexcept { return bankIndex; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    // ListBoxModel
    int getNumRows() override { return (int) visible.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    void refilter();
    void selectBank (int);
    void newBank();
    void importBank();
    void exportBank();
    juce::Rectangle<float> bankRow (int i) const;
    juce::Rectangle<float> starArea (int w, int h) const { return { (float) w - 34.0f, 0.0f, 30.0f, (float) h }; }

    BankLibrary& lib;
    int bankIndex = 0;
    juce::String tag { "ALL" };
    std::vector<int> visible;           // indices into the bank's presets
    juce::TextEditor search;
    juce::ListBox list { "presets", this };
    juce::OwnedArray<juce::TextButton> tagButtons;
    juce::TextButton closeButton, newButton { "+ NEW BANK" }, importButton { "IMPORT" }, exportButton { "EXPORT" },
                     coverButton { "+ CHANGE IMAGE" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> dialog;
    juce::Rectangle<float> panel, coverArea;
    int bankScroll = 0;
};

} // namespace nebula
