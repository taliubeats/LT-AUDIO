#include "PluginEditor.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace cosmos
{

using Rect = juce::Rectangle<float>;
static const juce::String dot = juce::CharPointer_UTF8 (" \xc2\xb7 ");
bool Editor::activationShownThisSession = false;

Editor::Editor (Processor& p)
    : AudioProcessorEditor (p), proc (p)
{
    analyzer.prepare (proc.getCurrentSampleRate());
    proc.flushAudio();
    analyzerBase = proc.getSamplesRead();

    if (proc.layout.isNotEmpty()) strip.setLayout (proc.layout);
    strip.onLayoutChanged = [this] { proc.layout = strip.getLayout(); };
    strip.onResetLoudness = [this] { proc.meter.requestReset(); };
    addAndMakeVisible (strip);

    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe]
        {
            if (safe == nullptr) return;
            if (safe->proc.layout.isNotEmpty()) safe->strip.setLayout (safe->proc.layout);
            if (safe->proc.width > 0) safe->setSize (safe->proc.width, safe->proc.height);
            safe->repaint();
        });
    };

#if JucePlugin_Build_Standalone
    if (proc.isStandalone())
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            holder->getMuteInputValue().setValue (false);   // the app outputs silence, so listening is safe
#endif

    // read the saved size first: setResizeLimits() triggers resized(), which stores the current size
    const auto startW = proc.width > 0 ? proc.width : defaultWidth, startH = proc.height > 0 ? proc.height : defaultHeight;
    setResizable (true, true);
    setResizeLimits (520, 110, 3840, 2160);
    setSize (startW, startH);

    if (! proc.license.isLicensed() && (! activationShownThisSession || ! proc.license.isUsable()))
    {
        activationShownThisSession = true;
        showActivation (true);
    }

    startTimerHz (60);

    // the app listens to the whole computer by default (Spotify, YouTube, any player)
    if (proc.isStandalone() && proc.wantSystemAudio && SystemAudio::isSupported()
        && juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {}).isEmpty())
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Editor> (this)] { if (safe != nullptr) safe->setSystemAudio (true); });

    if (proc.isStandalone() && proc.pinned)
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Editor> (this)] { if (safe != nullptr) safe->setPinned (true); });

    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty()) runSnapshot (snapshot);
}

Editor::~Editor()
{
    proc.onStateRestored = nullptr;
}

juce::DocumentWindow* Editor::window() const
{
    return dynamic_cast<juce::DocumentWindow*> (getTopLevelComponent());
}

//==============================================================================
void Editor::pullAudio()
{
    scratchL.resize (8192);
    scratchR.resize (8192);
    for (int guard = 0; guard < 64; ++guard)
    {
        auto n = proc.readAudio (scratchL.data(), scratchR.data(), 8192);
        if (n <= 0) break;
        analyzer.push (scratchL.data(), scratchR.data(), n);
    }
    auto t = proc.getTiming();
    // timing position is in "samples written"; the analyzer counts samples it has read since it started
    analyzer.setHostTiming (t.valid, t.playing, t.bpm, t.ppq, t.barStart, t.num, t.den, t.atWritten - analyzerBase);
}

void Editor::timerCallback()
{
    if (std::abs (analyzer.getSampleRate() - proc.getCurrentSampleRate()) > 1.0)
    {
        analyzer.prepare (proc.getCurrentSampleRate());
        proc.flushAudio();
        analyzerBase = proc.getSamplesRead();
    }
    pullAudio();
    analyzer.frame();

    auto& m = proc.meter;
    readings.momentary = m.getMomentary();
    readings.shortTerm = m.getShortTerm();
    readings.integrated = m.getIntegrated();
    readings.range = m.getRange();
    readings.maxMomentary = m.getMaxMomentary();
    readings.maxShortTerm = m.getMaxShortTerm();
    for (int c = 0; c < 2; ++c) { readings.truePeakMax[(size_t) c] = m.getTruePeakMax (c); readings.truePeakNow[(size_t) c] = m.getTruePeakNow (c); }
    if (++frame % 6 == 0) analyzer.pushLoudness (readings.momentary, readings.shortTerm);

    auto& skin = Skin::get (proc.skin);
    strip.setContext (&analyzer, &readings, &skin, (float) proc.target, (juce::Time::getMillisecondCounterHiRes() - startTime) * 0.001);
    strip.repaint();
    if (frame % 4 == 0)
    {
        buildBar();
        repaint (0, 0, getWidth(), barHeight);
    }
}

