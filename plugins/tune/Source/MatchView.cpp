#include "MatchView.h"
#include "Scales.h"

namespace tune
{

using lt::ui::Fonts;
using lt::ui::Glass;
using lt::ui::Logo;

namespace
{
    const char* rowNames[] = { "KEY & SCALE", "RETUNE SPEED", "HUMANIZE", "FLEX TUNE", "VIBRATO", "FORMANT" };
    juce::Colour glowOf (const Palette& p) { return p.isLight ? juce::Colour (0xffffd60a) : p.accent; }
}

MatchView::MatchView (const Palette& p, std::shared_ptr<MatchResult> previous) : palette (p), result (std::move (previous))
{
    for (auto& s : switches)
    {
        s.palette = &palette;
        s.setToggleState (true, juce::dontSendNotification);
        addAndMakeVisible (s);
    }
    amount.palette = &palette;
    amount.setRange (0.0, 100.0, 1.0);
    amount.setValue (100.0, juce::dontSendNotification);
    amount.setDoubleClickReturnValue (true, 100.0);
    amount.formatValue = [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; };
    addAndMakeVisible (amount);

    applyButton.getProperties().set ("accent", true);
    applyButton.onClick = [this]
    {
        if (result == nullptr || ! result->ok || onApply == nullptr) return;
        std::array<bool, rowCount> en {};
        for (int i = 0; i < rowCount; ++i) en[(size_t) i] = switches[i].getToggleState();
        onApply (*result, en, (float) amount.getValue() * 0.01f);
    };
    cancelButton.onClick = [this] { if (onClose) onClose(); };
    closeButton.onClick = [this] { if (onClose) onClose(); };
    closeButton.palette = &palette;
    for (auto* c : { (juce::Component*) &applyButton, (juce::Component*) &cancelButton, (juce::Component*) &closeButton })
        addAndMakeVisible (c);

    status = juce::String (juce::CharPointer_UTF8 (result != nullptr && result->ok
                 ? "Vocal isolated \xc2\xb7 Analysis complete"
                 : "or click to browse (WAV, AIFF, FLAC, MP3) \xc2\xb7 a vocal stem works best"));
    refreshRows();
    startTimerHz (30);
}

MatchView::~MatchView()
{
    if (cancel) cancel->store (true);
}

void MatchView::setPalette (const Palette& p)
{
    palette = p;
    repaint();
}

void MatchView::refreshRows()
{
    auto ok = result != nullptr && result->ok;
    for (auto& s : switches) s.setEnabled (ok);
    applyButton.setEnabled (ok && ! analysing);
}

bool MatchView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void MatchView::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) { analyse (juce::File (f)); return; }
}

void MatchView::mouseDown (const juce::MouseEvent& e)
{
    if (dropArea().contains (e.position) && ! analysing) browse();
}

void MatchView::browse()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a reference song or vocal stem",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<MatchView> (this)] (const juce::FileChooser& fc)
    {
        if (safe != nullptr && fc.getResult().existsAsFile()) safe->analyse (fc.getResult());
    });
}

void MatchView::analyse (const juce::File& file)
{
    if (cancel) cancel->store (true);
    cancel = std::make_shared<std::atomic<bool>> (false);
    analysing = true;
    analysingName = file.getFileName();
    status = "Isolating the vocal and analysing...";
    refreshRows();

    auto flag = cancel;
    juce::Thread::launch ([file, flag, safe = juce::Component::SafePointer<MatchView> (this)]
    {
        auto r = std::make_shared<MatchResult> (MatchAnalysis::analyseFile (file, [flag] { return flag->load(); }));
        if (flag->load()) return;
        juce::MessageManager::callAsync ([safe, r, flag]
        {
            if (safe == nullptr || flag->load()) return;
            safe->analysing = false;
            if (r->ok)
            {
                safe->result = r;
                safe->status = juce::String (juce::CharPointer_UTF8 ("Vocal isolated \xc2\xb7 Analysis complete"));
                if (safe->onResult) safe->onResult (r);
            }
            else
            {
                safe->status = r->error;
            }
            safe->refreshRows();
            safe->repaint();
        });
    });
}

