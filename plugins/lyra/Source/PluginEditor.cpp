#include "PluginEditor.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace lyra
{

using Rect = juce::Rectangle<float>;
using juce::Colour;
using lt::ui::Fonts;

bool Editor::activationShownThisSession = false;
static const juce::String dotSep = juce::CharPointer_UTF8 ("  \xc2\xb7  ");

//============================================================================== theme
const std::array<Colour, 8>& Theme::swatches()
{
    static const std::array<Colour, 8> s { Colour (0xff8b5cf6), Colour (0xfff59e0b), Colour (0xfff43f5e), Colour (0xff14b8a6),
                                           Colour (0xff3b82f6), Colour (0xff84cc16), Colour (0xfff97316), Colour (0xffec4899) };
    return s;
}

Theme Theme::make (bool light, Colour hi)
{
    Theme t;
    t.hi = hi;
    t.onHi = hi.getPerceivedBrightness() > 0.62f ? Colour (0xff111111) : juce::Colours::white;
    if (light)
    {
        t.bg = Colour (0xfff3f0e8); t.panel = Colour (0xfffffdf8); t.edge = Colour (0x1a000000);
        t.text = Colour (0xff1b1a17); t.dim = Colour (0x991b1a17); t.faint = Colour (0x2a1b1a17);
        t.white = Colour (0xfffbfaf6); t.black = Colour (0xff23211d);
    }
    else
    {
        t.bg = Colour (0xff0c0b11); t.panel = Colour (0xff15141c); t.edge = Colour (0x1fffffff);
        t.text = Colour (0xfff2efe9); t.dim = Colour (0x99f2efe9); t.faint = Colour (0x22f2efe9);
        t.white = Colour (0xffe9e6df); t.black = Colour (0xff1c1b22);
    }
    return t;
}

//============================================================================== editor
Editor::Editor (Processor& p) : AudioProcessorEditor (p), proc (p)
{
    addAndMakeVisible (content);
    content.setBounds (0, 0, baseWidth, baseHeight);
    theme = Theme::make (proc.theme == 1, Theme::swatches()[(size_t) proc.colour]);

    proc.onStateRestored = [safe = juce::Component::SafePointer<Editor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->content.repaint(); });
    };

    const auto startW = proc.width > 0 ? proc.width : baseWidth * 5 / 6;
    setResizable (true, true);
    setResizeLimits (baseWidth / 2, baseHeight / 2, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseWidth / baseHeight);
    setSize (startW, startW * baseHeight / baseWidth);

    if (! proc.license.isLicensed() && (! activationShownThisSession || ! proc.license.isUsable()))
    {
        activationShownThisSession = true;
        showActivation (true);
    }
    startTimerHz (30);

    // Developer preview: LT_EDITOR_SNAPSHOT=/path.png LT_NOTES="48,64,71,74" [LT_LIGHT=1] [LT_LANG=0..3] [LT_KEY=-1..23] [LT_COLOUR=0..7]
    auto snapshot = juce::SystemStats::getEnvironmentVariable ("LT_EDITOR_SNAPSHOT", {});
    if (snapshot.isNotEmpty())
    {
        showActivation (false);
        for (auto& n : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("LT_NOTES", "48,64,67,71,74"), ",", ""))
            mouseNotes[(size_t) juce::jlimit (0, 127, n.getIntValue())] = true;
        proc.theme = juce::SystemStats::getEnvironmentVariable ("LT_LIGHT", {}).isNotEmpty() ? 1 : 0;
        proc.language = juce::SystemStats::getEnvironmentVariable ("LT_LANG", "0").getIntValue();
        proc.key = juce::SystemStats::getEnvironmentVariable ("LT_KEY", "0").getIntValue();
        proc.colour = juce::SystemStats::getEnvironmentVariable ("LT_COLOUR", "0").getIntValue();
        juce::Timer::callAfterDelay (900, [this, snapshot]
        {
            auto img = content.createComponentSnapshot (content.getLocalBounds(), true, 2.0f);
            juce::File (snapshot).deleteFile();
            juce::FileOutputStream out { juce::File (snapshot) };
            juce::PNGImageFormat().writeImageToStream (img, out);
            out.flush();
            juce::JUCEApplicationBase::quit();
        });
    }
}

Editor::~Editor()
{
    proc.onStateRestored = nullptr;
}

void Editor::resized()
{
    auto scale = (float) getWidth() / (float) baseWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    if (activation) activation->setBounds (content.getLocalBounds());
    proc.width = getWidth();
}

