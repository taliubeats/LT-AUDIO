#include "Views.h"

namespace typebeats
{

using lt::ui::Fonts;
using lt::ui::Glass;

namespace
{
    juce::String formatTime (double s)
    {
        auto m = (int) (s / 60.0);
        return juce::String (m) + ":" + juce::String ((int) s % 60).paddedLeft ('0', 2);
    }

    juce::Colour smallText (const Palette& p)  { return p.isLight ? p.text : p.textDim; }
}

// ================================================================== WaveformView

void WaveformView::setResult (const AnalysisResult* r, const juce::String& name)
{
    result = r;
    fileName = name;
    rebuildDust();
    repaint();
}

void WaveformView::setPlayhead (double s, bool isPlaying)
{
    if (std::abs (s - playhead) > 0.01 || isPlaying != playing)
    {
        playhead = s;
        playing = isPlaying;
        repaint();
    }
}

void WaveformView::setBusy (bool b, float p, const juce::String& s)
{
    if (b != busy || std::abs (p - progress) > 0.005f || s != status)
    {
        busy = b; progress = p; status = s;
        repaint();
    }
}

void WaveformView::rebuildDust()
{
    dust.clear();
    if (result == nullptr || result->peaks.empty() || getWidth() <= 0)
        return;

    juce::Random rnd (42);
    auto area = getLocalBounds().toFloat().reduced (16.0f, 26.0f);
    auto cy = area.getCentreY();
    auto half = area.getHeight() * 0.5f;
    auto& peaks = result->peaks;

    for (float x = area.getX(); x < area.getRight(); x += 1.6f)
    {
        auto idx = (size_t) juce::jlimit (0, (int) peaks.size() - 1, (int) ((x - area.getX()) / area.getWidth() * (float) peaks.size()));
        auto a = std::pow (peaks[idx], 0.8f);
        auto count = 2 + (int) (a * 9.0f);
        for (int i = 0; i < count; ++i)
        {
            // denser near the centre line, sparser at the edges -> "stardust"
            auto r = (rnd.nextFloat() + rnd.nextFloat() + rnd.nextFloat()) / 3.0f * 2.0f - 1.0f;
            auto y = cy + r * a * half * 1.15f;
            dust.push_back ({ x + rnd.nextFloat() * 1.6f, y, 0.5f + rnd.nextFloat() * (0.6f + a * 1.4f), 0.25f + 0.75f * rnd.nextFloat() });
        }
        if (rnd.nextFloat() < 0.04f)
            dust.push_back ({ x, cy + (rnd.nextFloat() * 2 - 1) * half, 1.0f + rnd.nextFloat() * 1.5f, 0.6f });
    }
}

juce::Rectangle<float> WaveformView::fileChip() const
{
    if (fileName.isEmpty()) return {};
    auto w = juce::jmin (300.0f, juce::GlyphArrangement::getStringWidth (Fonts::regular (13.0f), fileName) + 52.0f);
    return { (float) getWidth() - w - 14.0f, 12.0f, w, 28.0f };
}

void WaveformView::paint (juce::Graphics& g)
{
    auto& p = pal();
    auto b = getLocalBounds().toFloat();
    Glass::drawPanel (g, b.reduced (0.5f), 14.0f, p, 0.9f);

    auto area = b.reduced (16.0f, 26.0f);
    auto cy = area.getCentreY();

    if (result != nullptr && ! result->peaks.empty())
    {
        auto playX = area.getX() + (float) (playhead / juce::jmax (0.001, result->duration)) * area.getWidth();
        auto dustColour = p.isLight ? p.value : p.accent.brighter (0.5f);

        // soft glow along the centre
        juce::ColourGradient band (dustColour.withAlpha (0.0f), area.getX(), cy - area.getHeight() * 0.3f,
                                   dustColour.withAlpha (0.0f), area.getX(), cy + area.getHeight() * 0.3f, false);
        band.addColour (0.5, dustColour.withAlpha (0.10f));
        g.setGradientFill (band);
        g.fillRect (area);

        for (auto& d : dust)
        {
            auto played = d.x <= playX;
            auto colour = (played || ! playing) ? dustColour : dustColour.withMultipliedSaturation (0.5f);
            g.setColour (colour.withAlpha (d.alpha * (played ? 1.0f : 0.8f)));
            g.fillEllipse (d.x, d.y, d.size, d.size);
        }
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawHorizontalLine ((int) cy, area.getX(), area.getRight());

        // playhead
        g.setColour ((p.isLight ? p.text : juce::Colours::white).withAlpha (0.6f));
        g.drawLine (playX, b.getY() + 10.0f, playX, b.getBottom() - 10.0f, 1.0f);
        juce::Path tri;
        tri.addTriangle (playX - 5.0f, b.getY() + 6.0f, playX + 5.0f, b.getY() + 6.0f, playX, b.getY() + 12.0f);
        g.fillPath (tri);

        // time labels
        g.setColour (smallText (p));
        g.setFont (Fonts::regular (11.0f));
        g.drawText (formatTime (playhead), juce::Rectangle<float> (playX + 6.0f, b.getBottom() - 22.0f, 60.0f, 16.0f), juce::Justification::centredLeft);
        g.drawText (formatTime (result->duration), b.withTrimmedRight (16.0f).removeFromBottom (22.0f), juce::Justification::centredRight);

        if (! playing)
        {
            g.setColour (smallText (p).withMultipliedAlpha (0.8f));
            g.setFont (Fonts::regular (11.5f));
            g.drawText (juce::String (juce::CharPointer_UTF8 ("CLICK TO PREVIEW  \xc2\xb7  DROP ANOTHER SONG TO REPLACE")), b.removeFromBottom (22.0f), juce::Justification::centred);
        }
    }
    else if (! busy)
    {
        g.setColour (p.text.withAlpha (0.75f));
        g.setFont (Fonts::regular (16.0f));
        g.drawText ("DRAG & DROP AUDIO OR BROWSE", b, juce::Justification::centred);
        g.setColour (smallText (p).withMultipliedAlpha (0.8f));
        g.setFont (Fonts::regular (12.0f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("WAV  \xc2\xb7  MP3  \xc2\xb7  AIFF  \xc2\xb7  FLAC  \xc2\xb7  M4A")), b.translated (0, 26.0f), juce::Justification::centred);
    }

    // file chip with a drag icon (drag the audio into an Ableton audio track)
    if (auto chip = fileChip(); ! chip.isEmpty())
    {
        Glass::drawPill (g, chip, p, false, isMouseOver() && chip.contains (getMouseXYRelative().toFloat()), false);
        g.setColour (p.text);
        g.setFont (Fonts::regular (13.0f));
        g.drawFittedText (fileName, chip.reduced (14.0f, 0).withTrimmedRight (22.0f).toNearestInt(), juce::Justification::centredLeft, 1);

        auto icon = chip.removeFromRight (30.0f).reduced (8.0f);
        g.drawRoundedRectangle (icon.reduced (1.0f), 2.0f, 1.0f);
        g.drawRoundedRectangle (icon.translated (3.0f, -3.0f).reduced (1.0f), 2.0f, 1.0f);
    }

    if (busy)
    {
        g.setColour (p.background.withAlpha (0.35f));
        g.fillRoundedRectangle (b, 14.0f);
        auto bar = juce::Rectangle<float> (b.getWidth() * 0.4f, 4.0f).withCentre (b.getCentre().translated (0, 18.0f));
        g.setColour (p.text.withAlpha (0.15f));
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (p.accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.02f, 1.0f, progress)), 2.0f);
        g.setColour (p.text);
        g.setFont (Fonts::regular (14.0f));
        g.drawText (status, b.translated (0, -10.0f), juce::Justification::centred);
    }
}

