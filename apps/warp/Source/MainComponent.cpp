#include "MainComponent.h"

namespace warp
{

using Rect = juce::Rectangle<float>;
using juce::Colour;
using lt::ui::Fonts;

//============================================================================== skins
const Skin& Skin::get (int i)
{
    static const std::vector<Skin> skins = []
    {
        std::vector<Skin> s;
        // COLOR: sunny paper, pop accents, black ink
        s.push_back ({ "COLOR", Colour (0xffffd84a), Colour (0xffffb800), juce::Colours::white, Colour (0xff121212), Colour (0xff121212), Colour (0x99121212),
                       Colour (0xffff3d6e), Colour (0xff22b8ff), Colour (0xff7b2cff), juce::Colours::white });
        // INK: paper and black ink only
        s.push_back ({ "INK", Colour (0xfff3eee2), Colour (0xffd9d1bf), Colour (0xfffffcf4), Colour (0xff141414), Colour (0xff141414), Colour (0x99141414),
                       Colour (0xff141414), Colour (0xff141414), Colour (0xff141414), Colour (0xfffffcf4) });
        // NOIR: night, white ink, one red
        s.push_back ({ "NOIR", Colour (0xff0d0d0f), Colour (0xff1e1e22), Colour (0xff17171a), Colour (0xffefefef), Colour (0xffefefef), Colour (0x99efefef),
                       Colour (0xffe63946), Colour (0xffbdbdbd), Colour (0xffe63946), juce::Colours::white });
        return s;
    }();
    return skins[(size_t) juce::jlimit (0, 2, i)];
}

static juce::Font heavy (float h) { return Fonts::bold (h); }

//============================================================================== component
MainComponent::MainComponent()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName = "LT WARP";
    opts.folderName = "LT Audio";
    opts.filenameSuffix = "settings";
    opts.osxLibrarySubFolder = "Application Support";
    settings = std::make_unique<juce::PropertiesFile> (opts);

    queue.tools = Tools::locate();
    load();
    queue.onChange = [safe = juce::Component::SafePointer<MainComponent> (this)] { if (safe != nullptr) safe->repaint(); };

    for (auto* e : { &trimStart, &trimEnd })
    {
        e->setJustification (juce::Justification::centred);
        e->setInputRestrictions (10, "0123456789:.");
        e->setTextToShowWhenEmpty (e == &trimStart ? "START" : "END", Colour (0x66000000));
        e->onTextChange = [this] { applyTrim(); };
        e->setFont (Fonts::semiBold (13.0f));
        addAndMakeVisible (e);
    }

    setSize (baseWidth, baseHeight);
    startTimerHz (30);

    license.refresh();
    if (! license.isLicensed()) showActivation (true);
}

MainComponent::~MainComponent()
{
    save();
}

void MainComponent::save()
{
    if (! persist) return;
    settings->setValue ("skin", skin);
    settings->setValue ("audio", (int) audioTarget);
    settings->setValue ("image", (int) imageTarget);
    settings->setValue ("mp3", queue.options.mp3Kbps);
    settings->setValue ("aac", queue.options.aacKbps);
    settings->setValue ("opus", queue.options.opusKbps);
    settings->setValue ("rate", queue.options.sampleRate);
    settings->setValue ("quality", queue.options.imageQuality);
    settings->setValue ("out", queue.outputDir.getFullPathName());
    settings->saveIfNeeded();
}

void MainComponent::load()
{
    skin = settings->getIntValue ("skin", 0);
    audioTarget = (Target) juce::jlimit (0, (int) Target::opus, settings->getIntValue ("audio", (int) Target::wav24));
    imageTarget = (Target) juce::jlimit ((int) Target::jpg, (int) Target::heic, settings->getIntValue ("image", (int) Target::png));
    queue.options.mp3Kbps = settings->getIntValue ("mp3", 320);
    queue.options.aacKbps = settings->getIntValue ("aac", 256);
    queue.options.opusKbps = settings->getIntValue ("opus", 160);
    queue.options.sampleRate = settings->getIntValue ("rate", 0);
    queue.options.imageQuality = settings->getIntValue ("quality", 90);
    auto out = settings->getValue ("out", {});
    queue.outputDir = out.isNotEmpty() && juce::File (out).isDirectory() ? juce::File (out) : juce::File();
}

