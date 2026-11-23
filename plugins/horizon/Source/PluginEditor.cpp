#include "PluginEditor.h"

namespace horizon
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace
{
    constexpr float edgeGrab = 6.0f;

    juce::String timeLabel (double secondsAgo)
    {
        if (secondsAgo < 0.0005) return "NOW";
        if (secondsAgo < 60.0)
            return "-" + juce::String (secondsAgo, secondsAgo < 1.0 ? 2 : (secondsAgo < 10.0 && std::fmod (secondsAgo, 1.0) > 0.01 ? 1 : 0)) + " s";
        auto m = (int) (secondsAgo / 60.0);
        auto s = (int) std::round (secondsAgo - m * 60.0);
        return "-" + juce::String (m) + ":" + juce::String (s).paddedLeft ('0', 2);
    }

    juce::String durationLabel (double seconds)
    {
        if (seconds < 60.0) return juce::String (seconds, 2) + " s";
        auto m = (int) (seconds / 60.0);
        return juce::String (m) + ":" + juce::String (seconds - m * 60.0, 1).paddedLeft ('0', 4);
    }

    juce::String lengthLabel (double seconds)
    {
        if (seconds < 60.0) return juce::String ((int) std::round (seconds)) + " s";
        auto m = (int) (seconds / 60.0);
        auto s = (int) std::round (seconds - m * 60.0);
        return s == 0 ? juce::String (m) + " min" : juce::String (m) + ":" + juce::String (s).paddedLeft ('0', 2);
    }

    const double lengthChoices[] = { 10, 20, 30, 45, 60, 90, 120, 180, 300, 600 };
}

// ================================================================== WaveView

WaveView::WaveView (Processor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::IBeamCursor);
}

juce::Rectangle<float> WaveView::waveArea() const
{
    return getLocalBounds().toFloat().reduced (18.0f, 0.0f).withTrimmedTop (34.0f).withTrimmedBottom (30.0f);
}

juce::int64 WaveView::viewSpan() const
{
    auto full = proc.recorder.getLengthSeconds() * proc.recorder.getSampleRate();
    return (juce::int64) (span > 0.0 ? juce::jmin (span, full) : full);
}

juce::int64 WaveView::viewEnd() const
{
    return following ? proc.recorder.getEnd() : frozenEnd;
}

double WaveView::xToPos (float x) const
{
    auto a = waveArea();
    auto frac = (double) (x - a.getX()) / (double) a.getWidth();
    return (double) viewEnd() - (1.0 - frac) * (double) viewSpan();
}

float WaveView::posToX (juce::int64 pos) const
{
    auto a = waveArea();
    auto frac = 1.0 - (double) (viewEnd() - pos) / (double) viewSpan();
    return a.getX() + (float) frac * a.getWidth();
}

void WaveView::setSpan (double newSpan, float anchorX)
{
    auto sr = proc.recorder.getSampleRate();
    auto full = proc.recorder.getLengthSeconds() * sr;
    auto a = waveArea();
    auto frac = juce::jlimit (0.0, 1.0, (double) (anchorX - a.getX()) / (double) a.getWidth());
    auto anchorPos = xToPos (anchorX);
    newSpan = juce::jlimit (0.25 * sr, full, newSpan);
    auto newEnd = (juce::int64) (anchorPos + (1.0 - frac) * newSpan);
    span = newSpan >= full - 1.0 ? 0.0 : newSpan;
    if (newEnd >= proc.recorder.getEnd()) following = true;
    else { following = false; frozenEnd = newEnd; }
    repaint();
}

void WaveView::tick()
{
    // drop a selection that has rolled out of the buffer
    if (! selection.isEmpty())
    {
        auto avail = proc.recorder.clampToAvailable (selection);
        if (avail.isEmpty() && drag != Drag::select) selection = {};
        else if (drag == Drag::none) selection = avail;
    }
    if (! following && frozenEnd < proc.recorder.getStart())   // the frozen view rolled out of memory
        following = true;
    repaint();
}

