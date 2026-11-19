#include "Widgets.h"

namespace quasar::ui
{

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float startAngle = pi * 1.25f, endAngle = pi * 2.75f;
}

// ================================================================== drawing

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner, bool highlight)
{
    juce::ColourGradient body (juce::Colour (0xb30b0717), r.getX(), r.getY(), juce::Colour (0x9906040e), r.getX(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, corner);
    juce::ColourGradient sheen (juce::Colour (0x14ffffff), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getY() + juce::jmin (60.0f, r.getHeight() * 0.4f), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r, corner);
    if (highlight) Glass::drawRectGlow (g, r, corner, Colours::accent, 10.0f, 0.5f);
    juce::ColourGradient edge (juce::Colour (0x55d9ccff), r.getX(), r.getY(), juce::Colour (0x22a58cff), r.getX(), r.getBottom(), false);
    g.setGradientFill (edge);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

void drawDisplay (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour (Colours::display);
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colour (0x26c9b8ff));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

void drawTitle (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float size, juce::Justification j)
{
    g.setColour (Colours::text);
    g.setFont (Fonts::regular (size));
    g.drawFittedText (t, r.toNearestInt(), j, 1);
}

void drawKnob (juce::Graphics& g, juce::Rectangle<float> b, float pos, bool bipolar, float glow, bool enabled)
{
    auto size = juce::jmin (b.getWidth(), b.getHeight());
    auto r = b.withSizeKeepingCentre (size, size);
    auto c = r.getCentre();
    auto arcR = size * 0.5f - 2.0f;
    auto discR = arcR * 0.74f;
    auto angle = startAngle + pos * (endAngle - startAngle);

    Glass::drawGlow (g, c, arcR * 1.25f, Colours::arcGlow, (0.06f + 0.16f * pos) * glow);
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0, startAngle, endAngle, true);
    g.setColour (juce::Colour (0x2ee0d8ff));
    g.strokePath (track, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    auto from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f && enabled)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (Colours::arcGlow.withAlpha (0.35f * glow));
        g.strokePath (arc, juce::PathStrokeType (6.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Colours::arc);
        g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // dark glass disc
    auto disc = juce::Rectangle<float> (discR * 2, discR * 2).withCentre (c);
    juce::ColourGradient body (juce::Colour (0xff2a2338), disc.getX(), disc.getY(), juce::Colour (0xff0b0910), disc.getRight(), disc.getBottom(), false);
    g.setGradientFill (body);
    g.fillEllipse (disc);
    juce::ColourGradient rim (juce::Colour (0x88ffffff), disc.getX(), disc.getY(), juce::Colour (0x10ffffff), disc.getX(), disc.getBottom(), false);
    g.setGradientFill (rim);
    g.drawEllipse (disc.reduced (0.5f), 1.0f);
    auto hl = disc.reduced (discR * 0.22f).withTrimmedBottom (discR * 0.8f);
    juce::ColourGradient sheen (juce::Colour (0x22ffffff), hl.getCentreX(), hl.getY(), juce::Colours::transparentWhite, hl.getCentreX(), hl.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillEllipse (hl);
    // pointer line
    auto p1 = c.getPointOnCircumference (discR * 0.35f, angle), p2 = c.getPointOnCircumference (discR * 0.82f, angle);
    g.setColour (enabled ? juce::Colours::white : Colours::dim);
    g.drawLine ({ p1, p2 }, juce::jmax (1.5f, size * 0.045f));
}

// ================================================================== look and feel

LookAndFeel::LookAndFeel()
{
    auto p = Palette::dark();
    setPalette (p);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf20c0818));
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha (0.55f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xcc06030d));
    setColour (juce::TextEditor::textColourId, Colours::text);
    setColour (juce::TextEditor::outlineColourId, Colours::edge);
    setColour (juce::TextEditor::focusedOutlineColourId, Colours::arc);
    setColour (juce::ScrollBar::thumbColourId, Colours::arc.withAlpha (0.5f));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    if ((bool) b.getProperties().getWithDefault ("textOnly", false))
    {
        if (b.getToggleState())
        {
            g.setColour (Colours::arc);
            g.fillRoundedRectangle (r.removeFromBottom (2.0f).reduced (r.getWidth() * 0.15f, 0), 1.0f);
            Glass::drawGlow (g, { r.getCentreX(), r.getBottom() }, r.getWidth() * 0.4f, Colours::accent, 0.35f);
        }
        else if (hover) Glass::drawGlow (g, r.getCentre(), r.getWidth() * 0.4f, Colours::accent, 0.12f);
        return;
    }
    auto corner = (float) b.getProperties().getWithDefault ("corner", 6.0);
    auto sel = b.getToggleState() || (bool) b.getProperties().getWithDefault ("accent", false);
    if (sel)
    {
        Glass::drawRectGlow (g, r, corner, Colours::accent, 8.0f, 0.55f);
        juce::ColourGradient f (juce::Colour (0xff5a22b8), r.getX(), r.getY(), juce::Colour (0xff3a1680), r.getX(), r.getBottom(), false);
        g.setGradientFill (f);
        g.fillRoundedRectangle (r, corner);
        g.setColour (juce::Colour (0xccb78cff));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    }
    else
    {
        g.setColour (juce::Colour (hover ? 0x33ffffff : 0x1effffff).withMultipliedAlpha (0.6f));
        g.fillRoundedRectangle (r, corner);
        g.setColour (juce::Colour (hover ? 0x66d9ccff : 0x40d9ccff));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    }
    if (down) { g.setColour (juce::Colour (0x22000000)); g.fillRoundedRectangle (r, corner); }
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    auto size = (float) b.getProperties().getWithDefault ("fontSize", 13.0);
    g.setFont (Fonts::regular (size));
    auto textOnly = (bool) b.getProperties().getWithDefault ("textOnly", false);
    g.setColour (textOnly ? (b.getToggleState() ? juce::Colours::white : Colours::dim) : (b.getToggleState() ? juce::Colours::white : Colours::text));
    if (! b.isEnabled()) g.setColour (Colours::faint);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (juce::Colour (0xf20c0818));
    g.setColour (Colours::edge);
    g.drawRect (0, 0, w, h, 1);
}

void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                     bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator) { g.setColour (Colours::faint); g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY())); return; }
    auto r = area.reduced (4, 1).toFloat();
    if (isHighlighted && isActive) { g.setColour (Colours::accent.withAlpha (0.55f)); g.fillRoundedRectangle (r, 4.0f); }
    if (isTicked) { g.setColour (Colours::arc); g.fillEllipse (r.getX() + 6, r.getCentreY() - 3, 6, 6); }
    g.setColour (isActive ? (isHighlighted ? juce::Colours::white : Colours::text) : Colours::faint);
    g.setFont (Fonts::regular (14.0f));
    g.drawFittedText (text, r.withTrimmedLeft (18).toNearestInt(), juce::Justification::centredLeft, 1);
    if (hasSubMenu)
    {
        juce::Path arrow;
        auto x = r.getRight() - 10, y = r.getCentreY();
        arrow.addTriangle (x - 3, y - 4, x - 3, y + 4, x + 2, y);
        g.fillPath (arrow);
    }
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s); return; }
    auto r = juce::Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - 2.0f, (float) w, 4.0f);
    g.setColour (juce::Colour (0x30ffffff)); g.fillRoundedRectangle (r, 2.0f);
    g.setColour (Colours::arc); g.fillRoundedRectangle (r.withRight (pos), 2.0f);
    Glass::drawGlow (g, { pos, r.getCentreY() }, 9.0f, Colours::arc, 0.6f);
    g.setColour (juce::Colours::white); g.fillEllipse (pos - 5, r.getCentreY() - 5, 10, 10);
}