static double parseTime (juce::String s)
{
    s = s.trim();
    if (s.isEmpty()) return 0;
    double total = 0;
    for (auto& part : juce::StringArray::fromTokens (s, ":", "")) total = total * 60.0 + part.getDoubleValue();
    return total;
}

void MainComponent::applyTrim()
{
    queue.options.trimStart = parseTime (trimStart.getText());
    queue.options.trimEnd = parseTime (trimEnd.getText());
    repaint();
}

void MainComponent::resized()
{
    auto scale = (float) getWidth() / baseWidth;
    for (auto* e : { &trimStart, &trimEnd })
        e->setTransform (juce::AffineTransform::scale (scale));
    trimStart.setBounds (606, 334, 74, 28);
    trimEnd.setBounds (712, 334, 74, 28);
    if (activation) activation->setBounds (getLocalBounds());
}

juce::Point<float> MainComponent::toBase (juce::Point<float> p) const
{
    return p / ((float) getWidth() / baseWidth);
}

void MainComponent::timerCallback()
{
    auto items = queue.snapshot();
    int done = 0;
    for (auto& i : items) done += i.state == Queue::State::done;
    if (done > lastDoneCount && ! queue.isBusy()) doneFlash = 1.0f;
    lastDoneCount = done;
    if (doneFlash > 0) { doneFlash = juce::jmax (0.0f, doneFlash - 0.012f); repaint(); }
    else if (queue.isBusy()) repaint();

    auto& s = Skin::get (skin);
    for (auto* e : { &trimStart, &trimEnd })
    {
        e->setColour (juce::TextEditor::backgroundColourId, s.panel);
        e->setColour (juce::TextEditor::textColourId, s.text);
        e->setColour (juce::TextEditor::outlineColourId, s.ink);
        e->setColour (juce::TextEditor::focusedOutlineColourId, s.a1);
        e->setColour (juce::CaretComponent::caretColourId, s.text);
        e->applyColourToAllText (s.text);
        e->setTextToShowWhenEmpty (e == &trimStart ? "START" : "END", s.dim.withMultipliedAlpha (0.7f));
    }
}

//============================================================================== input
void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    demo.clear();
    queue.add (files, audioTarget, imageTarget);
    repaint();
}

void MainComponent::browse()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose files to convert", juce::File(), "*", true);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this] (const juce::FileChooser& fc)
                          {
                              juce::StringArray paths;
                              for (auto& f : fc.getResults()) paths.add (f.getFullPathName());
                              if (! paths.isEmpty()) { demo.clear(); queue.add (paths, audioTarget, imageTarget); }
                          });
}

void MainComponent::chooseFolder()
{
    juce::PopupMenu m;
    m.addItem (1, "Next to each file", true, ! queue.outputDir.isDirectory());
    m.addItem (2, "Choose a folder...");
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this] (int r)
    {
        if (r == 1) { queue.outputDir = juce::File(); save(); repaint(); }
        if (r == 2)
        {
            chooser = std::make_unique<juce::FileChooser> ("Save converted files to", queue.outputDir);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult().isDirectory()) { queue.outputDir = fc.getResult(); save(); repaint(); }
            });
        }
    });
}

void MainComponent::mouseMove (const juce::MouseEvent& e)
{
    mouse = toBase (e.position);
    repaint();
}

void MainComponent::mouseDown (const juce::MouseEvent& e)
{
    auto p = toBase (e.position);
    for (int i = (int) hits.size() - 1; i >= 0; --i)
        if (hits[(size_t) i].r.contains (p) && hits[(size_t) i].fn)
        {
            auto fn = hits[(size_t) i].fn;
            fn();
            save();
            repaint();
            return;
        }
}

void MainComponent::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    auto n = (int) queue.snapshot().size();
    scroll = juce::jlimit (0, juce::jmax (0, n - 4), scroll - (w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0));
    repaint();
}