//==============================================================================
void Editor::buildBar()
{
    items.clear();
    auto& s = Skin::get (proc.skin);
    juce::ignoreUnused (s);
    auto font = monoFont (9.5f);
    float x = 118;
    auto add = [&] (const juce::String& text, std::function<void()> fn, bool active = false, bool accent = false)
    {
        auto w = juce::GlyphArrangement::getStringWidth (font, text) + 18.0f;
        items.push_back ({ text, Rect (x, 4, w, (float) barHeight - 8), std::move (fn), active, accent });
        x += w + 4;
    };
    add ("+ PANEL", [this] { showAddMenu(); });
    add ("SKIN " + Skin::get (proc.skin).name, [this] { showSkinMenu(); });
    add ("SIZE", [this] { showSizeMenu(); });
    add (proc.target == 0 ? juce::String ("TARGET OFF") : "TARGET " + juce::String (proc.target), [this] { showTargetMenu(); });
    add ("RESET", [this] { proc.meter.requestReset(); });
    add ("PRESET: " + (proc.presetName.isEmpty() ? juce::String ("-") : proc.presetName.toUpperCase()), [this] { showPresetMenu(); });
    if (proc.isStandalone())
    {
        add ("PIN", [this] { setPinned (! proc.pinned); }, proc.pinned);
        add ("DOCK", [this] { setDocked (! docked); }, docked);
        add ("FULL", [this] { setFullscreen (! fullscreen); }, fullscreen);
        add (proc.isSystemAudio() ? "SOURCE: SYSTEM" : "SOURCE: INPUT", [this] { setSystemAudio (! proc.isSystemAudio()); }, proc.isSystemAudio());
        add ("INPUT", [this] { setSystemAudio (false); showAudioSettings(); });
    }
    if (! proc.license.isLicensed())
    {
        auto chip = proc.license.isUsable() ? "TRIAL " + juce::String (proc.license.getTrialDaysLeft()) + "D" : juce::String ("ACTIVATE");
        add (chip, [this] { showActivation (true); }, false, true);
    }
}

void Editor::paint (juce::Graphics& g)
{
    auto& s = Skin::get (proc.skin);
    g.fillAll (s.bg);
    auto bar = getLocalBounds().removeFromTop (barHeight).toFloat();

    g.setColour (s.text);
    g.setFont (monoFont (10.5f, true));
    g.drawText ("LT COSMOS", Rect (10, 0, 104, bar.getHeight()), juce::Justification::centredLeft);
    if (items.empty()) buildBar();
    g.setFont (monoFont (9.5f));
    for (size_t i = 0; i < items.size(); ++i)
    {
        auto& it = items[i];
        auto hot = (int) i == hoverItem;
        if (it.active || hot)
        {
            g.setColour ((it.active ? s.line2 : s.text).withAlpha (it.active ? 0.22f : 0.08f));
            g.fillRect (it.bounds);
        }
        g.setColour (it.accent ? s.accent : it.active ? s.text : s.text.withAlpha (hot ? 1.0f : 0.72f));
        g.drawText (it.text, it.bounds, juce::Justification::centred);
    }

    // musical readout on the right
    auto& a = analyzer;
    juce::String bb = a.barTime >= 0 ? juce::String ((int) std::floor (a.barTime) + 1) + "." + juce::String ((int) ((a.barTime - std::floor (a.barTime)) * a.beatsPerBar) + 1)
                                     : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94"));
    auto info = (a.bpm > 0 ? juce::String (a.bpm, 1) + (a.hostTiming ? " BPM" : " BPM~") : juce::String ("--- BPM")) + dot + bb + dot
                + music::keyName (a.key) + dot + a.chordText + dot + "I " + draw::lufsText (readings.integrated);
    auto infoX = items.empty() ? 400.0f : items.back().bounds.getRight() + 12;
    g.setColour (s.dim);
    g.drawText (info, Rect (infoX, 0, (float) getWidth() - infoX - 10, bar.getHeight()), juce::Justification::centredRight, true);
}

void Editor::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (barHeight);
    strip.setBounds (r.reduced (6, 0).withTrimmedBottom (6));
    if (activation) activation->setBounds (getLocalBounds());
    if (! docked && ! fullscreen) { proc.width = getWidth(); proc.height = getHeight(); }
}

