#include "PluginEditor.h"
#include "MidiExport.h"

namespace typebeats
{

using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Logo;

bool Editor::activationShownThisSession = false;

Editor::Editor (Processor& p)
    : AudioProcessorEditor (p), proc (p), downloader (p.formats)
{
    palette = proc.lightTheme ? Palette::light() : Palette::dark();
    setLookAndFeel (&lnf);

    addAndMakeVisible (content);
    content.addAndMakeVisible (background);
    content.addAndMakeVisible (decor);

    for (auto* v : std::initializer_list<ThemedView*> { &waveform, &bpmView, &keyView, &piano, &chordView, &dragHandle })
    {
        v->palette = &palette;
        content.addAndMakeVisible (v);
    }

    // header
    themeButton.onClick = [this]
    {
        proc.lightTheme = ! proc.lightTheme;
        palette = proc.lightTheme ? Palette::light() : Palette::dark();
        applyTheme();
    };
    settingsButton.onClick = [this] { showSettingsMenu(); };
    licenseButton.onClick = [this] { showActivation (true); };
    for (auto* b : { &themeButton, &settingsButton, &licenseButton })
        content.addAndMakeVisible (b);

    // YouTube bar
    linkEditor.setFont (Fonts::regular (15.0f));
    linkEditor.setBorder ({ 0, 0, 0, 0 });
    linkEditor.setIndents (44, 0);
    linkEditor.setJustification (juce::Justification::centredLeft);
    linkEditor.onReturnKey = [this] { startDownload(); };
    content.addAndMakeVisible (linkEditor);

    getButton.getProperties().set ("accent", true);
    getButton.onClick = [this] { startDownload(); };
    content.addAndMakeVisible (getButton);

    downloader.onProgress = [this] (const juce::String& s, float pr)
    {
        downloadStatus = s;
        downloadProgress = pr;
        if (pr >= 1.0f && ! downloader.isBusy())
        {
            transientMessage = s;
            transientUntil = juce::Time::getMillisecondCounter() + 4000;
        }
    };
    downloader.onFinished = [this] (const juce::File& wav)
    {
        downloading = false;
        linkEditor.clear();
        proc.loadFile (wav);
    };
    downloader.onError = [this] (const juce::String& err)
    {
        downloading = false;
        transientMessage = err;
        transientUntil = juce::Time::getMillisecondCounter() + 8000;
        getButton.setEnabled (true);
    };

    // waveform
    waveform.onBrowse = [this] { browse(); };
    waveform.onClick = [this] (double t) { proc.togglePreview (t); };
    waveform.onDragOut = [this]
    {
        if (proc.getLoadedFile().existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ proc.getLoadedFile().getFullPathName() }, false, &waveform);
    };

    bpmView.onBpmChanged = [this] (double bpm) { proc.getEditableResult().bpm = bpm; };
    keyView.onChooseAlternative = [this] (int index)
    {
        auto& keys = proc.getEditableResult().keys;
        if (index > 0 && index < (int) keys.size())
        {
            std::swap (keys[0].tonic, keys[(size_t) index].tonic);
            std::swap (keys[0].minor, keys[(size_t) index].minor);
            refreshFromResult();
        }
    };
    chordView.onChordEdited = [this] { updateSelectionNotes(); };
    chordView.onSelect = [this] (int) { updateSelectionNotes(); };

    exportButton.getProperties().set ("accent", true);
    exportButton.onClick = [this] { exportMidi(); };
    content.addAndMakeVisible (exportButton);
    dragHandle.createFile = [this] { return createTempMidi(); };

    proc.onResultChanged = [this] { refreshFromResult(); };

