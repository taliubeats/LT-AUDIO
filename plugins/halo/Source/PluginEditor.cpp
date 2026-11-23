#include "PluginEditor.h"
#include "Lyrics.h"
#include "AIHelper.h"

namespace halo
{

using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Logo;

bool Editor::activationShownThisSession = false;

namespace
{
    const juce::Colour white = juce::Colours::white, green (0xff34d399), amber (0xffffd60a);

    juce::String pct (double v)    { return juce::String (juce::roundToInt (v)) + "%"; }
    juce::String dbText (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String panText (double v)
    {
        auto r = juce::roundToInt (v);
        return r == 0 ? juce::String ("C") : (r < 0 ? "L" + juce::String (-r) : "R" + juce::String (r));
    }
}

Editor::Editor (Processor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    lnf.setPalette (palette);
    addAndMakeVisible (content);
    content.addAndMakeVisible (background);
    background.setPalette (palette);
    content.addAndMakeVisible (decor);
    auto& s = proc.state;

    auto attachSlider = [&] (juce::Slider& sl, const juce::String& id)
    {
        sliderAttachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*s.getParameter (id), sl));
    };
    auto attachCombo = [&] (juce::ComboBox& cb, const juce::String& id)
    {
        comboAttachments.push_back (std::make_unique<juce::ComboBoxParameterAttachment> (*s.getParameter (id), cb));
    };
    auto attachButton = [&] (juce::Button& b, const juce::String& id)
    {
        buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*s.getParameter (id), b));
    };

    // ---------------------------------------------------------------- top bar
    tabs.palette = &palette;
    tabs.onChange = [this] (int t) { setTab (t); };
    content.addAndMakeVisible (tabs);

    for (auto* b : { &keyBox, &presetBox }) { b->palette = &palette; content.addAndMakeVisible (b); }
    keyBox.onClick = [this] { showKeyMenu(); };
    keyBox.setTooltip ("Tonalidad usada para componer las armonias");
    presetBox.onClick = [this] { showPresetMenu(); };
    detectButton.onClick = [this]
    {
        setParam (proc.state, IDs::keyAuto, 1.0f);
        proc.session.setKey (proc.session.getDetectedKey());
    };
    detectButton.setTooltip ("Usar la tonalidad detectada en la voz");
    saveButton.onClick = [this] { savePreset(); };
    licenseButton.onClick = [this] { showActivation (true); };
    for (auto* b : { &detectButton, &saveButton, &licenseButton }) content.addAndMakeVisible (b);
    detectButton.getProperties().set ("flat", true);

    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    undoButton.onClick = [this] { proc.undo.undo(); };
    redoButton.onClick = [this] { proc.undo.redo(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };
    powerButton.onClick = [this] { setParam (proc.state, IDs::bypass, getParam (proc.state, IDs::bypass) > 0.5f ? 0.0f : 1.0f); };
    for (auto* b : { &prevPreset, &nextPreset, &undoButton, &redoButton, &settingsButton, &powerButton })
    {
        b->palette = &palette;
        content.addAndMakeVisible (b);
    }

    // ---------------------------------------------------------------- vocal
    for (auto* b : { &browseButton, &browseButton2 }) { b->onClick = [this] { browse(); }; content.addAndMakeVisible (b); }
    for (auto* b : { &useTrackButton, &useTrackButton2 })
    {
        b->onClick = [this] { toggleCapture(); };
        b->setTooltip ("Graba lo que suena en esta pista (dale play en el DAW) y usalo como voz");
        content.addAndMakeVisible (b);
    }
    for (auto* w : { &wave, &choirWave })
    {
        w->palette = &palette;
        w->onWordClicked = [this] (int i) { proc.session.toggleWordReinforce (i); };
        w->onSeek = [this] (double t) { proc.setPlayPositionSeconds (t); };
        content.addAndMakeVisible (w);
    }
    choirWave.vertical = true;

    // ---------------------------------------------------------------- HARMONY
    orbits.palette = &palette;
    orbits.onMoonClicked = [this] (int m)
    {
        proc.soloStrip = (m >= 0 && proc.soloStrip.load() != m) ? m : -1;
        orbits.soloMoon = proc.soloStrip.load();
        if (proc.soloStrip.load() >= 0 && ! proc.isPlaying()) proc.play();
    };
    content.addAndMakeVisible (orbits);

    auto parts = partNames(), styles = styleNames(), voices = voiceNames();
    for (int i = 0; i < numStrips; ++i)
    {
        auto& st = strips[(size_t) i];
        st.index = i;
        st.palette = &palette;
        for (auto* k : { &st.level, &st.pan, &st.delay, &st.humanize }) k->palette = &palette;
        st.smart.palette = &palette;
        st.regen.palette = &palette;
        st.power.palette = &palette;
        st.drag.palette = &palette;

        // part menu: harmonies, then layers
        st.part.addSectionHeading ("HARMONIES");
        for (int k = 0; k < (int) Part::count; ++k)
        {
            if (k == (int) Part::doubleTake) st.part.addSectionHeading ("LAYERS");
            st.part.addItem (parts[k], k + 1);
        }
        st.part.onChange = [this, i]
        {
            if (syncingCombos) return;
            auto id = strips[(size_t) i].part.getSelectedId();
            if (id <= 0) return;
            proc.undo.beginNewTransaction();
            setParam (proc.state, IDs::strip (i, IDs::part), (float) (id - 1));
            applyPartDefaults (proc.state, i, (Part) (id - 1));
            setParam (proc.state, IDs::strip (i, IDs::on), 1.0f);
        };
        st.style.addItemList (styles, 1);
        st.voice.addItemList (voices, 1);
        attachCombo (st.style, IDs::strip (i, IDs::style));
        attachCombo (st.voice, IDs::strip (i, IDs::voice));
        attachButton (st.smart, IDs::strip (i, IDs::smart));
        st.smart.setTooltip ("SMART: sigue la tonalidad y el acorde de cada momento. FIXED: intervalo exacto");
        attachSlider (st.level, IDs::strip (i, IDs::level));
        attachSlider (st.pan, IDs::strip (i, IDs::pan));
        attachSlider (st.delay, IDs::strip (i, IDs::delay));
        attachSlider (st.humanize, IDs::strip (i, IDs::humanize));
        st.level.formatValue = dbText;
        st.pan.formatValue = panText;
        st.delay.formatValue = [] (double v) { return juce::String (v, 0) + " ms"; };
        st.humanize.formatValue = pct;
        attachButton (st.mute, IDs::strip (i, IDs::mute));
        attachButton (st.solo, IDs::strip (i, IDs::solo));
        st.power.onClick = [this, i]
        {
            setParam (proc.state, IDs::strip (i, IDs::on), proc.stripEnabled (i) ? 0.0f : 1.0f);
        };
        st.regen.onClick = [this, i] { proc.generateHarmony (i, true); };
        st.drag.makeFile = [this, i] { return dragStrip (i); };
        st.drag.onClickNoDrag = [this, i]
        {
            auto f = dragStrip (i);
            if (f.existsAsFile()) f.revealToUser();
        };
        content.addAndMakeVisible (st);
    }

    std::pair<PlanetKnob*, const char*> planets[] = { { &dry, IDs::dry }, { &wet, IDs::wet }, { &width, IDs::width },
                                                      { &tightness, IDs::tightness }, { &spread, IDs::cSpread },
                                                      { &loose, IDs::cLoose }, { &cWidth, IDs::cWidth }, { &room, IDs::cRoom },
                                                      { &breath, IDs::cBreath } };
    for (auto& [k, id] : planets)
    {
        k->palette = &palette;
        k->formatValue = pct;
        attachSlider (*k, id);
        content.addAndMakeVisible (k);
    }
    dry.setTooltip ("Volumen de tu voz original en la escucha");
    wet.setTooltip ("Volumen de las voces generadas");
    width.setTooltip ("Apertura estereo de las voces");
    tightness.setTooltip ("0 = sueltas como humanos, 100 = pegadas a tu voz (se aplica al generar)");

    quality.palette = &palette;
    quality.pillStyle = false;
    quality.isEnabledSide = [] (int side) { return side == 0 || AIHelper::available(); };
    quality.setTooltip ("FAST: segundos. BEST: la IA vuelve a cantar cada voz con tu timbre (minutos; requiere el ayudante de IA)");
    qualityAttachment = std::make_unique<juce::ParameterAttachment> (*s.getParameter (IDs::quality),
                                                                     [this] (float v) { quality.setSelected (v > 0.5f ? 1 : 0); });
    quality.onChange = [this] (int side) { qualityAttachment->setValueAsCompleteGesture ((float) side); };
    qualityAttachment->sendInitialUpdate();
    content.addAndMakeVisible (quality);

    inMeter.palette = outMeter.palette = &palette;
    content.addAndMakeVisible (inMeter);
    content.addAndMakeVisible (outMeter);

    for (auto* b : { &playButton, &stopButton, &playButton2, &stopButton2 }) { b->palette = &palette; content.addAndMakeVisible (b); }
    playButton.onClick = playButton2.onClick = [this] { if (proc.isPlaying()) proc.stop(); else proc.play(); };
    stopButton.onClick = stopButton2.onClick = [this] { proc.stop(); proc.setPlayPositionSeconds (0.0); proc.soloStrip = -1; orbits.soloMoon = -1; };

    for (auto* sw : { &leadSwitch, &leadSwitch2 }) { sw->palette = &palette; attachButton (*sw, IDs::withLead); content.addAndMakeVisible (sw); }
    for (auto* l : { &leadLabel, &leadLabel2 })
    {
        l->palette = &palette;
        l->pillStyle = false;
        l->onChange = [this] (int side) { setParam (proc.state, IDs::withLead, (float) side); };
        content.addAndMakeVisible (l);
    }

    for (auto* g : { &generate, &generate2 }) { g->palette = &palette; content.addAndMakeVisible (g); }
    generate.onClick = generate2.onClick = [this] { onGenerate(); };

    for (auto* d : { &dragAll, &dragChoir }) { d->palette = &palette; content.addAndMakeVisible (d); }
    dragAll.makeFile = [this] { return dragMix(); };
    dragAll.onClickNoDrag = [this] { auto f = dragAll.makeFile(); if (f.existsAsFile()) f.revealToUser(); };
    dragAll.setTooltip ("Arrastra todas las voces mezcladas al DAW (clic = exportar)");
    dragChoir.makeFile = [this] { return dragChoirFile(); };
    dragChoir.onClickNoDrag = [this] { auto f = dragChoir.makeFile(); if (f.existsAsFile()) f.revealToUser(); };
    for (auto* b : { &exportButton, &exportButton2 }) { b->onClick = [this] { exportAll(); }; content.addAndMakeVisible (b); }

    // ---------------------------------------------------------------- CHOIR
    voicesSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    voicesSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    attachSlider (voicesSlider, IDs::cVoices);
    content.addAndMakeVisible (voicesSlider);
    cluster.palette = &palette;
    content.addAndMakeVisible (cluster);
    typeCards.palette = &palette;
    cTypeAttachment = std::make_unique<juce::ParameterAttachment> (*s.getParameter (IDs::cType),
                                                                   [this] (float v) { typeCards.selected = juce::roundToInt (v); typeCards.repaint(); });
    typeCards.onSelect = [this] (int t)
    {
        proc.undo.beginNewTransaction();
        cTypeAttachment->setValueAsCompleteGesture ((float) t);
        applyChoirDefaults (proc.state, (ChoirType) t);
    };
    cTypeAttachment->sendInitialUpdate();
    content.addAndMakeVisible (typeCards);
    for (int i = 0; i < (int) ChoirHarmony::count; ++i) choirHarmony.addItem (juce::String ("HARMONY: ") + choirHarmonyName ((ChoirHarmony) i), i + 1);
    attachCombo (choirHarmony, IDs::cHarmony);
    content.addAndMakeVisible (choirHarmony);
    std::pair<SectionSlider*, const char*> secs[] = { { &sop, IDs::cSop }, { &alto, IDs::cAlto }, { &tenor, IDs::cTenor }, { &bass, IDs::cBass } };
    for (auto& [sl, id] : secs) { sl->palette = &palette; attachSlider (*sl, id); content.addAndMakeVisible (sl); }
    attachButton (separateToggle, IDs::cSeparate);
    content.addAndMakeVisible (separateToggle);

    harmonyOnly = { &wave, &browseButton, &useTrackButton, &orbits, &dry, &wet, &width, &tightness, &quality, &inMeter, &outMeter,
                    &playButton, &stopButton, &leadSwitch, &leadLabel, &generate, &dragAll, &exportButton };
    for (auto& st : strips) harmonyOnly.push_back (&st);
    choirOnly = { &choirWave, &browseButton2, &useTrackButton2, &voicesSlider, &cluster, &typeCards, &choirHarmony, &sop, &alto,
                  &tenor, &bass, &spread, &loose, &cWidth, &room, &breath, &generate2, &playButton2, &stopButton2, &leadSwitch2,
                  &leadLabel2, &dragChoir, &exportButton2, &separateToggle };

    for (juce::Component* c : { (juce::Component*) &decor, (juce::Component*) &dry, (juce::Component*) &wet, (juce::Component*) &width,
                                (juce::Component*) &tightness, (juce::Component*) &spread, (juce::Component*) &loose,
                                (juce::Component*) &cWidth, (juce::Component*) &room, (juce::Component*) &breath,
                                (juce::Component*) &typeCards, (juce::Component*) &sop, (juce::Component*) &alto,
                                (juce::Component*) &tenor, (juce::Component*) &bass, (juce::Component*) &wave, (juce::Component*) &choirWave })
        c->setBufferedToImage (true);
    for (auto& st : strips) st.setBufferedToImage (true);

    proc.session.addChangeListener (this);
    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->setTab (safe->proc.tab.load()); safe->refreshSessionViews(); } });
    };

    setTab (proc.tab.load());
    refreshSessionViews();
    syncStripCombos();

    setResizable (true, true);
    setResizeLimits (baseWidth * 6 / 10, baseHeight * 6 / 10, baseWidth * 3 / 2, baseHeight * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (baseWidth * 8 / 10, baseHeight * 8 / 10);

    if (! proc.license.isLicensed() && (! activationShownThisSession || ! proc.license.isUsable()))
    {
        activationShownThisSession = true;
        showActivation (true);
    }
    startTimerHz (30);

    // Developer: LT_ICON_SNAPSHOT=/icon.png renders the app icon (shared/brand/icon-halo.png) and quits
    auto iconFile = juce::SystemStats::getEnvironmentVariable ("LT_ICON_SNAPSHOT", {});
    if (iconFile.isNotEmpty())
    {
        juce::Image icon (juce::Image::ARGB, 512, 512, true);
        {
            juce::Graphics g (icon);
            Logo::drawIcon (g, { 8.0f, 8.0f, 496.0f, 496.0f }, "HALO", palette, false);
        }
        juce::File (iconFile).deleteFile();
        juce::FileOutputStream out { juce::File (iconFile) };
        juce::PNGImageFormat().writeImageToStream (icon, out);
        out.flush();
        juce::MessageManager::callAsync ([] { juce::JUCEApplicationBase::quit(); });
    }

    // Developer preview: LT_EDITOR_SNAPSHOT=/x.png [LT_HALO_FILE=/vocal.wav] [LT_HALO_TAB=1] [LT_HALO_GENERATE=1]
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
        setSize (baseWidth, baseHeight);
        auto file = juce::SystemStats::getEnvironmentVariable ("LT_HALO_FILE", {});
        if (juce::SystemStats::getEnvironmentVariable ("LT_HALO_TAB", {}) == "1") setTab (1);
        if (file.isNotEmpty()) proc.session.loadFile (juce::File (file));
        bool gen = juce::SystemStats::getEnvironmentVariable ("LT_HALO_GENERATE", {}).isNotEmpty();
        auto delay = file.isEmpty() ? 2500 : 45000;
        if (gen)
            juce::Timer::callAfterDelay (25000, [this] { onGenerate(); });
        juce::Timer::callAfterDelay (delay + (gen ? 40000 : 0), [this, snapshot]
        {
            auto exportDir = juce::SystemStats::getEnvironmentVariable ("LT_HALO_EXPORT", {});
            if (exportDir.isNotEmpty())
            {
                juce::File dir (exportDir);
                std::array<Session::StripGains, numStrips> gains;
                for (int i = 0; i < numStrips; ++i)
                {
                    gains[(size_t) i] = proc.stripGains (i);
                    if (gains[(size_t) i].audible) proc.session.writeStrip (i, gains[(size_t) i], dir);
                }
                proc.session.writeMix (gains, dir);
                proc.session.writeChoir (1.0f, true, dir);
            }
            if (juce::SystemStats::getEnvironmentVariable ("LT_HALO_DRAGTEST", {}).isNotEmpty())
            {
                // the Ableton report: a muted voice + solo on a voice that is not sung, then drag twice
                setParam (proc.state, IDs::strip (0, IDs::mute), 1.0f);
                setParam (proc.state, IDs::strip (numStrips - 1, IDs::solo), 1.0f);
                for (int i = 0; i < numStrips; ++i) dragStrip (i);
                dragStrip (0);
                dragMix();
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
    proc.session.removeChangeListener (this);
    proc.onStateRestored = nullptr;
    download.reset();
    setLookAndFeel (nullptr);
}

// ------------------------------------------------------------------ tabs / layout

void Editor::setTab (int t)
{
    proc.tab = t;
    tabs.setSelected (t);
    for (auto* c : harmonyOnly) c->setVisible (t == 0);
    for (auto* c : choirOnly) c->setVisible (t == 1);
    decor.repaint();
}

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

    // top bar
    tabs.setBounds (300, 40, 222, 46);
    keyBox.setBounds (534, 38, 150, 50);
    detectButton.setBounds (618, 52, 60, 24);
    prevPreset.setBounds (716, 51, 24, 26);
    presetBox.setBounds (740, 38, 112, 50);
    nextPreset.setBounds (852, 51, 24, 26);
    saveButton.setBounds (876, 49, 52, 28);
    undoButton.setBounds (948, 41, 44, 44);
    redoButton.setBounds (998, 41, 44, 44);
    settingsButton.setBounds (1048, 41, 44, 44);
    powerButton.setBounds (1108, 41, 44, 44);
    licenseButton.setBounds (1036, 8, 124, 24);

    // HARMONY
    browseButton.setBounds (228, 124, 84, 30);
    useTrackButton.setBounds (318, 124, 96, 30);
    wave.setBounds (440, 134, 702, 72);
    orbits.setBounds (52, 228, 883, 296);
    for (int i = 0; i < numStrips; ++i)
        strips[(size_t) i].setBounds (47 + (int) std::lround (i * 112.4), 543, 106, 262);
    dry.setBounds (963, 228, 182, 124);
    wet.setBounds (963, 352, 182, 124);
    width.setBounds (963, 476, 182, 124);
    tightness.setBounds (963, 600, 182, 124);
    quality.setBounds (1040, 734, 110, 26);
    inMeter.setBounds (1003, 774, 136, 8);
    outMeter.setBounds (1003, 788, 136, 8);
    playButton.setBounds (72, 837, 34, 34);
    stopButton.setBounds (118, 837, 34, 34);
    leadSwitch.setBounds (198, 843, 40, 22);
    leadLabel.setBounds (240, 836, 240, 36);
    generate.setBounds (522, 824, 150, 60);
    dragAll.setBounds (930, 834, 116, 42);
    exportButton.setBounds (1056, 836, 90, 38);

    // CHOIR
    browseButton2.setBounds (58, 688, 135, 34);
    useTrackButton2.setBounds (206, 688, 140, 34);
    choirWave.setBounds (46, 300, 310, 300);
    voicesSlider.setBounds (452, 232, 300, 24);
    cluster.setBounds (392, 268, 418, 318);
    typeCards.setBounds (390, 628, 422, 96);
    choirHarmony.setBounds (870, 172, 260, 40);
    const int sx[] = { 858, 930, 1002, 1074 };
    SectionSlider* ss[] = { &sop, &alto, &tenor, &bass };
    for (int k = 0; k < 4; ++k) ss[k]->setBounds (sx[k], 228, 66, 270);
    spread.setBounds (850, 512, 100, 104);
    loose.setBounds (950, 512, 100, 104);
    cWidth.setBounds (1050, 512, 100, 104);
    room.setBounds (850, 620, 100, 104);
    breath.setBounds (950, 620, 100, 104);
    generate2.setBounds (54, 770, 200, 72);
    playButton2.setBounds (628, 776, 34, 34);
    stopButton2.setBounds (672, 776, 34, 34);
    leadSwitch2.setBounds (718, 782, 40, 22);
    leadLabel2.setBounds (604, 816, 230, 28);
    dragChoir.setBounds (850, 780, 112, 50);
    exportButton2.setBounds (986, 774, 160, 36);
    separateToggle.setBounds (994, 816, 160, 26);

    if (activation) activation->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::Decor::paint (juce::Graphics& g)
{
    auto& e = owner;
    auto& p = e.palette;
    auto tab = e.proc.tab.load();
    auto& session = e.proc.session;

    Logo::draw (g, { 40.0f, 30.0f, 232.0f, 58.0f }, Logo::specFor ("HALO"), p, juce::Justification::centredLeft);
    Glass::drawPanel (g, { 712.0f, 38.0f, 220.0f, 50.0f }, 9.0f, p, 0.9f);
    Glass::drawPanel (g, { 942.0f, 38.0f, 216.0f, 50.0f }, 9.0f, p, 0.9f);

    auto statusText = [&]
    {
        auto st = session.getStatus();
        if (session.hasVocal() && ! session.isBusy())
        {
            auto bpm = e.proc.getHostBpm() > 0 ? e.proc.getHostBpm() : session.getBpm();
            st = "Voice learned  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  " + e.keyText()
               + (bpm > 0 ? "  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  " + juce::String (juce::roundToInt (bpm)) + " BPM" : juce::String());
        }
        if (e.proc.isCapturing())
            st = "Recording the track... " + juce::String (e.proc.getCaptureSeconds(), 1) + " s  (press USE TRACK again to finish)";
        if (e.notice.isNotEmpty())
            st = e.notice;
        if (e.downloadProgress >= 0.0f)
            st = "Downloading lyrics model... " + juce::String (juce::roundToInt (e.downloadProgress * 100.0f)) + "%";
        return st;
    };
    auto drawStatus = [&] (juce::Rectangle<float> r, juce::Justification j)
    {
        auto st = statusText();
        if (st.isEmpty()) return;
        bool ok = session.hasVocal() && ! session.isBusy() && ! e.proc.isCapturing();
        g.setFont (Fonts::regular (13.5f));
        auto tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), st);
        auto dotX = j.testFlags (juce::Justification::right) ? r.getRight() - 8.0f
                  : j.testFlags (juce::Justification::horizontallyCentred) ? r.getCentreX() + tw * 0.5f + 12.0f
                  : r.getX() + tw + 14.0f;
        g.setColour (ok ? green.withAlpha (0.9f) : p.text);
        g.drawText (st, r.withTrimmedRight (j.testFlags (juce::Justification::right) ? 18.0f : 0.0f), j);
        auto dotCol = ok ? green : (e.proc.isCapturing() ? p.danger : amber);
        Glass::drawGlow (g, { dotX, r.getCentreY() }, 8.0f, dotCol, 0.6f);
        g.setColour (dotCol);
        g.fillEllipse (juce::Rectangle<float> (7, 7).withCentre ({ dotX, r.getCentreY() }));
    };
    auto chip = [&] (juce::Rectangle<float> r)
    {
        auto name = session.getVocalName();
        if (name.isEmpty()) return;
        Glass::drawPanel (g, r, r.getHeight() * 0.5f, p, 1.2f);
        IconButton::drawIcon (g, IconButton::Icon::drag, r.withWidth (r.getHeight()).reduced (6.0f), p.text, 1.0f);
        g.setColour (p.text);
        g.setFont (Fonts::regular (13.0f));
        g.drawFittedText (name, r.withTrimmedLeft (r.getHeight()).reduced (4, 0).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    };
    auto progressBar = [&] (juce::Rectangle<float> r)
    {
        if (! session.isBusy()) return;
        g.setColour (p.text.withAlpha (0.12f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (juce::Colour (0xffb794ff));
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * juce::jlimit (0.02f, 1.0f, session.getProgress())), r.getHeight() * 0.5f);
    };

    if (tab == 0)
    {
        Glass::drawPanel (g, { 45.0f, 112.0f, 1110.0f, 100.0f }, 12.0f, p, 0.6f, e.dropHover);
        g.setColour (p.text);
        Fonts::drawSpaced (g, "DROP YOUR VOCAL", { 60.0f, 124.0f, 170.0f, 30.0f }, Fonts::regular (16.0f), 0.03f, juce::Justification::centredLeft);
        chip ({ 60.0f, 166.0f, 260.0f, 28.0f });
        g.setColour (white.withAlpha (0.12f));
        g.drawVerticalLine (426, 124.0f, 200.0f);
        drawStatus ({ 760.0f, 112.0f, 388.0f, 22.0f }, juce::Justification::centredRight);

        Glass::drawPanel (g, { 45.0f, 222.0f, 897.0f, 310.0f }, 12.0f, p, 0.35f);
        Glass::drawPanel (g, { 953.0f, 222.0f, 202.0f, 582.0f }, 12.0f, p, 0.6f);
        g.setColour (white.withAlpha (0.12f));
        g.drawHorizontalLine (728, 968.0f, 1140.0f);
        g.setColour (p.text);
        g.setFont (Fonts::regular (14.0f));
        g.drawText ("QUALITY:", juce::Rectangle<float> (966.0f, 734.0f, 80.0f, 26.0f), juce::Justification::centredLeft);
        g.setFont (Fonts::regular (12.0f));
        g.drawText ("IN", juce::Rectangle<float> (968.0f, 769.0f, 30.0f, 16.0f), juce::Justification::centredLeft);
        g.drawText ("OUT", juce::Rectangle<float> (968.0f, 783.0f, 34.0f, 16.0f), juce::Justification::centredLeft);

        Glass::drawPanel (g, { 45.0f, 816.0f, 1110.0f, 76.0f }, 12.0f, p, 0.6f);
        auto stText = session.isBusy() ? session.getStatus() + "  " + juce::String (juce::roundToInt (session.getProgress() * 100.0f)) + "%"
                    : e.anyDirty() && session.hasVocal() ? juce::String ("Press GENERATE to sing the changes") : session.getStatus();
        g.setColour (p.text);
        g.setFont (Fonts::regular (13.5f));
        g.drawFittedText (stText, juce::Rectangle<int> (690, 828, 228, 26), juce::Justification::centredLeft, 1, 0.8f);
        progressBar ({ 690.0f, 862.0f, 220.0f, 6.0f });
    }
    else
    {
        Glass::drawPanel (g, { 38.0f, 112.0f, 326.0f, 630.0f }, 12.0f, p, 0.6f, e.dropHover);
        g.setColour (p.text);
        Fonts::drawSpaced (g, "DROP YOUR VOCAL", { 38.0f, 126.0f, 326.0f, 30.0f }, Fonts::regular (18.0f), 0.03f, juce::Justification::centred);
        chip ({ 70.0f, 170.0f, 262.0f, 30.0f });

        Glass::drawPanel (g, { 378.0f, 112.0f, 446.0f, 630.0f }, 12.0f, p, 0.45f);
        drawStatus ({ 392.0f, 124.0f, 420.0f, 22.0f }, juce::Justification::centred);
        g.setColour (p.text);
        g.setFont (Fonts::regular (21.0f));
        auto v = juce::roundToInt (getParam (e.proc.state, IDs::cVoices));
        g.drawText ("VOICES:  " + juce::String (v), juce::Rectangle<float> (378.0f, 186.0f, 446.0f, 34.0f), juce::Justification::centred);
        g.setFont (Fonts::regular (12.0f));
        g.setColour (p.textDim);
        g.drawText ("2", juce::Rectangle<float> (452.0f, 256.0f, 20.0f, 14.0f), juce::Justification::centredLeft);
        g.drawText ("64", juce::Rectangle<float> (732.0f, 256.0f, 20.0f, 14.0f), juce::Justification::centredRight);
        g.setColour (p.text);
        Fonts::drawSpaced (g, "CHOIR TYPE", { 378.0f, 594.0f, 446.0f, 26.0f }, Fonts::regular (16.0f), 0.03f, juce::Justification::centred);

        Glass::drawPanel (g, { 838.0f, 112.0f, 324.0f, 630.0f }, 12.0f, p, 0.6f);
        g.setColour (p.text);
        Fonts::drawSpaced (g, "SECTIONS", { 838.0f, 126.0f, 324.0f, 30.0f }, Fonts::regular (18.0f), 0.03f, juce::Justification::centred);
        g.setColour (white.withAlpha (0.12f));
        g.drawHorizontalLine (506, 856.0f, 1146.0f);

        Glass::drawPanel (g, { 38.0f, 756.0f, 1124.0f, 104.0f }, 12.0f, p, 0.6f);
        g.setColour (white.withAlpha (0.12f));
        for (auto x : { 596, 838, 972 }) g.drawVerticalLine (x, 772.0f, 846.0f);
        progressBar ({ 276.0f, 790.0f, 300.0f, 8.0f });
        g.setColour (p.text);
        g.setFont (Fonts::regular (13.5f));
        auto stText = session.isBusy() ? session.getStatus() + "  " + juce::String (juce::roundToInt (session.getProgress() * 100.0f)) + "%"
                    : session.choirReady() ? (session.choirDirty (e.proc.choirSpec()) ? juce::String ("Press GENERATE to sing the changes")
                                                                                       : juce::String ("Choir ready"))
                    : session.hasVocal() ? juce::String ("Choose voices and type, then GENERATE") : juce::String ("Drop a vocal first");
        g.drawFittedText (stText, juce::Rectangle<int> (276, 806, 300, 24), juce::Justification::centredLeft, 1, 0.8f);
    }
}

// ------------------------------------------------------------------ session

void Editor::changeListenerCallback (juce::ChangeBroadcaster*) { refreshSessionViews(); }

void Editor::refreshSessionViews()
{
    auto an = proc.session.getAnalysis();
    auto words = proc.session.getWords();
    std::vector<Region> regions;
    if (an != nullptr)
    {
        auto usesRefuerzo = false;
        for (int i = 0; i < numStrips; ++i)
            usesRefuerzo = usesRefuerzo || (proc.stripEnabled (i) && partInfo (proc.stripSpec (i).part).refuerzo);
        if (usesRefuerzo) regions = fx::refuerzoRegions (words, *an, juce::roundToInt (getParam (proc.state, IDs::refuerzoWords)));
    }
    wave.setData (an, words, regions);
    choirWave.setData (an, words, {});
    decor.repaint();
}

juce::String Editor::keyText() const
{
    auto an = proc.session.getAnalysis();
    if (getParam (proc.state, IDs::keyAuto) > 0.5f)
        return an != nullptr ? juce::String (proc.session.getDetectedKey().name()) : juce::String ("Auto");
    return juce::String (proc.currentKey().name());
}

bool Editor::anyDirty() const
{
    for (int i = 0; i < numStrips; ++i)
        if (proc.stripEnabled (i) && proc.session.isStripDirty (i, proc.stripSpec (i))) return true;
    return false;
}

void Editor::syncStripCombos()
{
    syncingCombos = true;
    for (int i = 0; i < numStrips; ++i)
    {
        auto id = (int) proc.stripSpec (i).part + 1;
        if (strips[(size_t) i].part.getSelectedId() != id) strips[(size_t) i].part.setSelectedId (id, juce::dontSendNotification);
    }
    syncingCombos = false;
}

void Editor::timerCallback()
{
    auto& session = proc.session;
    syncStripCombos();

    // top bar
    keyBox.setValue (keyText());
    presetBox.setValue (proc.presetName + (proc.isPresetModified() ? " *" : ""));
    undoButton.setEnabled (proc.undo.canUndo());
    redoButton.setEnabled (proc.undo.canRedo());
    auto bypassed = getParam (proc.state, IDs::bypass) > 0.5f;
    if (powerButton.active == bypassed) { powerButton.active = ! bypassed; powerButton.repaint(); }
    if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()) proc.undo.beginNewTransaction();
    auto chipText = proc.license.isLicensed() ? juce::String() : proc.license.isUsable()
                      ? juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (proc.license.getTrialDaysLeft()) + " DAYS"
                      : juce::String ("ACTIVATE");
    if (licenseButton.getButtonText() != chipText) licenseButton.setButtonText (chipText);
    licenseButton.setVisible (chipText.isNotEmpty());

    // the key the user chose is the key the voices are composed in
    if (session.hasVocal() && ! session.isBusy())
    {
        auto k = proc.currentKey();
        auto an = session.getAnalysis();
        if (an != nullptr && (an->key.tonic != k.tonic || an->key.minor != k.minor)) session.setKey (k);
    }

    // strips + moons
    auto busy = session.isBusy();
    for (int i = 0; i < numStrips; ++i)
    {
        auto& st = strips[(size_t) i];
        auto spec = proc.stripSpec (i);
        bool on = proc.stripEnabled (i);
        bool dirty = session.hasVocal() && session.isStripDirty (i, spec);
        auto gains = proc.stripGains (i);
        if (st.enabledStrip != on || st.dirty != dirty || st.valid != ! dirty)
        {
            st.enabledStrip = on; st.dirty = dirty; st.valid = session.hasVocal() && ! dirty;
            st.repaint();
        }
        st.power.active = on;
        st.regen.setEnabled (on && session.hasVocal() && ! busy);
        auto& m = orbits.moons[(size_t) i];
        m.on = on;
        m.valid = st.valid;
        m.dirty = dirty;
        m.muted = ! gains.audible;
        auto label = juce::String (partInfo (spec.part).name);
        if (spec.style != Style::natural && ! partInfo (spec.part).isLayer) label += "\n" + juce::String (styleName (spec.style));
        m.label = label;
    }
    orbits.soloMoon = proc.soloStrip.load();

    // centre note: the lead note at the playhead, or the key
    auto an = session.getAnalysis();
    juce::String centre = "A";
    if (an != nullptr)
    {
        centre = juce::String (pitchClassName (an->key.tonic));
        auto t = proc.getPlayPositionSeconds();
        auto frame = (int) (t / frameSec);
        for (auto& n : an->notes)
            if (frame >= n.start && frame < n.end) { centre = juce::String (pitchClassName (n.midi)); break; }
    }
    orbits.centreNote = centre;

    auto level = proc.getOutputLevel();
    orbits.tick (proc.isPlaying() ? juce::jlimit (0.0f, 1.0f, level * 2.0f) : 0.0f);
    cluster.setVoices (juce::roundToInt (getParam (proc.state, IDs::cVoices)));
    cluster.tick (proc.isPlaying() ? juce::jlimit (0.0f, 1.0f, level * 2.0f) : 0.0f);
    wave.setPlayhead (proc.getPlayPositionSeconds(), proc.isPlaying());
    choirWave.setPlayhead (proc.getPlayPositionSeconds(), proc.isPlaying());
    wave.tick();
    choirWave.tick();
    inMeter.setLevel (proc.getInputLevel());
    outMeter.setLevel (level);
    background.setEnergy (juce::jlimit (0.0f, 1.0f, level * 1.5f));

    for (auto* b : { &playButton, &playButton2 })
        if (b->active != proc.isPlaying()) { b->active = proc.isPlaying(); b->repaint(); }
    leadLabel.setSelected (getParam (proc.state, IDs::withLead) > 0.5f ? 1 : 0);
    leadLabel2.setSelected (leadLabel.selected);

    for (auto* g : { &generate, &generate2 })
    {
        auto wasBusy = g->busy;
        auto wasAttention = g->attention;
        g->busy = busy;
        g->progress = session.getProgress();
        g->time += 1.0f / 30.0f;
        g->attention = session.hasVocal() && ! busy && (g == &generate ? anyDirty() : session.choirDirty (proc.choirSpec()));
        g->setEnabled (session.hasVocal() || busy);
        if (g->isVisible() && (busy || g->attention || wasBusy != busy || wasAttention != g->attention)) g->repaint();
    }
    for (auto* b : { &useTrackButton, &useTrackButton2 })
        b->setButtonText (proc.isCapturing() ? "STOP REC" : "USE TRACK");

    auto sig = session.getStatus() + keyText() + juce::String (busy ? session.getProgress() : -1.0f, 2)
             + juce::String (proc.isCapturing() ? proc.getCaptureSeconds() : -1.0, 1) + juce::String (downloadProgress, 2)
             + (anyDirty() ? "d" : "") + (session.choirDirty (proc.choirSpec()) ? "c" : "")
             + juce::String (getParam (proc.state, IDs::cVoices)) + juce::String (proc.getHostBpm(), 0);
    if (notice.isNotEmpty() && juce::Time::getMillisecondCounter() > noticeUntil) notice.clear();
    sig += notice;
    if (sig != lastDecorSignature)
    {
        lastDecorSignature = sig;
        decor.repaint();
    }
}

// ------------------------------------------------------------------ actions

void Editor::showNotice (const juce::String& text)
{
    notice = text;
    noticeUntil = juce::Time::getMillisecondCounter() + 6000;
    decor.repaint();
}

void Editor::logDrag (const juce::String& line)
{
    auto f = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                 .getChildFile ("Library/Logs/LT Audio/HALO.log");
    f.getParentDirectory().createDirectory();
    if (f.getSize() > 512 * 1024) f.deleteFile();
    f.appendText (juce::Time::getCurrentTime().toString (true, true, true, true) + "  " + line + "\n");
}

/** One voice on its own: level / pan apply, but not power / mute / solo (dragging a voice means "give me this voice"). */
juce::File Editor::dragStrip (int i)
{
    if (! proc.session.stripReady (i))
    {
        logDrag ("strip " + juce::String (i + 1) + ": not generated");
        showNotice ("Voice " + juce::String (i + 1) + " is not sung yet: press GENERATE first");
        return {};
    }
    float peak = 0.0f;
    auto f = proc.session.writeStrip (i, proc.stripGains (i, false), {}, true, &peak);
    logDrag ("strip " + juce::String (i + 1) + " -> " + f.getFullPathName() + "  peak " + juce::String (peak, 4));
    if (! f.existsAsFile())
    {
        showNotice ("Could not write the WAV to " + proc.session.defaultExportFolder().getFullPathName());
        return {};
    }
    if (peak < 1.0e-5f)
    {
        f.deleteFile();
        auto name = juce::String (partInfo (proc.stripSpec (i).part).name);
        showNotice (name + " is silent in this take (nothing to reinforce / level too low)");
        return {};
    }
    return f;
}

/** Every voice that is on and not muted. Solo only narrows it when the soloed voices are sung. */
juce::File Editor::dragMix()
{
    std::array<Session::StripGains, numStrips> g;
    bool any = false;
    for (int i = 0; i < numStrips; ++i)
    {
        g[(size_t) i] = proc.stripGains (i);
        any = any || (g[(size_t) i].audible && proc.session.stripReady (i));
    }
    if (! any)   // solo on a voice that has no audio: fall back to every voice that is on and not muted
        for (int i = 0; i < numStrips; ++i)
        {
            auto& s = proc.state;
            bool on = proc.stripEnabled (i) && getParam (s, IDs::strip (i, IDs::mute)) < 0.5f;
            g[(size_t) i] = proc.stripGains (i, false);
            g[(size_t) i].audible = on;
            if (! on) g[(size_t) i].gl = g[(size_t) i].gr = 0.0f;
            any = any || (on && proc.session.stripReady (i));
        }
    if (! any)
    {
        logDrag ("mix: no sung voice is on");
        showNotice ("No voice to drag: turn a voice on and press GENERATE");
        return {};
    }
    float peak = 0.0f;
    auto f = proc.session.writeMix (g, {}, true, &peak);
    logDrag ("mix -> " + f.getFullPathName() + "  peak " + juce::String (peak, 4));
    if (! f.existsAsFile())
    {
        showNotice ("Could not write the WAV to " + proc.session.defaultExportFolder().getFullPathName());
        return {};
    }
    if (peak < 1.0e-5f)
    {
        f.deleteFile();
        showNotice ("The harmonies are silent: raise the voice levels");
        return {};
    }
    return f;
}

juce::File Editor::dragChoirFile()
{
    if (! proc.session.choirReady())
    {
        showNotice ("The choir is not sung yet: press GENERATE first");
        return {};
    }
    auto files = proc.session.writeChoir (juce::Decibels::decibelsToGain (getParam (proc.state, IDs::cLevel)), false, {}, true);
    logDrag ("choir -> " + (files.isEmpty() ? juce::String ("(none)") : files[0].getFullPathName()));
    if (files.isEmpty()) showNotice ("Could not write the choir WAV");
    return files.isEmpty() ? juce::File() : files[0];
}

void Editor::onGenerate()
{
    if (proc.session.isBusy()) { proc.session.cancel(); return; }
    if (! proc.session.hasVocal()) { browse(); return; }
    if (proc.tab.load() == 0) proc.generateHarmony();
    else proc.generateChoir (! proc.session.choirDirty (proc.choirSpec()));   // pressing again = a new choir take
}

bool Editor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a;caf")) return true;
    return false;
}

