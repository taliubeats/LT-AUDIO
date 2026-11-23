#include "Views.h"
#include "DrumSeparator.h"

namespace eclipse
{

using juce::Colour;
namespace Colours = juce::Colours;
using lt::ui::Fonts;

namespace
{
juce::String bankName (int pad) { return juce::String::charToString ((juce::juce_wchar) ('A' + pad / padsPerBank)); }

bool isAudioFile (const juce::String& f)
{
    return juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;m4a;aac;ogg;caf;mp4");
}

juce::String familyOutName (Family f) { return juce::String (familyLabel (f)); }
int pads0Bank (const Processor& p) { return p.selectedPad / padsPerBank; }
}

// ==================================================================== KeyButton
void KeyButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    look::key (g, proc.skin(), r, icon ? juce::String() : getButtonText(), getToggleState(), down, over, led, fontSize, onColour);
    if (icon)
    {
        const bool on = getToggleState();
        g.setColour (on ? Colours::white : proc.skin().keyTx);
        icon (g, r.withTrimmedBottom (3).translated (0, on || down ? 1.5f : 0.0f));
    }
}

// ==================================================================== Knob
Knob::Knob (Processor& p, juce::String n, bool b) : proc (p), name (std::move (n)), big (b)
{
    setSliderStyle (RotaryVerticalDrag);
    setTextBoxStyle (NoTextBox, false, 0, 0);
    setMouseDragSensitivity (big ? 260 : 200);
    setVelocityBasedMode (false);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void Knob::paint (juce::Graphics& g)
{
    const auto& s = proc.skin();
    auto r = getLocalBounds().toFloat();
    const float v = (float) valueToProportionOfLength (getValue());
    if (big)
    {
        auto k = r.removeFromTop (r.getWidth()).reduced (4);
        look::knob (g, s, k, v, true, s.acc);
        g.setColour (s.tx);
        look::spaced (g, name, r.removeFromTop (22), Fonts::bold (15), 6.0f, juce::Justification::centred);
        look::label (g, s, caption + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + getTextFromValue (getValue()), r.removeFromTop (16),
                     juce::Justification::centred, 8.5f, 1.6f);
    }
    else
    {
        auto k = r.removeFromTop (r.getWidth()).reduced (2);
        look::knob (g, s, k, v, false, s.acc);
        g.setColour (s.tx.withAlpha (0.8f));
        g.setFont (Fonts::semiBold (10));
        g.drawText (getTextFromValue (getValue()), r.removeFromTop (16), juce::Justification::centred, false);
        look::label (g, s, name, r.removeFromTop (14), juce::Justification::centred, 9.5f, 1.4f);
    }
}

// ==================================================================== SourceView
SourceView::SourceView (Processor& p)
    : aiKey (p, "AI", true), recloneKey (p, "RE-CLONE"), browseKey (p, "BROWSE"),
      prevKey (p, juce::CharPointer_UTF8 ("\xe2\x80\xb9")), nextKey (p, juce::CharPointer_UTF8 ("\xe2\x80\xba")),
      outKey (p, juce::CharPointer_UTF8 ("\xe2\x88\x92")), inKey (p, "+"), sensitivity (p), proc (p)
{
    for (auto* k : { &aiKey, &recloneKey, &browseKey, &prevKey, &nextKey, &outKey, &inKey }) addAndMakeVisible (*k);
    for (auto* k : { &prevKey, &nextKey, &outKey, &inKey }) k->fontSize = 14;
    addAndMakeVisible (sensitivity);
    sensAttach = std::make_unique<juce::SliderParameterAttachment> (*p.state.getParameter (IDs::sensitivity), sensitivity, nullptr);
    aiKey.setClickingTogglesState (true);
    aiKey.setToggleState (true, juce::dontSendNotification);
    aiKey.onClick = [this] { if (proc.kit.sourceAudio != nullptr || proc.kit.sourceFile.existsAsFile()) proc.recloneSource (aiKey.getToggleState()); };
    recloneKey.onClick = [this] { proc.recloneSource (aiKey.getToggleState()); };
    browseKey.onClick = [this] { browse(); };
    prevKey.onClick = [this] { jumpSection (-1); };
    nextKey.onClick = [this] { jumpSection (1); };
    outKey.onClick = [this] { zoom (1.6f, 0.5f); };
    inKey.onClick = [this] { zoom (1.0f / 1.6f, 0.5f); };
}

void SourceView::resized()
{
    sensitivity.setBounds (898, 20, 144, 28);
    aiKey.setBounds (1056, 17, 66, 35);
    recloneKey.setBounds (1130, 17, 102, 35);
    browseKey.setBounds (1240, 17, 82, 35);
    wave = { 20, 62, 1300, 126 };
    prevKey.setBounds (1188, 194, 30, 31); nextKey.setBounds (1222, 194, 30, 31);
    outKey.setBounds (1256, 194, 30, 31);  inKey.setBounds (1290, 194, 30, 31);
}

void SourceView::browse()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a song, loop or one-shot", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.aac;*.ogg;*.caf");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& c)
    {
        if (c.getResult().existsAsFile()) proc.cloneFile (c.getResult(), aiKey.getToggleState());
    });
}

void SourceView::buildPeaks()
{
    auto src = proc.kit.sourceAudio;
    peaksOf = src.get();
    peaks.clear();
    if (src == nullptr) return;
    const int n = src->getNumSamples(), blocks = (n + 255) / 256;
    peaks.resize ((size_t) blocks * 2);
    float mx = 1.0e-6f;
    for (int b = 0; b < blocks; ++b)
    {
        float lo = 0, hi = 0;
        const int a = b * 256, len = juce::jmin (256, n - a);
        for (int c = 0; c < src->getNumChannels(); ++c)
        {
            auto r = juce::FloatVectorOperations::findMinAndMax (src->getReadPointer (c, a), len);
            lo = juce::jmin (lo, r.getStart()); hi = juce::jmax (hi, r.getEnd());
        }
        peaks[(size_t) b * 2] = lo; peaks[(size_t) b * 2 + 1] = hi;
        mx = juce::jmax (mx, hi, -lo);
    }
    for (auto& v : peaks) v /= mx;
}

