#include "PanelStrip.h"

namespace cosmos
{

using Rect = juce::Rectangle<float>;
static constexpr float gap = 2.0f;

PanelStrip::PanelStrip()
{
    setLayout (defaultLayout());
}

juce::String PanelStrip::defaultLayout()
{
    return "0:1.0,1:0.7,3:1.3,6:0.6,7:1.1,9:0.7,4:0.5";   // spectrum, spectrogram, loudness, vector, scope, starmap, levels
}

void PanelStrip::setContext (const Analyzer* a, const Readings* r, const Skin* s, float t, double tm)
{
    analyzer = a; readings = r; skin = s; target = t; time = tm;
}

void PanelStrip::addPanel (VisualType t, int index)
{
    Panel p { t, Visual::preferredWeight (t), Visual::create (t) };
    p.visual->onResetLoudness = [this] { if (onResetLoudness) onResetLoudness(); };
    if (index < 0 || index > (int) panels.size()) panels.push_back (std::move (p));
    else panels.insert (panels.begin() + index, std::move (p));
    changed();
    repaint();
}

void PanelStrip::removePanel (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) panels.size()) || panels.size() <= 1) return;
    panels.erase (panels.begin() + index);
    hover = -1;
    changed();
    repaint();
}

void PanelStrip::setType (int index, VisualType t)
{
    if (! juce::isPositiveAndBelow (index, (int) panels.size())) return;
    panels[(size_t) index].type = t;
    panels[(size_t) index].visual = Visual::create (t);
    panels[(size_t) index].visual->onResetLoudness = [this] { if (onResetLoudness) onResetLoudness(); };
    changed();
    repaint();
}

juce::String PanelStrip::getLayout() const
{
    juce::StringArray parts;
    for (auto& p : panels) parts.add (juce::String ((int) p.type) + ":" + juce::String (p.weight, 3));
    return parts.joinIntoString (",");
}

void PanelStrip::setLayout (const juce::String& text)
{
    std::vector<Panel> fresh;
    for (auto& item : juce::StringArray::fromTokens (text, ",", ""))
    {
        auto t = item.upToFirstOccurrenceOf (":", false, false).getIntValue();
        auto w = item.fromFirstOccurrenceOf (":", false, false).getFloatValue();
        if (! juce::isPositiveAndBelow (t, (int) VisualType::count)) continue;
        Panel p { (VisualType) t, juce::jlimit (0.15f, 6.0f, w > 0 ? w : 1.0f), Visual::create ((VisualType) t) };
        p.visual->onResetLoudness = [this] { if (onResetLoudness) onResetLoudness(); };
        fresh.push_back (std::move (p));
        if (fresh.size() >= 24) break;
    }
    if (fresh.empty())
    {
        if (text != defaultLayout()) setLayout (defaultLayout());
        return;
    }
    panels = std::move (fresh);
    hover = -1;
    repaint();
}

std::vector<std::vector<int>> PanelStrip::rows() const
{
    std::vector<std::vector<int>> out;
    auto n = (int) panels.size();
    auto w = (float) juce::jmax (1, getWidth()), h = (float) juce::jmax (1, getHeight());
    int numRows = 1;
    if (forceGrid || h > 330.0f)
        numRows = juce::jlimit (1, n, (int) std::round (std::sqrt ((float) n * h / w * 2.2f)));
    // spread panels evenly over the rows, in order
    int idx = 0;
    for (int r = 0; r < numRows; ++r)
    {
        auto count = n / numRows + (r < n % numRows ? 1 : 0);
        std::vector<int> row;
        for (int k = 0; k < count; ++k) row.push_back (idx++);
        out.push_back (row);
    }
    return out;
}

std::vector<Rect> PanelStrip::computeRects() const
{
    std::vector<Rect> rects (panels.size());
    auto rs = rows();
    auto bounds = getLocalBounds().toFloat();
    auto rowH = (bounds.getHeight() - gap * (float) (rs.size() - 1)) / (float) juce::jmax ((size_t) 1, rs.size());
    for (size_t r = 0; r < rs.size(); ++r)
    {
        float total = 0;
        for (auto i : rs[r]) total += panels[(size_t) i].weight;
        auto avail = bounds.getWidth() - gap * (float) (rs[r].size() - 1);
        auto x = bounds.getX();
        auto y = bounds.getY() + (float) r * (rowH + gap);
        for (auto i : rs[r])
        {
            auto w = avail * panels[(size_t) i].weight / juce::jmax (0.001f, total);
            rects[(size_t) i] = Rect (x, y, w, rowH);
            x += w + gap;
        }
    }
    return rects;
}

PanelStrip::Header PanelStrip::headerFor (Rect p) const
{
    Header h;
    auto top = p.reduced (4).removeFromTop (16);
    h.close = top.removeFromRight (16);
    h.right = top.removeFromRight (16);
    h.left = top.removeFromRight (16);
    h.name = top.withWidth (juce::jmin (top.getWidth(), 120.0f));
    return h;
}