void Editor::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i].bounds.contains (e.position)) h = (int) i;
    if (h != hoverItem) { hoverItem = h; repaint (0, 0, getWidth(), barHeight); }
}

void Editor::mouseDown (const juce::MouseEvent& e)
{
    for (auto& it : items)
        if (it.bounds.contains (e.position) && it.click) { auto fn = it.click; fn(); buildBar(); repaint(); return; }
}

//============================================================================== menus
void Editor::showAddMenu()
{
    juce::PopupMenu menu;
    for (auto group : { "METER", "SPECTRAL", "GENERATIVE", "MUSICAL" })
    {
        menu.addSectionHeader (group);
        for (int t = 0; t < (int) VisualType::count; ++t)
            if (Visual::groupOf ((VisualType) t) == group) menu.addItem (t + 1, Visual::nameOf ((VisualType) t));
    }
    menu.addSeparator();
    menu.addItem (500, "RESET LAYOUT");
    juce::Component::SafePointer<Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (items[0].bounds.toNearestInt())), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 500) { safe->strip.setLayout (PanelStrip::defaultLayout()); safe->proc.layout = safe->strip.getLayout(); return; }
        safe->strip.addPanel ((VisualType) (r - 1));
    });
}

void Editor::showSkinMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < (int) Skin::all().size(); ++i)
    {
        auto& sk = Skin::get (i);
        juce::PopupMenu::Item item (sk.name);
        item.itemID = i + 1;
        item.isTicked = i == proc.skin;
        item.colour = sk.light ? sk.line : sk.line2;   // each skin listed in its own colour
        menu.addItem (item);
    }
    juce::Component::SafePointer<Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (items[1].bounds.toNearestInt())), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        safe->proc.skin = r - 1;
        safe->buildBar();
        safe->repaint();
    });
}

void Editor::showSizeMenu()
{
    struct Size { const char* name; int w, h; };
    static const Size sizes[] = { { "STRIP S   900 x 150", 900, 150 }, { "STRIP M  1180 x 176", 1180, 176 }, { "STRIP L  1500 x 230", 1500, 230 },
                                  { "WIDE     1600 x 420", 1600, 420 }, { "STUDIO   1280 x 760", 1280, 760 }, { "BIG      1900 x 1040", 1900, 1040 } };
    juce::PopupMenu menu;
    for (int i = 0; i < 6; ++i) menu.addItem (i + 1, sizes[i].name);
    juce::Component::SafePointer<Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (items[2].bounds.toNearestInt())), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        safe->applySize (sizes[r - 1].w, sizes[r - 1].h);
    });
}

void Editor::showTargetMenu()
{
    juce::PopupMenu menu;
    const int targets[] = { -14, -16, -23, -9, 0 };
    for (int i = 0; i < 5; ++i)
        menu.addItem (i + 1, targets[i] == 0 ? juce::String ("OFF") : juce::String (targets[i]) + " LUFS", true, proc.target == targets[i]);
    juce::Component::SafePointer<Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (items[3].bounds.toNearestInt())), [safe, targets] (int r)
    {
        if (safe == nullptr || r == 0) return;
        safe->proc.target = targets[r - 1];
        safe->buildBar();
        safe->repaint();
    });
}

void Editor::applyLoadedPreset()
{
    if (proc.layout.isNotEmpty()) strip.setLayout (proc.layout);
    if (proc.width > 0 && proc.height > 0) applySize (proc.width, proc.height);
    buildBar();
    repaint();
}