void SourceView::kitChanged()
{
    if (proc.kit.sourceAudio.get() != peaksOf)
    {
        buildPeaks();
        viewStart = 0;
        viewLen = proc.kit.sourceAudio != nullptr ? (double) proc.kit.sourceAudio->getNumSamples() : 1.0;
    }
    aiKey.setToggleState (proc.kit.usedAI || proc.kit.sourceAudio == nullptr ? aiKey.getToggleState() : false, juce::dontSendNotification);
    if (proc.kit.sourceAudio != nullptr) aiKey.setToggleState (proc.kit.usedAI, juce::dontSendNotification);
    repaint();
}

void SourceView::zoom (float factor, float at01)
{
    auto src = proc.kit.sourceAudio;
    if (src == nullptr) return;
    const double total = src->getNumSamples();
    const double centre = viewStart + viewLen * at01;
    viewLen = juce::jlimit (proc.sampleRateForUi() * 0.5, total, viewLen * factor);
    viewStart = juce::jlimit (0.0, juce::jmax (0.0, total - viewLen), centre - viewLen * at01);
    repaint();
}

void SourceView::jumpSection (int dir)
{
    auto an = proc.kit.analysis;
    auto src = proc.kit.sourceAudio;
    if (an == nullptr || src == nullptr || an->drumSections.empty()) return;
    const double centre = viewStart + viewLen * 0.5;
    const juce::Range<juce::int64>* target = nullptr;
    if (dir > 0) { for (auto& s : an->drumSections) if ((double) s.getStart() > centre + 1) { target = &s; break; } }
    else for (auto it = an->drumSections.rbegin(); it != an->drumSections.rend(); ++it) if ((double) it->getEnd() < centre - 1) { target = &*it; break; }
    if (target == nullptr) return;
    viewLen = juce::jmin ((double) src->getNumSamples(), juce::jmax (viewLen, (double) target->getLength() * 1.15));
    viewStart = juce::jlimit (0.0, juce::jmax (0.0, (double) src->getNumSamples() - viewLen), (double) target->getStart() - viewLen * 0.05);
    repaint();
}

void SourceView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! wave.contains (e.position)) return;
    if (std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        auto src = proc.kit.sourceAudio;
        if (src == nullptr) return;
        viewStart = juce::jlimit (0.0, juce::jmax (0.0, src->getNumSamples() - viewLen), viewStart - w.deltaX * viewLen * 0.5);
        repaint();
    }
    else zoom (w.deltaY > 0 ? 0.85f : 1.0f / 0.85f, (e.position.x - wave.getX()) / wave.getWidth());
}

void SourceView::mouseMagnify (const juce::MouseEvent& e, float scale)
{
    zoom (1.0f / juce::jlimit (0.5f, 2.0f, scale), (e.position.x - wave.getX()) / wave.getWidth());
}

void SourceView::mouseDown (const juce::MouseEvent& e)
{
    dragX = e.position.x; dragStart = viewStart;
    if (proc.kit.sourceAudio == nullptr && wave.contains (e.position)) browse();
}

void SourceView::mouseDrag (const juce::MouseEvent& e)
{
    auto src = proc.kit.sourceAudio;
    if (src == nullptr) return;
    viewStart = juce::jlimit (0.0, juce::jmax (0.0, src->getNumSamples() - viewLen), dragStart - (e.position.x - dragX) / wave.getWidth() * viewLen);
    repaint();
}

bool SourceView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (isAudioFile (f)) return true;
    return false;
}

void SourceView::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files) if (isAudioFile (f)) { proc.cloneFile (juce::File (f), aiKey.getToggleState()); break; }
    repaint();
}