void WaveformView::mouseMove (const juce::MouseEvent&) { repaint(); }

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    draggingOut = false;
    if (busy) return;

    if (fileChip().contains (e.position))
        return; // a drag may follow

    if (result == nullptr || result->peaks.empty())
    {
        if (onBrowse) onBrowse();
        return;
    }

    auto area = getLocalBounds().toFloat().reduced (16.0f, 26.0f);
    auto t = juce::jlimit (0.0, 1.0, (double) ((e.position.x - area.getX()) / area.getWidth())) * result->duration;
    if (onClick) onClick (t);
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingOut && fileChip().contains (e.mouseDownPosition) && e.getDistanceFromDragStart() > 4)
    {
        draggingOut = true;
        if (onDragOut) onDragOut();
    }
}

// ================================================================== BpmView

BpmView::BpmView()
{
    tapButton.onClick = [this] { tap(); };
    halfButton.onClick = [this] { if (bpm > 0) { setBpm (bpm / 2.0, tapped); if (onBpmChanged) onBpmChanged (bpm); } };
    doubleButton.onClick = [this] { if (bpm > 0) { setBpm (bpm * 2.0, tapped); if (onBpmChanged) onBpmChanged (bpm); } };
    for (auto* b : { &tapButton, &halfButton, &doubleButton })
        addAndMakeVisible (*b);
    startTimerHz (30);
}