// ================================================================== knob

Knob::Knob (Processor& p, const juce::String& id, const juce::String& l, float d)
    : label (l), paramId (id), proc (p), diameter (d)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (startAngle, endAngle, true);
    setMouseDragSensitivity (200);
    setRepaintsOnMouseActivity (true);
    setVelocityBasedMode (false);
    param = p.param (id);
    dest = ModDefs::destIndex (id);
    if (param != nullptr)
    {
        auto lo = param->getNormalisableRange().start;
        bipolar = lo < 0.0f && param->getNormalisableRange().end > 0.0f && id.contains ("pan");
    }
}

void Knob::bind (Bindings& b)
{
    if (param == nullptr) return;
    b.sliders.push_back (std::make_unique<juce::SliderParameterAttachment> (*param, *this, &proc.undo));
    setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

juce::String Knob::getTextFromValue (double v)
{
    return param != nullptr ? param->getText (param->convertTo0to1 ((float) v), 12) : juce::Slider::getTextFromValue (v);
}

juce::Rectangle<float> Knob::knobArea() const
{
    auto b = getLocalBounds().toFloat();
    return juce::Rectangle<float> (diameter, diameter).withCentre ({ b.getCentreX(), b.getY() + diameter * 0.5f + 2.0f });
}

void Knob::paint (juce::Graphics& g)
{
    auto k = knobArea();
    auto pos = (float) valueToProportionOfLength (getValue());
    drawKnob (g, k, pos, bipolar, 1.0f, isEnabled());

    // modulation rings
    auto routes = proc.routesFor (dest);
    if (! routes.empty())
    {
        auto c = k.getCentre();
        auto radius = diameter * 0.5f + 2.5f;
        int ring = 0;
        for (auto& r : routes)
        {
            auto rr = radius + (float) ring++ * 3.0f;
            auto lo = r.bipolar ? pos - std::abs (r.amount) : juce::jmin (pos, pos + r.amount);
            auto hi = r.bipolar ? pos + std::abs (r.amount) : juce::jmax (pos, pos + r.amount);
            lo = juce::jlimit (0.0f, 1.0f, lo); hi = juce::jlimit (0.0f, 1.0f, hi);
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, rr, rr, 0, startAngle + lo * (endAngle - startAngle), startAngle + juce::jmax (lo + 0.005f, hi) * (endAngle - startAngle), true);
            g.setColour (ModDefs::sourceColour (r.source).withAlpha (editingMod && r.slot == editSlot ? 1.0f : 0.8f));
            g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        auto now = juce::jlimit (0.0f, 1.0f, pos + proc.engine.getModOffset (dest));
        auto pt = c.getPointOnCircumference (radius, startAngle + now * (endAngle - startAngle));
        Glass::drawGlow (g, pt, 6.0f, juce::Colours::white, 0.6f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (pt.x - 2.2f, pt.y - 2.2f, 4.4f, 4.4f);
    }
    if (dropHover) Glass::drawGlow (g, k.getCentre(), diameter * 0.75f, juce::Colours::white, 0.35f);

    if (valueInside)
    {
        g.setColour (juce::Colours::white);
        g.setFont (Fonts::regular (diameter * 0.2f));
        g.drawFittedText (getTextFromValue (getValue()), k.reduced (diameter * 0.2f).toNearestInt(), juce::Justification::centred, 1);
    }
    if (showLabel)
    {
        auto showValue = isMouseOverOrDragging() && ! valueInside;
        juce::String text = showValue ? getTextFromValue (getValue()) : label;
        if (editingMod && editSlot >= 0)
            text = juce::String (juce::roundToInt (proc.state.getRawParameterValue (IDs::mod (editSlot, "amt"))->load())) + "%";
        g.setColour (showValue || editingMod ? juce::Colours::white : labelColour);
        g.setFont (Fonts::regular (labelSize));
        g.drawFittedText (text, getLocalBounds().withTop ((int) k.getBottom() + 5), juce::Justification::centredTop, 1);
    }
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showModMenu(); return; }
    if (e.mods.isAltDown())
    {
        auto routes = proc.routesFor (dest);
        if (! routes.empty())
        {
            editingMod = true;
            editSlot = routes.back().slot;
            editStart = proc.state.getRawParameterValue (IDs::mod (editSlot, "amt"))->load();
            proc.undo.beginNewTransaction();
            if (auto* p = proc.param (IDs::mod (editSlot, "amt"))) p->beginChangeGesture();
            return;
        }
    }
    juce::Slider::mouseDown (e);
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (editingMod)
    {
        auto v = juce::jlimit (-100.0f, 100.0f, editStart - (float) e.getDistanceFromDragStartY() * 0.6f + (float) e.getDistanceFromDragStartX() * 0.3f);
        if (auto* p = proc.param (IDs::mod (editSlot, "amt"))) p->setValueNotifyingHost (p->convertTo0to1 (v));
        repaint();
        return;
    }
    juce::Slider::mouseDrag (e);
}

void Knob::mouseUp (const juce::MouseEvent& e)
{
    if (editingMod)
    {
        if (auto* p = proc.param (IDs::mod (editSlot, "amt"))) p->endChangeGesture();
        editingMod = false; repaint();
        return;
    }
    juce::Slider::mouseUp (e);
}

void Knob::showModMenu()
{
    juce::PopupMenu m;
    auto routes = proc.routesFor (dest);
    m.addSectionHeader (label);
    m.addItem (1, "Reset to default");
    if (dest > 0)
    {
        juce::PopupMenu add;
        for (int s = 1; s < numModSources; ++s) add.addItem (1000 + s, ModDefs::sourceNames()[s]);
        m.addSubMenu ("Modulate with", add);
    }
    for (auto& r : routes)
    {
        m.addSeparator();
        auto name = ModDefs::sourceNames()[r.source] + "  " + juce::String (juce::roundToInt (r.amount * 100)) + "%";
        juce::PopupMenu sub;
        sub.addItem (2000 + r.slot, "Invert");
        sub.addItem (3000 + r.slot, r.bipolar ? "Unipolar" : "Bipolar");
        sub.addItem (4000 + r.slot, "Remove");
        m.addSubMenu (name, sub);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int id)
    {
        if (id == 1 && param != nullptr) { param->beginChangeGesture(); param->setValueNotifyingHost (param->getDefaultValue()); param->endChangeGesture(); }
        else if (id >= 1000 && id < 2000) proc.addModulation (id - 1000, dest, 50.0f);
        else if (id >= 2000 && id < 3000) proc.setParam (IDs::mod (id - 2000, "amt"), -proc.state.getRawParameterValue (IDs::mod (id - 2000, "amt"))->load());
        else if (id >= 3000 && id < 4000) proc.setParam (IDs::mod (id - 3000, "bip"), proc.state.getRawParameterValue (IDs::mod (id - 3000, "bip"))->load() > 0.5f ? 0.0f : 1.0f);
        else if (id >= 4000) proc.removeModulation (id - 4000);
        repaint();
    });
}

bool Knob::isInterestedInDragSource (const SourceDetails& d)
{
    return dest > 0 && d.description.toString().startsWith ("mod:");
}

void Knob::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    auto src = d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue();
    if (src > 0) proc.addModulation (src, dest, 50.0f);
    repaint();
}