void SourceView::paint (juce::Graphics& g)
{
    const auto& s = proc.skin();
    look::panel (g, s, getLocalBounds().toFloat()); look::panelArt (g, s, getLocalBounds().toFloat(), getPosition());
    look::label (g, s, "Source", { 26, 22, 80, 26 }, juce::Justification::centredLeft, 10.5f, 2.2f);
    auto an = proc.kit.analysis;
    auto src = proc.kit.sourceAudio;
    g.setColour (s.tx); g.setFont (Fonts::bold (14.5f));
    const auto name = proc.kit.sourceName.isEmpty() ? juce::String ("No source") : proc.kit.sourceName;
    g.drawText (name, 104, 22, 340, 26, juce::Justification::centredLeft, true);
    if (an != nullptr && an->length > 0)
    {
        const int secs = (int) (an->length / an->sampleRate);
        juce::String info = juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2) + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "))
                          + juce::String ((int) an->hits.size()) + " hits" + (proc.kit.usedAI ? juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) + (an->parts ? "AI kick / snare / hats separated" : an->neural ? "AI drum stem (neural)" : "drum stem (DSP)") : juce::String());
        g.setColour (s.dim); g.setFont (Fonts::regular (12.5f));
        g.drawText (info, 104 + (int) Fonts::bold (14.5f).getStringWidthFloat (name) + 14, 22, 360, 26, juce::Justification::centredLeft, true);
    }
    look::label (g, s, "Sensitivity", { 760, 22, 132, 26 }, juce::Justification::centredRight, 10.0f, 2.0f);
    // the AI models must be installed: say it loud when they are not (the results are much worse without them)
    static bool modelsOk = DrumSeparator::available() && DrumSeparator::partsAvailable();
    static double lastCheck = 0;
    if (juce::Time::getMillisecondCounterHiRes() - lastCheck > 5000) { lastCheck = juce::Time::getMillisecondCounterHiRes(); modelsOk = DrumSeparator::available() && DrumSeparator::partsAvailable(); }
    if (! modelsOk)
    {
        auto warn = juce::Rectangle<float> (480, 18, 270, 32);
        g.setColour (juce::Colour (0xffd8342c)); g.fillRoundedRectangle (warn, 6);
        g.setColour (Colours::white); g.setFont (Fonts::bold (11.5f));
        g.drawText ("AI MODELS MISSING - REINSTALL ECLIPSE", warn.toNearestInt(), juce::Justification::centred, false);
    }

    look::lcd (g, s, wave);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (wave.reduced (2).toNearestInt());
        const float m = wave.getCentreY(), h = wave.getHeight() * 0.44f;
        if (src != nullptr && ! peaks.empty())
        {
            const double total = src->getNumSamples();
            auto xOf = [&] (double smp) { return wave.getX() + (float) ((smp - viewStart) / viewLen) * wave.getWidth(); };
            // drum sections
            if (an != nullptr)
                for (auto& sec : an->drumSections)
                {
                    const float x0 = xOf ((double) sec.getStart()), x1 = xOf ((double) sec.getEnd());
                    if (x1 < wave.getX() || x0 > wave.getRight()) continue;
                    g.setColour (s.acc.withAlpha (0.08f)); g.fillRect (x0, wave.getY(), x1 - x0, wave.getHeight());
                    g.setColour (s.acc); g.fillRect (x0, wave.getBottom() - 4, x1 - x0, 2.0f);
                }
            // body
            const int blocks = (int) (peaks.size() / 2);
            for (float x = wave.getX(); x < wave.getRight(); x += 1.5f)
            {
                const double a = viewStart + (x - wave.getX()) / wave.getWidth() * viewLen, b = a + 1.5 / wave.getWidth() * viewLen;
                const int b0 = juce::jlimit (0, blocks - 1, (int) (a / 256)), b1 = juce::jlimit (b0 + 1, blocks, (int) (b / 256) + 1);
                float lo = 0, hi = 0;
                for (int k = b0; k < b1; ++k) { lo = juce::jmin (lo, peaks[(size_t) k * 2]); hi = juce::jmax (hi, peaks[(size_t) k * 2 + 1]); }
                g.setColour (s.dim.withAlpha (s.light ? 0.7f : 0.55f));
                g.fillRect (x, m - hi * h, 1.0f, (hi - lo) * h + 0.5f);
            }
            // hits
            if (an != nullptr)
                for (auto& hit : an->hits)
                {
                    const float x = xOf ((double) hit.pos);
                    if (x < wave.getX() - 2 || x > wave.getRight() + 2) continue;
                    const auto c = s.family (hit.family);
                    const float hh = h * (0.55f + 0.45f * hit.strength);
                    juce::ColourGradient cg (c.withAlpha (0.0f), x, m - hh, c.withAlpha (0.0f), x, m + hh, false);
                    cg.addColour (0.2, c.withAlpha (0.8f)); cg.addColour (0.5, c); cg.addColour (0.8, c.withAlpha (0.8f));
                    g.setGradientFill (cg);
                    g.fillRect (x, m - hh, viewLen < total * 0.3 ? 1.6f : 1.1f, hh * 2);
                    g.setColour (c); g.fillRect (x - 0.5f, wave.getY() + 4, 2.5f, 6.0f);
                }
            // which kit plays where (A, B... under the waveform)
            if (an != nullptr && an->numKits > 1)
                for (auto& [range, kitIdx] : an->kitSections)
                {
                    const float x0 = juce::jmax (wave.getX(), xOf ((double) range.getStart())), x1 = juce::jmin (wave.getRight(), xOf ((double) range.getEnd()));
                    if (x1 <= x0) continue;
                    auto tag = juce::Rectangle<float> (x0 + 2, wave.getBottom() - 21, x1 - x0 - 4, 14);
                    g.setColour (s.acc.withAlpha (kitIdx == pads0Bank (proc) ? 0.55f : 0.22f)); g.fillRoundedRectangle (tag, 3);
                    g.setColour (Colours::white); g.setFont (Fonts::bold (10));
                    if (tag.getWidth() > 14) g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + kitIdx)), tag.toNearestInt(), juce::Justification::centred, false);
                }
            // scrollbar
            const float sbW = (float) (viewLen / total) * wave.getWidth();
            if (sbW < wave.getWidth() - 1)
            {
                g.setColour (s.tx.withAlpha (0.25f));
                g.fillRoundedRectangle (wave.getX() + (float) (viewStart / total) * wave.getWidth(), wave.getBottom() - 9, sbW, 3, 1.5f);
            }
        }
        else
        {
            g.setColour (s.dim.withAlpha (0.35f));
            for (float x = wave.getX() + 6; x < wave.getRight() - 6; x += 3)
            {
                const float t = (x - wave.getX()) / wave.getWidth();
                const float a = 0.08f + 0.05f * std::sin (t * 40) * std::sin (t * 7);
                g.fillRect (x, m - a * h * 2, 1.5f, a * h * 4);
            }
            g.setColour (s.tx); g.setFont (Fonts::bold (16));
            look::spaced (g, "DROP A SONG, A LOOP OR A ONE-SHOT", wave.withTrimmedBottom (18), Fonts::bold (16), 3.0f, juce::Justification::centred);
            look::label (g, s, juce::String (juce::CharPointer_UTF8 ("or click to browse  \xc2\xb7  WAV AIFF FLAC MP3 M4A OGG")), wave.withTrimmedTop (40),
                         juce::Justification::centred, 10, 1.6f);
        }
        if (proc.isCloning())
        {
            g.setColour (s.lcd1.withAlpha (0.78f)); g.fillRect (wave);
            auto bar = wave.withSizeKeepingCentre (420, 6).translated (0, 14);
            g.setColour (s.ringOff); g.fillRoundedRectangle (bar, 3);
            g.setColour (s.acc); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.02f, 1.0f, proc.cloneProgress())), 3);
            g.setColour (Colours::white);
            look::spaced (g, proc.cloneStatus().toUpperCase(), wave.withTrimmedBottom (26), Fonts::bold (14), 3.0f, juce::Justification::centred);
        }
        if (dragOver)
        {
            g.setColour (s.acc.withAlpha (0.18f)); g.fillRect (wave);
            g.setColour (s.acc); g.drawRect (wave.reduced (2), 2.0f);
        }
    }
    look::lcdGlass (g, wave);

    // legend
    float x = 26;
    for (int f = 0; f < numFamilies; ++f)
    {
        const auto fam = (Family) f;
        const int n = an != nullptr ? an->count (fam) : 0;
        look::dot (g, s.family (fam), { x + 4, 209 }, 4, true);
        const juce::String t (familyName (fam));
        auto font = Fonts::semiBold (10.5f);
        look::label (g, s, t, { x + 13, 200, 120, 18 }, juce::Justification::centredLeft, 10.5f, 1.2f);
        const float tw = font.getStringWidthFloat (t) + 1.2f * (float) t.length();
        g.setColour (s.tx); g.setFont (Fonts::bold (10.5f));
        g.drawText (juce::String (n), (int) (x + 18 + tw), 200, 40, 18, juce::Justification::centredLeft, false);
        x += 18 + tw + 14 + Fonts::bold (10.5f).getStringWidthFloat (juce::String (n)) + 12;
    }
    if (! proc.lastError.isEmpty())
    {
        g.setColour (Colour (0xffff5a5a)); g.setFont (Fonts::semiBold (12));
        g.drawText (proc.lastError, 700, 200, 470, 18, juce::Justification::centredRight, true);
    }
}

