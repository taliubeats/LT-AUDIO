#include "SkinsPanel.h"
#include "EclipseData.h"

namespace eclipse
{

namespace Colours = juce::Colours;
using lt::ui::Fonts;

SkinsPanel::SkinsPanel (Processor& p) : proc (p), doneKey (p, "DONE")
{
    addAndMakeVisible (doneKey);
    doneKey.onClick = [this] { if (onClose) onClose(); };
    for (auto& s : allSkins())
    {
        int size = 0;
        const auto res = ("thumb" + s.id + "_jpg").toStdString();
        const char* data = EclipseData::getNamedResource (res.c_str(), size);
        thumbs.push_back (data != nullptr ? juce::ImageFileFormat::loadFrom (data, (size_t) size) : juce::Image());
    }
    setInterceptsMouseClicks (true, true);
}

void SkinsPanel::resized()
{
    doneKey.setBounds (getWidth() - 130, 34, 90, 36);
    cards.clear();
    const float x0 = 46, w = (getWidth() - 92 - 4 * 18.0f) / 5, h = w * 0.5f + 52;
    int neutral = 0, artist = 0;
    for (auto& s : allSkins())
    {
        if (s.neutral)
        {
            cards.push_back ({ x0 + (float) neutral * (w + 18), 128, w, h });
            ++neutral;
        }
        else
        {
            const int col = artist % 5, row = artist / 5;
            cards.push_back ({ x0 + (float) col * (w + 18), 128 + h + 62 + (float) row * (h + 18), w, h });
            ++artist;
        }
    }
}

void SkinsPanel::paint (juce::Graphics& g)
{
    const auto& s = proc.skin();
    g.setColour (s.room.withAlpha (0.9f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 14);
    g.setColour (s.tx);
    look::spaced (g, "SKINS", { 46, 30, 400, 44 }, Fonts::semiBold (32), 14.0f, juce::Justification::centredLeft);
    look::label (g, s, "Neutral", { 46, 96, 300, 22 }, juce::Justification::centredLeft, 11, 3.0f);
    if (cards.size() > 3) look::label (g, s, "Artists", { 46, cards[3].getY() - 34, 300, 22 }, juce::Justification::centredLeft, 11, 3.0f);
    g.setColour (s.dim); g.setFont (Fonts::regular (12));
    g.drawText ("Each skin changes the look and the character of the VIBE knob.", 46, 66, 600, 24, juce::Justification::centredLeft, false);

    const auto& all = allSkins();
    for (size_t i = 0; i < all.size() && i < cards.size(); ++i)
    {
        const auto& sk = all[i];
        auto c = cards[i].translated (0, hover == (int) i ? -2.0f : 0.0f);
        const bool on = sk.id == proc.skinId;
        look::key (g, s, c, {}, false, false, hover == (int) i);
        if (on)
        {
            g.setColour (sk.acc.withAlpha (0.25f)); g.drawRoundedRectangle (c.expanded (3), 8, 6);
            g.setColour (sk.acc); g.drawRoundedRectangle (c.withTrimmedBottom (3).expanded (1), 6, 2);
        }
        auto th = c.reduced (8).withHeight (c.getWidth() * 0.5f - 16);
        if (thumbs[i].isValid())
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip; clip.addRoundedRectangle (th, 5);
            g.reduceClipRegion (clip);
            g.drawImage (thumbs[i], th, juce::RectanglePlacement::fillDestination);
        }
        g.setColour (Colours::black.withAlpha (0.4f)); g.drawRoundedRectangle (th, 5, 1);
        look::dot (g, sk.acc, { c.getX() + 16, th.getBottom() + 17 }, 4.5f, true);
        g.setColour (s.keyTx);
        look::spaced (g, sk.name, { c.getX() + 28, th.getBottom() + 7, c.getWidth() - 36, 20 }, Fonts::bold (12.5f), 1.8f, juce::Justification::centredLeft);
        g.setColour (s.dim); g.setFont (Fonts::regular (11));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("VIBE \xc2\xb7 ")) + sk.caption, (int) c.getX() + 10, (int) th.getBottom() + 27, (int) c.getWidth() - 20, 18,
                    juce::Justification::centredLeft, true);
    }
}

void SkinsPanel::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (size_t i = 0; i < cards.size(); ++i) if (cards[i].contains (e.position)) h = (int) i;
    if (h != hover) { hover = h; repaint(); }
}

void SkinsPanel::mouseDown (const juce::MouseEvent& e)
{
    for (size_t i = 0; i < cards.size(); ++i)
        if (cards[i].contains (e.position) && onPick) { onPick (allSkins()[i].id); return; }
}

} // namespace eclipse