void Editor::showPresetMenu()
{
    proc.layout = strip.getLayout();
    auto names = proc.listPresets();
    auto startup = Processor::getStartupPreset();
    juce::PopupMenu menu;
    menu.addSectionHeader ("LOAD");
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (100 + i, names[i] + (names[i] == startup ? "   (opens at start)" : ""), true, names[i] == proc.presetName);
    if (names.isEmpty()) menu.addItem (1, "no presets yet", false, false);
    menu.addSeparator();
    menu.addItem (2, "SAVE AS NEW...");
    if (proc.presetName.isNotEmpty())
    {
        menu.addItem (3, "SAVE \"" + proc.presetName + "\"");
        menu.addItem (4, "OPEN \"" + proc.presetName + "\" AT START", true, startup == proc.presetName);
        menu.addItem (5, "DELETE \"" + proc.presetName + "\"");
    }
    menu.addItem (6, "START WITH THE DEFAULT LOOK", true, startup.isEmpty());
    menu.addItem (7, "SHOW PRESETS FOLDER");
    juce::Component::SafePointer<Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, names] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->proc;
        if (r >= 100) { if (p.loadPreset (names[r - 100])) safe->applyLoadedPreset(); return; }
        switch (r)
        {
            case 2:
            {
                auto* w = new juce::AlertWindow ("Save preset", "Name for this look (it will open like this every time):", juce::MessageBoxIconType::NoIcon);
                w->addTextEditor ("name", p.presetName.isEmpty() ? juce::String ("My layout") : p.presetName);
                w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
                w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
                {
                    if (safe != nullptr && result == 1)
                    {
                        safe->proc.layout = safe->strip.getLayout();
                        safe->proc.savePreset (w->getTextEditorContents ("name"), true);
                        safe->buildBar();
                        safe->repaint();
                    }
                }), true);
                break;
            }
            case 3: p.layout = safe->strip.getLayout(); p.savePreset (p.presetName, Processor::getStartupPreset() == p.presetName); break;
            case 4: Processor::setStartupPreset (Processor::getStartupPreset() == p.presetName ? juce::String() : p.presetName); break;
            case 5: p.deletePreset (p.presetName); break;
            case 6: Processor::setStartupPreset ({}); break;
            case 7: Processor::presetFolder().createDirectory(); Processor::presetFolder().startAsProcess(); break;
            default: break;
        }
        safe->buildBar();
        safe->repaint();
    });
}

void Editor::applySize (int w, int h)
{
    if (docked) setDocked (false);
    if (fullscreen) setFullscreen (false);
    if (auto* win = window(); win != nullptr && proc.isStandalone())
        win->setSize (w, h + win->getTitleBarHeight());
    else
        setSize (w, h);
}

//============================================================================== app window modes
void Editor::setPinned (bool on)
{
    proc.pinned = on;
    if (auto* w = window()) w->setAlwaysOnTop (on || docked);
}

void Editor::setDocked (bool on)
{
    auto* w = window();
    if (w == nullptr || on == docked) return;
    if (fullscreen) setFullscreen (false);
    if (on)
    {
        undockedBounds = w->getBounds();
        auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (w->getBounds());
        auto area = display != nullptr ? display->userArea : w->getBounds();
        w->setTitleBarHeight (0);
        w->setResizeLimits (400, 80, 10000, 10000);
        w->setBounds (area.withHeight (150));
        w->setAlwaysOnTop (true);
    }
    else
    {
        w->setTitleBarHeight (26);
        w->setBounds (undockedBounds);
        w->setAlwaysOnTop (proc.pinned);
    }
    docked = on;
}

void Editor::setFullscreen (bool on)
{
    auto* w = window();
    if (w == nullptr || on == fullscreen) return;
    if (docked) setDocked (false);
    if (on) undockedBounds = w->getBounds();
    strip.forceGrid = on;
    w->setFullScreen (on);
    if (! on) w->setBounds (undockedBounds);
    fullscreen = on;
}

void Editor::setSystemAudio (bool on)
{
    proc.wantSystemAudio = on;
    if (! on) { if (proc.isSystemAudio()) proc.stopSystemAudio(); }
    else if (! proc.isSystemAudio())
    {
        auto err = proc.startSystemAudio();
        if (err.isNotEmpty())
        {
            proc.wantSystemAudio = false;
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "System audio", err);
        }
    }
    analyzer.clear();
    buildBar();
    repaint();
}

void Editor::showAudioSettings()
{
#if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        holder->showAudioSettingsDialog();
#endif
}

void Editor::showActivation (bool show)
{
    if (! show)
    {
        if (activation) { removeChildComponent (activation.get()); activation.reset(); }
        return;
    }
    activation = std::make_unique<lt::ui::ActivationComponent> (proc.license, ltPalette);
    activation->onDismiss = [this]
    {
        proc.refreshLicense();
        juce::Component::SafePointer<Editor> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->showActivation (false); });
    };
    addAndMakeVisible (*activation);
    activation->setBounds (getLocalBounds());
    if (getHeight() < 420 && ! proc.isStandalone())
        setSize (juce::jmax (getWidth(), 760), 560);   // the activation card needs room
}