// ================================================================== mod source tab

ModSourceTab::ModSourceTab (const juce::String& t, int s) : text (t), source (s)
{
    setRepaintsOnMouseActivity (true);
    setTooltip ("Click to show, drag onto any knob to modulate it");
}

void ModSourceTab::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setFont (Fonts::regular (fontSize));
    g.setColour (selected ? juce::Colours::white : (isMouseOver() ? Colours::text : Colours::dim));
    g.drawFittedText (text, getLocalBounds(), juce::Justification::centred, 1);
    if (selected && underline)
    {
        auto line = r.removeFromBottom (2.0f).reduced (r.getWidth() * 0.12f, 0);
        Glass::drawGlow (g, line.getCentre(), r.getWidth() * 0.45f, Colours::accent, 0.4f);
        g.setColour (Colours::arc);
        g.fillRoundedRectangle (line, 1.0f);
    }
    if (isMouseOver() && source > 0)
    {
        // small crosshair: "drag me"
        auto c = juce::Point<float> (r.getRight() - 6, r.getY() + 7);
        g.setColour (ModDefs::sourceColour (source).withAlpha (0.8f));
        g.drawEllipse (c.x - 3, c.y - 3, 6, 6, 1.0f);
    }
}

void ModSourceTab::mouseDown (const juce::MouseEvent&) { dragging = false; }

void ModSourceTab::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && source > 0 && e.getDistanceFromDragStart() > 6)
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragging = true;
            juce::Image img (juce::Image::ARGB, 90, 26, true);
            {
                juce::Graphics gg (img);
                gg.setColour (ModDefs::sourceColour (source).withAlpha (0.85f));
                gg.fillRoundedRectangle (0, 0, 90, 26, 13);
                gg.setColour (juce::Colours::black);
                gg.setFont (Fonts::semiBold (12.0f));
                gg.drawFittedText (ModDefs::sourceNames()[source], 0, 0, 90, 26, juce::Justification::centred, 1);
            }
            c->startDragging ("mod:" + juce::String (source), this, juce::ScaledImage (img), true);
        }
}

void ModSourceTab::mouseUp (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() < 6 && onClick) onClick();
    dragging = false;
}

// ================================================================== value box

ValueBox::ValueBox (juce::RangedAudioParameter& p, const juce::String& l, float step) : param (p), label (l), stepValue (step)
{
    att = std::make_unique<juce::ParameterAttachment> (param, [this] (float v) { value = v; repaint(); });
    att->sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}
ValueBox::~ValueBox() { att.reset(); }

void ValueBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto box = labelBelow ? r.removeFromTop (r.getHeight() * 0.56f) : r;
    drawDisplay (g, box.reduced (1.0f), 4.0f);
    g.setColour (juce::Colours::white);
    g.setFont (Fonts::regular (13.0f));
    auto text = format ? format (value) : (param.getNumSteps() < 200 || std::abs (value - std::round (value)) < 0.001f ? juce::String (juce::roundToInt (value)) : juce::String (value, 1));
    g.drawFittedText (text, box.toNearestInt(), juce::Justification::centred, 1);
    if (labelBelow)
    {
        g.setColour (Colours::dim);
        g.setFont (Fonts::regular (11.0f));
        g.drawFittedText (label, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

void ValueBox::mouseDown (const juce::MouseEvent&) { startValue = value; att->beginGesture(); }
void ValueBox::mouseDrag (const juce::MouseEvent& e)
{
    auto range = param.getNormalisableRange();
    auto steps = std::round (-(float) e.getDistanceFromDragStartY() / (e.mods.isShiftDown() ? 20.0f : 5.0f));
    att->setValueAsPartOfGesture (juce::jlimit (range.start, range.end, startValue + steps * stepValue));
}
void ValueBox::mouseUp (const juce::MouseEvent&) { att->endGesture(); }
void ValueBox::mouseDoubleClick (const juce::MouseEvent&) { att->setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); }

// ================================================================== text tabs

TextTabs::TextTabs (juce::StringArray i, juce::RangedAudioParameter* p) : items (std::move (i)), param (p)
{
    if (param != nullptr)
    {
        att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { selected = juce::roundToInt (v); repaint(); });
        att->sendInitialUpdate();
    }
}
TextTabs::~TextTabs() { att.reset(); }

juce::Rectangle<float> TextTabs::itemArea (int i) const
{
    auto w = (float) getWidth() / (float) juce::jmax (1, items.size());
    return { w * (float) i, 0, w, (float) getHeight() };
}

void TextTabs::paint (juce::Graphics& g)
{
    if (boxed) drawPanel (g, getLocalBounds().toFloat(), 8.0f);
    g.setFont (Fonts::regular (fontSize));
    for (int i = 0; i < items.size(); ++i)
    {
        auto r = itemArea (i);
        auto sel = i == selected;
        if (sel)
        {
            auto line = r.withTop (r.getBottom() - 2.0f).reduced (r.getWidth() * 0.1f, 0);
            Glass::drawGlow (g, line.getCentre(), r.getWidth() * 0.4f, Colours::accent, 0.35f);
            g.setColour (Colours::arc);
            g.fillRect (line);
        }
        g.setColour (sel ? juce::Colour (0xffc9a8ff) : Colours::dim);
        g.drawFittedText (items[i], r.toNearestInt(), juce::Justification::centred, 1);
        if (separators && i > 0)
        {
            g.setColour (Colours::faint);
            g.drawText ("/", juce::Rectangle<float> (r.getX() - 4, r.getY(), 8, r.getHeight()).toNearestInt(), juce::Justification::centred);
        }
    }
}

void TextTabs::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < items.size(); ++i)
        if (itemArea (i).contains (e.position))
        {
            selected = i;
            if (att) att->setValueAsCompleteGesture ((float) i);
            if (onChange) onChange (i);
            repaint();
        }
}

// ================================================================== switch

Switch::Switch (juce::RangedAudioParameter& p) : param (p)
{
    att = std::make_unique<juce::ParameterAttachment> (param, [this] (float v) { on = v > 0.5f; repaint(); });
    att->sendInitialUpdate();
}
Switch::~Switch() { att.reset(); }

void Switch::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    auto corner = r.getHeight() * 0.5f;
    if (on)
    {
        Glass::drawRectGlow (g, r, corner, Colours::accent, 8.0f, 0.6f);
        juce::ColourGradient f (juce::Colour (0xff8a4dff), r.getX(), r.getY(), juce::Colour (0xff5a22b8), r.getRight(), r.getY(), false);
        g.setGradientFill (f);
    }
    else g.setColour (juce::Colour (0x30ffffff));
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colour (0x55ffffff));
    g.drawRoundedRectangle (r, corner, 1.0f);
    auto d = r.getHeight() - 4.0f;
    auto knob = juce::Rectangle<float> (d, d).withCentre ({ on ? r.getRight() - corner : r.getX() + corner, r.getCentreY() });
    g.setColour (juce::Colours::white);
    g.fillEllipse (knob);
}