void Editor::filesDropped (const juce::StringArray& files, int, int)
{
    dropHover = false;
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a;caf"))
        {
            proc.stop();
            if (! proc.session.loadFile (juce::File (f)))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "LT HALO", proc.session.getError());
            break;
        }
}

void Editor::browse()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a dry vocal (acapella)", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f.existsAsFile()) { proc.stop(); proc.session.loadFile (f); }
    });
}

void Editor::toggleCapture()
{
    if (proc.isCapturing()) proc.stopCapture();
    else proc.startCapture();
}

void Editor::exportAll()
{
    chooser = std::make_unique<juce::FileChooser> ("Export to folder", proc.session.defaultExportFolder());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
    {
        auto dir = fc.getResult();
        if (dir == juce::File()) return;
        juce::Array<juce::File> written;
        if (proc.tab.load() == 0)
        {
            std::array<Session::StripGains, numStrips> g;
            for (int i = 0; i < numStrips; ++i)
            {
                g[(size_t) i] = proc.stripGains (i);
                if (g[(size_t) i].audible)
                    if (auto f = proc.session.writeStrip (i, g[(size_t) i], dir); f.existsAsFile()) written.add (f);
            }
            if (auto f = proc.session.writeMix (g, dir); f.existsAsFile()) written.add (f);
        }
        else
            written = proc.session.writeChoir (juce::Decibels::decibelsToGain (getParam (proc.state, IDs::cLevel)),
                                               getParam (proc.state, IDs::cSeparate) > 0.5f, dir);
        if (written.isEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "LT HALO", "Nothing to export yet: press GENERATE first.");
        else
            written.getFirst().revealToUser();
    });
}