    applyTheme();
    refreshFromResult();

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

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png [LT_LOAD_FILE=song.wav] [LT_LIGHT=1]
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
        if (juce::SystemStats::getEnvironmentVariable ("LT_LIGHT", {}).isNotEmpty()) themeButton.triggerClick();
        auto song = juce::SystemStats::getEnvironmentVariable ("LT_LOAD_FILE", {});
        if (song.isNotEmpty()) proc.loadFile (juce::File (song));
        juce::Timer::callAfterDelay (4000, [this, snapshot]
        {
            if (juce::SystemStats::getEnvironmentVariable ("LT_SELECT_CHORD", {}).isNotEmpty())
            {
                auto stars = chordView.getBounds().getCentre();
                juce::ignoreUnused (stars);
            }
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
    proc.onResultChanged = nullptr;
    setLookAndFeel (nullptr);
}

void Editor::applyTheme()
{
    lnf.setPalette (palette);
    background.setPalette (palette);
    linkEditor.setTextToShowWhenEmpty ("Paste YouTube link...", palette.textDim);
    linkEditor.applyColourToAllText (lnf.findColour (juce::TextEditor::textColourId));
    if (activation) activation->setPalette (palette);
    content.repaint();
    for (auto* c : content.getChildren())
        c->repaint();
}

void Editor::refreshFromResult()
{
    auto& r = proc.getEditableResult();
    waveform.setResult (r.isValid() ? &r : nullptr, proc.getLoadedFile().getFileName());
    bpmView.setBpm (r.bpm, false);
    keyView.setKeys (r.isValid() ? &r.keys : nullptr);
    chordView.setResult (r.isValid() ? &r : nullptr);
    updateSelectionNotes();
    exportButton.setEnabled (! r.chords.empty());
    content.repaint();
}

void Editor::updateSelectionNotes()
{
    auto& r = proc.getResult();
    auto idx = chordView.getSelected();
    if (idx < 0 && r.isValid())
    {
        auto t = proc.getPreviewPosition();
        for (int i = 0; i < (int) r.chords.size(); ++i)
            if (t >= r.chords[(size_t) i].start && t < r.chords[(size_t) i].end) idx = i;
        if (idx < 0 && ! r.chords.empty()) idx = 0;
    }

    if (idx >= 0 && idx < (int) r.chords.size())
        piano.setActiveNotes (r.chords[(size_t) idx].chord.pitchClasses(), r.chords[(size_t) idx].chord.root);
    else
        piano.setActiveNotes ({}, -1);
    chordView.repaint();
}

void Editor::timerCallback()
{
    auto playing = proc.isPreviewing();
    auto pos = proc.getPreviewPosition();
    waveform.setPlayhead (pos, playing);
    if (playing)
    {
        chordView.setPlayhead (pos);
        if (chordView.getSelected() < 0) updateSelectionNotes();
    }
    bpmView.setActive (playing || proc.getResult().bpm > 0);

    auto busy = proc.isAnalysing() || downloading;
    auto status = downloading ? downloadStatus : proc.getStatus();
    waveform.setBusy (busy, downloading ? downloadProgress : proc.getProgress(), status);
    getButton.setEnabled (! downloading);

    // the space reacts to the song while previewing
    auto& r = proc.getResult();
    auto energy = 0.0f;
    if (playing && ! r.peaks.empty() && r.duration > 0)
        energy = r.peaks[(size_t) juce::jlimit (0, (int) r.peaks.size() - 1, (int) (pos / r.duration * (double) r.peaks.size()))];
    background.setEnergy (energy);

    // trial chip
    auto chipText = proc.license.isLicensed() ? juce::String() : proc.license.isUsable()
                      ? juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (proc.license.getTrialDaysLeft()) + " DAYS" : juce::String ("ACTIVATE");
    if (licenseButton.getButtonText() != chipText) licenseButton.setButtonText (chipText);
    licenseButton.setVisible (chipText.isNotEmpty());

    if (transientUntil != 0 && juce::Time::getMillisecondCounter() > transientUntil)
    {
        transientUntil = 0;
        transientMessage.clear();
        decor.repaint();
    }
    else if (transientUntil != 0)
        decor.repaint (linkBar.translated (0, linkBar.getHeight()).withHeight (20));
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
    background.setBounds (0, 0, baseWidth, baseHeight);
    decor.setBounds (0, 0, baseWidth, baseHeight);
    auto b = juce::Rectangle<int> (0, 0, baseWidth, baseHeight).reduced (30, 0);

    auto header = b.removeFromTop (92);
    themeButton.setBounds (header.getRight() - 104, 26, 104, 36);
    settingsButton.setBounds (header.getX(), 26, 110, 36);
    licenseButton.setBounds (settingsButton.getRight() + 10, 26, 150, 36);

    linkBar = b.removeFromTop (48);
    getButton.setBounds (linkBar.withLeft (linkBar.getRight() - 112).reduced (6));
    linkEditor.setBounds (linkBar.withTrimmedRight (116));
    b.removeFromTop (14);

    waveform.setBounds (b.removeFromTop (190));
    b.removeFromTop (16);

    auto bottom = b.withTrimmedBottom (20);
    bpmArea = bottom.removeFromLeft (310);
    chordArea = bottom.removeFromRight (360);
    keyArea = bottom.reduced (12, 0);

    bpmView.setBounds (bpmArea.reduced (14, 10));
    keyView.setBounds (keyArea.withHeight (keyArea.getHeight() - 92).reduced (14, 10));
    piano.setBounds (keyArea.withTop (keyArea.getBottom() - 92).reduced (22, 14));

    auto chordInner = chordArea.reduced (14, 10);
    auto exportRow = chordInner.removeFromBottom (54);
    chordView.setBounds (chordInner);
    dragHandle.setBounds (exportRow.removeFromRight (82).reduced (0, 6));
    exportButton.setBounds (exportRow.reduced (0, 4));

    if (activation)
        activation->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::Decor::paint (juce::Graphics& g)
{
    auto& e = owner;
    auto& p = e.palette;

    // title
    Logo::draw (g, { 0.0f, 20.0f, (float) baseWidth, 44.0f }, Logo::specFor ("TYPEBEATS"), p);
    g.setColour (p.isLight ? p.text : p.textDim);
    lt::ui::Fonts::drawSpaced (g, "AUDIO ANALYSIS", { 0.0f, 62.0f, (float) baseWidth, 18.0f }, Fonts::regular (12.0f), 0.15f,
                               juce::Justification::centred);

    // link bar
    Glass::drawPanel (g, e.linkBar.toFloat(), (float) e.linkBar.getHeight() * 0.5f, p, 1.4f, e.linkEditor.hasKeyboardFocus (true));
    auto icon = juce::Rectangle<float> ((float) e.linkBar.getX() + 16.0f, (float) e.linkBar.getCentreY() - 8.0f, 16.0f, 16.0f);
    g.setColour (p.isLight ? p.text : p.textDim);
    g.drawRoundedRectangle (icon.withWidth (10.0f).withHeight (6.0f).translated (0, 2.0f), 3.0f, 1.4f);
    g.drawRoundedRectangle (icon.withWidth (10.0f).withHeight (6.0f).translated (6.0f, 8.0f), 3.0f, 1.4f);
    g.drawLine ((float) e.linkBar.getX() + 44.0f - 6.0f, (float) e.linkBar.getY() + 12.0f,
                (float) e.linkBar.getX() + 44.0f - 6.0f, (float) e.linkBar.getBottom() - 12.0f, 1.0f);

    if (e.transientMessage.isNotEmpty())
    {
        g.setColour (p.isLight ? p.text : p.textDim);
        g.setFont (Fonts::regular (12.0f));
        g.drawText (e.transientMessage, e.linkBar.translated (20, e.linkBar.getHeight()).withHeight (16), juce::Justification::centredLeft);
    }

    // thin dividers between the bottom sections
    g.setColour (p.glassEdge.withMultipliedAlpha (0.7f));
    g.drawVerticalLine (e.bpmArea.getRight() + 6, (float) e.bpmArea.getY() + 10.0f, (float) e.bpmArea.getBottom() - 10.0f);
    g.drawVerticalLine (e.chordArea.getX() - 6, (float) e.chordArea.getY() + 10.0f, (float) e.chordArea.getBottom() - 10.0f);
}

// ------------------------------------------------------------------ actions

bool Editor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (proc.formats.findFormatForFileExtension (juce::File (f).getFileExtension()) != nullptr)
            return true;
    return false;
}

void Editor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (proc.formats.findFormatForFileExtension (juce::File (f).getFileExtension()) != nullptr)
        {
            proc.loadFile (juce::File (f));
            return;
        }
}

void Editor::browse()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a song", proc.getDownloadFolder(), proc.formats.getWildcardForAllFormats());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult().existsAsFile())
            proc.loadFile (fc.getResult());
    });
}