void Switch::mouseDown (const juce::MouseEvent&) { att->setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

// ================================================================== dropdown

Dropdown::Dropdown (juce::RangedAudioParameter& p, juce::StringArray n) : names (std::move (n)), param (p)
{
    att = std::make_unique<juce::ParameterAttachment> (param, [this] (float v) { value = toIndex ? toIndex (v) : juce::roundToInt (v); repaint(); });
    att->sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}
Dropdown::~Dropdown() { att.reset(); }

void Dropdown::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour (isMouseOver() ? 0x26ffffff : 0x14ffffff));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colours::edge.withMultipliedAlpha (1.5f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setColour (juce::Colours::white);
    g.setFont (Fonts::regular (fontSize));
    auto text = juce::isPositiveAndBelow (value, names.size()) ? names[value] : juce::String ("-");
    g.drawFittedText (text, r.withTrimmedRight (16).withTrimmedLeft (centred ? 4 : 8).toNearestInt(), centred ? juce::Justification::centred : juce::Justification::centredLeft, 1);
    juce::Path chev;
    auto cx = r.getRight() - 10, cy = r.getCentreY();
    chev.startNewSubPath (cx - 3.5f, cy - 2); chev.lineTo (cx, cy + 2); chev.lineTo (cx + 3.5f, cy - 2);
    g.setColour (Colours::dim);
    g.strokePath (chev, juce::PathStrokeType (1.3f));
}

void Dropdown::mouseDown (const juce::MouseEvent&)
{
    juce::PopupMenu m;
    if (buildMenu) buildMenu (m);
    else for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == value);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()), [this] (int id)
    {
        if (id > 0) att->setValueAsCompleteGesture (fromIndex ? fromIndex (id - 1) : (float) (id - 1));
    });
}

// ================================================================== fader + meter

Fader::Fader()
{
    setSliderStyle (juce::Slider::LinearVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRepaintsOnMouseActivity (true);
}

void Fader::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto track = r.withSizeKeepingCentre (4.0f, r.getHeight() - 18.0f);
    g.setColour (juce::Colour (0x40000000)); g.fillRoundedRectangle (track.expanded (1.0f), 2.0f);
    g.setColour (juce::Colour (0x30ffffff)); g.fillRoundedRectangle (track, 2.0f);
    auto pos = (float) valueToProportionOfLength (getValue());
    auto y = track.getBottom() - pos * track.getHeight();
    auto cap = juce::Rectangle<float> (juce::jmin (r.getWidth() - 2.0f, 26.0f), 16.0f).withCentre ({ r.getCentreX(), y });
    Glass::drawRectGlow (g, cap, 4.0f, Colours::accent, 6.0f, isMouseOverOrDragging() ? 0.7f : 0.4f);
    juce::ColourGradient body (juce::Colour (0xff3a3150), cap.getX(), cap.getY(), juce::Colour (0xff15111f), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (body); g.fillRoundedRectangle (cap, 4.0f);
    g.setColour (juce::Colour (0x88d9ccff)); g.drawRoundedRectangle (cap, 4.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.8f)); g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 8, 1.5f));
}

Meter::Meter (std::function<float (int)> l, int ch, bool r) : level (std::move (l)), channels (ch), rainbow (r) { startTimerHz (30); }

void Meter::timerCallback()
{
    for (int c = 0; c < channels; ++c)
    {
        auto v = juce::jlimit (0.0f, 1.5f, level (c));
        shown[(size_t) c] = v > shown[(size_t) c] ? v : shown[(size_t) c] * 0.86f;
        peak[(size_t) c] = juce::jmax (v, peak[(size_t) c] * 0.995f);
    }
    repaint();
}

void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto w = (r.getWidth() - 3.0f * (float) (channels - 1)) / (float) channels;
    const int segs = juce::jmax (8, (int) (r.getHeight() / 6.0f));
    for (int c = 0; c < channels; ++c)
    {
        auto col = juce::Rectangle<float> (r.getX() + (float) c * (w + 3.0f), r.getY(), w, r.getHeight());
        auto db = 20.0f * std::log10 (juce::jmax (1.0e-5f, shown[(size_t) c]));
        auto norm = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 54.0f);
        auto sh = col.getHeight() / (float) segs;
        for (int s = 0; s < segs; ++s)
        {
            auto seg = juce::Rectangle<float> (col.getX(), col.getBottom() - (float) (s + 1) * sh + 1.0f, col.getWidth(), sh - 1.5f);
            auto t = (float) s / (float) segs;
            auto lit = t < norm;
            juce::Colour colr = rainbow ? (t > 0.88f ? juce::Colour (0xffff5a6a) : t > 0.72f ? juce::Colour (0xffffd35a) : t > 0.45f ? juce::Colour (0xff7dff9b) : juce::Colour (0xff4fd2ff))
                                        : juce::Colour (0xff6b3cff).interpolatedWith (juce::Colour (0xffd9b8ff), t);
            g.setColour (lit ? colr : juce::Colour (0x18ffffff));
            g.fillRoundedRectangle (seg, 1.0f);
        }
    }
}

// ================================================================== wave view

WaveView::WaveView (Processor& p, int o) : osc (o), proc (p) { startTimerHz (20); }

bool WaveView::isInterestedInFileDrag (const juce::StringArray& f)
{
    return osc < numOscs && f.size() > 0 && juce::File (f[0]).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
}

void WaveView::filesDropped (const juce::StringArray& f, int, int)
{
    juce::String err;
    auto type = (int) proc.state.getRawParameterValue (IDs::osc (osc, "type"))->load();
    juce::File file (f[0]);
    if (type == 2) proc.loadSample (osc, file, err);
    else proc.loadUserTable (osc, file, err);
    if (err.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "LT QUASAR", err);
    repaint();
}

void WaveView::mouseDown (const juce::MouseEvent&)
{
    if (osc >= numOscs)
    {
        juce::PopupMenu m;
        auto& names = osc == 3 ? Choices::subShapes() : Choices::noiseTypes();
        auto id = osc == 3 ? juce::String (IDs::subShape) : juce::String (IDs::noiseType);
        auto cur = (int) proc.state.getRawParameterValue (id)->load();
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == cur);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, id] (int r) { if (r > 0) proc.setParam (id, (float) (r - 1)); });
        return;
    }
    auto type = (int) proc.state.getRawParameterValue (IDs::osc (osc, "type"))->load();
    juce::PopupMenu m;
    if (type == 0)
    {
        auto cur = (int) proc.state.getRawParameterValue (IDs::osc (osc, "table"))->load();
        auto& names = WavetableLibrary::names();
        for (int i = 0; i < names.size() - 1; ++i) m.addItem (i + 1, names[i], true, i == cur);
        m.addSeparator();
        m.addItem (900, "Import wavetable (.wav)...");
    }
    else if (type == 2)
    {
        m.addItem (901, "Load sample...");
        m.addItem (902, "Clear sample", proc.engine.getSample (osc) != nullptr);
    }
    else return;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int id)
    {
        if (id > 0 && id < 900) proc.setParam (IDs::osc (osc, "table"), (float) (id - 1));
        else if (id == 900 || id == 901)
        {
            auto chooser = std::make_shared<juce::FileChooser> (id == 900 ? "Import wavetable" : "Load sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser, id] (const juce::FileChooser& c)
            {
                if (c.getResult() == juce::File()) return;
                juce::String err;
                if (id == 900) proc.loadUserTable (osc, c.getResult(), err); else proc.loadSample (osc, c.getResult(), err);
                if (err.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "LT QUASAR", err);
            });
        }
        else if (id == 902) proc.clearSample (osc);
    });
}

void WaveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplay (g, r, 6.0f);
    auto inner = r.reduced (8.0f, 10.0f);
    // grid
    g.setColour (juce::Colour (0x10ffffff));
    for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (inner.getX() + inner.getWidth() * (float) i / 8.0f), inner.getY(), inner.getBottom());
    g.drawHorizontalLine ((int) inner.getCentreY(), inner.getX(), inner.getRight());

    auto lineCol = juce::Colour (0xffb98cff);
    if (osc == 4)   // noise
    {
        juce::Path p; juce::Random rnd (7);
        for (int i = 0; i <= 80; ++i)
        {
            auto x = inner.getX() + inner.getWidth() * (float) i / 80.0f;
            auto env = std::sin (pi * (float) i / 80.0f);
            auto y = inner.getCentreY() + (rnd.nextFloat() * 2 - 1) * inner.getHeight() * 0.42f * env;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (lineCol); g.strokePath (p, juce::PathStrokeType (1.4f));
        return;
    }

    auto* engineTable = [&]() -> const Wavetable*
    {
        if (osc == 3) return proc.engine.analogTable;
        auto type = (int) proc.state.getRawParameterValue (IDs::osc (osc, "type"))->load();
        if (type == 1) return proc.engine.analogTable;
        auto t = (int) proc.state.getRawParameterValue (IDs::osc (osc, "table"))->load();
        if (t == WavetableLibrary::userIndex()) return proc.engine.getUserTable (osc);
        return proc.engine.tables != nullptr ? proc.engine.tables->tryGet (t) : nullptr;
    }();

    float pos;
    if (osc == 3) { static constexpr float sp[] = { 0, 0.25f, 0.5f, 0.75f, 1 }; pos = sp[juce::jlimit (0, 4, (int) proc.state.getRawParameterValue (IDs::subShape)->load())]; }
    else
    {
        auto d = ModDefs::destIndex (IDs::osc (osc, "wtpos"));
        pos = juce::jlimit (0.0f, 1.0f, proc.state.getRawParameterValue (IDs::osc (osc, "wtpos"))->load() * 0.01f + proc.engine.getModOffset (d));
    }

    if (osc < numOscs && (int) proc.state.getRawParameterValue (IDs::osc (osc, "type"))->load() == 2)
    {
        auto* s = proc.engine.getSample (osc);
        if (s == nullptr)
        {
            g.setColour (Colours::dim); g.setFont (Fonts::regular (13.0f));
            g.drawFittedText ("Drop a sample here\n(or click)", inner.toNearestInt(), juce::Justification::centred, 2);
            return;
        }
        auto ov = s->overview ((int) inner.getWidth());
        juce::Path p;
        for (int i = 0; i < (int) ov.size(); ++i)
        {
            auto x = inner.getX() + (float) i, h = ov[(size_t) i] * inner.getHeight() * 0.48f;
            p.addRectangle (x, inner.getCentreY() - h, 1.0f, juce::jmax (0.5f, 2 * h));
        }
        g.setColour (lineCol.withAlpha (0.8f)); g.fillPath (p);
        auto sx = inner.getX() + pos * inner.getWidth();
        g.setColour (juce::Colours::white); g.drawVerticalLine ((int) sx, inner.getY(), inner.getBottom());
        g.setFont (Fonts::regular (11.0f)); g.setColour (Colours::dim);
        g.drawText (s->getName(), inner.removeFromBottom (12).toNearestInt(), juce::Justification::centredLeft);
        return;
    }
    if (engineTable == nullptr) return;

    // 3D stack of frames behind (wavetable mode), the current frame in front
    auto frames = engineTable->getNumFrames();
    const int points = 96;
    if (osc < numOscs && (int) proc.state.getRawParameterValue (IDs::osc (osc, "type"))->load() == 0 && frames > 1)
    {
        for (int k = 11; k >= 0; --k)
        {
            auto fp = (float) k / 11.0f;
            engineTable->drawFrame (fp, scratch, points);
            juce::Path p;
            auto dx = (float) k * 4.5f, dy = -(float) k * 3.2f;
            for (int i = 0; i < points; ++i)
            {
                auto x = inner.getX() + inner.getWidth() * 0.72f * (float) i / (float) (points - 1) + dx + inner.getWidth() * 0.06f;
                auto y = inner.getCentreY() + 8.0f - scratch[(size_t) i] * inner.getHeight() * 0.32f + dy;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (juce::Colour (0x22d9ccff));
            g.strokePath (p, juce::PathStrokeType (0.8f));
        }
    }
    engineTable->drawFrame (pos, scratch, points);
    juce::Path wave, fill;
    for (int i = 0; i < points; ++i)
    {
        auto x = inner.getX() + inner.getWidth() * (float) i / (float) (points - 1);
        auto y = inner.getCentreY() - scratch[(size_t) i] * inner.getHeight() * 0.42f;
        if (i == 0) { wave.startNewSubPath (x, y); fill.startNewSubPath (x, inner.getCentreY()); }
        wave.lineTo (x, y); fill.lineTo (x, y);
    }
    fill.lineTo (inner.getRight(), inner.getCentreY()); fill.closeSubPath();
    juce::ColourGradient grad (juce::Colour (0x887b2cff), inner.getX(), inner.getY(), juce::Colour (0x107b2cff), inner.getX(), inner.getCentreY(), false);
    g.setGradientFill (grad); g.fillPath (fill);
    g.setColour (lineCol.withAlpha (0.35f)); g.strokePath (wave, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved));
    g.setColour (lineCol); g.strokePath (wave, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));
    if (osc < numOscs)
    {
        auto type = (int) proc.state.getRawParameterValue (IDs::osc (osc, "type"))->load();
        g.setFont (Fonts::regular (10.5f)); g.setColour (Colours::dim);
        g.drawText (type == 1 ? juce::String ("ANALOG") : engineTable->getName(), r.reduced (8, 4).removeFromTop (12).toNearestInt(), juce::Justification::centredLeft);
    }
}

// ================================================================== filter view

FilterView::FilterView (Processor& p, int f) : proc (p), filter (f) { startTimerHz (20); }