// ------------------------------------------------------------------ paint

void WaveView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    Glass::drawPanel (g, b, 18.0f, palette, 0.9f);
    auto a = waveArea();
    auto& rec = proc.recorder;
    auto sr = rec.getSampleRate();
    auto end = viewEnd(), spanS = viewSpan();
    auto start = rec.getStart(), recEnd = rec.getEnd();
    auto paused = rec.isPaused();

    // ---- time grid (relative to now)
    {
        auto spanSec = (double) spanS / sr;
        const double steps[] = { 0.05, 0.1, 0.25, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300 };
        double step = steps[0];
        for (auto s : steps) { step = s; if (spanSec / s <= 9.0) break; }
        auto newestAgo = (double) (recEnd - end) / sr;
        auto first = std::ceil (newestAgo / step) * step;
        g.setFont (Fonts::regular (10.5f));
        for (auto ago = first; ago <= newestAgo + spanSec + 1e-9; ago += step)
        {
            auto x = posToX (recEnd - (juce::int64) (ago * sr));
            if (x < a.getX() - 1.0f || x > a.getRight() + 1.0f) continue;
            g.setColour (palette.text.withAlpha (0.06f));
            g.fillRect (x - 0.5f, a.getY(), 1.0f, a.getHeight());
            g.setColour (palette.textDim.withAlpha (0.7f));
            g.drawText (timeLabel (ago), juce::Rectangle<float> (x - 40.0f, a.getBottom() + 6.0f, 80.0f, 14.0f), juce::Justification::centred);
        }
        g.setColour (palette.text.withAlpha (0.08f));
        g.fillRect (a.getX(), a.getCentreY() - 0.5f, a.getWidth(), 1.0f);
    }

    // ---- waveform
    const float colW = 2.0f;
    auto mid = a.getCentreY(), halfH = a.getHeight() * 0.46f;
    auto spp = (double) spanS / (double) a.getWidth();
    for (float x = a.getX(); x < a.getRight(); x += colW)
    {
        auto p0 = (juce::int64) xToPos (x), p1 = (juce::int64) xToPos (x + colW);
        if (p1 <= start || p0 >= recEnd) continue;
        auto pk = rec.peakOver (juce::jmax (p0, start), juce::jmax (p1, p0 + 1));
        if (pk <= 0.0f) continue;
        auto h = juce::jmax (0.75f, std::pow (juce::jmin (1.0f, pk), 0.65f) * halfH);
        auto ageFrac = juce::jlimit (0.0f, 1.0f, (float) ((double) (recEnd - p1) / (double) juce::jmax ((juce::int64) 1, rec.getCapacity())));
        auto inSel = ! selection.isEmpty() && p1 > selection.getStart() && p0 < selection.getEnd();
        auto c = inSel ? palette.value
                       : palette.nebulaC.interpolatedWith (palette.accent, juce::jlimit (0.0f, 1.0f, 1.0f - ageFrac * 1.6f));
        g.setColour (c.withAlpha ((paused ? 0.45f : 0.9f) * (1.0f - 0.55f * ageFrac)));
        g.fillRoundedRectangle (x, mid - h, colW - 0.6f, h * 2.0f, 0.6f);
    }
    juce::ignoreUnused (spp);

    // ---- the edge of memory (oldest audio about to disappear)
    if (start > 0)
    {
        auto x = posToX (start);
        if (x > a.getX())
        {
            juce::ColourGradient fade (palette.background.withAlpha (0.0f), x + 40.0f, 0, palette.background.withAlpha (0.65f), x, 0, false);
            g.setGradientFill (fade);
            g.fillRect (juce::Rectangle<float> (a.getX(), a.getY(), x + 40.0f - a.getX(), a.getHeight()));
        }
    }

    // ---- selection
    if (! selection.isEmpty())
    {
        auto x0 = juce::jmax (a.getX(), posToX (selection.getStart()));
        auto x1 = juce::jmin (a.getRight(), posToX (selection.getEnd()));
        if (x1 > x0)
        {
            auto r = juce::Rectangle<float> (x0, a.getY(), x1 - x0, a.getHeight());
            g.setColour (palette.accent.withAlpha (0.13f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (palette.accent.withAlpha (0.9f));
            g.fillRect (x0 - 1.0f, a.getY(), 2.0f, a.getHeight());
            g.fillRect (x1 - 1.0f, a.getY(), 2.0f, a.getHeight());
            for (auto hx : { x0, x1 })
                g.fillRoundedRectangle (hx - 4.0f, mid - 12.0f, 8.0f, 24.0f, 3.0f);

            auto label = durationLabel ((double) selection.getLength() / sr) + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  DRAG OUT \xe2\x86\x97"));
            auto font = Fonts::semiBold (11.0f);
            auto w = juce::GlyphArrangement::getStringWidth (font, label) + 24.0f;
            auto pill = juce::Rectangle<float> (w, 22.0f).withCentre ({ juce::jlimit (a.getX() + w / 2, a.getRight() - w / 2, (x0 + x1) / 2), a.getY() - 17.0f });
            Glass::drawRectGlow (g, pill, 11.0f, palette.accent, 8.0f, 0.35f);
            g.setColour (palette.accent);
            g.fillRoundedRectangle (pill, 11.0f);
            g.setColour (palette.accentText);
            g.setFont (font);
            g.drawText (label, pill, juce::Justification::centred);
        }
    }

    // ---- preview playhead
    if (auto pp = proc.getPreviewPosition(); pp >= 0)
    {
        auto x = posToX (pp);
        if (x >= a.getX() && x <= a.getRight())
        {
            Glass::drawGlow (g, { x, mid }, 30.0f, palette.value, 0.25f);
            g.setColour (palette.value);
            g.fillRect (x - 1.0f, a.getY(), 2.0f, a.getHeight());
        }
    }

    // ---- hover line (alt = preview)
    if (hoverX >= a.getX() && juce::ModifierKeys::currentModifiers.isAltDown())
    {
        g.setColour (palette.value.withAlpha (0.5f));
        g.fillRect (hoverX - 0.5f, a.getY(), 1.0f, a.getHeight());
    }

    // ---- the horizon: "now"
    if (following)
    {
        auto x = a.getRight();
        auto glow = paused ? 0.25f : 0.55f + 0.45f * juce::jmin (1.0f, rec.peakOver (recEnd - 4096, recEnd) * 2.0f);
        juce::ColourGradient haze (palette.accent.withAlpha (0.0f), x - 70.0f, 0, palette.accent.withAlpha (0.22f * glow), x, 0, false);
        g.setGradientFill (haze);
        g.fillRect (juce::Rectangle<float> (x - 70.0f, a.getY(), 70.0f, a.getHeight()));
        Glass::drawGlow (g, { x, mid }, 46.0f, palette.accent, 0.5f * glow);
        g.setColour (paused ? palette.textDim : palette.accent.brighter (0.3f));
        g.fillRect (x - 1.0f, a.getY() - 4.0f, 2.0f, a.getHeight() + 8.0f);
    }
    else
    {
        g.setFont (Fonts::semiBold (10.0f));
        g.setColour (palette.textDim);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("LIVE \xe2\x86\x92")), juce::Rectangle<float> (a.getRight() - 70.0f, 8.0f, 70.0f, 18.0f),
                    juce::Justification::centredRight);
    }

    // ---- empty / paused states
    g.setFont (Fonts::regular (12.5f));
    if (recEnd == 0 || rec.peakOver (start, recEnd) <= 0.0f)
    {
        g.setColour (palette.textDim);
        g.drawText ("Listening. Everything that plays through here is kept for the last " + lengthLabel (rec.getLengthSeconds()) + ".",
                    a.toNearestInt(), juce::Justification::centred);
    }
    if (paused)
    {
        auto chip = juce::Rectangle<float> (92.0f, 22.0f).withCentre ({ a.getCentreX(), 17.0f });
        Glass::drawPill (g, chip, palette, false, false, false);
        g.setColour (palette.value);
        g.setFont (Fonts::semiBold (11.0f));
        Fonts::drawSpaced (g, "PAUSED", chip, Fonts::semiBold (11.0f), 2.0f, juce::Justification::centred);
    }
}

