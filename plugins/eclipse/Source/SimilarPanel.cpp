#include "SimilarPanel.h"

namespace eclipse
{

namespace Colours = juce::Colours;
using lt::ui::Fonts;

SimilarPanel::SimilarPanel (Processor& p)
    : proc (p), chooseKey (p, "CHOOSE FOLDER"), sameKey (p, "SAME TYPE", true), useKey (p, "USE ON PAD"), doneKey (p, "DONE"), againKey (p, "SEARCH")
{
    for (auto* k : { &chooseKey, &sameKey, &useKey, &doneKey, &againKey }) addAndMakeVisible (*k);
    sameKey.setClickingTogglesState (true);
    sameKey.setToggleState (true, juce::dontSendNotification);
    sameKey.onClick = [this] { search(); };
    againKey.onClick = [this] { search(); };
    doneKey.onClick = [this] { proc.stopPreview(); if (onDone) onDone(); };
    chooseKey.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Your sample library", proc.similar.folder());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& c)
        {
            if (c.getResult().isDirectory()) { proc.similar.setFolder (c.getResult()); results.clear(); repaint(); }
        });
    };
    useKey.onClick = [this]
    {
        if (! juce::isPositiveAndBelow (selected, (int) results.size())) return;
        proc.stopPreview();
        proc.loadSampleToPad (proc.selectedPad, results[(size_t) selected].file);
        if (onDone) onDone();
    };
    startTimerHz (10);
}

void SimilarPanel::resized()
{
    chooseKey.setBounds (getWidth() - 186 - 168, 16, 160, 33);
    doneKey.setBounds (getWidth() - 98, 16, 80, 33);
    sameKey.setBounds (getWidth() - 186, 16, 80, 33);
    againKey.setBounds (18, getHeight() - 52, 100, 35);
    useKey.setBounds (getWidth() - 168, getHeight() - 52, 150, 35);
    list = { 18, 96, (float) getWidth() - 36, (float) getHeight() - 160 };
}

void SimilarPanel::timerCallback()
{
    const bool sc = proc.similar.isScanning();
    if (sc || wasScanning) repaint();
    if (wasScanning && ! sc) search();          // the index is ready: show the results
    wasScanning = sc;
    if (forPad != proc.selectedPad && isVisible()) search();
}

void SimilarPanel::search()
{
    forPad = proc.selectedPad;
    results.clear(); selected = -1; scroll = 0;
    auto pd = proc.kit.pad (proc.selectedPad);
    if (! pd.empty() && proc.similar.size() > 0)
        results = proc.similar.find (pd.clean != nullptr ? *pd.clean : *pd.raw, pd.sampleRate, pd.type, sameKey.getToggleState(), 60);
    repaint();
}

void SimilarPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! list.contains (e.position)) return;
    const int r = (int) ((e.position.y - list.getY()) / rowH) + scroll;
    if (! juce::isPositiveAndBelow (r, (int) results.size())) return;
    selected = r;
    proc.previewFile (results[(size_t) r].file);
    repaint();
}

void SimilarPanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (list.contains (e.position) && juce::isPositiveAndBelow (selected, (int) results.size())) useKey.triggerClick();
}

void SimilarPanel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int visible = (int) (list.getHeight() / rowH);
    scroll = juce::jlimit (0, juce::jmax (0, (int) results.size() - visible), scroll + (w.deltaY < 0 ? 2 : -2));
    repaint();
}

void SimilarPanel::paint (juce::Graphics& g)
{
    const auto& s = proc.skin();
    look::panel (g, s, getLocalBounds().toFloat(), false);
    look::panelArt (g, s, getLocalBounds().toFloat(), getPosition());
    auto pd = proc.kit.pad (proc.selectedPad);
    look::label (g, s, "Find similar", { 22, 16, 150, 33 }, juce::Justification::centredLeft, 10, 2.0f);
    g.setColour (s.tx); g.setFont (Fonts::bold (15));
    g.drawText (pd.empty() ? juce::String ("Select a pad") : (pd.name.isEmpty() ? juce::String (familyLabel (pd.type)) : pd.name), 150, 16, 200, 33, juce::Justification::centredLeft, true);
    const auto dir = proc.similar.folder();
    g.setColour (s.dim); g.setFont (Fonts::regular (12));
    juce::String info = dir == juce::File() ? juce::String ("Choose the folder of your samples (kicks, snares, hats...). It is analysed once.")
                       : dir.getFullPathName() + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) + juce::String (proc.similar.size()) + " sounds";
    g.drawText (info, 22, 56, getWidth() - 44, 22, juce::Justification::centredLeft, true);
    if (proc.similar.isScanning())
    {
        auto bar = juce::Rectangle<float> (22, 80, (float) getWidth() - 44, 4);
        g.setColour (s.ringOff); g.fillRoundedRectangle (bar, 2);
        g.setColour (s.acc); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.02f, 1.0f, proc.similar.scanProgress())), 2);
    }
    look::lcd (g, s, list, false);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (list.reduced (2).toNearestInt());
        if (results.empty())
        {
            look::label (g, s, proc.similar.isScanning() ? "Analysing your library..." : dir == juce::File() ? "No library yet" : "Nothing found", list, juce::Justification::centred, 11, 2.0f);
        }
        const int visible = (int) (list.getHeight() / rowH) + 1;
        for (int i = 0; i < visible; ++i)
        {
            const int r = i + scroll;
            if (r >= (int) results.size()) break;
            const auto& m = results[(size_t) r];
            auto row = juce::Rectangle<float> (list.getX() + 4, list.getY() + 4 + (float) i * rowH, list.getWidth() - 8, rowH - 4);
            if (r == selected) { g.setColour (s.acc.withAlpha (0.35f)); g.fillRoundedRectangle (row, 4); }
            look::dot (g, s.family (m.family), { row.getX() + 12, row.getCentreY() }, 4, false);
            g.setColour (Colours::white); g.setFont (Fonts::semiBold (12.5f));
            g.drawText (m.file.getFileNameWithoutExtension(), row.withTrimmedLeft (26).withTrimmedRight (240).toNearestInt(), juce::Justification::centredLeft, true);
            g.setColour (Colours::white.withAlpha (0.45f)); g.setFont (Fonts::regular (11));
            g.drawText (m.file.getParentDirectory().getFileName(), row.withTrimmedLeft (row.getWidth() - 240).withTrimmedRight (70).toNearestInt(), juce::Justification::centredLeft, true);
            // match bar
            auto bar = juce::Rectangle<float> (row.getRight() - 64, row.getCentreY() - 3, 56, 6);
            g.setColour (Colours::white.withAlpha (0.12f)); g.fillRoundedRectangle (bar, 3);
            g.setColour (s.acc); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * m.score), 3);
        }
    }
    look::lcdGlass (g, list);
    g.setColour (s.dim); g.setFont (Fonts::regular (11));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("Click = listen  \xc2\xb7  double-click or USE ON PAD = replace the pad")), 130, getHeight() - 52, getWidth() - 320, 35, juce::Justification::centred, true);
}

} // namespace eclipse