void FilterView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplay (g, r, 6.0f);
    auto in = r.reduced (6.0f, 8.0f);
    g.setColour (juce::Colour (0x10ffffff));
    for (float hz : { 100.0f, 1000.0f, 10000.0f })
        g.drawVerticalLine ((int) (in.getX() + in.getWidth() * std::log (hz / 20.0f) / std::log (1000.0f)), in.getY(), in.getBottom());
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (in.getY() + in.getHeight() * (float) i / 4.0f), in.getX(), in.getRight());

    auto on = proc.state.getRawParameterValue (IDs::flt (filter, "on"))->load() > 0.5f;
    auto type = (int) proc.state.getRawParameterValue (IDs::flt (filter, "type"))->load();
    auto d = ModDefs::destIndex (IDs::flt (filter, "cutoff"));
    auto* cp = proc.param (IDs::flt (filter, "cutoff"));
    auto fc = cp->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, cp->getValue() + proc.engine.getModOffset (d)));
    auto res = proc.state.getRawParameterValue (IDs::flt (filter, "reso"))->load() * 0.01f;
    auto q = 0.6f + res * 9.0f;

    juce::Path p, fill;
    for (int i = 0; i <= 120; ++i)
    {
        auto t = (float) i / 120.0f;
        auto f = 20.0f * std::pow (1000.0f, t);
        auto x = f / fc;
        // analytic magnitude of 2-pole sections (approximation for the display)
        auto lp2 = 1.0f / std::sqrt ((1 - x * x) * (1 - x * x) + x * x / (q * q));
        auto hp2 = x * x * lp2, bp2 = x / q * lp2;
        float m;
        switch (type)
        {
            case 0: case 2: m = lp2 * lp2 / q * 1.0f * (q > 1.0f ? 1.0f : 1.0f); break;
            case 1: case 3: m = lp2; break;
            case 4: m = hp2 * hp2 / q; break;
            case 5: m = hp2; break;
            case 6: m = bp2 * bp2 * q; break;
            case 7: m = bp2 * q * 0.7f; break;
            case 8: m = std::abs (1 - x * x) * lp2; break;
            case 9: m = 1 + (q - 0.6f) * 0.3f * bp2; break;
            case 10: case 11: m = 0.6f + 0.4f * std::abs (std::cos (pi * f / fc * (type == 10 ? 1.0f : 0.5f))) * (1 + res); break;
            case 12: m = 0.3f + bp2 * 0.8f; break;
            default: m = lp2 * lp2 / q; break;
        }
        auto db = juce::jlimit (-36.0f, 18.0f, 20.0f * std::log10 (juce::jmax (1.0e-4f, m)));
        auto y = in.getY() + in.getHeight() * (1.0f - (db + 36.0f) / 54.0f);
        auto px = in.getX() + in.getWidth() * t;
        if (i == 0) { p.startNewSubPath (px, y); fill.startNewSubPath (px, in.getBottom()); }
        p.lineTo (px, y); fill.lineTo (px, y);
    }
    fill.lineTo (in.getRight(), in.getBottom()); fill.closeSubPath();
    juce::ColourGradient grad (juce::Colour (on ? 0x997b2cff : 0x33ffffff), in.getX(), in.getY(), juce::Colour (0x087b2cff), in.getX(), in.getBottom(), false);
    g.setGradientFill (grad); g.fillPath (fill);
    g.setColour (on ? juce::Colour (0xffc9a8ff) : Colours::dim);
    g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));
}

// ================================================================== envelope view

EnvView::EnvView (Processor& p) : proc (p) { startTimerHz (30); }

float EnvView::get (const char* w) const { return proc.state.getRawParameterValue (IDs::env (env, w))->load(); }
void EnvView::set (const char* w, float v)
{
    if (auto* p = proc.param (IDs::env (env, w))) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

EnvView::Geo EnvView::geometry() const
{
    Geo g;
    g.area = getLocalBounds().toFloat().reduced (12.0f, 10.0f);
    // time axis: each stage gets a share that grows with the log of its time
    auto share = [] (float ms) { return 0.08f + std::log10 (1.0f + juce::jmax (0.0f, ms)) * 0.25f; };
    g.att = share (get ("att")); g.hold = get ("hold") > 0.5f ? share (get ("hold")) * 0.6f : 0.0f;
    g.dec = share (get ("dec")); g.rel = share (get ("rel"));
    g.total = g.att + g.hold + g.dec + 0.45f + g.rel;
    return g;
}

juce::Point<float> EnvView::pointFor (int h, const Geo& g) const
{
    auto a = g.area;
    auto x = [&] (float t) { return a.getX() + a.getWidth() * t / g.total; };
    auto sus = get ("sus") * 0.01f;
    switch (h)
    {
        case 0: return { x (g.att), a.getY() };                                                    // attack peak
        case 1: return { x (g.att + g.hold + g.dec), a.getBottom() - sus * a.getHeight() };         // decay / sustain
        default: return { x (g.total), a.getBottom() };                                             // release end
    }
}

void EnvView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplay (g, r, 6.0f);
    auto geo = geometry();
    auto a = geo.area;
    g.setColour (juce::Colour (0x10ffffff));
    for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (a.getX() + a.getWidth() * (float) i / 8.0f), a.getY(), a.getBottom());
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * (float) i / 4.0f), a.getX(), a.getRight());

    auto X = [&] (float t) { return a.getX() + a.getWidth() * t / geo.total; };
    auto Y = [&] (float lvl) { return a.getBottom() - lvl * a.getHeight(); };
    auto sus = get ("sus") * 0.01f;
    auto ac = get ("acurve") * 0.01f, dc = get ("dcurve") * 0.01f, rc = get ("rcurve") * 0.01f;
    juce::Path p;
    p.startNewSubPath (X (0), Y (0));
    const int N = 40;
    for (int i = 1; i <= N; ++i) { auto t = (float) i / N; p.lineTo (X (geo.att * t), Y (dsp::Envelope::shape (t, ac))); }
    if (geo.hold > 0) p.lineTo (X (geo.att + geo.hold), Y (1));
    for (int i = 1; i <= N; ++i) { auto t = (float) i / N; p.lineTo (X (geo.att + geo.hold + geo.dec * t), Y (sus + (1 - sus) * (1 - dsp::Envelope::shape (t, -dc)))); }
    p.lineTo (X (geo.att + geo.hold + geo.dec + 0.45f), Y (sus));
    for (int i = 1; i <= N; ++i) { auto t = (float) i / N; p.lineTo (X (geo.att + geo.hold + geo.dec + 0.45f + geo.rel * t), Y (sus * (1 - dsp::Envelope::shape (t, -rc)))); }
    auto fill = p; fill.lineTo (X (geo.total), Y (0)); fill.closeSubPath();
    juce::ColourGradient grad (juce::Colour (0x997b2cff), a.getX(), a.getY(), juce::Colour (0x0a7b2cff), a.getX(), a.getBottom(), false);
    g.setGradientFill (grad); g.fillPath (fill);
    g.setColour (juce::Colour (0x55b98cff)); g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
    g.setColour (juce::Colour (0xffd9c4ff)); g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));

    for (int h = 0; h < 3; ++h)
    {
        auto pt = pointFor (h, geo);
        auto big = h == hover || h == dragging;
        Glass::drawGlow (g, pt, big ? 10.0f : 7.0f, Colours::arc, 0.6f);
        g.setColour (juce::Colours::white);
        auto rr = big ? 4.5f : 3.5f;
        g.fillEllipse (pt.x - rr, pt.y - rr, rr * 2, rr * 2);
    }
    if (env == 3)
    {
        g.setColour (Colours::dim); g.setFont (Fonts::regular (11.0f));
        g.drawText ("AMOUNT " + juce::String (proc.state.getRawParameterValue (IDs::envPAmount)->load(), 1) + " st (drag up/down on the left)",
                    a.removeFromTop (14).toNearestInt(), juce::Justification::centredRight);
    }
}

void EnvView::mouseMove (const juce::MouseEvent& e)
{
    auto geo = geometry(); int h = -1;
    for (int i = 0; i < 3; ++i) if (pointFor (i, geo).getDistanceFrom (e.position) < 10.0f) h = i;
    if (h != hover) { hover = h; repaint(); }
}

void EnvView::mouseDown (const juce::MouseEvent& e)
{
    auto geo = geometry();
    dragging = -1;
    for (int i = 0; i < 3; ++i) if (pointFor (i, geo).getDistanceFrom (e.position) < 12.0f) dragging = i;
    if (dragging < 0 && env == 3 && e.position.x < geo.area.getX() + geo.area.getWidth() * 0.15f) dragging = 9;   // pitch amount
    proc.undo.beginNewTransaction();
    for (auto* w : { "att", "dec", "sus", "rel" }) if (auto* p = proc.param (IDs::env (env, w))) p->beginChangeGesture();
}