// ------------------------------------------------------------------ mouse

void WaveView::updateCursor (const juce::MouseEvent& e)
{
    auto cursor = juce::MouseCursor::IBeamCursor;
    if (! selection.isEmpty())
    {
        auto x0 = posToX (selection.getStart()), x1 = posToX (selection.getEnd());
        if (std::abs (e.position.x - x0) < edgeGrab || std::abs (e.position.x - x1) < edgeGrab) cursor = juce::MouseCursor::LeftRightResizeCursor;
        else if (e.position.x > x0 && e.position.x < x1) cursor = juce::MouseCursor::DraggingHandCursor;
    }
    if (e.mods.isAltDown()) cursor = juce::MouseCursor::PointingHandCursor;
    setMouseCursor (cursor);
}

void WaveView::mouseMove (const juce::MouseEvent& e)
{
    hoverX = e.position.x;
    updateCursor (e);
}

void WaveView::mouseDown (const juce::MouseEvent& e)
{
    exportStarted = false;
    drag = Drag::none;
    auto pos = (juce::int64) xToPos (e.position.x);

    if (e.mods.isMiddleButtonDown())
    {
        drag = Drag::pan;
        dragStartEnd = viewEnd();
        return;
    }
    if (e.mods.isPopupMenu() && ! e.mods.isCommandDown())
    {
        if (onRightClick) onRightClick();
        return;
    }
    if (e.mods.isCommandDown() && e.mods.isShiftDown())
    {
        drag = Drag::zoom;
        dragStartSpan = (double) viewSpan();
        return;
    }
    if (e.mods.isCommandDown())
    {
        proc.recorder.setPaused (! proc.recorder.isPaused());
        if (onPauseToggled) onPauseToggled();
        repaint();
        return;
    }
    if (e.mods.isAltDown())
    {
        if (proc.isPreviewing()) { proc.stopPreview(); return; }
        if (selection.contains (pos)) proc.startPreview (selection);
        else                          proc.startPreview ({ pos, proc.recorder.getEnd() });
        return;
    }

    if (! selection.isEmpty())
    {
        auto x0 = posToX (selection.getStart()), x1 = posToX (selection.getEnd());
        if (std::abs (e.position.x - x0) < edgeGrab) { drag = Drag::moveStart; return; }
        if (std::abs (e.position.x - x1) < edgeGrab) { drag = Drag::moveEnd; return; }
        if (e.position.x > x0 && e.position.x < x1)  { drag = Drag::exportOut; return; }
    }
    drag = Drag::select;
    dragAnchor = pos;
    selection = {};
    repaint();
}