void BpmView::setBpm (double v, bool t)
{
    bpm = v;
    tapped = t;
    repaint();
}

void BpmView::tap()
{
    auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    if (! taps.empty() && now - taps.back() > 2.0)
        taps.clear();
    taps.push_back (now);
    if (taps.size() > 8) taps.erase (taps.begin());

    if (taps.size() >= 3)
    {
        auto interval = (taps.back() - taps.front()) / (double) (taps.size() - 1);
        setBpm (std::round (600.0 / interval) / 10.0, true);
        if (onBpmChanged) onBpmChanged (bpm);
    }
}

void BpmView::resized()
{
    auto b = getLocalBounds();
    tapButton.setBounds (b.getRight() - 84, 4, 76, 32);
    halfButton.setBounds (b.getRight() - 84, b.getBottom() - 36, 36, 28);
    doubleButton.setBounds (b.getRight() - 44, b.getBottom() - 36, 36, 28);
}

void BpmView::paint (juce::Graphics& g)
{
    auto& p = pal();
    auto b = getLocalBounds().toFloat();

    g.setColour (p.text);
    g.setFont (Fonts::light (20.0f));
    g.drawText ("BPM", b.withHeight (36.0f).withTrimmedLeft (4.0f), juce::Justification::centredLeft);

    g.setColour (smallText (p));
    g.setFont (Fonts::regular (10.5f));
    g.drawText (bpm <= 0 ? "" : tapped ? "TAPPED" : "DETECTED",
                juce::Rectangle<float> (b.getRight() - 160.0f, 38.0f, 152.0f, 14.0f), juce::Justification::centredRight);

    auto c = juce::Point<float> (b.getCentreX() - 20.0f, b.getCentreY() + 14.0f);
    auto radius = juce::jmin (b.getWidth() * 0.34f, b.getHeight() * 0.36f);

    // pulsar rings, beating at the song tempo
    auto beatPhase = 0.0f;
    if (bpm > 0 && active)
    {
        auto t = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        beatPhase = (float) std::fmod (t * bpm / 60.0, 1.0);
    }
    auto pulse = std::exp (-beatPhase * 4.0f);
    auto ringColour = p.isLight ? juce::Colours::white : p.accent;

    Glass::drawGlow (g, c, radius * (1.25f + 0.15f * pulse), ringColour, 0.25f + 0.25f * pulse);
    for (int i = 0; i < 4; ++i)
    {
        auto rr = radius * (0.78f + 0.12f * (float) i) + (bpm > 0 ? beatPhase * 10.0f : 0.0f);
        g.setColour (ringColour.withAlpha ((0.55f - 0.12f * (float) i) * (0.5f + 0.5f * pulse)));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 1.4f);
    }

    auto text = bpm > 0 ? (std::abs (bpm - std::round (bpm)) < 0.05 ? juce::String ((int) std::round (bpm)) : juce::String (bpm, 1))
                        : juce::String ("--");
    g.setFont (Fonts::regular (radius * 0.9f));
    auto numArea = juce::Rectangle<float> (radius * 2.4f, radius * 1.2f).withCentre (c);
    if (p.isLight) { g.setColour (juce::Colour (0x33000000)); g.drawText (text, numArea.translated (0, 2.0f), juce::Justification::centred); }
    g.setColour (p.value);
    g.drawText (text, numArea, juce::Justification::centred);
}