void MatchView::timerCallback()
{
    time += 1.0f / 30.0f;
    repaint();
}

void MatchView::resized()
{
    for (int i = 0; i < rowCount; ++i)
    {
        auto r = rowArea (i);
        switches[i].setBounds ((int) r.getRight() - 62, (int) r.getY() + 2, 56, (int) r.getHeight() - 4);
    }
    amount.setBounds (30, 534, 76, 76 + 26);
    applyButton.setBounds (558, 544, 362, 48);
    cancelButton.setBounds (942, 549, 124, 38);
    closeButton.setBounds (1024, 22, 44, 44);
}

void MatchView::paint (juce::Graphics& g)
{
    auto& p = palette;
    auto b = getLocalBounds().toFloat();
    auto glowC = glowOf (p);

    // the window floats over the plugin (its nebula stays visible, dimmed)
    g.setColour (p.background.withAlpha (p.isLight ? 0.94f : 0.96f));
    g.fillRect (b);
    Glass::drawPanel (g, b.reduced (8.0f), 14.0f, p, 0.3f);

    Logo::draw (g, { 30.0f, 26.0f, 190.0f, 38.0f }, Logo::specFor ("TUNE"), p, juce::Justification::centredLeft);
    g.setColour (p.isLight ? juce::Colour (0xff111111) : juce::Colours::white);
    Fonts::drawSpaced (g, "MATCH REFERENCE", { 300.0f, 24.0f, 500.0f, 42.0f }, Fonts::light (38.0f), 0.04f, juce::Justification::centred);

    // ---- drop zone
    auto drop = dropArea();
    Glass::drawPanel (g, drop, 10.0f, p, dragOver ? 2.6f : 1.2f, dragOver);
    g.setColour (p.text);
    Fonts::drawSpaced (g, "DROP A SONG TO COPY ITS TUNING", drop.withHeight (54.0f).translated (0.0f, 8.0f),
                       Fonts::regular (18.0f), 0.04f, juce::Justification::centred);

    auto wave = drop.reduced (6.0f, 0.0f).withTrimmedTop (52.0f).withHeight (64.0f);
    juce::Random rnd (3);
    if (result != nullptr && result->ok && ! result->envelope.empty())
    {
        auto& env = result->envelope;
        auto& pitch = result->pitch;
        const int cols = 340;
        // star dust: the full mix
        for (int i = 0; i < cols; ++i)
        {
            auto idx = (size_t) ((double) i / cols * (double) env.size());
            auto e = env[juce::jmin (idx, env.size() - 1)];
            auto x = wave.getX() + wave.getWidth() * (float) i / (float) cols;
            for (int k = 0; k < 4; ++k)
            {
                auto spread = e * wave.getHeight() * 0.5f * (rnd.nextFloat() * 2.0f - 1.0f);
                auto s = 0.8f + rnd.nextFloat() * 1.8f;
                auto tw = 0.5f + 0.5f * std::sin (time * (1.0f + rnd.nextFloat() * 3.0f) + (float) i);
                g.setColour (glowC.brighter (0.6f).withAlpha ((0.15f + 0.5f * e) * (0.5f + 0.5f * tw)));
                g.fillEllipse (x, wave.getCentreY() + spread, s, s);
            }
        }
        // the isolated vocal as a glowing line
        float lo = 200, hi = 0;
        for (auto m : pitch) if (m > 0) { lo = juce::jmin (lo, m); hi = juce::jmax (hi, m); }
        if (hi > lo)
        {
            juce::Path line;
            bool started = false;
            for (int i = 0; i < cols * 2; ++i)
            {
                auto idx = (size_t) ((double) i / (cols * 2) * (double) pitch.size());
                auto m = pitch[juce::jmin (idx, pitch.size() - 1)];
                auto x = wave.getX() + wave.getWidth() * (float) i / (float) (cols * 2);
                if (m <= 0) { started = false; continue; }
                auto y = wave.getBottom() - 6.0f - (m - lo) / (hi - lo) * (wave.getHeight() - 12.0f);
                if (! started) { line.startNewSubPath (x, y); started = true; } else line.lineTo (x, y);
            }
            g.setColour (glowC.withAlpha (0.3f));
            g.strokePath (line, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (p.isLight ? juce::Colours::white : glowC.brighter (0.7f));
            g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
    else
    {
        // idle: a faint drifting star line
        for (int i = 0; i < 260; ++i)
        {
            auto x = wave.getX() + wave.getWidth() * (float) i / 260.0f;
            auto busy = analysing ? 0.6f + 0.4f * std::sin (time * 4.0f + (float) i * 0.08f) : 0.25f;
            auto y = wave.getCentreY() + std::sin ((float) i * 0.07f + time * 1.3f) * 6.0f * busy + (rnd.nextFloat() - 0.5f) * 10.0f * busy;
            auto s = 0.8f + rnd.nextFloat() * 1.4f;
            g.setColour (glowC.brighter (0.6f).withAlpha (0.2f + 0.4f * busy * rnd.nextFloat()));
            g.fillEllipse (x, y, s, s);
        }
    }

    auto name = analysing ? analysingName : result != nullptr ? result->fileName : juce::String();
    g.setColour (p.isLight ? juce::Colour (0xff111111) : p.value);
    g.setFont (Fonts::regular (19.0f));
    g.drawText (name, juce::Rectangle<float> (drop.getX(), drop.getBottom() - 50.0f, drop.getWidth(), 24.0f), juce::Justification::centred);
    g.setColour (p.textDim);
    g.setFont (Fonts::regular (14.0f));
    g.drawText (status, juce::Rectangle<float> (drop.getX(), drop.getBottom() - 27.0f, drop.getWidth(), 20.0f), juce::Justification::centred);

    // ---- detected settings
    auto ok = result != nullptr && result->ok;
    for (int i = 0; i < rowCount; ++i)
    {
        auto r = rowArea (i);
        Glass::drawPill (g, r, p, false, false, false);
        auto on = switches[i].getToggleState() && ok;
        g.setColour (p.text.withAlpha (on || ! ok ? 1.0f : 0.5f));
        g.setFont (Fonts::regular (16.0f));
        auto labelW = juce::GlyphArrangement::getStringWidth (Fonts::regular (16.0f), rowNames[i]);
        g.drawText (rowNames[i], r.withTrimmedLeft (18.0f), juce::Justification::centredLeft);

        juce::String value = "--";
        if (ok)
        {
            auto& m = *result;
            switch (i)
            {
                case keyRow:      value = Scales::display (m.key, m.scale); break;
                case retuneRow:   value = juce::String (juce::roundToInt (m.retuneMs)) + juce::String (juce::CharPointer_UTF8 (" ms \xc2\xb7 ")) + (m.hard ? "HARD" : "SOFT"); break;
                case humanizeRow: value = juce::String (juce::roundToInt (m.humanize)); break;
                case flexRow:     value = juce::String (juce::roundToInt (m.flex)); break;
                case vibratoRow:  value = MatchAnalysis::vibratoText (m.vibrato); break;
                case formantRow:  value = (m.formant > 0 ? "+" : "") + juce::String (m.formant, std::abs (m.formant - std::round (m.formant)) > 0.01f ? 1 : 0); break;
                default: break;
            }
        }
        auto valueFont = Fonts::regular (17.0f);
        auto vw = juce::GlyphArrangement::getStringWidth (valueFont, value);
        auto valueRight = r.getRight() - 96.0f;
        // dotted leader
        g.setColour (p.text.withAlpha (0.35f));
        for (auto x = r.getX() + 30.0f + labelW; x < valueRight - vw - 12.0f; x += 6.0f)
            g.fillEllipse (x, r.getCentreY() - 0.8f, 1.6f, 1.6f);
        for (int k = 0; k < 4; ++k) g.fillEllipse (valueRight + 12.0f + 6.0f * (float) k, r.getCentreY() - 0.8f, 1.6f, 1.6f);
        g.setColour (on ? (p.isLight ? juce::Colour (0xff111111) : p.value) : p.textDim);
        g.setFont (valueFont);
        g.drawText (value, juce::Rectangle<float> (valueRight - vw - 4.0f, r.getY(), vw + 6.0f, r.getHeight()), juce::Justification::centredRight);
    }

    // ---- pitch preview
    auto pv = previewArea();
    Glass::drawPanel (g, pv, 10.0f, p, 1.2f);
    g.setColour (p.text);
    Fonts::drawSpaced (g, "PITCH PREVIEW", pv.withHeight (40.0f).translated (0.0f, 4.0f), Fonts::regular (17.0f), 0.04f, juce::Justification::centred);
    auto staff = pv.reduced (24.0f, 0.0f).withTrimmedTop (82.0f).withHeight (120.0f);
    g.setColour (p.text.withAlpha (0.18f));
    for (int k = 0; k < 5; ++k)
        g.drawHorizontalLine ((int) (staff.getY() + staff.getHeight() * (float) k / 4.0f), staff.getX(), staff.getRight());
    if (ok)
    {
        auto keyName = Keys::noteName (result->key);
        drawNoteName (g, keyName, { pv.getCentreX(), staff.getCentreY() + 4.0f }, 190.0f,
                      p.isLight ? juce::Colours::white : juce::Colours::white, p.isLight ? 0.45f : 0.12f);

        // the most voiced 3 seconds of the curve
        auto& pitch = result->pitch;
        const int span = (int) (3.0 / MatchResult::hopSeconds);
        int bestStart = 0, bestCount = -1;
        for (int s = 0; s + span < (int) pitch.size(); s += span / 4)
        {
            int c = 0;
            for (int i = s; i < s + span; ++i) c += pitch[(size_t) i] > 0 ? 1 : 0;
            if (c > bestCount) { bestCount = c; bestStart = s; }
        }
        float lo = 200, hi = 0;
        for (int i = bestStart; i < juce::jmin ((int) pitch.size(), bestStart + span); ++i)
            if (pitch[(size_t) i] > 0) { lo = juce::jmin (lo, pitch[(size_t) i]); hi = juce::jmax (hi, pitch[(size_t) i]); }
        if (hi > lo)
        {
            lo -= 0.5f; hi += 0.5f;
            auto area = staff.reduced (20.0f, 6.0f);
            juce::Path line;
            bool started = false;
            for (int i = bestStart; i < juce::jmin ((int) pitch.size(), bestStart + span); ++i)
            {
                auto m = pitch[(size_t) i];
                if (m <= 0) { started = false; continue; }
                auto x = area.getX() + area.getWidth() * (float) (i - bestStart) / (float) span;
                auto y = area.getBottom() - (m - lo) / (hi - lo) * area.getHeight();
                if (! started) { line.startNewSubPath (x, y); started = true; } else line.lineTo (x, y);
            }
            g.setColour (glowC.withAlpha (0.35f));
            g.strokePath (line, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (p.isLight ? juce::Colour (0xffffc300) : glowC.brighter (0.6f));
            g.strokePath (line, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    g.setColour (p.text);
    Fonts::drawSpaced (g, "MATCH AMOUNT", { 118.0f, 552.0f, 220.0f, 40.0f }, Fonts::regular (18.0f), 0.04f, juce::Justification::centredLeft);
}

} // namespace tune