void WaveView::mouseDrag (const juce::MouseEvent& e)
{
    auto pos = (juce::int64) xToPos (e.position.x);
    auto& rec = proc.recorder;
    switch (drag)
    {
        case Drag::select:
            selection = rec.clampToAvailable (juce::Range<juce::int64>::between (dragAnchor, pos));
            break;
        case Drag::moveStart:
            selection = rec.clampToAvailable (juce::Range<juce::int64>::between (pos, selection.getEnd()));
            if (pos > selection.getEnd()) drag = Drag::moveEnd;
            break;
        case Drag::moveEnd:
            selection = rec.clampToAvailable (juce::Range<juce::int64>::between (selection.getStart(), pos));
            if (pos < selection.getStart()) drag = Drag::moveStart;
            break;
        case Drag::exportOut:
            if (! exportStarted && e.getDistanceFromDragStart() > 6)
            {
                exportStarted = true;
                startExportDrag();
            }
            break;
        case Drag::zoom:
            setSpan (dragStartSpan * std::exp (-(double) e.getDistanceFromDragStartX() * 0.012
                                               + (double) e.getDistanceFromDragStartY() * 0.012), (float) e.getMouseDownX());
            break;
        case Drag::pan:
        {
            auto spp = (double) viewSpan() / (double) waveArea().getWidth();
            auto newEnd = dragStartEnd - (juce::int64) ((double) e.getDistanceFromDragStartX() * spp);
            if (newEnd >= rec.getEnd()) following = true;
            else { following = false; frozenEnd = juce::jmax (newEnd, rec.getStart() + viewSpan() / 4); }
            break;
        }
        case Drag::none: break;
    }
    repaint();
}

