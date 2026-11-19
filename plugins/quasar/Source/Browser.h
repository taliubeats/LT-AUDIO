#pragma once

#include "Pages.h"

namespace quasar::ui
{

/** Procedural cover art for a bank (space art, tinted by the bank colour). */
juce::Image makeCover (const juce::String& name, juce::Colour, int size);

/**
    The preset browser: sections (all / favourites / recent / user), banks, search,
    category and style filters, favourites, save / new bank / import / export.
*/
class BrowserPage : public Page
{
public:
    explicit BrowserPage (Processor&);
    ~BrowserPage() override;
    void paintPanels (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void refresh();
    void savePresetDialog();
    /** Next / previous preset in the current list (header arrows). */
    void step (int delta);

private:
    void timerCallback() override {}
    void filter();
    juce::Rectangle<int> sectionRow (int i) const { return { 28, 146 + i * 34, 210, 30 }; }
    juce::Rectangle<int> bankRow (int i) const { return { 28, 322 + i * 52, 210, 46 }; }
    juce::Rectangle<int> listRow (int i) const { return { 274, 318 + i * 50, 650, 44 }; }
    juce::Rectangle<int> catPill (int i) const;
    juce::Rectangle<int> stylePill (int i) const;
    void load (const Preset&);
    const juce::Image& coverFor (const Bank&);

    juce::TextEditor search;
    juce::TextButton importButton { "IMPORT BANK" }, exportBankButton { "EXPORT BANK" },
                     saveButton { "SAVE PRESET" }, newBankButton { "NEW BANK" }, exportButton { "EXPORT" };
    int section = 0;                     // 0 all 1 favourites 2 recent 3 user, 4 = a bank
    juce::String bankFilter, category, style;
    std::vector<const Preset*> list;
    int scroll = 0, bankScroll = 0;
    std::map<juce::String, juce::Image> covers;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace quasar::ui