juce::Font Editor::ui (float h, bool bold) const
{
    if (lang() == Language::japanese)
    {
       #if JUCE_MAC
        return juce::Font (juce::FontOptions ("Hiragino Sans", h * 0.95f, bold ? juce::Font::bold : juce::Font::plain));
       #else
        return juce::Font (juce::FontOptions ("Yu Gothic UI", h, bold ? juce::Font::bold : juce::Font::plain));
       #endif
    }
    return bold ? Fonts::semiBold (h) : Fonts::regular (h);
}

static juce::Font musicFont (float h)
{
   #if JUCE_MAC
    return juce::Font (juce::FontOptions ("Apple Symbols", h, juce::Font::plain));
   #else
    return juce::Font (juce::FontOptions ("Segoe UI Symbol", h, juce::Font::plain));
   #endif
}

/** Draws text where the sharp / flat signs come from the music font at a fitting size (text fonts lack them). */
static float drawRich (juce::Graphics& g, const juce::String& text, float x, float y, float h, const juce::Font& font, bool measureOnly = false)
{
    const juce::String sharp = juce::CharPointer_UTF8 ("\xe2\x99\xaf"), flat = juce::CharPointer_UTF8 ("\xe2\x99\xad");
    auto acc = musicFont (font.getHeight() * 0.62f);
    float cx = x;
    juce::String run;
    auto flush = [&]
    {
        if (run.isEmpty()) return;
        auto w = juce::GlyphArrangement::getStringWidth (font, run);
        if (! measureOnly) { g.setFont (font); g.drawText (run, Rect (cx, y, w + 2, h), juce::Justification::centredLeft, false); }
        cx += w;
        run.clear();
    };
    for (int i = 0; i < text.length(); ++i)
    {
        auto ch = juce::String::charToString (text[i]);
        if (ch == sharp || ch == flat)
        {
            flush();
            auto w = font.getHeight() * 0.36f;
            if (! measureOnly) { g.setFont (acc); g.drawText (ch, Rect (cx - w * 0.1f, y - font.getHeight() * 0.2f, w * 1.2f, h), juce::Justification::centred, false); }
            cx += w;
        }
        else run << ch;
    }
    flush();
    return cx - x;
}

Key Editor::currentKey() const
{
    if (proc.key >= 0) return { proc.key % 12, proc.key >= 12 };
    return autoKey;
}

//============================================================================== analysis
void Editor::timerCallback()
{
    theme = Theme::make (proc.theme == 1, Theme::swatches()[(size_t) juce::jlimit (0, 7, proc.colour)]);
    auto live = proc.getSounding();
    for (int n = 0; n < 128; ++n) sounding[(size_t) n] = live[(size_t) n] || mouseNotes[(size_t) n];

    std::vector<int> now;
    for (int n = 0; n < 128; ++n) if (sounding[(size_t) n]) now.push_back (n);

    // automatic key: a slowly fading histogram of what has been played (the bass counts a bit more)
    for (auto& h : histogram) h *= 0.997f;
    for (size_t i = 0; i < now.size(); ++i) histogram[(size_t) (now[i] % 12)] += (i == 0 ? 1.6f : 1.0f) / 30.0f;
    float total = 0;
    for (auto h : histogram) total += h;
    if (total > 1.5f)
    {
        auto k = ChordEngine::detectKey (histogram);
        if (k.tonic == candidate.tonic && k.minor == candidate.minor) ++candidateFrames;
        else { candidate = k; candidateFrames = 0; }
        if (candidateFrames > 20) autoKey = candidate;
    }

    if (now != notes)
    {
        notes = now;
        if (! notes.empty())
        {
            shownNotes = notes;
            readings = ChordEngine::analyse (notes);
            showingLast = false;
        }
        else showingLast = ! shownNotes.empty();
    }
    buildPills();
    content.repaint();
}