//============================================================================== drawing helpers
void MainComponent::panel (juce::Graphics& g, Rect r, Colour fill, float shadow) const
{
    auto& s = Skin::get (skin);
    g.setColour (s.ink);
    g.fillRoundedRectangle (r.translated (shadow, shadow), 14);   // hard comic shadow
    g.setColour (fill);
    g.fillRoundedRectangle (r, 14);
    g.setColour (s.ink);
    g.drawRoundedRectangle (r, 14, 3.0f);
}

void MainComponent::chip (juce::Graphics& g, Rect r, const juce::String& text, bool on, Colour accent, std::function<void()> fn)
{
    auto& s = Skin::get (skin);
    auto hot = r.contains (mouse);
    auto lift = on ? 0.0f : hot ? 1.0f : 3.0f;
    g.setColour (s.ink);
    g.fillRoundedRectangle (r.translated (3, 3), 8);
    auto face = r.translated (on ? 2.0f : 0.0f, on ? 2.0f : 0.0f);
    juce::ignoreUnused (lift);
    g.setColour (on ? accent : s.panel);
    g.fillRoundedRectangle (face, 8);
    g.setColour (s.ink);
    g.drawRoundedRectangle (face, 8, 2.0f);
    g.setColour (on ? s.onAccent : s.text);
    g.setFont (heavy (12.5f));
    g.drawText (text, face, juce::Justification::centred);
    hits.push_back ({ r, std::move (fn) });
}

void MainComponent::halftone (juce::Graphics& g, Rect r, Colour c, float spacing, float maxR, bool fadeDown) const
{
    g.setColour (c);
    int row = 0;
    for (float y = r.getY(); y < r.getBottom(); y += spacing, ++row)
    {
        auto t = (y - r.getY()) / juce::jmax (1.0f, r.getHeight());
        auto rad = maxR * (fadeDown ? (1.0f - t) : t);
        if (rad < 0.3f) continue;
        for (float x = r.getX() + (row % 2 ? spacing * 0.5f : 0.0f); x < r.getRight(); x += spacing)
            g.fillEllipse (x - rad, y - rad, rad * 2, rad * 2);
    }
}

void MainComponent::burst (juce::Graphics& g, juce::Point<float> c, float r, Colour fill, const juce::String& text, float rotation) const
{
    auto& s = Skin::get (skin);
    juce::Path p;
    const int pts = 14;
    for (int i = 0; i < pts * 2; ++i)
    {
        auto a = rotation + juce::MathConstants<float>::pi * (float) i / pts;
        auto rr = i % 2 == 0 ? r : r * 0.72f;
        auto pt = c + juce::Point<float> (std::cos (a), std::sin (a)) * rr;
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    p.closeSubPath();
    g.setColour (s.ink);
    g.fillPath (p, juce::AffineTransform::translation (4, 4));
    g.setColour (fill);
    g.fillPath (p);
    g.setColour (s.ink);
    g.strokePath (p, juce::PathStrokeType (3.0f));
    g.setColour (s.onAccent);
    g.setFont (heavy (r * 0.42f));
    g.addTransform (juce::AffineTransform::rotation (-0.12f, c.x, c.y));
    g.drawText (text, Rect (r * 2, r).withCentre (c), juce::Justification::centred);
    g.addTransform (juce::AffineTransform::rotation (0.12f, c.x, c.y));
}

//============================================================================== paint
void MainComponent::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / baseWidth));
    paintContent (g);
}