// ==================================================================== PadComp
PadComp::PadComp (Processor& p, PadGrid& g) : proc (p), grid (g) {}

void PadComp::paint (juce::Graphics& g)
{
    const auto& s = proc.skin();
    auto pd = proc.kit.pad (index);
    const bool full = ! pd.empty();
    const auto col = full ? s.family (pd.type) : s.dim;
    auto r = getLocalBounds().toFloat();
    look::pad (g, s, r, full, full && proc.selectedPad == index, juce::jmax (lit, hover ? 0.35f : 0.0f), hover && ! full ? s.acc : col);
    const float press = lit > 0.5f ? 5.0f : 0.0f;
    auto top = r.withTrimmedBottom (7).translated (0, press);
    g.setColour (s.dim); g.setFont (Fonts::semiBold (9.5f));
    g.drawText (noteName (noteForPad (index)), top.reduced (11, 9).toNearestInt(), juce::Justification::topRight, false);
    if (! full)
    {
        g.setColour (s.dim.withAlpha (0.6f));
        g.setFont (Fonts::light (30));
        g.drawText ("+", top.toNearestInt(), juce::Justification::centred, false);
        return;
    }
    look::dot (g, col, { top.getX() + 15, top.getY() + 15 }, 3.5f, false);
    g.setColour (s.light ? s.tx : Colours::white.withAlpha (0.95f));
    look::spaced (g, (pd.name.isEmpty() ? juce::String (familyName (pd.type)) : pd.name).toUpperCase(),
                       { top.getX() + 24, top.getY() + 6, top.getWidth() - 60, 18 }, Fonts::bold (11), 0.9f, juce::Justification::centredLeft);
    // the waveform is drawn once into an image
    const auto key = pd.raw.get();
    if (waveOf != key || waveSkin != s.id || wave.getWidth() != (int) top.getWidth() - 20)
    {
        waveOf = key; waveSkin = s.id;
        const int w = (int) top.getWidth() - 20, h = 64;
        wave = juce::Image (juce::Image::ARGB, w * 2, h * 2, true);
        juce::Graphics wg (wave);
        wg.addTransform (juce::AffineTransform::scale (2));
        look::waveform (wg, pd.clean != nullptr ? pd.clean.get() : pd.raw.get(), { 0, 0, (float) w, (float) h }, col, 0, 0.95f);
    }
    g.drawImage (wave, juce::Rectangle<float> (top.getX() + 10, top.getBottom() - 74, top.getWidth() - 20, 64));
}

void PadComp::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    proc.selectedPad = index;
    if (! proc.kit.pad (index).empty())
    {
        // MPC feel: lower on the pad = softer
        const float vel = juce::jlimit (0.35f, 1.0f, 1.05f - 0.55f * (e.position.y / (float) getHeight()) * (e.mods.isShiftDown() ? 1.0f : 0.25f));
        proc.triggerPad (index, vel);
    }
    if (grid.onSelect) grid.onSelect();
}

void PadComp::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 8 || proc.kit.pad (index).empty()) return;
    dragging = true;
    if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        juce::Image img (juce::Image::ARGB, getWidth(), getHeight(), true);
        { juce::Graphics g (img); paint (g); }
        img.multiplyAllAlphas (0.75f);
        dc->startDragging ("pad:" + juce::String (index), this, juce::ScaledImage (img), true);
    }
}

void PadComp::mouseUp (const juce::MouseEvent& e)
{
    // dragged outside the plugin: hand the WAV to the DAW / Finder
    if (dragging && ! getTopLevelComponent()->getLocalBounds().contains (e.getEventRelativeTo (getTopLevelComponent()).getPosition()))
    {
        auto f = proc.exportPad (index);
        if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
    }
    dragging = false;
}

bool PadComp::isInterestedInDragSource (const SourceDetails& d) { return d.description.toString().startsWith ("pad:"); }

void PadComp::itemDropped (const SourceDetails& d)
{
    hover = false;
    const int from = d.description.toString().fromFirstOccurrenceOf ("pad:", false, false).getIntValue();
    proc.movePad (from, index, juce::ModifierKeys::getCurrentModifiers().isAltDown());
    if (grid.onSelect) grid.onSelect();
}

bool PadComp::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (isAudioFile (f)) return true;
    return false;
}

void PadComp::filesDropped (const juce::StringArray& files, int, int)
{
    hover = false;
    for (auto& f : files) if (isAudioFile (f)) { proc.loadSampleToPad (index, juce::File (f)); break; }
    proc.selectedPad = index;
    if (grid.onSelect) grid.onSelect();
}

// ==================================================================== PadGrid
PadGrid::PadGrid (Processor& p) : proc (p)
{
    for (auto& pc : pads) { pc = std::make_unique<PadComp> (p, *this); addAndMakeVisible (*pc); }
    bank = proc.selectedPad / padsPerBank;
    refresh();
}

juce::Rectangle<float> PadGrid::bankRect (int b) const
{
    // A at the bottom, D on top
    const float top = 18, h = (getHeight() - 36 - 3 * 12.0f) / 4;
    return { 18, top + (float) (3 - b) * (h + 12), 52, h };
}

void PadGrid::resized()
{
    const float x0 = 84, y0 = 18, w = (getWidth() - x0 - 18 - 3 * 12.0f) / 4, h = (getHeight() - 36 - 3 * 12.0f) / 4;
    for (int i = 0; i < padsPerBank; ++i)
    {
        const int row = 3 - i / 4, col = i % 4;
        pads[(size_t) i]->setBounds (juce::Rectangle<float> (x0 + (float) col * (w + 12), y0 + (float) row * (h + 12), w, h).toNearestInt());
    }
}

void PadGrid::refresh()
{
    for (int i = 0; i < padsPerBank; ++i)
    {
        pads[(size_t) i]->index = bank * padsPerBank + i;
        pads[(size_t) i]->repaint();
    }
    repaint();
}

void PadGrid::tick (float dt)
{
    for (int i = 0; i < numPads; ++i)
    {
        const auto h = proc.padHitCount (i);
        if (h != lastHits[(size_t) i]) { lastHits[(size_t) i] = h; lights[(size_t) i] = 1.0f; }
        else if (lights[(size_t) i] > 0) lights[(size_t) i] = juce::jmax (0.0f, lights[(size_t) i] - dt / 0.22f);
    }
    for (auto& pc : pads)
    {
        // full light for a moment, then a quick fade (the pad comes back up at 0.5)
        const float l = lights[(size_t) pc->index];
        const float shown = l > 0.75f ? 1.0f : l / 0.75f;
        if (std::abs (shown - pc->lit) > 0.01f) { pc->lit = shown; pc->repaint(); }
    }
    repaint (bankRect (0).getUnion (bankRect (3)).toNearestInt().expanded (6));
}