void WaveView::mouseUp (const juce::MouseEvent&)
{
    if (drag == Drag::select && selection.getLength() < (juce::int64) (0.01 * proc.recorder.getSampleRate()))
        selection = {};
    drag = Drag::none;
    repaint();
}

void WaveView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isAnyModifierKeyDown()) return;
    selection = {};
    repaint();
}

void WaveView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (std::abs (w.deltaX) > std::abs (w.deltaY) * 1.5f && ! w.isInertial)
    {
        auto spp = (double) viewSpan() / (double) waveArea().getWidth();
        auto newEnd = viewEnd() + (juce::int64) ((double) w.deltaX * 400.0 * spp * (w.isReversed ? -1.0 : 1.0));
        if (newEnd >= proc.recorder.getEnd()) following = true;
        else { following = false; frozenEnd = juce::jmax (newEnd, proc.recorder.getStart() + viewSpan() / 4); }
        repaint();
        return;
    }
    setSpan ((double) viewSpan() * std::exp (-(double) w.deltaY * 1.6), e.position.x);
}

void WaveView::startExportDrag()
{
    juce::String error;
    auto file = proc.exportRange (selection, error);
    if (file == juce::File())
    {
        if (onMessage) onMessage (error);
        return;
    }
    if (onMessage) onMessage ("Saved " + file.getFileName());
    performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

void WaveView::exportSelection (bool reveal)
{
    if (selection.isEmpty()) { if (onMessage) onMessage ("Select something first: drag across the waveform."); return; }
    juce::String error;
    auto file = proc.exportRange (selection, error);
    if (file == juce::File()) { if (onMessage) onMessage (error); return; }
    if (onMessage) onMessage ("Saved " + file.getFileName());
    if (reveal) file.revealToUser();
}

// ================================================================== Editor

bool Editor::activationShownThisSession = false;

Editor::Editor (Processor& p) : AudioProcessorEditor (p), proc (p), wave (p)
{
    lnf.setPalette (palette);
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    background.setPalette (palette);
    background.setParticleAmount (0.45f);
    background.setDriftSpeed (0.6f);
    background.setInterceptsMouseClicks (false, false);
    content.addAndMakeVisible (background);
    content.addAndMakeVisible (decor);
    content.addAndMakeVisible (wave);
    wave.setPalette (palette);
    wave.onMessage = [this] (const juce::String& m) { flash (m); };
    wave.onPauseToggled = [this] { refreshButtons(); };
    wave.onRightClick = [this] { showSettingsMenu(); };

    recButton.onClick = [this] { proc.recorder.setPaused (! proc.recorder.isPaused()); refreshButtons(); };
    lengthButton.onClick = [this] { showLengthMenu(); };
    bpmButton.setClickingTogglesState (true);
    bpmButton.setToggleState (GlobalSettings::bpmInName(), juce::dontSendNotification);
    bpmButton.onClick = [this] { GlobalSettings::setBpmInName (bpmButton.getToggleState()); refreshButtons(); };
    folderButton.onClick = [this] { showSettingsMenu(); };
    saveButton.onClick = [this] { wave.exportSelection (true); };
    settingsButton.onClick = [this] { showSettingsMenu(); };
    licenseButton.onClick = [this] { showActivation (true); };

    recButton.setTooltip ("Pause / resume recording (or cmd/ctrl + click the waveform)");
    lengthButton.setTooltip ("How much audio is kept");
    bpmButton.setTooltip ("Put the host tempo in the file name, so DAWs don't auto-warp the clip");
    folderButton.setTooltip ("Where dragged-out files are saved");
    saveButton.setTooltip ("Save the selection and show it in the file browser");

    for (auto* b : { &recButton, &lengthButton, &bpmButton, &folderButton, &saveButton, &settingsButton, &licenseButton })
        content.addAndMakeVisible (b);

    setResizable (true, true);
    setResizeLimits (baseWidth * 6 / 10, baseHeight * 6 / 10, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (baseWidth * 9 / 10, baseHeight * 9 / 10);

    if (! proc.license.isLicensed() && (! activationShownThisSession || ! proc.license.isUsable()))
    {
        activationShownThisSession = true;
        showActivation (true);
    }
    refreshButtons();
    startTimerHz (30);

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        activation.reset();
        proc.recorder.setPaused (true);                // a fake 30 s beat so the waveform shows something
        {
            auto sr = proc.recorder.getSampleRate();
            juce::AudioBuffer<float> fake (2, (int) (sr * 30.0));
            juce::Random rnd (3);
            for (int i = 0; i < fake.getNumSamples(); ++i)
            {
                auto t = (double) i / sr, beat = std::fmod (t, 0.5), half = std::fmod (t + 0.25, 1.0);
                auto v = (float) (0.8 * std::exp (-beat * 14.0) * std::sin (2.0 * juce::MathConstants<double>::pi * 55.0 * beat)
                                + 0.35 * std::exp (-half * 22.0) * (rnd.nextFloat() * 2.0 - 1.0)
                                + 0.08 * std::sin (t * 1400.0) * (0.5 + 0.5 * std::sin (t * 0.4)));
                fake.setSample (0, i, v);
                fake.setSample (1, i, v * 0.9f);
            }
            proc.recorder.setPaused (false);
            proc.recorder.push (fake.getArrayOfReadPointers(), 2, fake.getNumSamples());
            proc.recorder.setPaused (true);
            auto end = proc.recorder.getEnd();
            wave.setSelection ({ end - (juce::int64) (sr * 12.0), end - (juce::int64) (sr * 8.0) });
            proc.recorder.setPaused (false);
        }
        juce::Timer::callAfterDelay (2500, [this, snapshot]
        {
            auto img = content.createComponentSnapshot (content.getLocalBounds(), true, 1.0f);
            juce::File (snapshot).deleteFile();
            if (auto out = juce::File (snapshot).createOutputStream())
                juce::PNGImageFormat().writeImageToStream (img, *out);
        });
    }
}

Editor::~Editor()
{
    setLookAndFeel (nullptr);
}

void Editor::paint (juce::Graphics& g)
{
    g.fillAll (palette.background);
}

void Editor::resized()
{
    auto scale = (float) getWidth() / (float) baseWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, baseWidth, baseHeight);

    background.setBounds (0, 0, baseWidth, baseHeight);
    decor.setBounds (0, 0, baseWidth, baseHeight);
    wave.setBounds (32, 84, baseWidth - 64, 262);

    int x = baseWidth - 32;
    auto place = [&x] (juce::Component& c, int w) { x -= w; c.setBounds (x, 28, w, 30); x -= 10; };
    place (settingsButton, 40);
    place (folderButton, 92);
    place (bpmButton, 128);
    place (lengthButton, 96);
    place (recButton, 108);
    licenseButton.setBounds (262, 33, 150, 22);
    saveButton.setBounds (baseWidth - 32 - 150, 356, 150, 28);
    if (activation != nullptr) activation->setBounds (0, 0, baseWidth, baseHeight);
}

