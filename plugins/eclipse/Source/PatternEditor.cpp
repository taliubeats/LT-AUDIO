#include "PatternEditor.h"

namespace eclipse
{

using juce::Colour;
namespace Colours = juce::Colours;
using lt::ui::Fonts;

namespace
{
constexpr float rowH = 30.0f;

void shapeIcon (juce::Graphics& g, juce::Rectangle<float> r, RollShape s)
{
    auto b = r.withSizeKeepingCentre (18, 12);
    juce::Path p;
    switch (s)
    {
        case RollShape::up:   p.startNewSubPath (b.getBottomLeft()); p.lineTo (b.getTopRight()); break;
        case RollShape::down: p.startNewSubPath (b.getTopLeft()); p.lineTo (b.getBottomRight()); break;
        case RollShape::vee:  p.startNewSubPath (b.getTopLeft()); p.lineTo (b.getCentreX(), b.getBottom()); p.lineTo (b.getTopRight()); break;
        case RollShape::flat: p.startNewSubPath (b.getX(), b.getCentreY()); p.lineTo (b.getRight(), b.getCentreY()); break;
    }
    g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
}

PatternEditor::PatternEditor (Processor& p)
    : proc (p), resetKey (p, "RESET"), doneKey (p, "DONE"), allKey (p, "ALL", true), kickKey (p, "KICK", true), snareKey (p, "SNARE", true),
      hatsKey (p, "HATS", true), origKey (p, "ORIGINAL"), prevKey (p, juce::CharPointer_UTF8 ("\xe2\x80\xb9")), nextKey (p, juce::CharPointer_UTF8 ("\xe2\x80\xba")),
      barsKey (p, "1 BAR"), amount (p), rnd (juce::Time::currentTimeMillis())
{
    static const char* rn[] = { "1/8", "1/16", "1/32", "1/64", "T" };
    for (int i = 0; i < 5; ++i)
    {
        resKeys[(size_t) i] = std::make_unique<KeyButton> (p, rn[i]);
        resKeys[(size_t) i]->onClick = [this, i]
        {
            if (i == 4) triplet = ! triplet;
            else res = std::array<int, 4> { 8, 4, 2, 1 }[(size_t) i];
            updateKeys(); repaint();
        };
        addAndMakeVisible (*resKeys[(size_t) i]);
    }
    static const char* rr[] = { "1/32", "1/32T", "1/64" };
    for (int i = 0; i < 3; ++i)
    {
        rollKeys[(size_t) i] = std::make_unique<KeyButton> (p, rr[i]);
        rollKeys[(size_t) i]->onClick = [this, i] { opts.rates[(size_t) i] = ! opts.rates[(size_t) i]; opts.rolls = opts.rates[0] || opts.rates[1] || opts.rates[2]; updateKeys(); };
        addAndMakeVisible (*rollKeys[(size_t) i]);
    }
    for (int i = 0; i < 4; ++i)
    {
        shapeKeys[(size_t) i] = std::make_unique<KeyButton> (p);
        shapeKeys[(size_t) i]->icon = [i] (juce::Graphics& g, juce::Rectangle<float> r) { shapeIcon (g, r, (RollShape) i); };
        shapeKeys[(size_t) i]->onClick = [this, i] { opts.shape = (RollShape) i; updateKeys(); };
        addAndMakeVisible (*shapeKeys[(size_t) i]);
    }
    for (auto* k : { &resetKey, &doneKey, &allKey, &kickKey, &snareKey, &hatsKey, &origKey, &prevKey, &nextKey, &barsKey }) addAndMakeVisible (*k);
    prevKey.fontSize = nextKey.fontSize = 15;
    addAndMakeVisible (amount);
    amount.setRange (0, 1);
    amount.setValue (0.5, juce::dontSendNotification);
    amount.onValueChange = [this] { opts.amount = (float) amount.getValue(); };

    allKey.onClick = [this] { doVary (VaryTarget::all); };
    kickKey.onClick = [this] { doVary (VaryTarget::kick); };
    snareKey.onClick = [this] { doVary (VaryTarget::snare); };
    hatsKey.onClick = [this] { doVary (VaryTarget::hats); };
    doneKey.onClick = [this] { if (onDone) onDone(); };
    resetKey.onClick = [this]
    {   // back to the pattern found in the song
        auto an = proc.kit.analysis;
        const int i = proc.kit.currentPattern;
        if (an != nullptr && juce::isPositiveAndBelow (i, (int) an->patterns.size())) current() = an->patterns[(size_t) i];
        else if (! proc.history.empty()) current() = proc.history.front();
        current().name = "Original";
        proc.patternEdited(); refresh();
        if (onChange) onChange();
    };
    origKey.onClick = [this]
    {
        if (proc.history.empty()) return;
        historyPos = 0; current() = proc.history.front(); proc.patternEdited(); refresh();
        if (onChange) onChange();
    };
    prevKey.onClick = [this]
    {
        if (historyPos <= 0) return;
        current() = proc.history[(size_t) --historyPos]; proc.patternEdited(); refresh();
        if (onChange) onChange();
    };
    nextKey.onClick = [this]
    {
        if (historyPos + 1 >= (int) proc.history.size()) return;
        current() = proc.history[(size_t) ++historyPos]; proc.patternEdited(); refresh();
        if (onChange) onChange();
    };
    barsKey.onClick = [this]
    {
        auto& p = current();
        p.bars = p.bars == 1 ? 2 : p.bars == 2 ? 4 : 1;
        tidy (p); proc.patternEdited(); refresh();
        if (onChange) onChange();
    };
    opts.amount = 0.5f;
    updateKeys();
}

Pattern& PatternEditor::current()
{
    auto& k = proc.kit;
    if (k.patterns.empty()) { Pattern p; p.name = "Original"; k.patterns.push_back (p); k.currentPattern = 0; }
    k.currentPattern = juce::jlimit (0, (int) k.patterns.size() - 1, k.currentPattern);
    return k.patterns[(size_t) k.currentPattern];
}

void PatternEditor::resized()
{
    // row 1: title + resolution + reset/done
    int x = 250;
    for (int i = 0; i < 5; ++i) { resKeys[(size_t) i]->setBounds (x, 16, i == 4 ? 34 : 50, 33); x += (i == 4 ? 34 : 50) + 6; }
    resetKey.setBounds (getWidth() - 186, 16, 80, 33);
    doneKey.setBounds (getWidth() - 98, 16, 80, 33);
    // row 2: vary
    allKey.setBounds (84, 60, 72, 33); kickKey.setBounds (162, 60, 84, 33); snareKey.setBounds (252, 60, 92, 33); hatsKey.setBounds (350, 60, 84, 33);
    amount.setBounds (500, 62, getWidth() - 560, 28);
    // row 3: rolls + shape
    rollKeys[0]->setBounds (84, 102, 56, 33); rollKeys[1]->setBounds (146, 102, 64, 33); rollKeys[2]->setBounds (216, 102, 56, 33);
    for (int i = 0; i < 4; ++i) shapeKeys[(size_t) i]->setBounds (370 + i * 44, 102, 38, 33);
    barsKey.setBounds (getWidth() - 98, 102, 80, 33);
    // row 4: history
    origKey.setBounds (18, 144, 96, 31); prevKey.setBounds (120, 144, 31, 31); nextKey.setBounds (getWidth() - 49, 144, 31, 31);
    labels = { 18, 186, 122, (float) getHeight() - 204 };
    grid = { 146, 186, (float) getWidth() - 164, (float) getHeight() - 204 };
}

void PatternEditor::updateKeys()
{
    static const int steps[] = { 8, 4, 2, 1 };
    for (int i = 0; i < 4; ++i) resKeys[(size_t) i]->setToggleState (res == steps[i], juce::dontSendNotification);
    resKeys[4]->setToggleState (triplet, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i) rollKeys[(size_t) i]->setToggleState (opts.rates[(size_t) i], juce::dontSendNotification);
    for (int i = 0; i < 4; ++i) shapeKeys[(size_t) i]->setToggleState ((int) opts.shape == i, juce::dontSendNotification);
    const auto& s = proc.skin();
    kickKey.onColour = s.family (Family::kick); snareKey.onColour = s.family (Family::snare); hatsKey.onColour = s.family (Family::hat);
    repaint();
}

void PatternEditor::refresh()
{
    rows.clear();
    for (int i = 0; i < numPads; ++i) if (! proc.kit.pad (i).empty()) rows.push_back (i);
    for (auto& n : current().notes) if (std::find (rows.begin(), rows.end(), n.pad) == rows.end()) rows.push_back (n.pad);
    std::sort (rows.begin(), rows.end());
    if (proc.history.empty()) { proc.history.push_back (current()); historyPos = 0; }
    historyPos = juce::jlimit (0, (int) proc.history.size() - 1, historyPos);
    const auto& p = current();
    barsKey.setButtonText (juce::String (p.bars) + (p.bars == 1 ? " BAR" : " BARS"));
    scroll = juce::jlimit (0, juce::jmax (0, (int) rows.size() - (int) (grid.getHeight() / rowH)), scroll);
    updateKeys();
    repaint();
}

void PatternEditor::pushHistory (const Pattern& p)
{
    if (proc.history.empty()) proc.history.push_back (current());
    proc.history.resize ((size_t) historyPos + 1);
    proc.history.push_back (p);
    while (proc.history.size() > 31) proc.history.erase (proc.history.begin() + 1);   // the original + the last 30
    historyPos = (int) proc.history.size() - 1;
}

void PatternEditor::doVary (VaryTarget t)
{
    if (proc.history.empty()) proc.history.push_back (current());
    opts.target = t;
    auto v = vary (proc.history.front(), proc.padFamilies(), opts, rnd);
    int count = 0;
    for (auto& h : proc.history) count += h.name.startsWith ("V") ? 1 : 0;
    v.name = varyName (t, count + 1);
    pushHistory (v);
    current() = v;
    proc.patternEdited();
    refresh();
    if (onChange) onChange();
    for (auto* k : { &allKey, &kickKey, &snareKey, &hatsKey }) k->setToggleState (false, juce::dontSendNotification);
    (t == VaryTarget::all ? allKey : t == VaryTarget::kick ? kickKey : t == VaryTarget::snare ? snareKey : hatsKey).setToggleState (true, juce::dontSendNotification);
}

int PatternEditor::tickAt (float x) const
{
    const float len = (float) const_cast<PatternEditor*> (this)->current().ticks();
    const float t = (x - grid.getX()) / grid.getWidth() * len;
    const float st = step();
    return juce::jlimit (0, (int) len - 1, (int) (std::floor (t / st) * st + 0.5f));
}

int PatternEditor::rowAt (float y) const
{
    const int r = (int) ((y - grid.getY()) / rowH) + scroll;
    return juce::isPositiveAndBelow (r, (int) rows.size()) ? r : -1;
}

void PatternEditor::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    scroll = juce::jlimit (0, juce::jmax (0, (int) rows.size() - (int) (grid.getHeight() / rowH)), scroll + (w.deltaY < 0 ? 1 : -1));
    repaint();
}

void PatternEditor::mouseDown (const juce::MouseEvent& e)
{
    dragRow = dragNote = -1; rollMode = false; lastRollTick = -1;
    if (labels.contains (e.position))
    {
        const int r = rowAt (e.position.y);
        if (r >= 0) { proc.triggerPad (rows[(size_t) r], 0.9f); proc.selectedPad = rows[(size_t) r]; }
        return;
    }
    if (! grid.contains (e.position)) return;
    const int r = rowAt (e.position.y);
    if (r < 0) return;
    dragRow = r;
    const int pad = rows[(size_t) r];
    const int tick = tickAt (e.position.x);
    auto& p = current();
    rollMode = e.mods.isCommandDown();
    // a note under the mouse?
    for (int i = 0; i < (int) p.notes.size(); ++i)
    {
        const auto& n = p.notes[(size_t) i];
        if (n.pad == pad && tick >= n.tick - (int) step() / 2 && tick < n.tick + juce::jmax (1, (int) step()))
        {
            dragNote = i; dragVel = n.velocity;
            return;
        }
    }
    Note n; n.pad = pad; n.tick = tick; n.length = juce::jmax (1, (int) step()); n.velocity = 0.8f;
    p.notes.push_back (n);
    dragNote = (int) p.notes.size() - 1; dragVel = n.velocity;
    lastRollTick = tick;
    proc.triggerPad (pad, 0.8f);
    tidy (p);
    for (int i = 0; i < (int) p.notes.size(); ++i) if (p.notes[(size_t) i].pad == pad && p.notes[(size_t) i].tick == tick) dragNote = i;
    dragTick = -2;     // "just added": don't delete on mouse up
    proc.patternEdited(); repaint();
}

void PatternEditor::mouseDrag (const juce::MouseEvent& e)
{
    auto& p = current();
    if (dragRow < 0) return;
    if (rollMode)
    {   // Cmd + drag: fill the row with notes at the grid step
        const int pad = rows[(size_t) dragRow];
        const int tick = tickAt (e.position.x);
        if (tick != lastRollTick)
        {
            bool exists = false;
            for (auto& n : p.notes) exists |= n.pad == pad && n.tick == tick;
            if (! exists) p.notes.push_back ({ pad, tick, juce::jmax (1, (int) step()), 0.75f });
            lastRollTick = tick;
            tidy (p); proc.patternEdited(); repaint();
        }
        return;
    }
    if (juce::isPositiveAndBelow (dragNote, (int) p.notes.size()) && std::abs (e.getDistanceFromDragStartY()) > 2)
    {
        p.notes[(size_t) dragNote].velocity = juce::jlimit (0.05f, 1.0f, dragVel - (float) e.getDistanceFromDragStartY() / 120.0f);
        dragTick = -3;     // velocity changed: keep the note
        proc.patternEdited(); repaint();
    }
}

void PatternEditor::mouseUp (const juce::MouseEvent& e)
{
    auto& p = current();
    if (! rollMode && dragTick == -1 && juce::isPositiveAndBelow (dragNote, (int) p.notes.size()) && e.getDistanceFromDragStart() < 3)
    {
        p.notes.erase (p.notes.begin() + dragNote);          // click on a note removes it
        proc.patternEdited();
    }
    if (dragRow >= 0 && onChange) onChange();
    dragRow = dragNote = -1; dragTick = -1;
    repaint();
}

void PatternEditor::paint (juce::Graphics& g)
{
    const auto& s = proc.skin();
    look::panel (g, s, getLocalBounds().toFloat(), false); look::panelArt (g, s, getLocalBounds().toFloat(), getPosition());
    auto& p = current();
    look::label (g, s, "Pattern", { 22, 16, 80, 33 }, juce::Justification::centredLeft, 10, 2.0f);
    g.setColour (s.tx); g.setFont (Fonts::bold (14));
    g.drawText ("Pattern " + juce::String (proc.kit.currentPattern + 1) + " of " + juce::String ((int) proc.kit.patterns.size()), 92, 16, 150, 33, juce::Justification::centredLeft, false);
    look::label (g, s, "Vary", { 22, 60, 60, 33 }, juce::Justification::centredLeft, 10, 2.0f);
    look::label (g, s, "Subtle", { 440, 60, 60, 33 }, juce::Justification::centredRight, 9, 1.4f);
    look::label (g, s, "Bold", { (float) getWidth() - 56, 60, 40, 33 }, juce::Justification::centredLeft, 9, 1.4f);
    look::label (g, s, "Rolls", { 22, 102, 60, 33 }, juce::Justification::centredLeft, 10, 2.0f);
    look::label (g, s, "Shape", { 300, 102, 64, 33 }, juce::Justification::centredLeft, 10, 2.0f);

    // history chips
    {
        auto strip = juce::Rectangle<float> (158, 144, (float) getWidth() - 214, 31);
        look::lcd (g, s, strip, false);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (strip.reduced (2).toNearestInt());
        float x = strip.getX() + 8;
        const int first = juce::jmax (0, historyPos - 4);
        for (int i = first; i < (int) proc.history.size(); ++i)
        {
            const auto name = i == 0 ? juce::String ("Original") : proc.history[(size_t) i].name;
            const float w = Fonts::bold (11).getStringWidthFloat (name) + 30;
            auto chip = juce::Rectangle<float> (x, strip.getY() + 5, w, strip.getHeight() - 10);
            const bool on = i == historyPos;
            g.setColour (on ? s.acc : s.ringOff); g.fillRoundedRectangle (chip, chip.getHeight() / 2);
            look::dot (g, on ? Colours::white : s.dim, { chip.getX() + 11, chip.getCentreY() }, 3, false);
            g.setColour (on ? Colours::white : s.tx); g.setFont (Fonts::bold (11));
            g.drawText (name, chip.withTrimmedLeft (18).toNearestInt(), juce::Justification::centredLeft, false);
            x += w + 6;
        }
    }

    // grid
    look::lcd (g, s, grid, false);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (grid.reduced (2).toNearestInt());
        const float len = (float) p.ticks(), w = grid.getWidth();
        const int visible = (int) (grid.getHeight() / rowH) + 1;
        for (int r = 0; r < visible; ++r)
        {
            const int idx = r + scroll;
            if (idx >= (int) rows.size()) break;
            const float y = grid.getY() + (float) r * rowH;
            if (rows[(size_t) idx] == proc.selectedPad) { g.setColour (Colours::white.withAlpha (0.05f)); g.fillRect (grid.getX(), y, w, rowH); }
            g.setColour (Colours::white.withAlpha (0.05f)); g.fillRect (grid.getX(), y + rowH - 1, w, 1.0f);
        }
        for (float t = 0; t <= len; t += step())
        {
            const float x = grid.getX() + t / len * w;
            const int ti = (int) std::round (t);
            g.setColour (Colours::white.withAlpha (ti % 64 == 0 ? 0.22f : ti % 16 == 0 ? 0.12f : 0.045f));
            g.fillRect (x, grid.getY(), 1.0f, grid.getHeight());
        }
        for (auto& n : p.notes)
        {
            auto it = std::find (rows.begin(), rows.end(), n.pad);
            if (it == rows.end()) continue;
            const int r = (int) std::distance (rows.begin(), it) - scroll;
            if (r < 0 || r >= visible) continue;
            auto pd = proc.kit.pad (n.pad);
            const auto c = s.family (pd.empty() ? Family::sample : pd.type);
            const float x = grid.getX() + (float) n.tick / len * w;
            const float nw = juce::jmax (4.0f, (float) juce::jmax (1, n.length) / len * w - 2);
            const float hh = (rowH - 10) * (0.45f + 0.55f * n.velocity);
            auto rr = juce::Rectangle<float> (x + 1, grid.getY() + (float) r * rowH + (rowH - hh) / 2, nw, hh);
            g.setColour (c.withAlpha (0.3f)); g.fillRoundedRectangle (rr.expanded (2), 4);
            g.setColour (c.withMultipliedAlpha (0.45f + 0.55f * n.velocity)); g.fillRoundedRectangle (rr, 3);
        }
        if (proc.isPreviewPlaying())
        {
            const float x = grid.getX() + (float) (proc.previewTick() / len) * w;
            g.setColour (Colours::white.withAlpha (0.3f)); g.fillRect (x - 2, grid.getY(), 5.0f, grid.getHeight());
            g.setColour (Colours::white); g.fillRect (x, grid.getY(), 1.5f, grid.getHeight());
        }
    }
    look::lcdGlass (g, grid);
    // row labels
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (labels.toNearestInt());
        const int visible = (int) (labels.getHeight() / rowH) + 1;
        for (int r = 0; r < visible; ++r)
        {
            const int idx = r + scroll;
            if (idx >= (int) rows.size()) break;
            auto pd = proc.kit.pad (rows[(size_t) idx]);
            const auto c = s.family (pd.empty() ? Family::sample : pd.type);
            const float y = labels.getY() + (float) r * rowH;
            look::dot (g, c, { labels.getX() + 7, y + rowH / 2 }, 3.5f, false);
            g.setColour (rows[(size_t) idx] == proc.selectedPad ? s.tx : s.tx.withAlpha (0.75f)); g.setFont (Fonts::semiBold (11.5f));
            g.drawText (pd.empty() ? noteName (noteForPad (rows[(size_t) idx])) : (pd.name.isEmpty() ? juce::String (familyName (pd.type)) : pd.name),
                        (int) labels.getX() + 16, (int) y, (int) labels.getWidth() - 16, (int) rowH, juce::Justification::centredLeft, true);
        }
    }
    g.setColour (s.dim); g.setFont (Fonts::regular (11));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("Click: add / remove  \xc2\xb7  drag up/down: velocity  \xc2\xb7  Cmd + drag: roll")),
                146, getHeight() - 18, getWidth() - 164, 16, juce::Justification::centredRight, false);
}

} // namespace eclipse