void PadGrid::mouseDown (const juce::MouseEvent& e)
{
    for (int b = 0; b < numBanks; ++b)
        if (bankRect (b).contains (e.position))
        {
            bank = b;
            refresh();
        }
}

void PadGrid::paint (juce::Graphics& g)
{
    const auto& s = proc.skin();
    look::panel (g, s, getLocalBounds().toFloat(), false); look::panelArt (g, s, getLocalBounds().toFloat(), getPosition());
    for (int b = 0; b < numBanks; ++b)
    {
        auto r = bankRect (b);
        const bool on = b == bank;
        bool playing = false;
        for (int i = 0; i < padsPerBank; ++i) playing |= lights[(size_t) (b * padsPerBank + i)] > 0.5f;
        look::key (g, s, r, {}, on, false, false);
        auto in = r.withTrimmedBottom (3).translated (0, on ? 1.5f : 0.0f);
        g.setColour (on ? Colours::white : s.dim);
        g.setFont (Fonts::bold (18));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + b)), in.withTrimmedBottom (in.getHeight() * 0.42f).toNearestInt(), juce::Justification::centredBottom, false);
        // mini map of the bank (bottom row = first pads, like the grid)
        const float cell = 5, gap = 2, mw = 4 * cell + 3 * gap;
        const float mx = in.getCentreX() - mw / 2, my = in.getCentreY() + 6;
        for (int i = 0; i < padsPerBank; ++i)
        {
            const int row = 3 - i / 4, col = i % 4;
            const bool full = ! proc.kit.pad (b * padsPerBank + i).empty();
            const bool lit = lights[(size_t) (b * padsPerBank + i)] > 0.3f;
            g.setColour (lit ? Colours::white : full ? (on ? Colours::white.withAlpha (0.9f) : s.acc) : Colours::black.withAlpha (on ? 0.25f : 0.3f));
            g.fillRoundedRectangle (mx + (float) col * (cell + gap), my + (float) row * (cell + gap), cell, cell, 1);
        }
        juce::ignoreUnused (playing);
    }
}

// ==================================================================== PadEditor
PadEditor::PadEditor (Processor& p)
    : proc (p), typeKey (p), muteKey (p, "M"), soloKey (p, "S"),
      prevHit (p, juce::CharPointer_UTF8 ("\xe2\x80\xb9")), nextHit (p, juce::CharPointer_UTF8 ("\xe2\x80\xba")),
      loadKey (p, "LOAD"), outKey (p), similarKey (p, "FIND SIMILAR")
{
    static const char* names[] = { "CLEAN", "GAIN", "PAN", "TUNE", "ATTACK", "FADE", "HP", "LP" };
    for (auto* n : names) { knobs.push_back (std::make_unique<Knob> (p, n)); addAndMakeVisible (*knobs.back()); }
    vibe = std::make_unique<Knob> (p, "VIBE", true);
    addAndMakeVisible (*vibe);
    for (auto* k : { &typeKey, &muteKey, &soloKey, &prevHit, &nextHit, &loadKey, &outKey, &similarKey }) addAndMakeVisible (*k);
    similarKey.onClick = [this] { if (onFindSimilar) onFindSimilar(); };
    prevHit.fontSize = nextHit.fontSize = 15;
    muteKey.setClickingTogglesState (true); soloKey.setClickingTogglesState (true);
    muteKey.onColour = Colour (0xffff5a4a);
    soloKey.onColour = Colour (0xfff5c542);
    for (int i = 0; i < 5; ++i)
    {
        choke[(size_t) i] = std::make_unique<KeyButton> (p, i == 0 ? juce::String ("OFF") : juce::String (i));
        choke[(size_t) i]->onClick = [this, i] { if (chokeAttach) chokeAttach->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (*choke[(size_t) i]);
    }
    typeKey.onClick = [this] { showTypeMenu(); };
    outKey.onClick = [this] { showOutMenu(); };
    prevHit.onClick = [this] { auto pd = proc.kit.pad (proc.selectedPad); proc.selectHit (proc.selectedPad, pd.hitIndex - 1); };
    nextHit.onClick = [this] { auto pd = proc.kit.pad (proc.selectedPad); proc.selectHit (proc.selectedPad, pd.hitIndex + 1); };
    loadKey.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.ogg;*.caf");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& c)
        {
            if (c.getResult().existsAsFile()) proc.loadSampleToPad (proc.selectedPad, c.getResult());
        });
    };
    attach();
}

void PadEditor::resized()
{
    titleArea = { 18, 16, 250, 34 };
    similarKey.setBounds (272, 16, 130, 35);
    typeKey.setBounds (410, 16, 120, 35);
    muteKey.setBounds (538, 16, 33, 35);
    soloKey.setBounds (577, 16, 33, 35);
    wave = { 18, 62, 590, 112 };
    prevHit.setBounds (52, 186, 32, 33);
    nextHit.setBounds (210, 186, 32, 33);
    loadKey.setBounds (526, 186, 82, 33);
    for (size_t i = 0; i < knobs.size(); ++i) knobs[i]->setBounds (12 + (int) i * 56, 240, 54, 84);
    vibe->setBounds (462, 222, 146, 186);
    choke[0]->setBounds (84, 420, 52, 33);
    for (int i = 1; i < 5; ++i) choke[(size_t) i]->setBounds (144 + (i - 1) * 40, 420, 33, 33);
    outKey.setBounds (356, 420, 180, 33);
}