//============================================================================== developer preview
// LT_EDITOR_SNAPSHOT=/path.png  [LT_LAYOUT="0:1,1:1"] [LT_SKIN=0..7] [LT_SIZE=WxH]
void Editor::runSnapshot (const juce::String& file)
{
    showActivation (false);
    auto layoutEnv = juce::SystemStats::getEnvironmentVariable ("LT_LAYOUT", {});
    if (layoutEnv.isNotEmpty()) strip.setLayout (layoutEnv);
    proc.skin = juce::SystemStats::getEnvironmentVariable ("LT_SKIN", "0").getIntValue();
    auto size = juce::SystemStats::getEnvironmentVariable ("LT_SIZE", {});
    if (size.isNotEmpty()) setSize (size.upToFirstOccurrenceOf ("x", false, false).getIntValue(), size.fromFirstOccurrenceOf ("x", false, false).getIntValue());

#if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance()) holder->stopPlaying();   // only the groove below feeds the processor
#endif
    proc.setRateAndBufferSizeDetails (48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    analyzer.prepare (48000.0);
    proc.flushAudio();
    analyzerBase = proc.getSamplesRead();

    // a small synthetic groove at 120 BPM: kick, snare, hats, bass and Am - F - C - G pads
    struct Groove
    {
        juce::int64 t = 0;
        juce::Random rnd { 3 };
        float hp = 0, prevNoise = 0;
        std::array<double, 8> ph {};
        void render (juce::AudioBuffer<float>& b)
        {
            const double sr = 48000.0, beat = 0.5;
            static const int chords[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 48, 52, 55 }, { 55, 59, 62 } };
            static const int bass[4] = { 33, 29, 36, 31 };
            for (int i = 0; i < b.getNumSamples(); ++i, ++t)
            {
                auto sec = t / sr;
                auto bar = (int) (sec / (beat * 4)) % 4;
                auto tb = std::fmod (sec, beat);
                auto t8 = std::fmod (sec, beat * 0.5);
                auto beatNo = (int) (sec / beat) % 4;
                double kick = std::exp (-tb * 9) * std::sin (6.2832 * (50 * tb + 70 * (1 - std::exp (-tb * 30)) / 30));
                auto noise = rnd.nextFloat() * 2 - 1;
                double snare = (beatNo % 2 == 1) ? std::exp (-tb * 18) * (0.6 * noise + 0.4 * std::sin (6.2832 * 190 * tb)) : 0;
                hp = noise - prevNoise; prevNoise = noise;
                double hat = std::exp (-t8 * 60) * hp * 0.5;
                auto bf = 440.0 * std::pow (2.0, (bass[bar] - 69) / 12.0);
                ph[0] += bf / sr;
                double bs = (std::fmod (ph[0], 1.0) * 2 - 1) * 0.25 * (1 - std::exp (-tb * 20) * 0.7);
                double padL = 0, padR = 0;
                for (int k = 0; k < 3; ++k)
                {
                    auto f = 440.0 * std::pow (2.0, (chords[bar][k] - 69) / 12.0);
                    ph[(size_t) k + 1] += f / sr;
                    ph[(size_t) k + 4] += f * 1.004 / sr;
                    padL += std::sin (6.2832 * ph[(size_t) k + 1]) * 0.08;
                    padR += std::sin (6.2832 * ph[(size_t) k + 4]) * 0.08;
                }
                auto mono = 0.45 * kick + 0.22 * snare + 0.2 * hat + 0.6 * bs;
                b.setSample (0, i, (float) (mono + 0.6 * padL));
                b.setSample (1, i, (float) (mono + 0.6 * padR));
            }
        }
    };
    auto groove = std::make_shared<Groove>();
    auto ticker = std::make_shared<std::function<void()>>();
    juce::Component::SafePointer<Editor> safe (this);
    *ticker = [safe, groove, ticker]
    {
        if (safe == nullptr) return;
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer m;
        for (int k = 0; k < 4; ++k) { groove->render (b); safe->proc.processBlock (b, m); }
        juce::Timer::callAfterDelay (16, *ticker);
    };
    juce::Timer::callAfterDelay (50, *ticker);
    juce::Timer::callAfterDelay (6500, [safe, file]
    {
        if (safe == nullptr) return;
        std::cout << "snapshot size " << safe->getWidth() << "x" << safe->getHeight() << std::endl;
        auto img = safe->createComponentSnapshot (safe->getLocalBounds(), true, 2.0f);
        juce::File (file).deleteFile();
        juce::FileOutputStream out { juce::File (file) };
        juce::PNGImageFormat().writeImageToStream (img, out);
        out.flush();
        juce::JUCEApplicationBase::quit();
    });
}

} // namespace cosmos