void Editor::paintContent (juce::Graphics& g)
{
    lt::ui::Logo::draw (g, { 32.0f, 24.0f, 220.0f, 38.0f }, lt::ui::Logo::specFor ("HORIZON"), palette, juce::Justification::centredLeft);

    auto hintArea = juce::Rectangle<float> (34.0f, 356.0f, (float) baseWidth - 34.0f - 200.0f, 28.0f);
    if (message.isNotEmpty() && juce::Time::getMillisecondCounterHiRes() < messageUntil)
    {
        g.setColour (palette.value);
        g.setFont (Fonts::semiBold (12.0f));
        g.drawText (message, hintArea, juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (palette.textDim);
        g.setFont (Fonts::regular (11.0f));
        auto cmd = juce::String (
           #if JUCE_MAC
            "cmd"
           #else
            "ctrl"
           #endif
        );
        auto dot = juce::String::fromUTF8 ("   \xc2\xb7   ");
        g.drawText ("drag: select" + dot + "drag the selection out: save it" + dot + "alt+click: preview" + dot
                    + cmd + "+click: pause" + dot + cmd + "+shift+drag or wheel: zoom" + dot + "middle drag: move",
                    hintArea, juce::Justification::centredLeft, true);
    }
}

void Editor::flash (const juce::String& m)
{
    message = m;
    messageUntil = juce::Time::getMillisecondCounterHiRes() + 3500.0;
    decor.repaint();
}

void Editor::refreshButtons()
{
    auto paused = proc.recorder.isPaused() || ! proc.license.isUsable();
    recButton.setButtonText (paused ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6  RESUME")) : juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x8f  REC")));
    recButton.getProperties().set ("accent", ! paused);
    lengthButton.setButtonText (lengthLabel (proc.recorder.getLengthSeconds()) + juce::String (juce::CharPointer_UTF8 ("  \xe2\x96\xbe")));
    auto bpm = proc.getHostBpm();
    bpmButton.setButtonText (bpm > 0.0 ? juce::String (juce::roundToInt (bpm)) + " BPM IN NAME" : juce::String ("BPM IN NAME"));
    bpmButton.setToggleState (GlobalSettings::bpmInName(), juce::dontSendNotification);
    saveButton.setEnabled (wave.hasSelection());

    auto chip = proc.license.isLicensed() ? juce::String() : proc.license.isUsable()
                  ? juce::String (juce::CharPointer_UTF8 ("TRIAL \xc2\xb7 ")) + juce::String (proc.license.getTrialDaysLeft()) + " DAYS"
                  : juce::String ("ACTIVATE");
    if (licenseButton.getButtonText() != chip) licenseButton.setButtonText (chip);
    licenseButton.setVisible (chip.isNotEmpty());
    for (auto* b : { &recButton, &lengthButton, &bpmButton, &saveButton })
        b->repaint();
}