//============================================================================== bar
void Editor::buildPills()
{
    pills.clear();
    auto L = lang();
    auto keyValue = proc.key < 0 ? tr ("auto", L) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + autoKey.name (L) : currentKey().name (L);
    const char* spellIds[3] = { "bykey", "sharps", "flats" };
    static const char* langs[4] = { "EN", "ES", "JA", "DE" };
    struct P { juce::String label, value; std::function<void()> fn; bool accent; };
    std::vector<P> list {
        { tr ("key", L), keyValue, [this] { showKeyMenu(); }, false },
        { tr ("spelling", L), tr (spellIds[proc.spelling], L), [this] { showSpellingMenu(); }, false },
        { tr ("pedal", L), proc.usePedal ? "ON" : "OFF", [this] { proc.usePedal = ! proc.usePedal; }, false },
        { tr ("theme", L), tr (proc.theme == 1 ? "light" : "dark", L), [this] { proc.theme = 1 - proc.theme; }, false },
        { tr ("colour", L), juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x8f")), [this] { showColourMenu(); }, false },
        { tr ("language", L), langs[proc.language], [this] { showLanguageMenu(); }, false },
    };
    if (proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        list.insert (list.begin(), { "MIDI", tr ("input", L), [] {
           #if JucePlugin_Build_Standalone
            if (auto* holder = juce::StandalonePluginHolder::getInstance()) holder->showAudioSettingsDialog();
           #endif
        }, false });
    if (! proc.license.isLicensed())
        list.push_back ({ "LICENSE", proc.license.isUsable() ? "TRIAL " + juce::String (proc.license.getTrialDaysLeft()) + "D" : juce::String ("ACTIVATE"),
                          [this] { showActivation (true); }, true });

    float x = baseWidth - 40.0f;
    for (int i = (int) list.size() - 1; i >= 0; --i)
    {
        auto& p = list[(size_t) i];
        auto w = juce::jmax (juce::GlyphArrangement::getStringWidth (ui (13.0f, true), p.value), juce::GlyphArrangement::getStringWidth (ui (9.5f), p.label)) + 26.0f;
        x -= w;
        pills.insert (pills.begin(), { p.label, p.value, Rect (x, 18, w, 42), p.fn, p.accent });
        x -= 8;
    }
}

void Editor::paintBar (juce::Graphics& g)
{
    auto& t = theme;
    g.setColour (t.text);
    g.setFont (Fonts::bold (20.0f).withExtraKerningFactor (0.18f));
    g.drawText ("LT LYRA", Rect (40, 18, 200, 26), juce::Justification::centredLeft);
    g.setColour (t.dim);
    g.setFont (ui (10.0f));
    g.drawText (tr ("readonly", lang()), Rect (40, 44, 300, 16), juce::Justification::centredLeft);

    for (size_t i = 0; i < pills.size(); ++i)
    {
        auto& p = pills[i];
        g.setColour (p.accent ? t.hi.withAlpha (0.18f) : t.panel);
        g.fillRoundedRectangle (p.bounds, 10);
        g.setColour (t.edge);
        g.drawRoundedRectangle (p.bounds.reduced (0.5f), 10, 1.0f);
        g.setColour (t.dim);
        g.setFont (ui (9.5f));
        g.drawText (p.label, p.bounds.withTrimmedTop (6).withHeight (12), juce::Justification::centred);
        auto isColour = p.label == tr ("colour", lang());
        g.setColour (isColour ? t.hi : p.accent ? t.hi : t.text);
        g.setFont (isColour ? ui (18.0f) : ui (13.0f, true));
        g.drawText (p.value, p.bounds.withTrimmedTop (18).withTrimmedBottom (4), juce::Justification::centred);
    }
}

//============================================================================== main paint
void Editor::paintContent (juce::Graphics& g)
{
    auto& t = theme;
    g.fillAll (t.bg);
    paintBar (g);

    auto card = Rect (40, 80, 520, 330), staff = Rect (576, 80, 340, 330), wheel = Rect (932, 80, 208, 330);
    pianoArea = Rect (40, 426, 1100, 250);
    for (auto r : { card, staff, wheel, pianoArea })
    {
        g.setColour (t.panel);
        g.fillRoundedRectangle (r, 16);
        g.setColour (t.edge);
        g.drawRoundedRectangle (r.reduced (0.5f), 16, 1.0f);
    }
    paintChord (g, card);
    paintStaff (g, staff);
    paintWheel (g, wheel);
    paintPiano (g, pianoArea);
}

void Editor::paintChord (juce::Graphics& g, Rect r)
{
    auto& t = theme;
    auto L = lang();
    auto key = currentKey();
    auto area = r.reduced (30, 24);
    if (readings.empty() || shownNotes.empty())
    {
        g.setColour (t.dim);
        g.setFont (ui (26.0f));
        g.drawText (tr ("play", L), area, juce::Justification::centred);
        return;
    }
    auto& best = readings[0];
    auto alpha = showingLast ? 0.45f : 1.0f;
    if (showingLast)
    {
        g.setColour (t.dim);
        g.setFont (ui (10.0f, true));
        g.drawText (tr ("held", L), area.withHeight (14), juce::Justification::topLeft);
    }

    // chord symbol, shrunk to fit
    auto sym = ChordEngine::symbol (best, key, mode(), L);
    float size = 104.0f;
    while (size > 40.0f && drawRich (g, sym, 0, 0, 1, Fonts::semiBold (size), true) > area.getWidth()) size -= 4.0f;
    g.setColour (t.text.withAlpha (alpha));
    drawRich (g, sym, area.getX(), area.getY() + 14, 116, Fonts::semiBold (size));

    // roman numeral with figures + function
    auto ro = ChordEngine::roman (best, key);
    float y = area.getY() + 142;
    if (ro.numeral.isNotEmpty())
    {
        auto numText = ro.numeral.replace ("b", juce::CharPointer_UTF8 ("\xe2\x99\xad")).replace ("#", juce::CharPointer_UTF8 ("\xe2\x99\xaf")) + ro.quality;
        g.setColour (t.hi.withAlpha (alpha));
        auto nf = juce::Font (juce::FontOptions ("Georgia", 42.0f, juce::Font::bold));
        auto nw = drawRich (g, numText, area.getX(), y, 48, nf);
        g.setFont (juce::Font (juce::FontOptions ("Georgia", 18.0f, juce::Font::bold)));
        auto fx = area.getX() + nw + 6;
        if (ro.figureBottom.isEmpty()) g.drawText (ro.figureTop, Rect (fx, y + 4, 40, 20), juce::Justification::centredLeft);
        else
        {
            g.drawText (ro.figureTop, Rect (fx, y + 3, 40, 20), juce::Justification::centredLeft);
            g.drawText (ro.figureBottom, Rect (fx, y + 22, 40, 20), juce::Justification::centredLeft);
        }
        const char* fnId = ro.function == Roman::tonic ? "tonic" : ro.function == Roman::subdominant ? "subdominant"
                         : ro.function == Roman::dominant ? "dominant" : ro.function == Roman::secondaryDominant ? "secondary" : "chromatic";
        auto fnCol = ro.function == Roman::tonic ? t.hi : ro.function == Roman::subdominant ? t.hi.withRotatedHue (0.3f)
                   : ro.function == Roman::dominant ? t.hi.withRotatedHue (-0.15f) : t.dim;
        auto label = tr (fnId, L);
        auto pf = ui (11.0f, true);
        auto pw = juce::GlyphArrangement::getStringWidth (pf, label) + 24;
        auto pill = Rect (fx + 50, y + 11, pw, 26);
        g.setColour (fnCol.withAlpha (0.16f * alpha));
        g.fillRoundedRectangle (pill, 13);
        g.setColour (fnCol.withAlpha (alpha));
        g.setFont (pf);
        g.drawText (label, pill, juce::Justification::centred);
        g.setColour (t.dim);
        g.setFont (ui (11.0f));
        g.drawText (key.name (L), Rect (pill.getRight() + 12, y + 11, 200, 26), juce::Justification::centredLeft);
    }
    y += 62;

    // inversion
    const char* invId = best.inversion == 0 ? "inv0" : best.inversion == 1 ? "inv1" : best.inversion == 2 ? "inv2" : best.inversion == 3 ? "inv3" : "slash";
    g.setColour (t.dim);
    g.setFont (ui (11.0f, true));
    if (best.numNotes >= 3) g.drawText (tr (invId, L), Rect (area.getX(), y, area.getWidth(), 16), juce::Justification::centredLeft);
    y += 24;

    // also
    auto alts = ChordEngine::alternatives (readings);
    if (! alts.empty())
    {
        juce::StringArray names;
        for (auto& a : alts) names.add (ChordEngine::symbol (a, key, mode(), L));
        g.setColour (t.dim);
        g.setFont (ui (11.0f, true));
        auto lw = juce::GlyphArrangement::getStringWidth (ui (11.0f, true), tr ("also", L)) + 14;
        g.drawText (tr ("also", L), Rect (area.getX(), y, lw, 20), juce::Justification::centredLeft);
        g.setColour (t.text.withAlpha (0.8f * alpha));
        drawRich (g, names.joinIntoString (dotSep), area.getX() + lw, y - 1, 22, Fonts::regular (17.0f));
    }
    y += 30;

    // notes, spelled
    auto sp = ChordEngine::spellChord (best, key, mode());
    juce::StringArray noteNames;
    for (auto n : shownNotes)
    {
        auto s = sp[(size_t) (n % 12)];
        if (s.letter < 0) s = ChordEngine::spellRoot (n % 12, key, mode());
        auto octave = (n - naturalPc (s.letter) - s.accidental) / 12 - 1;
        noteNames.add (noteText (s, L) + juce::String (octave));
    }
    g.setColour (t.dim);
    g.setFont (ui (11.0f, true));
    auto nw = juce::GlyphArrangement::getStringWidth (ui (11.0f, true), tr ("notes", L)) + 14;
    g.drawText (tr ("notes", L), Rect (area.getX(), y, nw, 20), juce::Justification::centredLeft);
    g.setColour (t.text.withAlpha (alpha));
    drawRich (g, noteNames.joinIntoString ("  "), area.getX() + nw, y, 20, Fonts::regular (15.0f));
}

//============================================================================== grand staff
void Editor::paintStaff (juce::Graphics& g, Rect r)
{
    auto& t = theme;
    auto key = currentKey();
    const float sp = 11.0f;
    const float left = r.getX() + 22, right = r.getRight() - 22;
    const float trebleBottom = r.getY() + 120, bassBottom = r.getY() + 250;   // E4 line and G2 line
    auto yFor = [&] (int step, bool treble) { return treble ? trebleBottom - (float) (step - 30) * sp * 0.5f : bassBottom - (float) (step - 18) * sp * 0.5f; };

    g.setColour (t.text.withAlpha (0.55f));
    for (int i = 0; i < 5; ++i)
    {
        g.drawHorizontalLine ((int) (trebleBottom - i * sp), left, right);
        g.drawHorizontalLine ((int) (bassBottom - i * sp), left, right);
    }
    g.drawVerticalLine ((int) left, trebleBottom - 4 * sp, bassBottom + 0.5f);
    g.setColour (t.text);
    g.setFont (musicFont (sp * 6.6f));
    g.drawText (juce::CharPointer_UTF8 ("\xf0\x9d\x84\x9e"), Rect (left + 2, trebleBottom - sp * 7.0f, 40, sp * 9.0f), juce::Justification::centred, false);
    g.setFont (musicFont (sp * 3.6f));
    g.drawText (juce::CharPointer_UTF8 ("\xf0\x9d\x84\xa2"), Rect (left + 4, bassBottom - sp * 4.75f, 36, sp * 4.6f), juce::Justification::centred, false);

    // key signature
    std::array<int, 7> keyAcc {};
    auto sig = key.signature();
    static const int sharpLetters[7] = { 3, 0, 4, 1, 5, 2, 6 }, flatLetters[7] = { 6, 2, 5, 1, 4, 0, 3 };
    static const int sharpSteps[7] = { 38, 35, 39, 36, 33, 37, 34 }, flatSteps[7] = { 34, 37, 33, 36, 32, 35, 31 };
    float x = left + 44;
    g.setFont (musicFont (sp * 2.6f));
    for (int i = 0; i < std::abs (sig); ++i)
    {
        auto sharp = sig > 0;
        keyAcc[(size_t) (sharp ? sharpLetters[i] : flatLetters[i])] = sharp ? 1 : -1;
        auto step = sharp ? sharpSteps[i] : flatSteps[i];
        auto glyph = juce::String (juce::CharPointer_UTF8 (sharp ? "\xe2\x99\xaf" : "\xe2\x99\xad"));
        for (int staffIdx = 0; staffIdx < 2; ++staffIdx)
        {
            auto y = staffIdx == 0 ? yFor (step, true) : yFor (step - 14, false);
            g.drawText (glyph, Rect (x - 6, y - sp * (sharp ? 1.45f : 1.75f), 12, sp * 3.0f), juce::Justification::centred, false);
        }
        x += 10;
    }

    if (shownNotes.empty()) return;
    auto best = readings.empty() ? ChordReading() : readings[0];
    auto spelled = ChordEngine::spellChord (best, key, mode());
    struct Head { int step; bool treble; int acc; bool root; };
    std::vector<Head> heads;
    for (auto n : shownNotes)
    {
        auto s = spelled[(size_t) (n % 12)];
        if (s.letter < 0) s = ChordEngine::spellRoot (n % 12, key, mode());
        auto octave = (n - naturalPc (s.letter) - s.accidental) / 12 - 1;
        heads.push_back ({ s.letter + 7 * octave, n >= 60, s.accidental, n % 12 == best.root });
    }

    const float noteX = juce::jmax (x + 50, (left + right) * 0.5f + 20);
    const float headW = sp * 1.55f, headH = sp * 1.05f;
    auto alpha = showingLast ? 0.45f : 1.0f;
    for (int staffIdx = 0; staffIdx < 2; ++staffIdx)
    {
        auto treble = staffIdx == 0;
        std::vector<Head> hs;
        for (auto& h : heads) if (h.treble == treble) hs.push_back (h);
        std::sort (hs.begin(), hs.end(), [] (const Head& a, const Head& b) { return a.step < b.step; });
        if (hs.empty()) continue;

        // ledger lines
        g.setColour (t.text.withAlpha (0.55f * alpha));
        auto lowLine = treble ? 30 : 18, highLine = treble ? 38 : 26;
        for (int s = lowLine - 2; s >= hs.front().step; s -= 2) g.drawHorizontalLine ((int) yFor (s, treble), noteX - headW * 0.9f, noteX + headW * 1.9f);
        for (int s = highLine + 2; s <= hs.back().step; s += 2) g.drawHorizontalLine ((int) yFor (s, treble), noteX - headW * 0.9f, noteX + headW * 1.9f);

        int prevStep = -100; bool prevShifted = false;
        int accColumn = 0; int lastAccStep = -100;
        for (auto& h : hs)
        {
            auto shifted = (h.step - prevStep == 1) && ! prevShifted;   // seconds sit to the right
            prevShifted = shifted; prevStep = h.step;
            auto cx = noteX + (shifted ? headW * 0.95f : 0.0f), cy = yFor (h.step, treble);
            auto col = h.root ? t.hi : t.text;
            juce::Path head;
            head.addEllipse (-headW * 0.5f, -headH * 0.5f, headW, headH);
            juce::Path hole;
            hole.addEllipse (-headW * 0.22f, -headH * 0.36f, headW * 0.44f, headH * 0.72f);
            hole.applyTransform (juce::AffineTransform::rotation (-0.6f));
            head.addPath (hole);
            head.setUsingNonZeroWinding (false);
            g.setColour (col.withAlpha (alpha));
            g.fillPath (head, juce::AffineTransform::translation (cx, cy));

            if (h.acc != keyAcc[(size_t) ((h.step % 7 + 7) % 7)])
            {
                accColumn = (h.step - lastAccStep < 6) ? accColumn + 1 : 0;
                lastAccStep = h.step;
                const char* glyph = h.acc == 0 ? "\xe2\x99\xae" : h.acc == 1 ? "\xe2\x99\xaf" : h.acc == -1 ? "\xe2\x99\xad" : h.acc == 2 ? "\xf0\x9d\x84\xaa" : "\xf0\x9d\x84\xab";
                g.setFont (musicFont (sp * 2.6f));
                auto ax = noteX - headW * 1.25f - accColumn * sp * 1.1f;
                g.drawText (juce::CharPointer_UTF8 (glyph), Rect (ax - 8, cy - sp * (h.acc < 0 ? 1.75f : 1.45f), 16, sp * 3.0f), juce::Justification::centred, false);
            }
        }
    }
}

//============================================================================== chromatic wheel
void Editor::paintWheel (juce::Graphics& g, Rect r)
{
    auto& t = theme;
    auto key = currentKey();
    auto L = lang();
    auto ctr = juce::Point<float> (r.getCentreX(), r.getY() + 140);
    auto rad = 78.0f;
    auto scale = key.scale();
    auto best = readings.empty() ? ChordReading() : readings[0];
    auto spelled = ChordEngine::spellChord (best, key, mode());
    std::array<bool, 12> on {};
    for (auto n : shownNotes) on[(size_t) (n % 12)] = true;
    auto alpha = showingLast ? 0.45f : 1.0f;

    auto posOf = [&] (int pc, float rr) { auto a = juce::MathConstants<float>::twoPi * (float) pc / 12.0f; return ctr + juce::Point<float> (std::sin (a), -std::cos (a)) * rr; };
    g.setColour (t.faint);
    g.drawEllipse (Rect (rad * 2, rad * 2).withCentre (ctr), 1.0f);

    // voicing shape
    juce::Path shape;
    bool first = true;
    int count = 0;
    for (int pc = 0; pc < 12; ++pc)
        if (on[(size_t) pc]) { auto p = posOf (pc, rad); if (first) { shape.startNewSubPath (p); first = false; } else shape.lineTo (p); ++count; }
    if (count >= 2)
    {
        shape.closeSubPath();
        g.setColour (t.hi.withAlpha (0.16f * alpha));
        g.fillPath (shape);
        g.setColour (t.hi.withAlpha (0.9f * alpha));
        g.strokePath (shape, juce::PathStrokeType (1.6f));
    }
    for (int pc = 0; pc < 12; ++pc)
    {
        auto p = posOf (pc, rad);
        auto isRoot = pc == best.root && on[(size_t) pc];
        auto d = isRoot ? 18.0f : on[(size_t) pc] ? 13.0f : 7.0f;
        g.setColour (isRoot ? t.hi.withAlpha (alpha) : on[(size_t) pc] ? t.text.withAlpha (alpha) : scale[(size_t) pc] ? t.dim : t.faint);
        g.fillEllipse (Rect (d, d).withCentre (p));
        if (pc == key.tonic)
        {
            g.setColour (t.hi.withAlpha (0.7f));
            g.drawEllipse (Rect (d + 8, d + 8).withCentre (p), 1.2f);
        }
        auto s = on[(size_t) pc] && spelled[(size_t) pc].letter >= 0 ? spelled[(size_t) pc] : ChordEngine::spellRoot (pc, key, mode());
        auto lp = posOf (pc, rad + 22);
        g.setColour (on[(size_t) pc] ? t.text : t.dim);
        auto lf = Fonts::semiBold (on[(size_t) pc] ? 13.0f : 11.0f);
        auto label = noteText (s, L);
        auto lw = drawRich (g, label, 0, 0, 1, lf, true);
        drawRich (g, label, lp.x - lw * 0.5f, lp.y - 9, 18, lf);
    }
    // intervals above the root
    if (best.root >= 0 && best.numNotes >= 2)
    {
        juce::StringArray iv;
        for (int i = 0; i < 12; ++i) if (best.letterOffset[(size_t) i] >= 0) iv.add (ChordEngine::intervalName (i, L));
        g.setColour (t.dim);
        g.setFont (ui (10.0f, true));
        g.drawText (iv.joinIntoString (" "), Rect (r.getX() + 8, r.getBottom() - 42, r.getWidth() - 16, 16), juce::Justification::centred);
    }
}

//============================================================================== piano
static bool isBlack (int n) { auto pc = n % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

void Editor::paintPiano (juce::Graphics& g, Rect r)
{
    auto& t = theme;
    auto key = currentKey();
    auto scale = key.scale();
    auto best = readings.empty() ? ChordReading() : readings[0];
    auto keys = r.reduced (20, 0).withTrimmedTop (44).withTrimmedBottom (22);
    const int lo = 21, hi = 108;
    int whites = 0;
    for (int n = lo; n <= hi; ++n) whites += ! isBlack (n);
    auto ww = keys.getWidth() / (float) whites;

    // header: scale name + pedal
    g.setColour (t.dim);
    g.setFont (ui (11.0f, true));
    g.drawText (key.name (lang()), Rect (keys.getX(), r.getY() + 14, 300, 18), juce::Justification::centredLeft);
    if (proc.isPedalDown())
    {
        g.setColour (t.hi);
        g.drawText (tr ("pedal", lang()), Rect (keys.getRight() - 120, r.getY() + 14, 120, 18), juce::Justification::centredRight);
    }

    auto alpha = showingLast ? 0.5f : 1.0f;
    std::array<bool, 128> lit {};
    for (auto n : shownNotes) lit[(size_t) n] = true;

    int wi = 0;
    std::vector<std::pair<int, float>> blackX;
    for (int n = lo; n <= hi; ++n)
    {
        if (isBlack (n)) { blackX.push_back ({ n, keys.getX() + wi * ww }); continue; }
        auto k = Rect (keys.getX() + wi * ww, keys.getY(), ww, keys.getHeight()).reduced (0.6f, 0);
        g.setColour (lit[(size_t) n] ? (n % 12 == best.root ? t.hi : t.hi.interpolatedWith (t.white, 0.35f)).withAlpha (alpha) : t.white);
        g.fillRoundedRectangle (k, 3);
        g.setColour (t.black.withAlpha (0.14f));
        g.drawRoundedRectangle (k, 3, 0.8f);
        if (scale[(size_t) (n % 12)] && ! lit[(size_t) n])
        {
            g.setColour (n % 12 == key.tonic ? t.hi.withAlpha (0.8f) : t.black.withAlpha (0.22f));
            g.fillEllipse (Rect (5, 5).withCentre ({ k.getCentreX(), k.getBottom() - 12 }));
        }
        if (n % 12 == 0)
        {
            g.setColour (t.black.withAlpha (0.45f));
            g.setFont (Fonts::regular (8.5f));
            g.drawText ("C" + juce::String (n / 12 - 1), k.withTrimmedTop (k.getHeight() - 30).withHeight (12), juce::Justification::centred);
        }
        ++wi;
    }
    for (auto [n, x] : blackX)
    {
        auto k = Rect (x - ww * 0.32f, keys.getY(), ww * 0.64f, keys.getHeight() * 0.62f);
        g.setColour (lit[(size_t) n] ? (n % 12 == best.root ? t.hi : t.hi.interpolatedWith (t.black, 0.25f)).withAlpha (juce::jmax (alpha, 0.7f)) : t.black);
        g.fillRoundedRectangle (k, 2.5f);
        if (scale[(size_t) (n % 12)] && ! lit[(size_t) n])
        {
            g.setColour (n % 12 == key.tonic ? t.hi : juce::Colours::white.withAlpha (0.35f));
            g.fillEllipse (Rect (4, 4).withCentre ({ k.getCentreX(), k.getBottom() - 9 }));
        }
    }
    g.setColour (t.dim);
    g.setFont (ui (9.5f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("A0 \xe2\x80\x94 C8  \xc2\xb7  ")) + tr ("hint", lang()), Rect (keys.getX(), keys.getBottom() + 4, keys.getWidth(), 14), juce::Justification::centredLeft);
}

int Editor::noteAt (juce::Point<float> p) const
{
    auto keys = pianoArea.reduced (20, 0).withTrimmedTop (44).withTrimmedBottom (22);
    if (! keys.contains (p)) return -1;
    int whites = 52;
    auto ww = keys.getWidth() / (float) whites;
    int wi = 0;
    int whiteHit = -1;
    for (int n = 21; n <= 108; ++n)
    {
        if (isBlack (n))
        {
            auto k = Rect (keys.getX() + wi * ww - ww * 0.32f, keys.getY(), ww * 0.64f, keys.getHeight() * 0.62f);
            if (k.contains (p)) return n;
            continue;
        }
        if (Rect (keys.getX() + wi * ww, keys.getY(), ww, keys.getHeight()).contains (p)) whiteHit = n;
        ++wi;
    }
    return whiteHit;
}

void Editor::contentMouseDown (const juce::MouseEvent& e)
{
    auto p = e.position;
    for (auto& pill : pills)
        if (pill.bounds.contains (p) && pill.click) { auto fn = pill.click; fn(); return; }
    if (pianoArea.contains (p))
    {
        if (e.mods.isPopupMenu()) { mouseNotes = {}; return; }
        auto n = noteAt (p);
        if (n >= 0) { mouseNotes[(size_t) n] = ! mouseNotes[(size_t) n]; dragNote = n; }
    }
}

void Editor::contentMouseDrag (const juce::MouseEvent& e)
{
    if (dragNote < 0) return;
    auto n = noteAt (e.position);
    if (n >= 0 && n != dragNote) { mouseNotes[(size_t) n] = mouseNotes[(size_t) dragNote]; dragNote = n; }
}

//============================================================================== menus
void Editor::showKeyMenu()
{
    auto L = lang();
    juce::PopupMenu menu, maj, min;
    menu.addItem (1, tr ("auto", L), true, proc.key < 0);
    for (int k = 0; k < 12; ++k) maj.addItem (100 + k, Key { k, false }.name (L), true, proc.key == k);
    for (int k = 0; k < 12; ++k) min.addItem (200 + k, Key { k, true }.name (L), true, proc.key == 12 + k);
    menu.addSubMenu (L == Language::english ? "MAJOR" : Key { 0, false }.name (L).fromFirstOccurrenceOf (" ", false, false).toUpperCase(), maj);
    menu.addSubMenu (L == Language::english ? "MINOR" : Key { 0, true }.name (L).fromFirstOccurrenceOf (" ", false, false).toUpperCase(), min);
    juce::Component::SafePointer<Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        safe->proc.key = r == 1 ? -1 : r >= 200 ? 12 + (r - 200) : r - 100;
    });
}

void Editor::showSpellingMenu()
{
    auto L = lang();
    juce::PopupMenu menu;
    const char* ids[3] = { "bykey", "sharps", "flats" };
    for (int i = 0; i < 3; ++i) menu.addItem (i + 1, tr (ids[i], L), true, proc.spelling == i);
    juce::Component::SafePointer<Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe != nullptr && r > 0) safe->proc.spelling = r - 1;
    });
}

