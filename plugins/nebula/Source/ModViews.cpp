#include "ModViews.h"
#include "Params.h"

namespace nebula
{

using lt::ui::Fonts;
using lt::ui::Glass;

// ================================================================== FX chain

FxChainView::FxChainView (juce::AudioProcessorValueTreeState& s) : state (s) { startTimerHz (10); }

int FxChainView::typeOf (int slot) const   { return juce::roundToInt (state.getRawParameterValue (IDs::fxSlot (slot))->load()); }
float FxChainView::amountOf (int slot) const { return state.getRawParameterValue (IDs::fxAmt (slot))->load(); }

int FxChainView::count() const
{
    int n = 0;
    for (int i = 0; i < 8; ++i) if (typeOf (i) != 0) n = i + 1;
    return n;
}

juce::Rectangle<float> FxChainView::row (int i) const
{
    auto h = (float) getHeight() / 9.0f;
    return { 0.0f, (float) i * h, (float) getWidth(), h - 3.0f };
}

int FxChainView::rowAt (float y) const { return juce::jlimit (0, 8, (int) (y / ((float) getHeight() / 9.0f))); }

void FxChainView::setSlot (int slot, int type, float amount)
{
    for (auto [id, value] : { std::pair<juce::String, float> { IDs::fxSlot (slot), (float) type }, { IDs::fxAmt (slot), amount } })
        if (auto* p = state.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
}

void FxChainView::compact()
{
    std::vector<std::pair<int, float>> fx;
    for (int i = 0; i < 8; ++i) if (typeOf (i) != 0) fx.push_back ({ typeOf (i), amountOf (i) });
    for (int i = 0; i < 8; ++i)
        setSlot (i, i < (int) fx.size() ? fx[(size_t) i].first : 0, i < (int) fx.size() ? fx[(size_t) i].second : 50.0f);
}

void FxChainView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto n = count();
    for (int i = 0; i <= juce::jmin (n, 7); ++i)
    {
        auto r = row (i);
        if (i == n)
        {
            Glass::drawPanel (g, r, 5.0f, p, 0.9f);
            g.setColour (p.text);
            g.setFont (Fonts::regular (17.0f));
            g.drawText ("+", r, juce::Justification::centred);
            break;
        }
        auto type = typeOf (i);
        auto amt = amountOf (i) * 0.01f;
        Glass::drawPanel (g, r, 5.0f, p, 1.2f, reordering && i == dragRow);
        auto fill = r.reduced (1.5f).withWidth ((r.getWidth() - 3.0f) * amt);
        g.setColour ((p.isLight ? juce::Colours::white : p.accent).withAlpha (0.28f));
        g.fillRoundedRectangle (fill, 4.0f);
        g.setColour (p.text);
        g.setFont (Fonts::regular (13.5f));
        g.drawText (FxTypes::names()[type], r, juce::Justification::centred);
        g.setColour (p.textDim);
        g.setFont (Fonts::regular (11.0f));
        g.drawText (juce::String (juce::roundToInt (amt * 100.0f)) + "%", r.reduced (8.0f, 0), juce::Justification::centredRight);
        if (i < n - 1)
        {
            g.setColour (p.glassEdge);
            g.drawVerticalLine ((int) r.getCentreX(), r.getBottom(), r.getBottom() + 3.0f);
        }
    }
    if (reordering && dropRow >= 0)
    {
        g.setColour (p.accent);
        g.fillRect (row (dropRow).withHeight (2.0f).translated (0, -2.0f));
    }
}

void FxChainView::mouseDown (const juce::MouseEvent& e)
{
    dragRow = rowAt (e.position.y);
    draggingAmount = reordering = false;
    dropRow = -1;
    if (dragRow < count()) dragStartAmount = amountOf (dragRow);
}

void FxChainView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow < 0 || dragRow >= count()) return;
    auto dx = e.getDistanceFromDragStartX(), dy = e.getDistanceFromDragStartY();
    if (! draggingAmount && ! reordering)
    {
        if (std::abs (dx) > 4 && std::abs (dx) >= std::abs (dy)) draggingAmount = true;
        else if (std::abs (dy) > 8) reordering = true;
    }
    if (draggingAmount)
    {
        auto v = juce::jlimit (0.0f, 100.0f, dragStartAmount + (float) dx / (float) getWidth() * 100.0f);
        if (auto* p = state.getParameter (IDs::fxAmt (dragRow))) p->setValueNotifyingHost (p->convertTo0to1 (v));
    }
    else if (reordering)
        dropRow = juce::jlimit (0, count() - 1, rowAt (e.position.y));
    repaint();
}