void Editor::showKeyMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    auto autoOn = getParam (proc.state, IDs::keyAuto) > 0.5f;
    m.addItem (1000, "Auto (detected: " + juce::String (proc.session.hasVocal() ? proc.session.getDetectedKey().name() : std::string ("-")) + ")",
               true, autoOn);
    const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    auto cur = proc.currentKey();
    for (int minor = 0; minor < 2; ++minor)
    {
        m.addSeparator();
        m.addSectionHeader (minor ? "MINOR" : "MAJOR");
        for (int k = 0; k < 12; ++k)
            m.addItem (1 + minor * 12 + k, juce::String (names[k]) + (minor ? " Minor" : " Major"), true,
                       ! autoOn && cur.tonic == k && cur.minor == (minor == 1));
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&keyBox), [this] (int r)
    {
        if (r == 1000) { setParam (proc.state, IDs::keyAuto, 1.0f); return; }
        if (r < 1 || r > 24) return;
        proc.undo.beginNewTransaction();
        setParam (proc.state, IDs::keyAuto, 0.0f);
        setParam (proc.state, IDs::key, (float) ((r - 1) % 12));
        setParam (proc.state, IDs::scale, (float) ((r - 1) / 12));
    });
}

void Editor::showPresetMenu()
{
    proc.presets.rescan();
    auto& all = proc.presets.all();
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    juce::String group;
    for (int i = 0; i < (int) all.size(); ++i)
    {
        auto& pr = all[(size_t) i];
        if (! pr.factory) continue;
        if (pr.group != group && i > 0) m.addSeparator();
        if (pr.group != group) { m.addSectionHeader (pr.group); group = pr.group; }
        m.addItem (i + 1, pr.name, true, pr.name == proc.presetName);
    }
    bool anyUser = false;
    for (int i = 0; i < (int) all.size(); ++i)
        if (! all[(size_t) i].factory)
        {
            if (! anyUser) { m.addSeparator(); m.addSectionHeader ("MY PRESETS"); anyUser = true; }
            m.addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.presetName);
        }
    m.addSeparator();
    m.addItem (10001, "Save as...");
    auto ci = proc.presets.indexOf (proc.presetName);
    m.addItem (10002, "Delete \"" + proc.presetName + "\"", ci >= 0 && ! all[(size_t) ci].factory);
    m.addItem (10003, "Open presets folder");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBox), [this] (int r)
    {
        auto& list = proc.presets.all();
        if (r >= 1 && r <= (int) list.size())
        {
            proc.undo.beginNewTransaction();
            auto& pr = list[(size_t) r - 1];
            proc.applyPreset (pr);
            setTab (pr.group == "CHOIR" ? 1 : 0);
        }
        else if (r == 10001) savePreset();
        else if (r == 10002) proc.presets.remove (proc.presetName);
        else if (r == 10003) { proc.presets.userFolder().createDirectory(); proc.presets.userFolder().startAsProcess(); }
    });
}