void PadEditor::attach()
{
    attachedPad = proc.selectedPad;
    const int pad = attachedPad;
    attachments.clear(); buttonAttachments.clear();
    static const PadParam order[] = { PadParam::clean, PadParam::gain, PadParam::pan, PadParam::tune, PadParam::attack, PadParam::fade, PadParam::hp, PadParam::lp };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto* prm = proc.state.getParameter (padParamId (pad, order[i]));
        attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*prm, *knobs[i], nullptr));
        knobs[i]->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }
    auto* v = proc.state.getParameter (padParamId (pad, PadParam::vibe));
    attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*v, *vibe, nullptr));
    vibe->setDoubleClickReturnValue (true, 0.0);
    vibe->caption = vibeName (proc.skin().vibe);
    buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (padParamId (pad, PadParam::mute)), muteKey, nullptr));
    buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.state.getParameter (padParamId (pad, PadParam::solo)), soloKey, nullptr));
    chokeAttach = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (padParamId (pad, PadParam::choke)), [this] (float x)
    {
        for (int i = 0; i < 5; ++i) choke[(size_t) i]->setToggleState (juce::roundToInt (x) == i, juce::dontSendNotification);
    });
    outAttach = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (padParamId (pad, PadParam::out)), [this] (float) { padChanged(); });
    startAttach = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (padParamId (pad, PadParam::start)), [this] (float x) { startV = x; repaint(); });
    endAttach = std::make_unique<juce::ParameterAttachment> (*proc.state.getParameter (padParamId (pad, PadParam::end)), [this] (float x) { endV = x; repaint(); });
    chokeAttach->sendInitialUpdate(); startAttach->sendInitialUpdate(); endAttach->sendInitialUpdate();
}

void PadEditor::padChanged()
{
    if (attachedPad != proc.selectedPad) attach();
    vibe->caption = vibeName (proc.skin().vibe);
    auto pd = proc.kit.pad (proc.selectedPad);
    const auto fam = pd.empty() ? Family::sample : pd.type;
    typeKey.setButtonText ((pd.tag.isNotEmpty() ? pd.tag : juce::String (familyLabel (fam))).toUpperCase() + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbe")));
    const int out = juce::roundToInt (proc.state.getRawParameterValue (padParamId (proc.selectedPad, PadParam::out))->load());
    const juce::String famOut = fam == Family::sample ? juce::String() : familyOutName (fam).toUpperCase();
    outKey.setButtonText ((out == Outs::main || famOut.isEmpty() ? juce::String ("MAIN") : out == Outs::mainAndFamily ? "MAIN + " + famOut : famOut + " ONLY")
                          + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbe")));
    const bool hits = proc.hitCount (proc.selectedPad) > 1;
    prevHit.setEnabled (hits); nextHit.setEnabled (hits);
    repaint();
}

void PadEditor::tick()
{
    if (attachedPad != proc.selectedPad) padChanged();
}

void PadEditor::showTypeMenu()
{
    auto pd = proc.kit.pad (proc.selectedPad);
    if (pd.empty()) return;
    juce::PopupMenu m;
    for (int f = 0; f <= (int) Family::sample; ++f) m.addItem (f + 1, familyLabel ((Family) f), true, pd.type == (Family) f && pd.tag.isEmpty());
    m.addSeparator();
    m.addItem (100, "New tag...");
    m.addItem (101, "Rename pad...");
    m.addItem (102, "Clear pad");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (typeKey), [this] (int r)
    {
        if (r <= 0) return;
        const int pad = proc.selectedPad;
        if (r <= 9) { auto p = proc.kit.pad (pad); p.tag.clear(); proc.kit.setPad (pad, p); proc.setPadType (pad, (Family) (r - 1)); }
        else if (r == 100 || r == 101)
        {
            auto* w = new juce::AlertWindow (r == 100 ? "New tag" : "Rename pad", r == 100 ? "Name of the tag (it keeps the pad's output)" : "Name of the pad", juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("t", r == 100 ? juce::String() : proc.kit.pad (pad).name);
            w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, r, pad] (int res)
            {
                const auto t = w->getTextEditorContents ("t").trim();
                if (res == 1 && t.isNotEmpty())
                {
                    auto p = proc.kit.pad (pad);
                    if (r == 100) p.tag = t; else p.name = t.toUpperCase();
                    proc.kit.setPad (pad, p);
                    proc.setPadType (pad, p.type);
                }
            }), true);
        }
        else if (r == 102) proc.clearPad (pad);
    });
}

void PadEditor::showOutMenu()
{
    auto pd = proc.kit.pad (proc.selectedPad);
    const auto fam = pd.empty() ? Family::sample : pd.type;
    juce::PopupMenu m;
    const int out = juce::roundToInt (proc.state.getRawParameterValue (padParamId (proc.selectedPad, PadParam::out))->load());
    m.addItem (1, "Main", true, out == 0);
    if (fam != Family::sample)
    {
        m.addItem (2, "Main + " + familyOutName (fam), true, out == 1);
        m.addItem (3, familyOutName (fam) + " only", true, out == 2);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (outKey), [this] (int r)
    {
        if (r > 0 && outAttach) outAttach->setValueAsCompleteGesture ((float) (r - 1));
    });
}

void PadEditor::mouseDown (const juce::MouseEvent& e)
{
    dragHandle = 0;
    if (! wave.contains (e.position)) return;
    const float sx = wave.getX() + startV * wave.getWidth(), ex = wave.getX() + endV * wave.getWidth();
    if (std::abs (e.position.x - sx) < 12) dragHandle = 1;
    else if (std::abs (e.position.x - ex) < 12) dragHandle = 2;
    else proc.triggerPad (proc.selectedPad, 0.9f);
}

void PadEditor::mouseDrag (const juce::MouseEvent& e)
{
    const float v = juce::jlimit (0.0f, 1.0f, (e.position.x - wave.getX()) / wave.getWidth());
    if (dragHandle == 1 && startAttach) startAttach->setValueAsPartOfGesture (juce::jmin (v, endV - 0.01f));
    if (dragHandle == 2 && endAttach) endAttach->setValueAsPartOfGesture (juce::jmax (v, startV + 0.01f));
}

void PadEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (wave.contains (e.position))
    {
        if (startAttach) startAttach->setValueAsCompleteGesture (0.0f);
        if (endAttach) endAttach->setValueAsCompleteGesture (1.0f);
    }
}