void FxChainView::mouseUp (const juce::MouseEvent& e)
{
    if (reordering && dropRow >= 0 && dropRow != dragRow)
    {
        // move the effect to its new place, shifting the others
        std::vector<std::pair<int, float>> fx;
        for (int i = 0; i < count(); ++i) fx.push_back ({ typeOf (i), amountOf (i) });
        auto item = fx[(size_t) dragRow];
        fx.erase (fx.begin() + dragRow);
        fx.insert (fx.begin() + dropRow, item);
        for (int i = 0; i < (int) fx.size(); ++i) setSlot (i, fx[(size_t) i].first, fx[(size_t) i].second);
    }
    else if (! draggingAmount && ! reordering && e.mouseWasClicked())
        showMenu (rowAt (e.position.y));
    reordering = draggingAmount = false;
    dropRow = -1;
    repaint();
}

void FxChainView::showMenu (int slot)
{
    auto n = count();
    if (slot > n) return;
    auto adding = slot == n;
    if (adding && n >= 8) return;

    juce::PopupMenu m, engineFx, space, motion, colour, glitch;
    auto current = adding ? -1 : typeOf (slot);
    auto add = [&] (juce::PopupMenu& menu, int t) { menu.addItem (t, FxTypes::names()[t], true, t == current); };
    for (auto t : { FxTypes::particles, FxTypes::grainCloud, FxTypes::spectralFreeze }) add (engineFx, t);
    for (auto t : { FxTypes::reverb, FxTypes::shimmer, FxTypes::delay, FxTypes::pingPong, FxTypes::reverseDelay }) add (space, t);
    for (auto t : { FxTypes::chorus, FxTypes::flanger, FxTypes::phaser, FxTypes::tremolo, FxTypes::autoPan, FxTypes::widener,
                    FxTypes::tapeWobble }) add (motion, t);
    for (auto t : { FxTypes::saturation, FxTypes::distortion, FxTypes::tape, FxTypes::filterLP, FxTypes::filterHP, FxTypes::filterBP,
                    FxTypes::comb, FxTypes::eq, FxTypes::formant, FxTypes::pitchShifter, FxTypes::harmonizer, FxTypes::freqShifter })
        add (colour, t);
    for (auto t : { FxTypes::bitcrusher, FxTypes::stutter, FxTypes::ringMod, FxTypes::vinyl }) add (glitch, t);
    m.addSubMenu ("GRAINS", engineFx);
    m.addSubMenu ("SPACE", space);
    m.addSubMenu ("MOTION", motion);
    m.addSubMenu ("COLOUR & PITCH", colour);
    m.addSubMenu ("GLITCH & LO-FI", glitch);
    if (! adding)
    {
        m.addSeparator();
        m.addItem (1001, "Move up", slot > 0);
        m.addItem (1002, "Move down", slot < n - 1);
        m.addItem (1003, "Remove");
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, slot, n] (int r)
    {
        if (r <= 0) return;
        if (r < 1000) { setSlot (slot, r, slot < n ? amountOf (slot) : 50.0f); return; }
        auto swap = [this] (int a, int b)
        {
            auto ta = typeOf (a), tb = typeOf (b); auto aa = amountOf (a), ab = amountOf (b);
            setSlot (a, tb, ab); setSlot (b, ta, aa);
        };
        if (r == 1001) swap (slot, slot - 1);
        else if (r == 1002) swap (slot, slot + 1);
        else if (r == 1003) { setSlot (slot, 0, 50.0f); compact(); }
    });
}

// ================================================================== chips

void ModChip::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    auto col = Modulation::sourceColour (source);
    auto v = mod != nullptr ? mod->getSourceValue (source) : 0.0f;
    Glass::drawPanel (g, r, r.getHeight() * 0.5f, *palette, isMouseOver() ? 1.8f : 1.0f);
    auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 10.0f, r.getCentreY() });
    Glass::drawGlow (g, dot.getCentre(), 8.0f, col, 0.3f + 0.6f * v);
    g.setColour (col);
    g.fillEllipse (dot);
    g.setColour (palette->text);
    g.setFont (Fonts::semiBold (11.5f));
    g.drawText (Modulation::sourceNames()[source], r.withTrimmedLeft (16.0f), juce::Justification::centred);
}

void ModChip::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4) return;
    if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! dnd->isDragAndDropActive())
        {
            juce::Image img (juce::Image::ARGB, getWidth(), getHeight(), true);
            juce::Graphics g (img);
            paint (g);
            dnd->startDragging ("mod:" + juce::String (source), this, juce::ScaledImage (img));
        }
}

// ================================================================== MOD 1 steps

ModStepsView::ModStepsView (juce::AudioProcessorValueTreeState& s, const Modulation& m) : state (s), mod (m) { startTimerHz (20); }