// ================================================================== KeyView

KeyView::KeyView() { startTimerHz (30); }

void KeyView::setKeys (const std::vector<KeyCandidate>* k)
{
    keys = k;
    repaint();
}

juce::Point<float> KeyView::planetCentre() const
{
    return { (float) getWidth() * 0.64f, (float) getHeight() * 0.50f };
}

juce::Point<float> KeyView::moonPosition (int i) const
{
    auto c = planetCentre();
    auto t = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    auto base = -0.9f + (float) i * 0.95f;
    auto ang = base + 0.05f * (float) std::sin (t * 0.6 + i);
    auto r = juce::jmin ((float) getHeight() * 0.36f, 58.0f);
    return { c.x + std::cos (ang) * r * 1.05f, c.y + std::sin (ang) * r * 0.95f };
}

juce::Rectangle<float> KeyView::moonLabel (int i) const
{
    auto m = moonPosition (i);
    return { m.x + 10.0f, m.y - 16.0f, 118.0f, 32.0f };
}

void KeyView::paint (juce::Graphics& g)
{
    auto& p = pal();
    auto b = getLocalBounds().toFloat();

    g.setColour (p.text);
    g.setFont (Fonts::light (20.0f));
    g.drawText ("KEY", b.withHeight (36.0f).withTrimmedLeft (4.0f), juce::Justification::centredLeft);

    if (keys == nullptr || keys->empty())
    {
        g.setColour (smallText (p));
        g.setFont (Fonts::light (34.0f));
        g.drawText ("--", b.withTrimmedTop (30.0f).withWidth (b.getWidth() * 0.5f), juce::Justification::centred);
        return;
    }

    auto& top = keys->front();
    auto nameArea = juce::Rectangle<float> (4.0f, b.getHeight() * 0.24f, b.getWidth() * 0.46f, b.getHeight() * 0.40f);
    g.setFont (Fonts::regular (juce::jmin (40.0f, nameArea.getHeight() * 0.7f)));
    if (p.isLight) { g.setColour (juce::Colour (0x33000000)); g.drawFittedText (top.name(), nameArea.translated (0, 2.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f); }
    g.setColour (p.value);
    g.drawFittedText (top.name(), nameArea.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);

    // planet
    auto c = planetCentre();
    auto pr = juce::jmin ((float) getHeight() * 0.17f, 28.0f);
    auto planetCol = p.isLight ? p.value : p.accent;
    Glass::drawGlow (g, c, pr * 2.4f, planetCol, 0.45f);
    juce::ColourGradient grad (planetCol.brighter (0.7f), c.x - pr * 0.5f, c.y - pr * 0.6f, planetCol.darker (0.6f), c.x + pr, c.y + pr, true);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - pr, c.y - pr, pr * 2.0f, pr * 2.0f);
    juce::Path ring;
    ring.addEllipse (-pr * 1.7f, -pr * 0.35f, pr * 3.4f, pr * 0.7f);
    ring.applyTransform (juce::AffineTransform::rotation (-0.3f).translated (c.x, c.y));
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.strokePath (ring, juce::PathStrokeType (1.2f));

    // orbit + moons (alternative keys)
    auto orbitR = juce::jmin ((float) getHeight() * 0.36f, 58.0f);
    g.setColour (p.text.withAlpha (0.15f));
    g.drawEllipse (c.x - orbitR * 1.05f, c.y - orbitR * 0.95f, orbitR * 2.1f, orbitR * 1.9f, 1.0f);

    for (int i = 1; i < (int) keys->size() && i <= 3; ++i)
    {
        auto m = moonPosition (i - 1);
        auto mr = 6.0f - (float) i;
        Glass::drawGlow (g, m, mr * 3.0f, juce::Colours::white, hoverMoon == i - 1 ? 0.6f : 0.25f);
        juce::ColourGradient mg (juce::Colours::white, m.x - mr * 0.4f, m.y - mr * 0.4f, juce::Colour (0xff8f8aa6), m.x + mr, m.y + mr, true);
        g.setGradientFill (mg);
        g.fillEllipse (m.x - mr, m.y - mr, mr * 2.0f, mr * 2.0f);

        auto lab = moonLabel (i - 1);
        g.setColour (hoverMoon == i - 1 ? p.text : smallText (p));
        g.setFont (Fonts::regular (12.0f));
        g.drawText ((*keys)[(size_t) i].name(), lab.removeFromTop (16.0f), juce::Justification::centredLeft);
        g.drawText (juce::String ((*keys)[(size_t) i].percent) + "%", lab, juce::Justification::centredLeft);
    }

    // confidence chip
    auto chip = juce::Rectangle<float> (46.0f, 22.0f).withCentre ({ c.x - pr * 1.9f, c.y + pr * 1.7f });
    Glass::drawPill (g, chip, p, false, false, false);
    g.setColour (p.text);
    g.setFont (Fonts::regular (11.5f));
    g.drawText (juce::String (top.percent) + "%", chip, juce::Justification::centred);

    g.setColour (smallText (p).withMultipliedAlpha (0.7f));
    g.setFont (Fonts::regular (10.0f));
    g.drawText ("Click a moon to choose it", nameArea.translated (0, nameArea.getHeight() * 0.8f).withHeight (16.0f), juce::Justification::centredLeft);
}