void Editor::timerCallback()
{
    wave.tick();
    auto end = proc.recorder.getEnd();
    auto pk = proc.recorder.peakOver (end - 8192, end);
    level += (pk - level) * (pk > level ? 0.5f : 0.08f);
    background.setEnergy (proc.recorder.isPaused() ? 0.0f : level);
    background.setFrozen (proc.recorder.isPaused());
    static int n = 0;
    if (++n % 10 == 0) refreshButtons();
    if (message.isNotEmpty() && juce::Time::getMillisecondCounterHiRes() > messageUntil)
    {
        message = {};
        decor.repaint();
    }
}

void Editor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) showSettingsMenu();
}

void Editor::showLengthMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("KEEP THE LAST");
    auto current = proc.recorder.getLengthSeconds();
    for (int i = 0; i < (int) std::size (lengthChoices); ++i)
    {
        auto mb = lengthChoices[i] * proc.recorder.getSampleRate() * 2.0 * 4.0 / (1024.0 * 1024.0);
        m.addItem (i + 1, lengthLabel (lengthChoices[i]) + "   (" + juce::String (juce::roundToInt (mb)) + " MB)", true,
                   juce::approximatelyEqual (current, lengthChoices[i]));
    }
    m.addSeparator();
    m.addItem (100, "Use this length for new instances", true, juce::approximatelyEqual (GlobalSettings::defaultLength(), current));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lengthButton), [this] (int r)
    {
        if (r == 100) { GlobalSettings::setDefaultLength (proc.recorder.getLengthSeconds()); return; }
        if (r >= 1 && r <= (int) std::size (lengthChoices))
        {
            proc.setLength (lengthChoices[r - 1]);
            wave.clearSelection();
            refreshButtons();
        }
    });
}