void Editor::startDownload()
{
    auto link = linkEditor.getText().trim();
    if (! Downloader::looksLikeYouTubeLink (link))
    {
        transientMessage = "Paste a YouTube link (youtube.com or youtu.be).";
        transientUntil = juce::Time::getMillisecondCounter() + 4000;
        content.repaint();
        return;
    }
    if (! proc.license.isUsable())
    {
        showActivation (true);
        return;
    }

    downloading = true;
    downloadStatus = "Starting...";
    downloadProgress = 0.0f;
    proc.stopPreview();
    downloader.download (link, proc.getDownloadFolder());
}

juce::File Editor::createTempMidi()
{
    auto& r = proc.getResult();
    if (r.chords.empty()) return {};
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LT Type Beats MIDI");
    folder.createDirectory();
    auto song = proc.getLoadedFile().getFileNameWithoutExtension();
    return MidiExport::write (r, song.isEmpty() ? juce::String ("LT TYPE BEATS") : song, folder);
}

void Editor::exportMidi()
{
    auto& r = proc.getResult();
    if (r.chords.empty()) return;

    auto song = proc.getLoadedFile().getFileNameWithoutExtension();
    auto suggested = proc.getDownloadFolder().getChildFile (juce::File::createLegalFileName (song + " chords") + ".mid");
    chooser = std::make_unique<juce::FileChooser> ("Save chords as MIDI", suggested, "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, song] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        auto written = MidiExport::write (proc.getResult(), song, f.withFileExtension ("mid"));
        transientMessage = written.existsAsFile() ? "Saved " + written.getFileName() : juce::String ("Could not save the MIDI file.");
        transientUntil = juce::Time::getMillisecondCounter() + 5000;
        content.repaint();
    });
}

void Editor::showSettingsMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    m.addSectionHeader ("Downloads: " + proc.getDownloadFolder().getFullPathName());
    m.addItem (1, "Change download folder...");
    m.addItem (2, "Open download folder");
    m.addItem (3, "Update YouTube downloader", ! downloader.isBusy());
    m.addSeparator();
    m.addItem (4, "License...");
    m.addItem (5, "Load a song...");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton), [this] (int r)
    {
        if (r == 1)
        {
            chooser = std::make_unique<juce::FileChooser> ("Download folder", proc.getDownloadFolder());
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [this] (const juce::FileChooser& fc) { if (fc.getResult().isDirectory()) proc.setDownloadFolder (fc.getResult()); });
        }
        else if (r == 2) { proc.getDownloadFolder().createDirectory(); proc.getDownloadFolder().startAsProcess(); }
        else if (r == 3) { downloadStatus = "Updating downloader..."; downloader.updateTool(); }
        else if (r == 4) showActivation (true);
        else if (r == 5) browse();
    });
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
        if (! proc.license.isUsable())
            return; // trial over and not licensed: keep it up
        content.removeChildComponent (activation.get());
        activation.reset();
    }
}

} // namespace typebeats