void PadEditor::paint (juce::Graphics& g)
{
    const auto& s = proc.skin();
    look::panel (g, s, getLocalBounds().toFloat(), false); look::panelArt (g, s, getLocalBounds().toFloat(), getPosition());
    look::screw (g, s, { (float) getWidth() - 12, 12 });
    const int pad = proc.selectedPad;
    auto pd = proc.kit.pad (pad);
    const auto col = pd.empty() ? s.dim : s.family (pd.type);
    look::dot (g, col, { titleArea.getX() + 8, titleArea.getCentreY() }, 5.5f, ! pd.empty());
    g.setColour (s.tx); g.setFont (Fonts::bold (21));
    g.drawText (pd.empty() ? juce::String ("Empty pad") : (pd.name.isEmpty() ? juce::String (familyLabel (pd.type)) : pd.name.substring (0, 1) + pd.name.substring (1).toLowerCase()),
                titleArea.withTrimmedLeft (24).toNearestInt(), juce::Justification::centredLeft, true);

    // one-shot
    look::lcd (g, s, wave);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (wave.reduced (2).toNearestInt());
        auto w = wave.reduced (8, 10);
        if (! pd.empty())
        {
            const float clean = proc.state.getRawParameterValue (padParamId (pad, PadParam::clean))->load();
            if (clean < 0.98f) look::waveform (g, pd.raw.get(), w, col.withMultipliedAlpha (0.35f + 0.65f * (1 - clean)), 0);
            if (clean > 0.02f && pd.clean != nullptr) look::waveform (g, pd.clean.get(), w, col.withMultipliedAlpha (0.35f + 0.65f * clean), 6);
            // start / end and the fade curve
            const float sx = wave.getX() + startV * wave.getWidth(), ex = wave.getX() + endV * wave.getWidth();
            g.setColour (Colours::black.withAlpha (0.45f));
            g.fillRect (wave.getX(), wave.getY(), sx - wave.getX(), wave.getHeight());
            g.fillRect (ex, wave.getY(), wave.getRight() - ex, wave.getHeight());
            const float fade = proc.state.getRawParameterValue (padParamId (pad, PadParam::fade))->load();
            juce::Path fp;
            const float fx = sx + (ex - sx) * (1 - fade);
            fp.startNewSubPath (fx, wave.getY() + 10);
            for (int i = 1; i <= 24; ++i)
            {
                const float t = (float) i / 24;
                fp.lineTo (fx + (ex - fx) * t, wave.getY() + 10 + (wave.getHeight() - 20) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t)));
            }
            g.setColour (Colours::white.withAlpha (0.55f)); g.strokePath (fp, juce::PathStrokeType (1.2f));
            look::dot (g, Colours::white, { fx, wave.getY() + 10 }, 3.5f, false);
            g.setColour (Colours::white);
            g.fillRect (sx, wave.getY(), 2.0f, wave.getHeight()); g.fillRect (sx, wave.getY(), 10.0f, 10.0f);
            g.fillRect (ex - 2, wave.getY(), 2.0f, wave.getHeight()); g.fillRect (ex - 10, wave.getBottom() - 10, 10.0f, 10.0f);
        }
        else
        {
            look::label (g, s, "Drop a sample here, or LOAD", wave, juce::Justification::centred, 11, 2.0f);
        }
    }
    look::lcdGlass (g, wave);

    // HIT
    look::label (g, s, "Hit", { 18, 186, 34, 33 }, juce::Justification::centredLeft, 10, 2.0f);
    auto box = juce::Rectangle<float> (92, 186, 110, 33);
    look::lcd (g, s, box, false);
    const int n = proc.hitCount (pad);
    g.setColour (Colours::white); g.setFont (Fonts::bold (13.5f));
    g.drawText (n > 0 ? juce::String (pd.hitIndex + 1) + " of " + juce::String (n) : juce::String (pd.empty() ? "-" : "own sample"), box.toNearestInt(), juce::Justification::centred, false);
    if (! pd.empty())
    {
        const double ms = pd.raw->getNumSamples() * 1000.0 / pd.sampleRate;
        juce::String info = juce::String (juce::roundToInt (ms)) + " ms" + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "))
                          + (pd.raw->getNumChannels() > 1 ? "stereo" : "mono") + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "))
                          + bankName (pad) + " / " + noteName (noteForPad (pad)) + " / note " + juce::String (noteForPad (pad));
        g.setColour (s.dim); g.setFont (Fonts::regular (12));
        g.drawText (info, 250, 186, 268, 33, juce::Justification::centredRight, true);
    }

    look::label (g, s, "Choke", { 18, 420, 64, 33 }, juce::Justification::centredLeft, 10, 2.0f);
    look::label (g, s, "Out", { 312, 420, 44, 33 }, juce::Justification::centredLeft, 10, 2.0f);
    g.setColour (s.dim.withAlpha (0.25f)); g.fillRect (18.0f, 470.0f, (float) getWidth() - 36, 1.0f);
    g.setColour (s.dim); g.setFont (Fonts::regular (11.5f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("Drag a pad into your DAW  \xc2\xb7  onto a pad to move it  \xc2\xb7  Option-drag to copy it  \xc2\xb7  double-click a knob to reset")),
                18, 480, getWidth() - 36, 22, juce::Justification::centredLeft, true);
}

// ==================================================================== SeqStrip
SeqStrip::SeqStrip (Processor& p)
    : editKey (p), playKey (p), prevKey (p, juce::CharPointer_UTF8 ("\xe2\x80\xb9")), nextKey (p, juce::CharPointer_UTF8 ("\xe2\x80\xba")), proc (p)
{
    for (auto* k : { &editKey, &playKey, &prevKey, &nextKey }) addAndMakeVisible (*k);
    prevKey.fontSize = nextKey.fontSize = 15;
    editKey.setClickingTogglesState (true);
    editKey.icon = [] (juce::Graphics& g, juce::Rectangle<float> r)
    {   // pencil
        auto c = r.getCentre();
        juce::Path p;
        p.startNewSubPath (c.x - 6, c.y + 6); p.lineTo (c.x - 4.5f, c.y + 1.5f); p.lineTo (c.x + 3.5f, c.y - 6.5f);
        p.lineTo (c.x + 6.5f, c.y - 3.5f); p.lineTo (c.x - 1.5f, c.y + 4.5f); p.closeSubPath();
        g.fillPath (p);
    };
    playKey.icon = [this] (juce::Graphics& g, juce::Rectangle<float> r)
    {
        auto c = r.getCentre();
        if (proc.isPreviewPlaying()) { g.fillRect (c.x - 5, c.y - 5, 10.0f, 10.0f); return; }
        juce::Path p; p.addTriangle (c.x - 4, c.y - 6, c.x - 4, c.y + 6, c.x + 6, c.y);
        g.fillPath (p);
    };
    editKey.onClick = [this] { if (onEdit) onEdit (editKey.getToggleState()); };
    playKey.onClick = [this] { proc.setPreviewPlaying (! proc.isPreviewPlaying()); refresh(); };
    prevKey.onClick = [this]
    {
        auto& k = proc.kit;
        if (k.patterns.empty()) return;
        k.currentPattern = (k.currentPattern + (int) k.patterns.size() - 1) % (int) k.patterns.size();
        proc.history.clear(); proc.patternEdited(); refresh();
        if (onPatternChanged) onPatternChanged();
        if (onEdit) onEdit (editKey.getToggleState());
    };
    nextKey.onClick = [this]
    {
        auto& k = proc.kit;
        if (k.patterns.empty()) return;
        k.currentPattern = (k.currentPattern + 1) % (int) k.patterns.size();
        proc.history.clear(); proc.patternEdited(); refresh();
        if (onPatternChanged) onPatternChanged();
        if (onEdit) onEdit (editKey.getToggleState());
    };
}