void Editor::stepPreset (int delta)
{
    proc.presets.rescan();
    auto& all = proc.presets.all();
    if (all.empty()) return;
    auto i = proc.presets.indexOf (proc.presetName);
    i = ((i < 0 ? 0 : i + delta) + (int) all.size()) % (int) all.size();
    proc.undo.beginNewTransaction();
    proc.applyPreset (all[(size_t) i]);
    setTab (all[(size_t) i].group == "CHOIR" ? 1 : 0);
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
        auto preset = lt::presets::ParamPreset::capture (proc.state, name, "MY PRESETS");
        for (auto* id : { IDs::bypass, IDs::key, IDs::scale, IDs::keyAuto }) preset.values.erase (id);
        if (proc.presets.save (preset)) proc.presetName = name;
    }), false);
}

void Editor::showSettingsMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    m.addSectionHeader ("LT HALO " LT_VERSION);

    juce::PopupMenu refuerzo;
    auto words = juce::roundToInt (getParam (proc.state, IDs::refuerzoWords));
    for (int n = 1; n <= 3; ++n) refuerzo.addItem (100 + n, juce::String (n) + (n == 1 ? " word" : " words"), true, words == n);
    m.addSubMenu ("Refuerzo: ending words per phrase", refuerzo);

    if (Lyrics::available()) m.addItem (2, "Lyrics model installed (refuerzo follows the words)", false, true);
    else m.addItem (3, "Download lyrics model (190 MB) - better refuerzo");
    m.addItem (4, AIHelper::available() ? juce::String ("BEST quality: AI helper installed") : juce::String ("BEST quality: how to install the AI helper..."),
               ! AIHelper::available(), AIHelper::available());
    m.addSeparator();
    m.addItem (5, "Open export folder");
    m.addItem (6, "Open presets folder");
    m.addItem (7, "License...");
    m.addItem (8, "Reset to default");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton), [this] (int r)
    {
        if (r > 100 && r <= 103) { setParam (proc.state, IDs::refuerzoWords, (float) (r - 100)); refreshSessionViews(); }
        else if (r == 3) downloadLyricsModel();
        else if (r == 4)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "BEST quality",
                "BEST re-sings every voice with an AI model (Seed-VC) using the timbre of your vocal. It runs as a separate helper "
                "on this Mac.\n\nTo install it, run in Terminal:\n\n  ~/LT-AUDIO-build/source/tools/halo-prototype/install_ai_helper.sh\n\n"
                "(downloads about 3 GB once). FAST works without it.");
        else if (r == 5) { auto f = proc.session.defaultExportFolder(); f.createDirectory(); f.startAsProcess(); }
        else if (r == 6) { proc.presets.userFolder().createDirectory(); proc.presets.userFolder().startAsProcess(); }
        else if (r == 7) showActivation (true);
        else if (r == 8)
        {
            proc.undo.beginNewTransaction();
            for (auto* prm : proc.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                {
                    rp->beginChangeGesture();
                    rp->setValueNotifyingHost (rp->getDefaultValue());
                    rp->endChangeGesture();
                }
            proc.presetName = "Default";
        }
    });
}