void Editor::showColourMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < 8; ++i)
    {
        juce::PopupMenu::Item item (juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x8f  ")) + juce::String (i + 1));
        item.itemID = i + 1;
        item.colour = Theme::swatches()[(size_t) i];
        item.isTicked = proc.colour == i;
        menu.addItem (item);
    }
    juce::Component::SafePointer<Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe != nullptr && r > 0) safe->proc.colour = r - 1;
    });
}

void Editor::showLanguageMenu()
{
    juce::PopupMenu menu;
    const char* names[4] = { "English", "Espa\xc3\xb1ol", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", "Deutsch" };
    for (int i = 0; i < 4; ++i) menu.addItem (i + 1, juce::String (juce::CharPointer_UTF8 (names[i])), true, proc.language == i);
    juce::Component::SafePointer<Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
    {
        if (safe != nullptr && r > 0) safe->proc.language = r - 1;
    });
}

void Editor::showActivation (bool show)
{
    if (! show)
    {
        if (activation) { content.removeChildComponent (activation.get()); activation.reset(); }
        return;
    }
    activation = std::make_unique<lt::ui::ActivationComponent> (proc.license, ltPalette);
    activation->onDismiss = [this]
    {
        proc.refreshLicense();
        juce::Component::SafePointer<Editor> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->showActivation (false); });
    };
    content.addAndMakeVisible (*activation);
    activation->setBounds (content.getLocalBounds());
}

} // namespace lyra