void PanelStrip::paint (juce::Graphics& g)
{
    if (skin == nullptr || analyzer == nullptr || readings == nullptr) return;
    auto& s = *skin;
    auto rects = computeRects();
    VisualContext ctx { *analyzer, *readings, s, target, time };
    for (size_t i = 0; i < panels.size(); ++i)
    {
        auto r = rects[i];
        g.setColour (s.panel);
        g.fillRect (r);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (r.toNearestInt());
            panels[i].visual->paint (g, r, ctx);
        }
        if ((int) i == hover)
        {
            auto h = headerFor (r);
            g.setColour (s.panel.withAlpha (0.86f));
            g.fillRect (h.name.getUnion (h.close).expanded (2));
            g.setColour (s.text);
            g.setFont (monoFont (9.0f, true));
            g.drawText (Visual::nameOf (panels[i].type) + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbe")), h.name, juce::Justification::centredLeft);
            g.setFont (monoFont (10.0f));
            g.drawText (juce::CharPointer_UTF8 ("\xe2\x97\x82"), h.left, juce::Justification::centred);
            g.drawText (juce::CharPointer_UTF8 ("\xe2\x96\xb8"), h.right, juce::Justification::centred);
            g.setColour (s.warn);
            g.drawText (juce::CharPointer_UTF8 ("\xc3\x97"), h.close, juce::Justification::centred);
            g.setColour (s.line2.withAlpha (0.35f));
            g.drawRect (r, 1.0f);
        }
    }
}

int PanelStrip::panelAt (juce::Point<float> pt) const
{
    auto rects = computeRects();
    for (size_t i = 0; i < rects.size(); ++i)
        if (rects[i].expanded (gap * 0.5f).contains (pt)) return (int) i;
    return -1;
}

int PanelStrip::dividerAt (juce::Point<float> pt) const
{
    auto rects = computeRects();
    for (auto& row : rows())
        for (size_t k = 0; k + 1 < row.size(); ++k)
        {
            auto& r = rects[(size_t) row[k]];
            if (std::abs (pt.x - (r.getRight() + gap * 0.5f)) < 5.0f && pt.y >= r.getY() && pt.y <= r.getBottom())
                return row[k];
        }
    return -1;
}

void PanelStrip::mouseMove (const juce::MouseEvent& e)
{
    auto pt = e.position;
    setMouseCursor (dividerAt (pt) >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    auto h = panelAt (pt);
    if (h != hover) { hover = h; repaint(); }
}

void PanelStrip::mouseExit (const juce::MouseEvent&)
{
    if (hover >= 0) { hover = -1; repaint(); }
}

void PanelStrip::mouseDown (const juce::MouseEvent& e)
{
    dragDivider = dividerAt (e.position);
    if (dragDivider >= 0)
    {
        dragStartX = e.position.x;
        dragStartLeft = panels[(size_t) dragDivider].weight;
        dragStartRight = panels[(size_t) dragDivider + 1].weight;
        return;
    }
    auto i = panelAt (e.position);
    if (i < 0) return;
    if (e.mods.isPopupMenu()) { showTypeMenu (i); return; }
    auto h = headerFor (computeRects()[(size_t) i]);
    if (h.name.contains (e.position)) showTypeMenu (i);
    else if (h.close.contains (e.position)) removePanel (i);
    else if (h.left.contains (e.position) && i > 0) { std::swap (panels[(size_t) i], panels[(size_t) i - 1]); hover = i - 1; changed(); repaint(); }
    else if (h.right.contains (e.position) && i + 1 < (int) panels.size()) { std::swap (panels[(size_t) i], panels[(size_t) i + 1]); hover = i + 1; changed(); repaint(); }
}

void PanelStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragDivider < 0) return;
    auto rects = computeRects();
    auto pairPx = rects[(size_t) dragDivider].getWidth() + rects[(size_t) dragDivider + 1].getWidth();
    auto pairW = dragStartLeft + dragStartRight;
    auto dx = e.position.x - dragStartX;
    auto newLeft = juce::jlimit (pairW * 40.0f / pairPx, pairW - pairW * 40.0f / pairPx, dragStartLeft + dx / pairPx * pairW);
    panels[(size_t) dragDivider].weight = newLeft;
    panels[(size_t) dragDivider + 1].weight = pairW - newLeft;
    repaint();
}

void PanelStrip::mouseUp (const juce::MouseEvent&)
{
    if (dragDivider >= 0) { dragDivider = -1; changed(); }
}

void PanelStrip::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto i = panelAt (e.position);
    if (i >= 0) panels[(size_t) i].visual->doubleClicked();
}

void PanelStrip::showTypeMenu (int index)
{
    juce::PopupMenu menu;
    juce::String group;
    juce::PopupMenu sub;
    for (int t = 0; t < (int) VisualType::count; ++t)
        menu.addItem (t + 1, Visual::groupOf ((VisualType) t).substring (0, 3) + "  " + Visual::nameOf ((VisualType) t), true,
                      panels[(size_t) index].type == (VisualType) t);
    menu.addSeparator();
    menu.addItem (1000, "REMOVE PANEL", panels.size() > 1);
    juce::Component::SafePointer<PanelStrip> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, index] (int result)
    {
        if (safe == nullptr || result == 0) return;
        if (result == 1000) safe->removePanel (index);
        else safe->setType (index, (VisualType) (result - 1));
    });
}

} // namespace cosmos