void Editor::downloadLyricsModel()
{
    struct Listener : public juce::URL::DownloadTaskListener
    {
        explicit Listener (juce::Component::SafePointer<Editor> e) : editor (e) {}
        void finished (juce::URL::DownloadTask*, bool success) override
        {
            juce::MessageManager::callAsync ([e = editor, success]
            {
                if (e == nullptr) return;
                e->downloadProgress = -1.0f;
                juce::AlertWindow::showMessageBoxAsync (success ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                    "LT HALO", success ? "Lyrics model installed. Drop your vocal again so LT HALO reads the words."
                                       : "Download failed. Check your internet connection and try again.");
            });
        }
        void progress (juce::URL::DownloadTask*, juce::int64 done, juce::int64 total) override
        {
            auto p = total > 0 ? (float) done / (float) total : 0.0f;
            juce::MessageManager::callAsync ([e = editor, p] { if (e != nullptr) e->downloadProgress = p; });
        }
        juce::Component::SafePointer<Editor> editor;
    };
    static std::unique_ptr<Listener> listener;
    listener = std::make_unique<Listener> (juce::Component::SafePointer<Editor> (this));
    auto target = Lyrics::modelFile();
    target.getParentDirectory().createDirectory();
    downloadProgress = 0.0f;
    download = Lyrics::modelUrl().downloadToFile (target, juce::URL::DownloadTaskOptions().withListener (listener.get()));
    if (download == nullptr) downloadProgress = -1.0f;
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

} // namespace halo