void ModStepsView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto col = Modulation::sourceColour (Modulation::mod1);
    auto r = getLocalBounds().toFloat();
    auto w = r.getWidth() / 8.0f;
    juce::Path outline;
    for (int i = 0; i < 8; ++i)
    {
        auto v = state.getRawParameterValue (IDs::mod1Step (i))->load() * 0.01f;
        auto bar = juce::Rectangle<float> (r.getX() + (float) i * w, r.getBottom() - v * r.getHeight(), w, v * r.getHeight());
        auto on = mod.getMod1Step() == i;
        g.setColour (col.withAlpha (on ? 0.55f : 0.28f));
        g.fillRect (bar);
        if (i == 0) outline.startNewSubPath (bar.getX(), bar.getY()); else outline.lineTo (bar.getX(), bar.getY());
        outline.lineTo (bar.getRight(), bar.getY());
    }
    g.setColour (col);
    g.strokePath (outline, juce::PathStrokeType (1.8f));
    g.setColour (p.glassEdge);
    g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX(), r.getRight());
}

void ModStepsView::set (juce::Point<float> pos)
{
    auto i = juce::jlimit (0, 7, (int) (pos.x / ((float) getWidth() / 8.0f)));
    auto v = juce::jlimit (0.0f, 100.0f, (1.0f - pos.y / (float) getHeight()) * 100.0f);
    if (auto* p = state.getParameter (IDs::mod1Step (i)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
    repaint();
}

// ================================================================== MOD 2 curve

ModCurveView::ModCurveView (juce::AudioProcessorValueTreeState& s, const Modulation& m) : state (s), mod (m) { startTimerHz (20); }

void ModCurveView::paint (juce::Graphics& g)
{
    if (palette == nullptr) return;
    auto& p = *palette;
    auto col = Modulation::sourceColour (Modulation::mod2);
    auto r = getLocalBounds().toFloat().reduced (0, 3.0f);
    auto shape = juce::roundToInt (state.getRawParameterValue (IDs::mod2Shape)->load());
    auto swing = state.getRawParameterValue (IDs::mod2Swing)->load() * 0.01f;
    auto skew = 0.5f + 0.45f * swing;
    juce::Random fake (4);
    std::array<float, 9> rv {};
    for (auto& x : rv) x = fake.nextFloat();
    auto value = [&] (float ph)
    {
        auto sp = ph < skew ? ph / skew * 0.5f : 0.5f + (ph - skew) / (1.0f - skew) * 0.5f;
        switch (shape)
        {
            case 1: return sp < 0.5f ? sp * 2.0f : 2.0f - sp * 2.0f;
            case 2: return sp;
            case 3: return 1.0f - sp;
            case 4: return sp < 0.5f ? 1.0f : 0.0f;
            case 5: { auto k = ph * 8.0f; auto i = (int) k; auto f = k - (float) i; f = f * f * (3 - 2 * f); return rv[(size_t) i] + (rv[(size_t) i + 1] - rv[(size_t) i]) * f; }
            default: return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * sp);
        }
    };
    juce::Path path, fill;
    for (int i = 0; i <= 100; ++i)
    {
        auto ph = (float) i / 100.0f;
        auto pt = juce::Point<float> (r.getX() + ph * r.getWidth(), r.getBottom() - value (ph) * r.getHeight());
        if (i == 0) { path.startNewSubPath (pt); fill.startNewSubPath (r.getX(), r.getBottom()); }
        path.lineTo (pt); fill.lineTo (pt);
    }
    fill.lineTo (r.getRight(), r.getBottom()); fill.closeSubPath();
    juce::ColourGradient grad (col.withAlpha (0.35f), 0, r.getY(), col.withAlpha (0.02f), 0, r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (path, juce::PathStrokeType (2.0f));
    auto ph = mod.getMod2Phase();
    auto pos = juce::Point<float> (r.getX() + ph * r.getWidth(), r.getBottom() - mod.getSourceValue (Modulation::mod2) * r.getHeight());
    Glass::drawGlow (g, pos, 9.0f, col, 0.8f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (pos.x - 3, pos.y - 3, 6, 6);
    g.setColour (p.textDim);
    g.setFont (Fonts::regular (11.0f));
    g.drawText (Modulation::mod2Shapes()[shape] + "  (click)", r.reduced (4.0f, 0), juce::Justification::topRight);
}

void ModCurveView::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked()) return;
    if (auto* p = state.getParameter (IDs::mod2Shape))
    {
        auto cur = juce::roundToInt (p->convertFrom0to1 (p->getValue()));
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) ((cur + 1) % Modulation::mod2Shapes().size())));
        p->endChangeGesture();
    }
}

} // namespace nebula