void MainComponent::paintContent (juce::Graphics& g)
{
    auto& s = Skin::get (skin);
    hits.clear();
    g.fillAll (s.paper);
    halftone (g, Rect (0, 0, baseWidth, 260), s.dots.withAlpha (0.55f), 14, 3.2f, true);

    // ---------------------------------------------------------------- header
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::rotation (-0.035f, 40, 50));
        auto title = juce::String ("LT WARP");
        g.setFont (heavy (44.0f));
        g.setColour (s.ink);
        g.drawText (title, Rect (34, 18, 320, 56).translated (4, 4), juce::Justification::centredLeft);
        g.setColour (skin == 1 ? s.panel : s.a1);
        g.drawText (title, Rect (34, 18, 320, 56), juce::Justification::centredLeft);
    }
    g.setColour (s.text);
    g.setFont (heavy (11.5f));
    g.drawText (juce::CharPointer_UTF8 ("DROP \xc2\xb7 PICK \xc2\xb7 CONVERT  \xe2\x80\x94  100% ON THIS COMPUTER, NO CLOUD"), Rect (40, 70, 500, 18), juce::Justification::centredLeft);

    for (int i = 0; i < 3; ++i)
        chip (g, Rect (baseWidth - 290 + i * 86.0f, 30, 78, 32), Skin::get (i).name, skin == i, s.a3, [this, i] { skin = i; });

    if (! license.isLicensed())
        chip (g, Rect (baseWidth - 410, 30, 110, 32), license.isUsable() ? "TRIAL " + juce::String (license.getTrialDaysLeft()) + "D" : juce::String ("ACTIVATE"), false, s.a1,
              [this] { showActivation (true); });

    // ---------------------------------------------------------------- drop bubble
    {
        auto r = Rect (30, 104, 430, 270);
        // speech bubble: rounded box + a tail, drawn as one outline
        const float tx0 = r.getX() + 64, tx1 = r.getX() + 118, tipX = r.getX() + 46, tipY = r.getBottom() + 30;
        juce::Path bubble;
        bubble.addRoundedRectangle (r, 26);
        juce::Path tail;
        tail.startNewSubPath (tx0, r.getBottom() - 6);
        tail.lineTo (tipX, tipY);
        tail.lineTo (tx1, r.getBottom() - 6);
        tail.closeSubPath();
        auto fill = dragOver ? s.a2.withAlpha (skin == 1 ? 0.25f : 1.0f) : s.panel;
        g.setColour (s.ink);
        g.fillPath (bubble, juce::AffineTransform::translation (6, 6));
        g.fillPath (tail, juce::AffineTransform::translation (6, 6));
        g.setColour (fill);
        g.fillPath (bubble);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (bubble);
            halftone (g, r.withTrimmedTop (r.getHeight() * 0.45f).expanded (0, 30), (dragOver ? s.ink : s.dots).withAlpha (0.35f), 11, 2.6f, false);
        }
        g.setColour (s.ink);
        g.strokePath (bubble, juce::PathStrokeType (3.5f));
        g.setColour (fill);
        g.fillPath (tail);                                         // covers the box outline where the tail joins
        juce::Path tailEdges;
        tailEdges.startNewSubPath (tx0, r.getBottom() - 1);
        tailEdges.lineTo (tipX, tipY);
        tailEdges.lineTo (tx1, r.getBottom() - 1);
        g.setColour (s.ink);
        g.strokePath (tailEdges, juce::PathStrokeType (3.5f, juce::PathStrokeType::mitered));

        g.setColour (s.text);
        g.setFont (heavy (dragOver ? 46.0f : 38.0f));
        auto headline = dragOver ? juce::String ("LET GO!") : juce::String ("DROP FILES HERE!");
        g.drawText (headline, r.withTrimmedBottom (90).withTrimmedTop (40), juce::Justification::centred);
        g.setFont (heavy (13.0f));
        g.setColour (s.dim);
        g.drawText (juce::CharPointer_UTF8 ("AUDIO \xc2\xb7 VIDEO \xc2\xb7 IMAGES \xc2\xb7 FOLDERS"), r.withTrimmedTop (150).withHeight (20), juce::Justification::centred);
        chip (g, Rect (r.getCentreX() - 80, r.getY() + 186, 160, 34), "OR BROWSE...", false, s.a2, [this] { browse(); });
        hits.push_back ({ r, [this] { browse(); } });
        std::swap (hits[hits.size() - 1], hits[hits.size() - 2]);   // the button wins over the bubble
    }

    // ---------------------------------------------------------------- options
    {
        auto r = Rect (490, 104, 480, 270);
        panel (g, r, s.panel);
        auto label = [&] (const juce::String& t, float x, float y, float w = 90)
        {
            g.setColour (s.dim);
            g.setFont (heavy (10.5f));
            g.drawText (t, Rect (x, y, w, 26), juce::Justification::centredLeft);
        };
        g.setColour (s.text);
        g.setFont (heavy (14.0f));
        g.drawText (juce::CharPointer_UTF8 ("AUDIO + VIDEO \xe2\x86\x92"), Rect (510, 114, 300, 20), juce::Justification::centredLeft);
        for (int i = 0; i <= (int) Target::opus; ++i)
            chip (g, Rect (510 + i * 64.0f, 138, 58, 28), targetLabel ((Target) i).replace ("WAV ", "WAV"), audioTarget == (Target) i, s.a1, [this, i] { audioTarget = (Target) i; });

        auto& o = queue.options;
        label (audioTarget == Target::mp3 || audioTarget == Target::m4a || audioTarget == Target::opus ? "KBPS" : "LOSSLESS", 510, 174);
        if (audioTarget == Target::mp3 || audioTarget == Target::m4a || audioTarget == Target::opus)
        {
            auto& v = audioTarget == Target::mp3 ? o.mp3Kbps : audioTarget == Target::m4a ? o.aacKbps : o.opusKbps;
            std::vector<int> rates = audioTarget == Target::opus ? std::vector<int> { 96, 128, 160, 256 } : std::vector<int> { 128, 192, 256, 320 };
            for (size_t i = 0; i < rates.size(); ++i)
                chip (g, Rect (600 + i * 64.0f, 174, 58, 24), juce::String (rates[i]), v == rates[i], s.a2, [&v, x = rates[i]] { v = x; });
        }
        label ("SAMPLE RATE", 510, 204);
        const int srs[] = { 0, 44100, 48000, 96000 };
        for (int i = 0; i < 4; ++i)
            chip (g, Rect (600 + i * 64.0f, 204, 58, 24), srs[i] == 0 ? juce::String ("KEEP") : juce::String (srs[i] / 1000.0, srs[i] == 44100 ? 1 : 0), o.sampleRate == srs[i], s.a2,
                  [&o, x = srs[i]] { o.sampleRate = x; });

        g.setColour (s.text);
        g.setFont (heavy (14.0f));
        g.drawText (juce::CharPointer_UTF8 ("IMAGES \xe2\x86\x92"), Rect (510, 238, 300, 20), juce::Justification::centredLeft);
        for (int i = (int) Target::jpg; i <= (int) Target::heic; ++i)
        {
            auto enabled = (Target) i != Target::heic || queue.tools.sips.existsAsFile();
            if (! enabled) continue;
            chip (g, Rect (510 + (i - (int) Target::jpg) * 64.0f, 262, 58, 28), targetLabel ((Target) i), imageTarget == (Target) i, s.a3, [this, i] { imageTarget = (Target) i; });
        }
        label ("QUALITY", 510, 298);
        for (int i = 0; i < 4; ++i)
        {
            const int qs[] = { 70, 80, 90, 100 };
            chip (g, Rect (600 + i * 64.0f, 298, 58, 24), juce::String (qs[i]), o.imageQuality == qs[i], s.a2, [&o, x = qs[i]] { o.imageQuality = x; });
        }
        label ("TRIM", 510, 335);
        g.setColour (s.dim);
        g.setFont (heavy (14.0f));
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x86\x92"), Rect (682, 335, 28, 26), juce::Justification::centred);
        auto outLabel = queue.outputDir.isDirectory() ? queue.outputDir.getFileName().toUpperCase() : juce::String ("NEXT TO FILE");
        label ("SAVE TO", 800, 335, 60);
        chip (g, Rect (858, 335, 98, 26), outLabel.substring (0, 12), false, s.a2, [this] { chooseFolder(); });
    }

    // ---------------------------------------------------------------- queue
    auto items = demo.empty() ? queue.snapshot() : demo;
    {
        auto r = Rect (30, 404, 940, 196);
        panel (g, r, s.panel);
        if (items.empty())
        {
            g.setColour (s.dim);
            g.setFont (heavy (16.0f));
            g.drawText ("NOTHING HERE YET. DROP SOMETHING!", r, juce::Justification::centred);
        }
        scroll = juce::jlimit (0, juce::jmax (0, (int) items.size() - 4), scroll);
        for (int row = 0; row < 4 && scroll + row < (int) items.size(); ++row)
        {
            auto& it = items[(size_t) (scroll + row)];
            auto y = r.getY() + 12 + row * 45.0f;
            auto kindText = it.kind == Kind::video ? "VIDEO" : it.kind == Kind::image ? "IMAGE" : "AUDIO";
            auto kindCol = it.kind == Kind::video ? s.a3 : it.kind == Kind::image ? s.a2 : s.a1;
            auto tag = Rect (46, y + 6, 64, 24);
            g.setColour (kindCol);
            g.fillRoundedRectangle (tag, 6);
            g.setColour (s.ink);
            g.drawRoundedRectangle (tag, 6, 2.0f);
            g.setColour (s.onAccent);
            g.setFont (heavy (11.0f));
            g.drawText (kindText, tag, juce::Justification::centred);

            g.setColour (s.text);
            g.setFont (Fonts::semiBold (14.0f));
            g.drawText (it.input.getFileName(), Rect (122, y + 4, 300, 28), juce::Justification::centredLeft, true);
            g.setFont (heavy (14.0f));
            g.drawText (juce::CharPointer_UTF8 ("\xe2\x86\x92"), Rect (424, y + 4, 24, 28), juce::Justification::centred);

            auto id = it.id;
            auto kind = it.kind;
            chip (g, Rect (452, y + 5, 76, 26), targetLabel (it.target), true, isAudioTarget (it.target) ? s.a1 : s.a3, [this, id, kind]
            {
                juce::PopupMenu m;
                for (int t = 0; t < (int) Target::count; ++t)
                    if (canConvert (kind, (Target) t, queue.tools)) m.addItem (t + 1, targetLabel ((Target) t));
                m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, id] (int res) { if (res > 0) queue.setTarget (id, (Target) (res - 1)); });
            });

            // progress
            auto bar = Rect (544, y + 9, 230, 18);
            g.setColour (s.paper.withAlpha (0.6f));
            g.fillRoundedRectangle (bar, 9);
            auto fillW = bar.getWidth() * juce::jlimit (0.0f, 1.0f, it.progress);
            if (fillW > 1)
            {
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (bar.withWidth (fillW).toNearestInt());
                g.setColour (it.state == Queue::State::failed ? s.dim : s.a2);
                g.fillRoundedRectangle (bar, 9);
                g.setColour (s.ink.withAlpha (0.25f));
                auto phase = (float) std::fmod (juce::Time::getMillisecondCounter() * 0.03, 12.0);
                for (float x = bar.getX() - 20 + phase; x < bar.getRight(); x += 12)
                    g.drawLine (x, bar.getBottom(), x + 10, bar.getY(), 3.0f);
            }
            g.setColour (s.ink);
            g.drawRoundedRectangle (bar, 9, 2.0f);

            juce::String status;
            Colour sc = s.text;
            switch (it.state)
            {
                case Queue::State::waiting:   status = "READY"; sc = s.dim; break;
                case Queue::State::running:   status = juce::String (juce::roundToInt (it.progress * 100)) + "%"; break;
                case Queue::State::done:      status = "DONE!"; sc = skin == 1 ? s.text : s.a1; break;
                case Queue::State::failed:    status = "OOPS"; sc = s.a1; break;
                case Queue::State::cancelled: status = "STOPPED"; sc = s.dim; break;
            }
            g.setColour (sc);
            g.setFont (heavy (13.0f));
            g.drawText (status, Rect (782, y + 4, 70, 28), juce::Justification::centredLeft);
            if (it.state == Queue::State::failed && Rect (782, y + 4, 70, 28).contains (mouse))
            {
                g.setColour (s.ink);
                g.fillRoundedRectangle (Rect (420, y - 26, 440, 24), 6);
                g.setColour (s.paper);
                g.setFont (Fonts::semiBold (11.0f));
                g.drawText (it.message, Rect (428, y - 26, 428, 24), juce::Justification::centredLeft, true);
            }
            if (it.state == Queue::State::done)
                chip (g, Rect (852, y + 5, 64, 26), "SHOW", false, s.a2, [f = it.output] { f.revealToUser(); });
            if (it.state != Queue::State::running)
                chip (g, Rect (922, y + 5, 30, 26), juce::CharPointer_UTF8 ("\xc3\x97"), false, s.a1, [this, id] { queue.remove (id); });
        }
        if ((int) items.size() > 4)
        {
            g.setColour (s.dim);
            g.setFont (heavy (10.0f));
            g.drawText (juce::String (scroll + 1) + "-" + juce::String (juce::jmin ((int) items.size(), scroll + 4)) + " OF " + juce::String ((int) items.size()) + "  (SCROLL)",
                        Rect (r.getRight() - 200, r.getBottom() - 18, 186, 14), juce::Justification::centredRight);
        }
    }

    // ---------------------------------------------------------------- footer
    {
        int waiting = 0, done = 0, failed = 0;
        for (auto& i : items) { waiting += i.state == Queue::State::waiting; done += i.state == Queue::State::done; failed += i.state == Queue::State::failed; }
        chip (g, Rect (30, 614, 120, 32), "CLEAR DONE", false, s.a2, [this] { queue.clearFinished(); });
        chip (g, Rect (160, 614, 80, 32), "STOP", false, s.a1, [this] { queue.cancelAll(); });
        g.setColour (s.text);
        g.setFont (heavy (12.0f));
        auto summary = juce::String (items.size()) + " FILES" + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) + juce::String (done) + " DONE";
        if (failed > 0) summary << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) << failed << " OOPS";
        if (! queue.tools.ready()) summary = "FFMPEG NOT FOUND: install it or place it next to the app";
        g.drawText (summary, Rect (256, 614, 420, 32), juce::Justification::centredLeft);

        // the big button: a squashed starburst behind a chunky label
        auto c = juce::Point<float> (868, 630);
        auto hot = Rect (200, 44).withCentre (c).contains (mouse);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.addTransform (juce::AffineTransform::scale (2.5f, 0.62f, c.x, c.y));
            burst (g, c, hot ? 42.0f : 39.0f, waiting > 0 ? s.a1 : s.dim, {}, -0.1f);
        }
        g.setColour (skin == 2 ? juce::Colours::black : s.ink);
        g.setFont (heavy (22.0f));
        g.drawText ("CONVERT!", Rect (200, 30).withCentre (c).translated (2, 2), juce::Justification::centred);
        g.setColour (s.onAccent);
        g.drawText ("CONVERT!", Rect (200, 30).withCentre (c), juce::Justification::centred);
        hits.push_back ({ Rect (200, 44).withCentre (c), [this] { queue.startAll(); } });
    }

    if (doneFlash > 0)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.setOpacity (juce::jmin (1.0f, doneFlash * 2.0f));
        burst (g, { 500, 300 }, 90 + (1 - doneFlash) * 30, s.a3, "DONE!", 0.2f);
    }
}