void EnvView::mouseDrag (const juce::MouseEvent& e)
{
    auto geo = geometry();
    auto a = geo.area;
    auto timeFromX = [&] (float startT, float px)
    {
        // inverse of the log share: px -> ms
        auto t = juce::jmax (0.0f, (px - a.getX()) / a.getWidth() * geo.total - startT);
        return juce::jlimit (0.0f, 20000.0f, std::pow (10.0f, juce::jmax (0.0f, (t - 0.08f) / 0.25f)) - 1.0f);
    };
    switch (dragging)
    {
        case 0: set ("att", timeFromX (0.0f, e.position.x)); break;
        case 1:
            set ("dec", juce::jmax (1.0f, timeFromX (geo.att + geo.hold, e.position.x)));
            set ("sus", juce::jlimit (0.0f, 100.0f, (a.getBottom() - e.position.y) / a.getHeight() * 100.0f));
            break;
        case 2: set ("rel", juce::jmax (1.0f, timeFromX (geo.att + geo.hold + geo.dec + 0.45f, e.position.x))); break;
        case 9:
            if (auto* p = proc.param (IDs::envPAmount))
                p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (-48.0f, 48.0f, std::round (-(float) e.getDistanceFromDragStartY() * 0.25f + 0.0f))));
            break;
        default: break;
    }
    repaint();
}

void EnvView::mouseUp (const juce::MouseEvent&)
{
    for (auto* w : { "att", "dec", "sus", "rel" }) if (auto* p = proc.param (IDs::env (env, w))) p->endChangeGesture();
    dragging = -1;
}

// ================================================================== LFO view

LfoView::LfoView (Processor& p) : proc (p) { startTimerHz (30); }

juce::Point<float> LfoView::toScreen (float x, float y) const
{
    auto a = area();
    return { a.getX() + x * a.getWidth(), a.getBottom() - y * a.getHeight() };
}

void LfoView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplay (g, r, 6.0f);
    auto a = area();
    g.setColour (juce::Colour (0x10ffffff));
    for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (a.getX() + a.getWidth() * (float) i / 8.0f), a.getY(), a.getBottom());
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * (float) i / 4.0f), a.getX(), a.getRight());

    auto& shape = dragging >= 0 || bending >= 0 ? editing : proc.lfoTables.shape (lfo);
    juce::Path p, fill;
    const int N = 160;
    for (int i = 0; i <= N; ++i)
    {
        auto x = (float) i / N;
        float y;
        if (shape.kind == LfoShape::sampleHold) y = juce::Random ((juce::int64) (x * 8)).nextFloat();
        else if (shape.kind == LfoShape::smoothRandom) y = 0.5f + 0.35f * std::sin (x * 11.0f) * std::cos (x * 5.0f);
        else y = shape.evaluate (juce::jmin (x, 0.9999f));
        auto pt = toScreen (x, y);
        if (i == 0) { p.startNewSubPath (pt); fill.startNewSubPath (toScreen (0, 0)); }
        p.lineTo (pt); fill.lineTo (pt);
    }
    fill.lineTo (toScreen (1, 0)); fill.closeSubPath();
    juce::ColourGradient grad (juce::Colour (0x777b2cff), a.getX(), a.getY(), juce::Colour (0x087b2cff), a.getX(), a.getBottom(), false);
    g.setGradientFill (grad); g.fillPath (fill);
    g.setColour (juce::Colour (0x55b98cff)); g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
    g.setColour (juce::Colour (0xffd9c4ff)); g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));
    if (shape.kind == LfoShape::points)
        for (auto& pt : shape.pts)
        {
            auto s = toScreen (pt.x, pt.y);
            Glass::drawGlow (g, s, 8.0f, Colours::arc, 0.6f);
            g.setColour (juce::Colours::white); g.fillEllipse (s.x - 4, s.y - 4, 8, 8);
        }
    // playhead
    auto ph = proc.engine.getLfoPhase (lfo);
    auto x = a.getX() + ph * a.getWidth();
    g.setColour (juce::Colour (0x55ffffff));
    g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
}

void LfoView::mouseDown (const juce::MouseEvent& e)
{
    editing = proc.lfoTables.shape (lfo);
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        auto& names = LfoShape::presetNames();
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i]);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int id)
        { if (id > 0) { proc.setLfoShape (lfo, LfoShape::preset (id - 1)); repaint(); } });
        return;
    }
    if (editing.kind != LfoShape::points) { editing = LfoShape::preset (0); }
    dragging = -1; bending = -1;
    for (int i = 0; i < (int) editing.pts.size(); ++i)
        if (toScreen (editing.pts[(size_t) i].x, editing.pts[(size_t) i].y).getDistanceFrom (e.position) < 9.0f) dragging = i;
    if (dragging < 0 && e.mods.isAltDown())
    {
        auto a = area(); auto x = (e.position.x - a.getX()) / a.getWidth();
        for (int i = (int) editing.pts.size(); --i >= 0;) if (editing.pts[(size_t) i].x <= x) { bending = i; break; }
        if (bending < 0) bending = (int) editing.pts.size() - 1;
        bendStart = editing.pts[(size_t) bending].curve;
    }
}

void LfoView::mouseDrag (const juce::MouseEvent& e)
{
    auto a = area();
    if (dragging >= 0)
    {
        auto& pt = editing.pts[(size_t) dragging];
        auto lo = dragging > 0 ? editing.pts[(size_t) dragging - 1].x + 0.001f : 0.0f;
        auto hi = dragging + 1 < (int) editing.pts.size() ? editing.pts[(size_t) dragging + 1].x - 0.001f : 0.999f;
        if (dragging == 0) hi = juce::jmin (hi, 0.0f);   // the first point stays at x = 0
        pt.x = juce::jlimit (lo, juce::jmax (lo, hi), (e.position.x - a.getX()) / a.getWidth());
        pt.y = juce::jlimit (0.0f, 1.0f, (a.getBottom() - e.position.y) / a.getHeight());
        if (e.mods.isShiftDown()) { pt.y = std::round (pt.y * 8) / 8; pt.x = std::round (pt.x * 16) / 16; }
    }
    else if (bending >= 0)
        editing.pts[(size_t) bending].curve = juce::jlimit (-1.0f, 1.0f, bendStart - (float) e.getDistanceFromDragStartY() / 80.0f);
    else return;
    proc.setLfoShape (lfo, editing);
    repaint();
}

void LfoView::mouseUp (const juce::MouseEvent&) { dragging = bending = -1; }

void LfoView::mouseDoubleClick (const juce::MouseEvent& e)
{
    editing = proc.lfoTables.shape (lfo);
    if (editing.kind != LfoShape::points) editing = LfoShape::preset (0);
    for (int i = 1; i < (int) editing.pts.size(); ++i)
        if (toScreen (editing.pts[(size_t) i].x, editing.pts[(size_t) i].y).getDistanceFrom (e.position) < 9.0f)
        {
            editing.pts.erase (editing.pts.begin() + i);
            proc.setLfoShape (lfo, editing);
            return;
        }
    auto a = area();
    LfoShape::Point np { juce::jlimit (0.001f, 0.999f, (e.position.x - a.getX()) / a.getWidth()), juce::jlimit (0.0f, 1.0f, (a.getBottom() - e.position.y) / a.getHeight()), 0.0f };
    editing.pts.push_back (np);
    std::sort (editing.pts.begin(), editing.pts.end(), [] (auto& x, auto& y) { return x.x < y.x; });
    proc.setLfoShape (lfo, editing);
    repaint();
}

// ================================================================== keyboard

Keyboard::Keyboard (Processor& p) : proc (p) { startTimerHz (30); }