void KeyView::mouseMove (const juce::MouseEvent& e)
{
    auto h = -1;
    for (int i = 0; i < 3 && keys != nullptr && i + 1 < (int) keys->size(); ++i)
        if (moonLabel (i).expanded (14.0f, 4.0f).withLeft (moonPosition (i).x - 10.0f).contains (e.position))
            h = i;
    if (h != hoverMoon) { hoverMoon = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void KeyView::mouseDown (const juce::MouseEvent& e)
{
    mouseMove (e);
    if (hoverMoon >= 0 && onChooseAlternative)
        onChooseAlternative (hoverMoon + 1);
}

// ================================================================== PianoView

void PianoView::setActiveNotes (std::vector<int> pcs, int bassPc)
{
    if (pcs != active || bassPc != bass)
    {
        active = std::move (pcs);
        bass = bassPc;
        repaint();
    }
}

void PianoView::paint (juce::Graphics& g)
{
    auto& p = pal();
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    constexpr int octaves = 2, whites = 7 * octaves;
    static const int whitePcs[] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int blackPcs[] = { 1, 3, -1, 6, 8, 10, -1 };

    auto ww = b.getWidth() / (float) whites;
    auto isActive = [&] (int pc, int octave)
    {
        if (std::find (active.begin(), active.end(), pc) == active.end()) return false;
        // bass note lights in the low octave, chord tones in the upper octave
        return octave == 0 ? pc == bass : true;
    };
    auto lit = p.isLight ? p.value : p.accent;

    for (int i = 0; i < whites; ++i)
    {
        auto key = juce::Rectangle<float> (b.getX() + (float) i * ww, b.getY(), ww, b.getHeight()).reduced (1.0f, 0);
        auto on = isActive (whitePcs[i % 7], i / 7);
        if (on) Glass::drawRectGlow (g, key, 3.0f, lit, 6.0f, 0.6f);
        g.setColour (on ? lit.withAlpha (0.85f) : juce::Colours::white.withAlpha (p.isLight ? 0.55f : 0.16f));
        g.fillRoundedRectangle (key, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (p.isLight ? 0.8f : 0.3f));
        g.drawRoundedRectangle (key, 3.0f, 0.8f);
    }

    for (int i = 0; i < whites; ++i)
    {
        auto pc = blackPcs[i % 7];
        if (pc < 0 || i == whites - 1) continue;
        auto key = juce::Rectangle<float> (b.getX() + ((float) i + 0.68f) * ww, b.getY(), ww * 0.64f, b.getHeight() * 0.6f);
        auto on = isActive (pc, i / 7);
        if (on) Glass::drawRectGlow (g, key, 2.0f, lit, 5.0f, 0.7f);
        g.setColour (on ? lit : (p.isLight ? juce::Colour (0xff2b3a55) : juce::Colour (0xff15121f)));
        g.fillRoundedRectangle (key, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (key, 2.5f, 0.8f);
    }
}

// ================================================================== ChordView

ChordView::ChordView()
{
    prevButton.onClick = [this] { scroll (-visible); };
    nextButton.onClick = [this] { scroll (visible); };
    addChildComponent (prevButton);
    addChildComponent (nextButton);
}

void ChordView::setResult (AnalysisResult* r)
{
    result = r;
    first = 0; selected = -1; current = -1;
    followPlayhead = true;
    altButtons.clear();
    bubble = {};
    resized();
    repaint();
}

void ChordView::scroll (int delta)
{
    if (result == nullptr) return;
    followPlayhead = false;
    first = juce::jlimit (0, juce::jmax (0, (int) result->chords.size() - visible), first + delta);
    altButtons.clear();
    bubble = {};
    resized();
    repaint();
}

void ChordView::setPlayhead (double seconds)
{
    if (result == nullptr) return;
    auto idx = -1;
    for (int i = 0; i < (int) result->chords.size(); ++i)
        if (seconds >= result->chords[(size_t) i].start && seconds < result->chords[(size_t) i].end)
            idx = i;

    if (idx != current)
    {
        current = idx;
        if (followPlayhead && idx >= 0 && (idx < first || idx >= first + visible))
        {
            first = juce::jlimit (0, juce::jmax (0, (int) result->chords.size() - visible), idx);
            altButtons.clear();
            bubble = {};
        }
        resized();
        repaint();
    }
}

void ChordView::resized()
{
    auto b = getLocalBounds();
    auto many = result != nullptr && (int) result->chords.size() > visible;
    prevButton.setVisible (many);
    nextButton.setVisible (many);
    prevButton.setBounds (b.getRight() - 70, 4, 32, 28);
    nextButton.setBounds (b.getRight() - 34, 4, 32, 28);
    if (many)
    {
        prevButton.setEnabled (first > 0);
        nextButton.setEnabled (first + visible < (int) result->chords.size());
    }
}

std::vector<ChordView::StarPos> ChordView::layoutStars() const
{
    std::vector<StarPos> out;
    if (result == nullptr) return out;
    auto area = getLocalBounds().toFloat().reduced (24.0f, 0).withTrimmedTop (62.0f).withTrimmedBottom (34.0f);
    auto n = juce::jmin (visible, (int) result->chords.size() - first);
    for (int i = 0; i < n; ++i)
    {
        auto& c = result->chords[(size_t) (first + i)].chord;
        auto x = area.getX() + (n == 1 ? 0.5f : (float) i / (float) (visible - 1)) * area.getWidth();
        // height follows the root note, so the "constellation" has the shape of the progression
        auto h = c.isValid() ? (float) ((c.root + 3) % 12) / 11.0f : 0.5f;
        out.push_back ({ { x, area.getBottom() - h * area.getHeight() }, first + i });
    }
    return out;
}

void ChordView::paint (juce::Graphics& g)
{
    auto& p = pal();
    auto b = getLocalBounds().toFloat();

    g.setColour (p.text);
    g.setFont (Fonts::light (20.0f));
    g.drawText ("CHORDS", b.withHeight (36.0f).withTrimmedLeft (4.0f), juce::Justification::centredLeft);

    if (result == nullptr || result->chords.empty())
    {
        g.setColour (smallText (p));
        g.setFont (Fonts::regular (13.0f));
        g.drawText ("Load a song to see its chords", b, juce::Justification::centred);
        return;
    }

    auto stars = layoutStars();
    auto flats = result->prefersFlats();
    auto lineCol = p.isLight ? juce::Colours::white : p.accent.brighter (0.5f);

    // connecting light lines
    for (size_t i = 1; i < stars.size(); ++i)
    {
        g.setColour (lineCol.withAlpha (0.25f));
        g.drawLine ({ stars[i - 1].p, stars[i].p }, 3.0f);
        g.setColour (lineCol.withAlpha (0.7f));
        g.drawLine ({ stars[i - 1].p, stars[i].p }, 1.0f);
    }

    // time axis
    auto axisY = b.getBottom() - 30.0f;
    g.setColour (p.text.withAlpha (0.2f));
    g.drawHorizontalLine ((int) axisY, b.getX() + 16.0f, b.getRight() - 16.0f);

    for (auto& s : stars)
    {
        auto& seg = result->chords[(size_t) s.index];
        auto isSel = s.index == selected, isCur = s.index == current;
        auto starCol = (isSel || isCur) ? (p.isLight ? p.value : juce::Colours::white) : lineCol;

        Glass::drawGlow (g, s.p, isSel || isCur ? 22.0f : 14.0f, starCol, isSel || isCur ? 0.8f : 0.45f);
        juce::Path star;
        auto a = isSel || isCur ? 9.0f : 6.5f, bb = a * 0.22f;
        star.startNewSubPath (s.p.x, s.p.y - a);
        star.quadraticTo (s.p.x + bb, s.p.y - bb, s.p.x + a, s.p.y);
        star.quadraticTo (s.p.x + bb, s.p.y + bb, s.p.x, s.p.y + a);
        star.quadraticTo (s.p.x - bb, s.p.y + bb, s.p.x - a, s.p.y);
        star.quadraticTo (s.p.x - bb, s.p.y - bb, s.p.x, s.p.y - a);
        g.setColour (starCol);
        g.fillPath (star);

        g.setColour (isSel ? (p.isLight ? p.value : juce::Colours::white) : p.text);
        g.setFont (Fonts::regular (isSel || isCur ? 20.0f : 17.0f));
        g.drawText (seg.chord.name (flats), juce::Rectangle<float> (90.0f, 24.0f).withCentre (s.p.translated (0, -24.0f)), juce::Justification::centred);

        g.setColour (p.text.withAlpha (0.3f));
        g.drawVerticalLine ((int) s.p.x, s.p.y + 12.0f, axisY + 4.0f);
        g.setColour (smallText (p));
        g.setFont (Fonts::regular (11.0f));
        g.drawText (formatTime (seg.start), juce::Rectangle<float> (60.0f, 16.0f).withCentre ({ s.p.x, axisY + 14.0f }), juce::Justification::centred);
    }

    if (! bubble.isEmpty())
    {
        Glass::drawPanel (g, bubble, 10.0f, p, p.isLight ? 3.0f : 4.0f, true);
        g.setColour (smallText (p));
        g.setFont (Fonts::regular (10.0f));
        g.drawText ("ALTERNATIVES", bubble.withHeight (18.0f).translated (0, 3.0f), juce::Justification::centred);
    }
    else
    {
        g.setColour (smallText (p).withMultipliedAlpha (0.7f));
        g.setFont (Fonts::regular (10.0f));
        g.drawText ("Click a star for alternatives", b.removeFromTop (60.0f).removeFromBottom (18.0f).withTrimmedLeft (4.0f), juce::Justification::centredLeft);
    }
}

void ChordView::showAlternatives (int index, juce::Point<float> at)
{
    altButtons.clear();
    selected = index;
    if (onSelect) onSelect (index);

    auto& seg = result->chords[(size_t) index];
    auto flats = result->prefersFlats();
    auto w = 64.0f * (float) seg.alternatives.size() + 16.0f;
    bubble = juce::Rectangle<float> (w, 52.0f).withCentre (at.translated (0, 44.0f));
    bubble.setX (juce::jlimit (4.0f, (float) getWidth() - w - 4.0f, bubble.getX()));
    if (bubble.getBottom() > (float) getHeight() - 2.0f)
        bubble = bubble.withY (at.y - 92.0f);

    auto x = bubble.getX() + 8.0f;
    for (size_t i = 0; i < seg.alternatives.size(); ++i)
    {
        auto* btn = altButtons.add (new juce::TextButton (seg.alternatives[i].name (flats)));
        btn->setBounds (juce::Rectangle<float> (x, bubble.getY() + 20.0f, 60.0f, 26.0f).toNearestInt());
        x += 64.0f;
        btn->onClick = [this, index, i]
        {
            auto& s = result->chords[(size_t) index];
            std::swap (s.chord, s.alternatives[i]);   // the old chord becomes an alternative, so it can be undone
            altButtons.clear();
            bubble = {};
            if (onChordEdited) onChordEdited();
            repaint();
        };
        addAndMakeVisible (btn);
    }
    repaint();
}

void ChordView::mouseDown (const juce::MouseEvent& e)
{
    for (auto& s : layoutStars())
    {
        if (s.p.getDistanceFrom (e.position) < 22.0f || juce::Rectangle<float> (90.0f, 30.0f).withCentre (s.p.translated (0, -24.0f)).contains (e.position))
        {
            if (selected == s.index && ! bubble.isEmpty()) { altButtons.clear(); bubble = {}; selected = -1; repaint(); }
            else showAlternatives (s.index, s.p);
            return;
        }
    }
    altButtons.clear();
    bubble = {};
    selected = -1;
    repaint();
}

// ================================================================== DragHandle

void DragHandle::paint (juce::Graphics& g)
{
    auto& p = pal();
    auto b = getLocalBounds().toFloat();
    auto icon = b.removeFromLeft (b.getHeight()).reduced (b.getHeight() * 0.22f);
    auto c = icon.getCentre();
    auto s = icon.getWidth() * 0.5f;
    g.setColour (isMouseOver() ? p.text : smallText (p));

    juce::Path arrows;
    arrows.startNewSubPath (c.x - s, c.y); arrows.lineTo (c.x + s, c.y);
    arrows.startNewSubPath (c.x, c.y - s); arrows.lineTo (c.x, c.y + s);
    for (auto d : { juce::Point<float> (1, 0), juce::Point<float> (-1, 0), juce::Point<float> (0, 1), juce::Point<float> (0, -1) })
    {
        auto tip = c + d * s;
        auto side = juce::Point<float> (d.y, d.x) * (s * 0.3f);
        arrows.startNewSubPath (tip - d * (s * 0.3f) + side); arrows.lineTo (tip); arrows.lineTo (tip - d * (s * 0.3f) - side);
    }
    g.strokePath (arrows, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (Fonts::regular (11.0f));
    g.drawFittedText (label, b.toNearestInt(), juce::Justification::centredLeft, 2);
}

void DragHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 4 || ! createFile) return;
    auto f = createFile();
    if (f.existsAsFile())
    {
        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
    }
}

} // namespace typebeats