//============================================================================== misc
void MainComponent::showActivation (bool show)
{
    if (! show)
    {
        if (activation) { removeChildComponent (activation.get()); activation.reset(); }
        return;
    }
    activation = std::make_unique<lt::ui::ActivationComponent> (license, ltPalette);
    activation->onDismiss = [this]
    {
        license.refresh();
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)] { if (safe != nullptr) safe->showActivation (false); });
    };
    addAndMakeVisible (*activation);
    activation->setBounds (getLocalBounds());
}

void MainComponent::loadDemo()
{
    persist = false;
    showActivation (false);
    auto home = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    auto make = [&] (const char* name, Kind k, Target t, Queue::State st, float p, const char* msg = "")
    {
        Queue::Item it;
        it.id = (int) demo.size() + 1;
        it.input = home.getChildFile (name);
        it.kind = k; it.target = t; it.state = st; it.progress = p; it.message = msg;
        demo.push_back (it);
    };
    make ("beat_final_master.wav", Kind::audio, Target::mp3, Queue::State::done, 1.0f);
    make ("studio session take 3.mov", Kind::video, Target::wav24, Queue::State::running, 0.62f);
    make ("cover art.heic", Kind::image, Target::png, Queue::State::waiting, 0.0f);
    make ("vocal stems.aiff", Kind::audio, Target::flac, Queue::State::failed, 0.3f, "Invalid data found when processing input");
}

void MainComponent::saveScreenshot (const juce::File& f)
{
    auto img = createComponentSnapshot (getLocalBounds(), true, 2.0f);
    f.deleteFile();
    juce::FileOutputStream out (f);
    juce::PNGImageFormat().writeImageToStream (img, out);
}

} // namespace warp