juce::Rectangle<float> Keyboard::keyRect (int note) const
{
    auto r = getLocalBounds().toFloat().reduced (4.0f, 4.0f);
    auto whites = octaves * 7 + 1;
    auto ww = r.getWidth() / (float) whites;
    static constexpr int whiteIndex[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
    auto rel = note - lowest;
    auto oct = rel / 12, k = rel % 12;
    auto wi = (float) (oct * 7 + whiteIndex[k]);
    if (! isBlack (note)) return { r.getX() + wi * ww, r.getY(), ww, r.getHeight() };
    // black keys sit between whites (offsets like a real piano)
    static constexpr float offs[12] = { 0, 0.62f, 0, 0.78f, 0, 0, 0.58f, 0, 0.7f, 0, 0.82f, 0 };
    auto bw = ww * 0.6f;
    return { r.getX() + (wi + offs[k]) * ww - bw * 0.5f + ww * 0.5f - ww * 0.12f, r.getY(), bw, r.getHeight() * 0.62f };
}

int Keyboard::noteAt (juce::Point<float> p, float& vel) const
{
    auto top = lowest + octaves * 12;
    for (int n = lowest; n <= top; ++n) if (isBlack (n) && keyRect (n).contains (p)) { vel = juce::jlimit (0.2f, 1.0f, (p.y - keyRect (n).getY()) / keyRect (n).getHeight()); return n; }
    for (int n = lowest; n <= top; ++n) if (! isBlack (n) && keyRect (n).contains (p)) { vel = juce::jlimit (0.2f, 1.0f, (p.y - keyRect (n).getY()) / keyRect (n).getHeight()); return n; }
    return -1;
}

void Keyboard::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), 10.0f);
    auto top = lowest + octaves * 12;
    for (int pass = 0; pass < 2; ++pass)
        for (int n = lowest; n <= top; ++n)
        {
            if (isBlack (n) != (pass == 1)) continue;
            auto k = keyRect (n).reduced (pass == 0 ? 1.0f : 0.0f, 0);
            auto lit = proc.engine.isNoteOn (n) || n == down;
            if (pass == 0)
            {
                juce::ColourGradient body (juce::Colour (lit ? 0xffa56dff : 0x5cffffff), k.getX(), k.getY(), juce::Colour (lit ? 0xff6b2cdc : 0x26ffffff), k.getX(), k.getBottom(), false);
                g.setGradientFill (body); g.fillRoundedRectangle (k, 4.0f);
                if (lit) Glass::drawRectGlow (g, k, 4.0f, Colours::accent, 10.0f, 0.8f);
                g.setColour (juce::Colour (0x44ffffff)); g.drawRoundedRectangle (k, 4.0f, 1.0f);
                auto sheen = k.reduced (3).withHeight (k.getHeight() * 0.25f).withBottomY (k.getBottom() - 6);
                g.setColour (juce::Colour (0x10ffffff)); g.fillRoundedRectangle (sheen, 3.0f);
            }
            else
            {
                juce::ColourGradient body (juce::Colour (lit ? 0xffb07dff : 0xff1c1824), k.getX(), k.getY(), juce::Colour (lit ? 0xff7b2cff : 0xff07060a), k.getX(), k.getBottom(), false);
                g.setGradientFill (body); g.fillRoundedRectangle (k, 3.0f);
                if (lit) Glass::drawRectGlow (g, k, 3.0f, Colours::accent, 8.0f, 0.8f);
                g.setColour (juce::Colour (0x66ffffff)); g.drawRoundedRectangle (k, 3.0f, 0.8f);
                g.setColour (juce::Colour (0x22ffffff)); g.fillRoundedRectangle (k.reduced (2).withHeight (6), 2.0f);
            }
        }
    g.setColour (Colours::dim); g.setFont (Fonts::regular (10.0f));
    for (int o = 0; o <= octaves; ++o)
    {
        auto n = lowest + o * 12;
        auto k = keyRect (n);
        g.drawText ("C" + juce::String (n / 12 - 1), k.withTrimmedTop (k.getHeight() - 16).toNearestInt(), juce::Justification::centred);
    }
}

void Keyboard::mouseDown (const juce::MouseEvent& e)
{
    float v; auto n = noteAt (e.position, v);
    if (n >= 0) { down = n; proc.engine.addUiNote (n, true, v); }
}

void Keyboard::mouseDrag (const juce::MouseEvent& e)
{
    float v; auto n = noteAt (e.position, v);
    if (n != down)
    {
        if (down >= 0) proc.engine.addUiNote (down, false);
        down = n;
        if (n >= 0) proc.engine.addUiNote (n, true, v);
    }
}

void Keyboard::mouseUp (const juce::MouseEvent&)
{
    if (down >= 0) proc.engine.addUiNote (down, false);
    down = -1;
}

// ================================================================== wheels

Wheel::Wheel (Processor& p, bool isPitch) : proc (p), pitch (isPitch), value (isPitch ? 0.5f : 0.0f) {}

void Wheel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (juce::Colour (0x66000000)); g.fillRoundedRectangle (r, 8.0f);
    juce::ColourGradient glass (juce::Colour (0x22ffffff), r.getX(), r.getY(), juce::Colour (0x08ffffff), r.getRight(), r.getY(), false);
    g.setGradientFill (glass); g.fillRoundedRectangle (r, 8.0f);
    auto y = r.getBottom() - value * r.getHeight();
    auto lit = juce::Rectangle<float> (r.getX() + 2, juce::jmin (y, r.getCentreY()), r.getWidth() - 4, std::abs (y - (pitch ? r.getCentreY() : r.getBottom())));
    if (! pitch) lit = juce::Rectangle<float> (r.getX() + 2, y, r.getWidth() - 4, r.getBottom() - y);
    juce::ColourGradient vio (juce::Colour (0xcc9b5cff), lit.getX(), lit.getY(), juce::Colour (0x667b2cff), lit.getRight(), lit.getY(), false);
    g.setGradientFill (vio); g.fillRoundedRectangle (lit, 6.0f);
    for (int i = 0; i < 9; ++i)
    {
        auto ly = r.getY() + r.getHeight() * ((float) i + 0.5f) / 9.0f;
        g.setColour (juce::Colour (0x18ffffff)); g.drawHorizontalLine ((int) ly, r.getX() + 4, r.getRight() - 4);
    }
    Glass::drawRectGlow (g, { r.getX(), y - 3, r.getWidth(), 6 }, 3.0f, Colours::accent, 8.0f, 0.8f);
    g.setColour (juce::Colours::white); g.fillRoundedRectangle (r.getX() + 3, y - 2, r.getWidth() - 6, 4, 2);
    g.setColour (juce::Colour (0x66d9ccff)); g.drawRoundedRectangle (r, 8.0f, 1.0f);
}

void Wheel::mouseDown (const juce::MouseEvent&) { startValue = value; }
void Wheel::mouseDrag (const juce::MouseEvent& e)
{
    value = juce::jlimit (0.0f, 1.0f, startValue - (float) e.getDistanceFromDragStartY() / (float) getHeight());
    send(); repaint();
}
void Wheel::mouseUp (const juce::MouseEvent&)
{
    if (pitch) { value = 0.5f; send(); repaint(); }
}
void Wheel::send()
{
    if (pitch) proc.engine.addUiPitchBend (juce::jlimit (0, 16383, (int) (value * 16383.0f)));
    else proc.engine.addUiController (1, (int) (value * 127.0f));
}

} // namespace quasar::ui