void Editor::showSettingsMenu()
{
    juce::PopupMenu m, lengths;
    auto current = proc.recorder.getLengthSeconds();
    for (int i = 0; i < (int) std::size (lengthChoices); ++i)
        lengths.addItem (200 + i, lengthLabel (lengthChoices[i]), true, juce::approximatelyEqual (current, lengthChoices[i]));
    m.addSubMenu ("Buffer length", lengths);
    m.addItem (1, proc.recorder.isPaused() ? "Resume recording" : "Pause recording");
    m.addSeparator();
    m.addItem (2, "Save selection", wave.hasSelection());
    m.addItem (3, "Clear selection", wave.hasSelection());
    m.addSeparator();
    m.addItem (4, "BPM in file names", true, GlobalSettings::bpmInName());
    m.addItem (5, "Export folder: " + GlobalSettings::exportFolder().getFileName() + "...");
    m.addItem (6, "Show export folder");
    m.addSeparator();
    m.addItem (7, proc.license.isLicensed() ? "License..." : "Activate...");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (settingsButton), [this] (int r)
    {
        switch (r)
        {
            case 1: proc.recorder.setPaused (! proc.recorder.isPaused()); break;
            case 2: wave.exportSelection (true); break;
            case 3: wave.clearSelection(); break;
            case 4: GlobalSettings::setBpmInName (! GlobalSettings::bpmInName()); break;
            case 5: chooseFolder(); break;
            case 6: { auto f = GlobalSettings::exportFolder(); f.createDirectory(); f.revealToUser(); break; }
            case 7: showActivation (true); break;
            default:
                if (r >= 200 && r < 200 + (int) std::size (lengthChoices))
                {
                    proc.setLength (lengthChoices[r - 200]);
                    wave.clearSelection();
                }
                break;
        }
        refreshButtons();
    });
}

void Editor::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Where should LT HORIZON save the clips you drag out?", GlobalSettings::exportFolder());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f != juce::File())
        {
            GlobalSettings::setExportFolder (f);
            flash ("Clips will be saved in " + f.getFullPathName());
        }
    });
}

void Editor::showActivation (bool show)
{
    if (show && activation == nullptr)
    {
        proc.license.refresh();
        activation = std::make_unique<lt::ui::ActivationComponent> (proc.license, palette);
        activation->onDismiss = [safe = juce::Component::SafePointer<Editor> (this)] { juce::MessageManager::callAsync ([safe]
                                         { if (safe != nullptr) safe->showActivation (false); }); };
        content.addAndMakeVisible (*activation);
        activation->setBounds (0, 0, baseWidth, baseHeight);
    }
    else if (! show && activation != nullptr)
    {
        proc.refreshLicense();
        if (! proc.license.isUsable()) return;
        content.removeChildComponent (activation.get());
        activation.reset();
        refreshButtons();
    }
}

} // namespace horizon