void SeqStrip::resized()
{
    editKey.setBounds (20, 12, 36, 37);
    playKey.setBounds (64, 12, 36, 37);
    bpmBox = { 110, 12, 104, 34 };
    prevKey.setBounds (520, 12, 34, 37);
    patBox = { 562, 12, 216, 34 };
    nextKey.setBounds (786, 12, 34, 37);
    roll = { 20, 58, 1300, 40 };
}

void SeqStrip::refresh()
{
    playKey.setToggleState (proc.isPreviewPlaying(), juce::dontSendNotification);
    repaint();
}

void SeqStrip::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    dragBpm = proc.bpm();
}

void SeqStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (bpmBox.contains (e.mouseDownPosition))
    {
        proc.setBpm (std::round ((dragBpm - e.getDistanceFromDragStartY() * 0.25) * 2) / 2);
        repaint();
        return;
    }
    if (roll.contains (e.mouseDownPosition) && ! dragging && e.getDistanceFromDragStart() > 6)
    {
        dragging = true;
        auto f = proc.exportPattern (proc.kit.currentPattern);
        if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
    }
}

void SeqStrip::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! bpmBox.contains (e.position)) return;
    auto* w = new juce::AlertWindow ("BPM", "Tempo of the pattern", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("b", juce::String (proc.bpm(), 1));
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
    {
        const double v = w->getTextEditorContents ("b").getDoubleValue();
        if (r == 1 && v > 20) { proc.setBpm (v); repaint(); }
    }), true);
}

void SeqStrip::paint (juce::Graphics& g)
{
    const auto& s = proc.skin();
    look::panel (g, s, getLocalBounds().toFloat(), false); look::panelArt (g, s, getLocalBounds().toFloat(), getPosition());
    look::lcd (g, s, bpmBox, false);
    g.setColour (Colours::white); g.setFont (Fonts::bold (17));
    auto bt = bpmBox.reduced (14, 0);
    const auto bpmText = juce::String (proc.bpm(), std::abs (proc.bpm() - std::round (proc.bpm())) < 0.01 ? 0 : 1);
    g.drawText (bpmText, bt.toNearestInt(), juce::Justification::centredLeft, false);
    look::label (g, s, "BPM", bt.withTrimmedLeft (Fonts::bold (17).getStringWidthFloat (bpmText) + 8), juce::Justification::centredLeft, 9.5f, 2.0f);

    look::lcd (g, s, patBox, false);
    const auto& k = proc.kit;
    const int np = (int) k.patterns.size();
    const Pattern* pat = juce::isPositiveAndBelow (k.currentPattern, np) ? &k.patterns[(size_t) k.currentPattern] : nullptr;
    g.setColour (Colours::white); g.setFont (Fonts::bold (13.5f));
    const juce::String pt = "Pattern " + juce::String (k.currentPattern + 1) + " of " + juce::String (juce::jmax (1, np));
    g.drawText (pt, patBox.reduced (16, 0).toNearestInt(), juce::Justification::centredLeft, false);
    g.setColour (s.dim); g.setFont (Fonts::semiBold (12.5f));
    const bool kits = k.analysis != nullptr && k.analysis->numKits > 1;
    g.drawText ((kits && pat != nullptr ? "Kit " + juce::String::charToString ((juce::juce_wchar) ('A' + pat->kit)) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) : juce::String())
                + (pat != nullptr ? juce::String (pat->bars) : juce::String ("1")) + (pat != nullptr && pat->bars > 1 ? " bars" : " bar"),
                patBox.reduced (16, 0).toNearestInt(), juce::Justification::centredRight, false);
    g.setColour (s.dim); g.setFont (Fonts::semiBold (11.5f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("Drag into your DAW  \xe2\x87\xa2")), 1000, 12, 320, 34, juce::Justification::centredRight, false);

    look::lcd (g, s, roll);
    if (pat != nullptr)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (roll.reduced (2).toNearestInt());
        const float len = (float) pat->ticks(), w = roll.getWidth();
        for (int b = 0; b <= pat->bars * 16; ++b)
        {
            const float x = roll.getX() + (float) b * 4 / len * w;
            g.setColour (Colours::white.withAlpha (b % 16 == 0 ? 0.18f : b % 4 == 0 ? 0.09f : 0.04f));
            g.fillRect (x, roll.getY(), 1.0f, roll.getHeight());
        }
        for (auto& n : pat->notes)
        {
            auto pd = proc.kit.pad (n.pad);
            const auto fam = pd.empty() ? Family::sample : pd.type;
            const float lane = fam == Family::kick ? 0.72f : (fam == Family::snare || fam == Family::clap) ? 0.18f : fam == Family::hat || fam == Family::openHat ? 0.45f : 0.6f;
            const auto c = s.family (fam);
            const float x = roll.getX() + (float) n.tick / len * w;
            const float nw = juce::jmax (3.0f, (float) juce::jmax (1, juce::jmin (4, n.length)) / len * w - 3);
            auto r = juce::Rectangle<float> (x + 1.5f, roll.getY() + roll.getHeight() * lane - 4, nw, 8);
            g.setColour (c.withAlpha (0.3f)); g.fillRoundedRectangle (r.expanded (2), 3);
            g.setColour (c.withMultipliedAlpha (0.5f + 0.5f * n.velocity)); g.fillRoundedRectangle (r, 2);
        }
        if (proc.isPreviewPlaying())
        {
            const float x = roll.getX() + (float) (proc.previewTick() / len) * w;
            g.setColour (Colours::white.withAlpha (0.35f)); g.fillRect (x - 2, roll.getY(), 5.0f, roll.getHeight());
            g.setColour (Colours::white); g.fillRect (x, roll.getY(), 1.5f, roll.getHeight());
        }
    }
    look::lcdGlass (g, roll);
}

} // namespace eclipse
