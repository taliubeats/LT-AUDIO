#include "Visuals.h"

namespace cosmos
{

using juce::Colour;
using juce::Graphics;
using juce::Path;
using Rect = juce::Rectangle<float>;
using juce::jlimit;
using juce::jmax;
using juce::jmin;

static constexpr float twoPi = juce::MathConstants<float>::twoPi;

//============================================================================== helpers
namespace draw
{
    void label (Graphics& g, const juce::String& text, float x, float y, Colour c, float size, juce::Justification j, float width)
    {
        g.setColour (c);
        g.setFont (monoFont (size));
        auto r = Rect (x, y, width, size + 4);
        if (j.testFlags (juce::Justification::right)) r = r.withX (x - width);
        else if (j.testFlags (juce::Justification::horizontallyCentred)) r = r.withX (x - width * 0.5f);
        g.drawText (text, r, j, false);
    }

    float freqToX (float hz, Rect r, float lo, float hi)
    {
        return r.getX() + r.getWidth() * std::log (jmax (lo, hz) / lo) / std::log (hi / lo);
    }

    juce::String hzText (float hz)
    {
        if (hz >= 1000.0f)
        {
            auto k = hz / 1000.0f;
            return (std::abs (k - std::round (k)) < 0.05f || k >= 10.0f ? juce::String ((int) std::round (k)) : juce::String (k, 1)) + "K";
        }
        return juce::String ((int) std::round (hz));
    }

    juce::String lufsText (float v)
    {
        if (v <= -70.0f) return juce::CharPointer_UTF8 ("\xe2\x80\x94");
        return juce::String (v, 1).replace ("-", juce::CharPointer_UTF8 ("\xe2\x88\x92"));
    }
}

static juce::String barBeatText (const Analyzer& a)
{
    if (a.barTime < 0) return juce::CharPointer_UTF8 ("\xe2\x80\x94");
    auto bar = (int) std::floor (a.barTime);
    auto beat = (int) std::floor ((a.barTime - bar) * a.beatsPerBar);
    return juce::String (bar + 1) + "." + juce::String (beat + 1);
}

static void frequencyGrid (Graphics& g, Rect r, const Skin& s, bool labels)
{
    static const float lines[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    for (auto f : lines)
    {
        auto x = draw::freqToX (f, r);
        g.setColour (s.grid);
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        if (labels) draw::label (g, draw::hzText (f), x, r.getBottom() - 12, s.dim, 8.5f, juce::Justification::centredTop, 40);
    }
}

static float tilt (float db, float hz) { return db + 3.0f * std::log2 (jmax (20.0f, hz) / 1000.0f); }   // 3 dB/oct display slope

//============================================================================== SPECTRUM
struct SpectrumVisual : Visual
{
    std::array<float, Analyzer::numLogBands> hold {};
    SpectrumVisual() { hold.fill (-140.0f); }

    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto plot = r.reduced (6, 6).withTrimmedBottom (10);
        frequencyGrid (g, plot.withHeight (plot.getHeight() + 10), s, true);
        for (int db = -12; db >= -72; db -= 12)
        {
            auto y = plot.getY() + plot.getHeight() * (float) -db / 84.0f;
            g.setColour (s.grid.withMultipliedAlpha (0.6f));
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        }
        auto yOf = [plot] (float db) { return plot.getY() + plot.getHeight() * jlimit (0.0f, 1.0f, -db / 84.0f); };

        Path line, peak, fill;
        for (int b = 0; b < Analyzer::numLogBands; ++b)
        {
            auto hz = Analyzer::logBandFrequency ((float) b);
            auto x = draw::freqToX (hz, plot);
            auto v = tilt (c.a.logDb[(size_t) b], hz) + 6.0f;
            hold[(size_t) b] = jmax (v, hold[(size_t) b] - 0.25f);
            auto y = yOf (v), yp = yOf (hold[(size_t) b]);
            if (b == 0) { line.startNewSubPath (x, y); peak.startNewSubPath (x, yp); fill.startNewSubPath (x, plot.getBottom()); }
            else { line.lineTo (x, y); peak.lineTo (x, yp); }
            fill.lineTo (x, y);
        }
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (s.line.withAlpha (0.22f), 0, plot.getY(), s.line.withAlpha (0.02f), 0, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (s.accent.withAlpha (0.8f));
        g.strokePath (peak, juce::PathStrokeType (0.9f));
        g.setColour (s.line);
        g.strokePath (line, juce::PathStrokeType (1.3f));
    }
};

//============================================================================== SPECTROGRAM
struct SpectrogramVisual : Visual
{
    juce::Image img;
    int write = 0;
    juce::String skinName;

    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto w = jmax (16, (int) r.getWidth()), h = Analyzer::numLogBands;
        if (! img.isValid() || img.getWidth() != w || skinName != s.name)
        {
            img = juce::Image (juce::Image::RGB, w, h, true);
            juce::Graphics ig (img);
            ig.fillAll (s.heatAt (0));
            write = 0;
            skinName = s.name;
        }
        {
            juce::Image::BitmapData bd (img, write, 0, 1, h, juce::Image::BitmapData::writeOnly);
            for (int b = 0; b < h; ++b)
            {
                auto hz = Analyzer::logBandFrequency ((float) b);
                auto v = (tilt (c.a.logRaw[(size_t) b], hz) + 84.0f) / 78.0f;
                bd.setPixelColour (0, h - 1 - b, s.heatAt (v * v * 0.9f + v * 0.1f));
            }
        }
        write = (write + 1) % w;
        // oldest column on the left
        auto rest = w - write;
        auto scaleX = r.getWidth() / (float) w;
        g.drawImage (img, (int) r.getX(), (int) r.getY(), (int) std::ceil (rest * scaleX), (int) r.getHeight(), write, 0, rest, h);
        if (write > 0)
            g.drawImage (img, (int) (r.getX() + rest * scaleX), (int) r.getY(), (int) std::ceil (write * scaleX), (int) r.getHeight(), 0, 0, write, h);

        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            auto y = r.getBottom() - r.getHeight() * std::log (f / 25.0f) / std::log (800.0f);
            draw::label (g, draw::hzText (f), r.getX() + 4, y - 6, s.text.withAlpha (0.7f), 8.5f);
        }
    }
};

//============================================================================== WAVEFORM
struct WaveformVisual : Visual
{
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& a = c.a;
        auto plot = r.reduced (4, 8);
        auto cols = jmin (Analyzer::numWaveCols, (int) plot.getWidth());
        auto mid = plot.getCentreY(), half = plot.getHeight() * 0.5f;
        g.setColour (s.grid);
        g.drawHorizontalLine ((int) mid, plot.getX(), plot.getRight());
        for (int i = 0; i < cols; ++i)
        {
            auto& w = a.wave[(size_t) ((a.waveWrite - cols + i + Analyzer::numWaveCols) % Analyzer::numWaveCols)];
            auto top = jmax (w.maxL, w.maxR), bottom = jmin (w.minL, w.minR);
            auto lowShare = w.low / (w.low + w.high + 1.0e-6f);
            auto x = plot.getX() + (float) i * plot.getWidth() / (float) cols;
            g.setColour (s.line2.interpolatedWith (s.line, 1.0f - lowShare).withAlpha (0.92f));
            auto y0 = mid - jlimit (0.0f, 1.0f, top) * half, y1 = mid - jlimit (-1.0f, 0.0f, bottom) * half;
            g.fillRect (x, y0, jmax (1.0f, plot.getWidth() / (float) cols), jmax (1.0f, y1 - y0));
        }
        auto secs = cols * a.waveColLength / a.getSampleRate();
        draw::label (g, "WAVEFORM " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7 ")) + juce::String (juce::roundToInt (secs)) + "S", r.getX() + 6, r.getY() + 4, s.dim, 8.5f);
    }
};

//============================================================================== LOUDNESS
struct LoudnessVisual : Visual
{
    bool doubleClicked() override { if (onResetLoudness) onResetLoudness(); return true; }

    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& L = c.loud;
        auto area = r.reduced (8, 6);
        auto bigH = jlimit (14.0f, 34.0f, area.getHeight() * 0.2f);
        auto tooFar = c.target != 0 && L.integrated > -70 && std::abs (L.integrated - c.target) > 1.0f;

        auto block = [&] (float x, const juce::String& title, float v, float size, Colour col)
        {
            draw::label (g, title, x, area.getY(), s.dim, 8.5f);
            g.setColour (col);
            g.setFont (monoFont (size));
            g.drawText (draw::lufsText (v), Rect (x, area.getY() + 11, 140, size + 4), juce::Justification::topLeft, false);
        };
        bigH = juce::jmin (bigH, area.getWidth() * 0.4f / 5.2f);   // "-23.4" must fit the M column
        auto colW = area.getWidth() * 0.6f / 3.0f;
        auto mW = area.getWidth() * 0.4f;
        block (area.getX(), "M " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + " LUFS", L.momentary, bigH, s.text);
        block (area.getX() + mW, "S " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + " LUFS", L.shortTerm, bigH * 0.62f, s.text);
        block (area.getX() + mW + colW, "I " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + " LUFS", L.integrated, bigH * 0.62f, tooFar ? s.accent : s.text);
        {
            auto x = area.getX() + mW + colW * 2;
            draw::label (g, "TP MAX", x, area.getY(), s.dim, 8.5f);
            auto tp = jmax (L.truePeakMax[0], L.truePeakMax[1]);
            g.setColour (tp > -1.0f ? s.warn : s.text);
            g.setFont (monoFont (bigH * 0.62f));
            g.drawText (tp <= -100 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")) : juce::String (tp, 1),
                        Rect (x, area.getY() + 11, 120, bigH), juce::Justification::topLeft, false);
            draw::label (g, "LRA " + juce::String (L.range, 1), x, area.getY() + 13 + bigH * 0.7f, s.dim, 8.5f);
        }

        // momentary history (60 s)
        auto graph = area.withTrimmedTop (bigH + 18).withTrimmedBottom (12);
        if (graph.getHeight() < 10) return;
        auto lo = -48.0f, hi = -3.0f;
        auto yOf = [&] (float v) { return graph.getBottom() - graph.getHeight() * jlimit (0.0f, 1.0f, (v - lo) / (hi - lo)); };
        g.setColour (s.grid);
        g.drawRect (graph, 1.0f);
        Path p;
        auto& a = c.a;
        bool started = false;
        for (int i = 0; i < Analyzer::loudHistory; ++i)
        {
            auto v = a.momentaryHistory[(size_t) ((a.loudWrite + i) % Analyzer::loudHistory)];
            auto x = graph.getX() + graph.getWidth() * (float) i / (float) (Analyzer::loudHistory - 1);
            auto y = yOf (v <= -100 ? lo : v);
            if (! started) { p.startNewSubPath (x, y); started = true; }
            else p.lineTo (x, y);
        }
        g.setColour (s.line);
        g.strokePath (p, juce::PathStrokeType (1.1f));
        if (c.target != 0)
        {
            auto y = yOf (c.target);
            Path dash;
            dash.startNewSubPath (graph.getX(), y);
            dash.lineTo (graph.getRight(), y);
            float d[] = { 4, 3 };
            juce::PathStrokeType (1.0f).createDashedStroke (dash, dash, d, 2);
            g.setColour (s.accent);
            g.fillPath (dash);
            draw::label (g, juce::String ((int) c.target).replace ("-", juce::CharPointer_UTF8 ("\xe2\x88\x92")), graph.getRight() - 2, y - 12, s.accent, 8.5f, juce::Justification::topRight, 40);
        }
        draw::label (g, "M HISTORY " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + " 60S", graph.getX(), graph.getBottom() + 2, s.dim, 8.0f);
        draw::label (g, "DOUBLE-CLICK TO RESET", graph.getRight(), graph.getBottom() + 2, s.dim, 8.0f, juce::Justification::topRight, 200);
    }
};

//============================================================================== LEVELS
struct LevelsVisual : Visual
{
    std::array<float, 2> shown { -90, -90 }, hold { -90, -90 }, maxPeak { -90, -90 };
    std::array<int, 2> holdAge {};
    bool doubleClicked() override { maxPeak = { -90, -90 }; if (onResetLoudness) onResetLoudness(); return true; }

    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& a = c.a;
        auto area = r.reduced (6, 6);
        auto top = area.removeFromTop (14);
        auto scaleW = 30.0f;
        auto barW = jmin (20.0f, (area.getWidth() - scaleW) / 2.6f);
        auto total = barW * 2 + scaleW;
        auto x0 = area.getCentreX() - total * 0.5f;
        auto bars = area.withTrimmedBottom (12);
        auto yOf = [&] (float db) { return bars.getY() + bars.getHeight() * jlimit (0.0f, 1.0f, -db / 60.0f); };

        for (int ch = 0; ch < 2; ++ch)
        {
            auto pk = a.peak[(size_t) ch] > 1.0e-6f ? 20.0f * std::log10 (a.peak[(size_t) ch]) : -120.0f;
            shown[(size_t) ch] = pk > shown[(size_t) ch] ? pk : shown[(size_t) ch] - 0.9f;
            if (pk >= hold[(size_t) ch]) { hold[(size_t) ch] = pk; holdAge[(size_t) ch] = 0; }
            else if (++holdAge[(size_t) ch] > 90) hold[(size_t) ch] -= 0.6f;
            maxPeak[(size_t) ch] = jmax (maxPeak[(size_t) ch], c.loud.truePeakMax[(size_t) ch]);
            auto rmsDb = 10.0f * std::log10 (a.rms[(size_t) ch] + 1.0e-12f);

            auto bx = x0 + (ch == 0 ? 0.0f : barW + scaleW);
            auto bar = Rect (bx, bars.getY(), barW, bars.getHeight());
            g.setColour (s.grid);
            g.fillRect (bar);
            auto yPeak = yOf (shown[(size_t) ch]), yRms = yOf (rmsDb);
            g.setColour (s.dim.withAlpha (0.55f));
            g.fillRect (bar.withTop (yPeak));
            g.setColour (s.line.withAlpha (0.9f));
            g.fillRect (bar.withTop (yRms).reduced (2, 0));
            g.setColour (hold[(size_t) ch] > -1.0f ? s.warn : s.accent);
            g.fillRect (bx, yOf (hold[(size_t) ch]) - 1, barW, 2.0f);

            auto mp = maxPeak[(size_t) ch];
            draw::label (g, mp <= -89 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")) : juce::String (mp, 1).replace ("-", juce::CharPointer_UTF8 ("\xe2\x88\x92")),
                         bx + barW * 0.5f, top.getY(), mp > -1.0f ? s.warn : s.text, 9.5f, juce::Justification::centredTop, 60);
            draw::label (g, ch == 0 ? "L" : "R", bx + barW * 0.5f, bars.getBottom() + 1, s.dim, 8.5f, juce::Justification::centredTop, 20);
        }
        for (int db : { 0, -6, -12, -18, -24, -30, -40, -50 })
        {
            auto y = yOf ((float) db);
            draw::label (g, db == 0 ? juce::String ("0") : juce::String (db).replace ("-", juce::CharPointer_UTF8 ("\xe2\x88\x92")),
                         x0 + barW + scaleW * 0.5f, y - 5, s.dim, 7.5f, juce::Justification::centredTop, scaleW);
        }
        draw::label (g, "DBTP", x0 + barW + scaleW * 0.5f, bars.getBottom() + 1, s.dim, 7.5f, juce::Justification::centredTop, scaleW + 10);
    }
};

//============================================================================== VU
struct VuVisual : Visual
{
    std::array<float, 2> needle { -40, -40 };

    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto area = r.reduced (6);
        bool side = area.getWidth() > area.getHeight() * 1.6f;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto face = side ? (ch == 0 ? area.withWidth (area.getWidth() * 0.5f) : area.withTrimmedLeft (area.getWidth() * 0.5f))
                             : (ch == 0 ? area.withHeight (area.getHeight() * 0.5f) : area.withTrimmedTop (area.getHeight() * 0.5f));
            face = face.reduced (4);
            auto vu = c.a.vu[(size_t) ch] + 18.0f;       // -18 dBFS = 0 VU
            needle[(size_t) ch] += (vu - needle[(size_t) ch]) * 0.25f;
            auto radius = jmin (face.getWidth() * 0.6f, face.getHeight() * 0.95f);
            auto cx = face.getCentreX(), cy = face.getY() + radius + 4;
            auto angleOf = [] (float v) { return juce::jmap (jlimit (-20.0f, 3.0f, v), -20.0f, 3.0f, -0.85f, 0.85f); };
            // scale
            for (float v : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
            {
                auto ang = angleOf (v);
                auto p0 = juce::Point<float> (cx + std::sin (ang) * radius * 0.84f, cy - std::cos (ang) * radius * 0.84f);
                auto p1 = juce::Point<float> (cx + std::sin (ang) * radius * 0.95f, cy - std::cos (ang) * radius * 0.95f);
                g.setColour (v > 0 ? s.warn : s.dim);
                g.drawLine ({ p0, p1 }, 1.0f);
                if (radius > 40 && (v == -20.0f || v == -10.0f || v == -5.0f || v == 0.0f || v == 3.0f))
                    draw::label (g, juce::String ((int) v), cx + std::sin (ang) * radius * 0.72f, cy - std::cos (ang) * radius * 0.72f - 5,
                                 v > 0 ? s.warn : s.dim, 7.5f, juce::Justification::centredTop, 24);
            }
            Path arc;
            arc.addCentredArc (cx, cy, radius * 0.95f, radius * 0.95f, 0, angleOf (0), angleOf (3), true);
            g.setColour (s.warn);
            g.strokePath (arc, juce::PathStrokeType (2.0f));
            auto ang = angleOf (needle[(size_t) ch]);
            g.setColour (s.accent);
            g.drawLine (cx, cy, cx + std::sin (ang) * radius * 0.98f, cy - std::cos (ang) * radius * 0.98f, 1.4f);
            draw::label (g, ch == 0 ? "VU L" : "VU R", face.getX(), face.getY(), s.dim, 8.0f);
        }
    }
};

//============================================================================== VECTOR (goniometer)
struct VectorVisual : Visual
{
    std::vector<float> l, rr;
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto area = r.reduced (6);
        auto corrBar = area.removeFromBottom (14);
        auto size = jmin (area.getWidth(), area.getHeight());
        auto box = Rect (size, size).withCentre (area.getCentre());
        g.setColour (s.grid);
        g.drawEllipse (box, 1.0f);
        g.drawLine (box.getCentreX(), box.getY(), box.getCentreX(), box.getBottom(), 1.0f);
        g.drawLine (box.getX(), box.getCentreY(), box.getRight(), box.getCentreY(), 1.0f);
        draw::label (g, "L", box.getX() + size * 0.15f, box.getY() + size * 0.08f, s.dim, 8.0f);
        draw::label (g, "R", box.getRight() - size * 0.15f - 6, box.getY() + size * 0.08f, s.dim, 8.0f);

        const int n = 2048;
        l.resize (n); rr.resize (n);
        c.a.copyHistory (0, l.data(), n);
        c.a.copyHistory (1, rr.data(), n);
        auto half = size * 0.5f;
        float mx = 1.0e-4f;
        for (int i = 0; i < n; ++i) mx = jmax (mx, std::abs (l[(size_t) i]), std::abs (rr[(size_t) i]));
        auto gain = 0.7f / jmax (0.05f, mx);   // auto-scale so quiet material is still visible
        for (int i = 0; i < n; i += 2)
        {
            auto side = (l[(size_t) i] - rr[(size_t) i]) * 0.7071f * gain, mid = (l[(size_t) i] + rr[(size_t) i]) * 0.7071f * gain;
            auto x = box.getCentreX() - side * half, y = box.getCentreY() - mid * half;
            g.setColour (s.line2.interpolatedWith (s.line, (float) i / n).withAlpha (0.12f + 0.6f * (float) i / n));
            g.fillRect (x, y, 1.3f, 1.3f);
        }

        // correlation
        auto bar = corrBar.reduced (0, 4).withTrimmedRight (40);
        g.setColour (s.grid);
        g.fillRect (bar);
        auto cx = bar.getCentreX();
        auto x = cx + bar.getWidth() * 0.5f * jlimit (-1.0f, 1.0f, c.a.correlation);
        g.setColour (c.a.correlation < 0 ? s.warn : s.good);
        g.fillRect (Rect::leftTopRightBottom (jmin (cx, x), bar.getY(), jmax (cx, x), bar.getBottom()));
        draw::label (g, (c.a.correlation >= 0 ? "+" : "") + juce::String (c.a.correlation, 2), corrBar.getRight(), corrBar.getY() + 1, s.text, 8.5f,
                     juce::Justification::topRight, 40);
    }
};

//============================================================================== SCOPE
struct ScopeVisual : Visual
{
    std::vector<float> l, rr;
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& a = c.a;
        auto area = r.reduced (6, 6).withTrimmedBottom (12);
        for (int i = 1; i < 8; ++i)
        {
            g.setColour (s.grid);
            g.drawVerticalLine ((int) (area.getX() + area.getWidth() * i / 8.0f), area.getY(), area.getBottom());
        }
        for (int i = 1; i < 4; ++i)
            g.drawHorizontalLine ((int) (area.getY() + area.getHeight() * i / 4.0f), area.getX(), area.getRight());

        // window: about 3 periods of the detected pitch, else 20 ms
        auto srate = a.getSampleRate();
        auto span = a.tunerMidi >= 0 && a.tunerHz > 20 ? 3.0 / a.tunerHz : 0.02;
        auto n = jlimit (64, 8192, (int) (span * srate));
        const int look = n + (int) (srate / 20.0);
        l.resize ((size_t) look); rr.resize ((size_t) look);
        a.copyHistory (0, l.data(), look);
        a.copyHistory (1, rr.data(), look);
        // trigger on the newest rising zero crossing that leaves a full window after it
        int start = look - n;
        for (int i = look - n - 1; i > 1; --i)
        {
            auto m0 = l[(size_t) i - 1] + rr[(size_t) i - 1], m1 = l[(size_t) i] + rr[(size_t) i];
            if (m0 < 0 && m1 >= 0) { start = i; break; }
        }
        float mx = 1.0e-3f;
        for (int i = start; i < start + n; ++i) mx = jmax (mx, std::abs (l[(size_t) i]), std::abs (rr[(size_t) i]));
        auto gain = 0.9f / jmax (0.02f, mx);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& src = ch == 0 ? l : rr;
            Path p;
            auto stepPx = jmax (1, n / (int) jmax (1.0f, area.getWidth() * 2));
            for (int i = 0; i < n; i += stepPx)
            {
                auto x = area.getX() + area.getWidth() * (float) i / (float) n;
                auto y = area.getCentreY() - src[(size_t) (start + i)] * gain * area.getHeight() * 0.5f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour ((ch == 0 ? s.line : s.line2).withAlpha (ch == 0 ? 0.95f : 0.7f));
            g.strokePath (p, juce::PathStrokeType (1.2f));
        }
        auto msDiv = span * 1000.0 / 8.0;
        auto info = juce::String (msDiv, 1) + " MS/DIV";
        if (a.tunerMidi >= 0) info << "  " << juce::String (a.tunerHz, 1) << " HZ";
        draw::label (g, info, area.getRight(), area.getBottom() + 1, s.dim, 8.0f, juce::Justification::topRight, 200);
    }
};

//============================================================================== TUNER
struct TunerVisual : Visual
{
    float cents = 0;
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& a = c.a;
        auto area = r.reduced (8);
        bool has = a.tunerMidi >= 0 && a.tunerClarity > 0.5f;
        cents += ((has ? a.tunerCents : 0.0f) - cents) * 0.3f;
        auto big = jlimit (16.0f, 64.0f, area.getHeight() * 0.42f);
        auto inTune = has && std::abs (cents) < 5.0f;
        g.setColour (has ? (inTune ? s.good : s.text) : s.dim);
        g.setFont (monoFont (big, true));
        auto top = area.removeFromTop (big + 4);
        g.drawText (has ? music::noteNameWithOctave (a.tunerMidi, false) : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")), top, juce::Justification::centred);

        auto strip = area.removeFromTop (jmin (22.0f, area.getHeight() * 0.5f)).reduced (6, 2);
        for (int i = -50; i <= 50; i += 10)
        {
            auto x = strip.getX() + strip.getWidth() * (i + 50) / 100.0f;
            g.setColour (i == 0 ? s.text : s.dim);
            g.drawVerticalLine ((int) x, strip.getY() + (i == 0 ? 0 : strip.getHeight() * 0.35f), strip.getBottom());
        }
        if (has)
        {
            auto x = strip.getX() + strip.getWidth() * (jlimit (-50.0f, 50.0f, cents) + 50) / 100.0f;
            g.setColour (inTune ? s.good : s.accent);
            g.fillRoundedRectangle (x - 2, strip.getY() - 2, 4, strip.getHeight() + 4, 2);
        }
        auto info = has ? juce::String (a.tunerHz, 1) + " HZ   " + (cents >= 0 ? "+" : "") + juce::String ((int) std::round (cents)) + " CT"
                        : juce::String ("NO PITCH");
        draw::label (g, info, area.getCentreX(), area.getY() + 4, s.dim, 9.0f, juce::Justification::centredTop, 240);
    }
};

//============================================================================== STARMAP (constellation)
struct StarmapVisual : Visual
{
    struct Star { float x, y, size; int band; float glow = 0; };
    std::vector<Star> stars;
    StarmapVisual()
    {
        juce::Random rnd (7);
        for (int i = 0; i < 72; ++i)
            stars.push_back ({ rnd.nextFloat(), rnd.nextFloat(), 0.6f + rnd.nextFloat() * 1.6f, rnd.nextInt (Analyzer::numLogBands - 10) + 4 });
    }
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto area = r.reduced (8);
        for (auto& st : stars)
        {
            auto v = jlimit (0.0f, 1.0f, (tilt (c.a.logDb[(size_t) st.band], Analyzer::logBandFrequency ((float) st.band)) + 66.0f) / 54.0f);
            st.glow += (v * v - st.glow) * (v * v > st.glow ? 0.5f : 0.07f);
        }
        // links between bright neighbours
        for (size_t i = 0; i < stars.size(); ++i)
            for (size_t j = i + 1; j < stars.size(); ++j)
            {
                auto& p = stars[i]; auto& q = stars[j];
                auto dx = (p.x - q.x) * area.getWidth(), dy = (p.y - q.y) * area.getHeight();
                auto d2 = dx * dx + dy * dy;
                auto lim = area.getWidth() * area.getHeight() * 0.025f;
                auto link = jmin (p.glow, q.glow);
                if (d2 < lim && link > 0.25f)
                {
                    g.setColour (s.line2.withAlpha ((link - 0.25f) * 0.8f));
                    g.drawLine (area.getX() + p.x * area.getWidth(), area.getY() + p.y * area.getHeight(),
                                area.getX() + q.x * area.getWidth(), area.getY() + q.y * area.getHeight(), 0.7f);
                }
            }
        for (size_t i = 0; i < stars.size(); ++i)
        {
            auto& st = stars[i];
            auto twinkle = 0.75f + 0.25f * std::sin ((float) c.time * (1.3f + (float) (i % 7)) + (float) i);
            auto x = area.getX() + st.x * area.getWidth(), y = area.getY() + st.y * area.getHeight();
            auto rad = st.size * (1.0f + st.glow * 2.2f);
            g.setColour (s.line.withAlpha (jlimit (0.2f, 1.0f, 0.25f + st.glow * twinkle)));
            g.fillEllipse (x - rad * 0.5f, y - rad * 0.5f, rad, rad);
            if (st.glow > 0.5f)
            {
                g.setColour (s.accent.withAlpha ((st.glow - 0.5f) * 0.5f));
                g.drawEllipse (x - rad * 1.6f, y - rad * 1.6f, rad * 3.2f, rad * 3.2f, 0.6f);
            }
        }
    }
};

//============================================================================== ROSETTE (mandala)
struct RosetteVisual : Visual
{
    float rotation = 0;
    std::array<std::array<float, 48>, 4> layers {};
    int frameNo = 0;
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& a = c.a;
        rotation += 0.002f + a.energy * 0.012f;
        if (++frameNo % 6 == 0) { std::rotate (layers.rbegin(), layers.rbegin() + 1, layers.rend()); }
        for (int i = 0; i < 48; ++i)
        {
            auto b = (int) juce::jmap ((float) i, 0.0f, 47.0f, 4.0f, (float) Analyzer::numLogBands - 8);
            layers[0][(size_t) i] = jlimit (0.0f, 1.0f, (tilt (a.logDb[(size_t) b], Analyzer::logBandFrequency ((float) b)) + 70.0f) / 60.0f);
        }
        auto ctr = r.getCentre();
        auto radius = jmin (r.getWidth(), r.getHeight()) * 0.46f;
        const int folds = 8;
        for (int L = 3; L >= 0; --L)
        {
            Path p;
            const int pts = folds * 48;
            for (int i = 0; i <= pts; ++i)
            {
                auto seg = i % 96;
                auto idx = seg < 48 ? seg : 95 - seg;   // mirror inside each petal pair
                auto v = layers[(size_t) L][(size_t) idx];
                auto ang = rotation * (L % 2 == 0 ? 1.0f : -0.7f) + twoPi * (float) i / (float) pts;
                auto rad = radius * (0.18f + 0.12f * L + v * (0.55f - 0.08f * L));
                auto pt = ctr + juce::Point<float> (std::cos (ang), std::sin (ang)) * rad;
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            p.closeSubPath();
            auto col = L == 0 ? s.line : L == 1 ? s.line2 : s.accent;
            g.setColour (col.withAlpha (L == 0 ? 0.14f : 0.06f));
            g.fillPath (p);
            g.setColour (col.withAlpha (0.85f - L * 0.18f));
            g.strokePath (p, juce::PathStrokeType (L == 0 ? 1.2f : 0.8f));
        }
        g.setColour (s.accent.withAlpha (0.4f + a.beatPulse * 0.6f));
        auto core = radius * (0.06f + a.energyLow * 0.08f);
        g.fillEllipse (Rect (core * 2, core * 2).withCentre (ctr));
    }
};

//============================================================================== RIDGES (stacked spectra)
struct RidgesVisual : Visual
{
    static constexpr int rows = 28, pts = 96;
    std::array<std::array<float, pts>, rows> hist {};
    int frameNo = 0, write = 0;
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        if (++frameNo % 3 == 0)
        {
            for (int i = 0; i < pts; ++i)
            {
                auto b = (int) juce::jmap ((float) i, 0.0f, (float) pts - 1, 2.0f, (float) Analyzer::numLogBands - 4);
                hist[(size_t) write][(size_t) i] = jlimit (0.0f, 1.0f, (tilt (c.a.logDb[(size_t) b], Analyzer::logBandFrequency ((float) b)) + 70.0f) / 60.0f);
            }
            write = (write + 1) % rows;
        }
        auto area = r.reduced (10, 6);
        auto rowGap = area.getHeight() / (float) (rows + 6);
        for (int k = 0; k < rows; ++k)   // oldest (top) to newest (bottom)
        {
            auto& row = hist[(size_t) ((write + k) % rows)];
            auto base = area.getY() + rowGap * (k + 6);
            auto inset = area.getWidth() * 0.12f * (1.0f - (float) k / rows);
            Path p, f;
            for (int i = 0; i < pts; ++i)
            {
                auto x = area.getX() + inset + (area.getWidth() - 2 * inset) * (float) i / (float) (pts - 1);
                auto shape = std::sin (juce::MathConstants<float>::pi * (float) i / (float) (pts - 1));
                auto y = base - row[(size_t) i] * rowGap * 5.0f * (0.3f + 0.7f * shape);
                if (i == 0) { p.startNewSubPath (x, y); f.startNewSubPath (x, base + 1); }
                p.lineTo (x, y); f.lineTo (x, y);
            }
            f.lineTo (area.getRight() - inset, base + 1);
            f.closeSubPath();
            g.setColour (s.panel);
            g.fillPath (f);
            auto t = (float) k / (float) rows;
            g.setColour (s.line2.interpolatedWith (s.line, t).withAlpha (0.25f + 0.75f * t));
            g.strokePath (p, juce::PathStrokeType (0.9f + t * 0.5f));
        }
    }
};

//============================================================================== RIPPLES
struct RipplesVisual : Visual
{
    struct Ring { float age = 0, strength = 1; int kind = 0; float angle = 0; };
    std::vector<Ring> rings;
    juce::int64 lastSeen = 0;
    juce::Random rnd;
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        for (auto& h : c.a.hits)
            if (h.time > lastSeen) rings.push_back ({ 0.0f, h.strength, h.kind, rnd.nextFloat() * twoPi });
        if (! c.a.hits.empty()) lastSeen = c.a.hits.back().time;
        auto ctr = r.getCentre();
        auto maxR = std::hypot (r.getWidth(), r.getHeight()) * 0.5f;
        for (auto& ring : rings)
        {
            ring.age += ring.kind == 2 ? 0.035f : 0.016f;
            auto t = ring.age;
            auto alpha = jlimit (0.0f, 1.0f, (1.0f - t) * ring.strength);
            if (ring.kind == 0)
            {
                auto rad = maxR * (0.05f + t * 0.95f);
                g.setColour (s.line2.withAlpha (alpha));
                g.drawEllipse (Rect (rad * 2, rad * 2).withCentre (ctr), 1.0f + 3.0f * (1 - t));
            }
            else if (ring.kind == 1)
            {
                auto rad = maxR * (0.03f + t * 0.6f);
                g.setColour (s.accent.withAlpha (alpha));
                Path p;
                p.addCentredArc (ctr.x, ctr.y, rad, rad, ring.angle, 0, juce::MathConstants<float>::pi * 1.3f, true);
                g.strokePath (p, juce::PathStrokeType (1.6f));
            }
            else
            {
                auto rad = maxR * (0.15f + t * 0.5f);
                auto p = ctr + juce::Point<float> (std::cos (ring.angle), std::sin (ring.angle)) * rad;
                g.setColour (s.line.withAlpha (alpha));
                g.fillEllipse (Rect (2.5f, 2.5f).withCentre (p));
            }
        }
        rings.erase (std::remove_if (rings.begin(), rings.end(), [] (const Ring& x) { return x.age >= 1.0f; }), rings.end());
        if (rings.size() > 200) rings.erase (rings.begin(), rings.begin() + (long) (rings.size() - 200));
        auto core = 4.0f + c.a.energy * jmin (r.getWidth(), r.getHeight()) * 0.12f;
        g.setColour (s.line.withAlpha (0.85f));
        g.fillEllipse (Rect (core, core).withCentre (ctr));
    }
};

//============================================================================== NUCLEUS (orb)
struct NucleusVisual : Visual
{
    std::array<float, 64> shape {};
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& a = c.a;
        auto ctr = r.getCentre();
        auto radius = jmin (r.getWidth(), r.getHeight()) * (0.26f + a.energyLow * 0.1f);
        for (int i = 0; i < 64; ++i)
        {
            auto idx = i < 32 ? i : 63 - i;
            auto b = (int) juce::jmap ((float) idx, 0.0f, 31.0f, 6.0f, (float) Analyzer::numLogBands - 10);
            auto v = jlimit (0.0f, 1.0f, (tilt (a.logDb[(size_t) b], Analyzer::logBandFrequency ((float) b)) + 66.0f) / 56.0f);
            shape[(size_t) i] += (v - shape[(size_t) i]) * 0.25f;
        }
        auto t = (float) c.time;
        auto blob = [&] (float scale, float wobble)
        {
            Path p;
            for (int i = 0; i <= 128; ++i)
            {
                auto ang = twoPi * (float) i / 128.0f;
                auto fi = (float) i / 2.0f;
                auto i0 = (int) fi % 64, i1 = (i0 + 1) % 64;
                auto v = shape[(size_t) i0] + (shape[(size_t) i1] - shape[(size_t) i0]) * (fi - std::floor (fi));
                auto rad = radius * scale * (1.0f + v * 0.45f + wobble * std::sin (ang * 3 + t * 1.1f) * 0.04f);
                auto pt = ctr + juce::Point<float> (std::cos (ang + t * 0.15f), std::sin (ang + t * 0.15f)) * rad;
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            p.closeSubPath();
            return p;
        };
        auto outer = blob (1.25f, 1.0f);
        g.setColour (s.line2.withAlpha (0.12f));
        g.fillPath (outer);
        auto body = blob (1.0f, 0.6f);
        g.setGradientFill (juce::ColourGradient (s.line.withAlpha (0.95f), ctr.x - radius * 0.3f, ctr.y - radius * 0.4f,
                                                 s.line2.withAlpha (0.5f), ctr.x + radius, ctr.y + radius, true));
        g.fillPath (body);
        g.setColour (s.accent.withAlpha (0.5f + a.hitFlash[0] * 0.5f));
        g.strokePath (body, juce::PathStrokeType (1.0f + a.hitFlash[0] * 2.0f));
    }
};

//============================================================================== ATTRACTOR
struct AttractorVisual : Visual
{
    juce::Image img;
    float x = 0.1f, y = 0.1f, pa = 1.4f, pb = -2.3f, pc = 2.4f, pd = -2.1f;
    juce::String skinName;
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& a = c.a;
        auto w = jmax (8, (int) r.getWidth()), h = jmax (8, (int) r.getHeight());
        if (! img.isValid() || img.getWidth() != w || img.getHeight() != h || skinName != s.name)
        {
            img = juce::Image (juce::Image::ARGB, w, h, true);
            skinName = s.name;
        }
        // Peter de Jong attractor; parameters drift around a chaotic region with the music
        auto tm = (float) c.time;
        auto ta = 1.4f + 0.12f * std::sin (tm * 0.05f) + (a.energyLow - 0.4f) * 0.15f;
        auto tb = -2.3f + 0.12f * std::cos (tm * 0.037f) + (a.energyHigh - 0.3f) * 0.15f;
        auto tc = 2.4f + 0.12f * std::sin (tm * 0.023f + 1.0f) + (a.energyMid - 0.4f) * 0.15f;
        auto td = -2.1f + 0.12f * std::cos (tm * 0.031f + 2.0f);
        pa += (ta - pa) * 0.05f; pb += (tb - pb) * 0.05f; pc += (tc - pc) * 0.05f; pd += (td - pd) * 0.05f;
        {
            juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
            // fade
            for (int yy = 0; yy < h; ++yy)
            {
                auto* row = bd.getLinePointer (yy);
                for (int xx = 0; xx < w; ++xx)
                {
                    auto* px = row + xx * bd.pixelStride;
                    for (int k = 0; k < 4; ++k) px[k] = (juce::uint8) (px[k] * 0.86f);
                }
            }
            auto col = s.line.interpolatedWith (s.accent, a.hitFlash[0]);
            const int iters = 7000 + (int) (a.energy * 9000);
            for (int i = 0; i < iters; ++i)
            {
                auto nx = std::sin (pa * y) - std::cos (pb * x);
                auto ny = std::sin (pc * x) - std::cos (pd * y);
                x = nx; y = ny;
                auto scale = juce::jmin ((float) w, (float) h * 1.4f) * 0.23f;
                auto px = (int) ((float) w * 0.5f + x * scale);
                auto py = (int) ((float) h * 0.5f + y * scale * (float) h / juce::jmax (1.0f, juce::jmin ((float) w, (float) h * 1.4f)));
                if (px < 0 || py < 0 || px >= w || py >= h) continue;
                auto cur = bd.getPixelColour (px, py);
                auto alpha = jmin (1.0f, cur.getFloatAlpha() + 0.12f);
                bd.setPixelColour (px, py, col.withAlpha (alpha));
            }
        }
        g.drawImageAt (img, (int) r.getX(), (int) r.getY());
    }
};

//============================================================================== PLATE (annotated spectrum)
struct PlateVisual : Visual
{
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& a = c.a;
        auto area = r.reduced (8, 6);
        auto cardW = jmin (130.0f, area.getWidth() * 0.3f);
        auto card = area.removeFromRight (cardW);
        auto plot = area.withTrimmedRight (8).withTrimmedTop (14).withTrimmedBottom (12);
        draw::label (g, juce::String ("PLATE ") + juce::String (juce::CharPointer_UTF8 ("\xe2\x84\x96 ")) + juce::String (jmax (1, (int) std::floor (a.barTime) + 1)).paddedLeft ('0', 3)
                         + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  SPECTRUM, L / R")),
                     area.getX(), area.getY(), s.text, 8.5f, juce::Justification::topLeft, 400);
        frequencyGrid (g, plot.withHeight (plot.getHeight() + 12), s, true);
        auto yOf = [plot] (float db) { return plot.getY() + plot.getHeight() * jlimit (0.0f, 1.0f, -(db + 6.0f) / 80.0f); };

        auto curve = [&] (const std::vector<float>& bins, Colour col, float thick)
        {
            Path p;
            for (int b = 0; b < Analyzer::numLogBands; b += 2)
            {
                auto hz = Analyzer::logBandFrequency ((float) b);
                auto k = jlimit (1, Analyzer::numBins - 1, (int) std::round (hz / a.binFrequency (1)));
                auto x = draw::freqToX (hz, plot), y = yOf (tilt (bins[(size_t) k], hz));
                if (b == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (col);
            g.strokePath (p, juce::PathStrokeType (thick));
        };
        curve (a.leftDb, s.line, 1.1f);
        curve (a.rightDb, s.line2.withAlpha (0.8f), 0.9f);

        // annotations
        auto callout = [&] (float hz, float db, const juce::String& text, Colour col, int lane)
        {
            auto x = draw::freqToX (hz, plot), y = yOf (tilt (db, hz));
            auto ty = plot.getY() + 4 + lane * 12.0f;
            g.setColour (col.withAlpha (0.8f));
            g.drawLine (x, y - 2, x, ty + 10, 0.7f);
            g.fillEllipse (x - 2, y - 2, 4, 4);
            draw::label (g, text, x + 3, ty, col, 8.0f);
        };
        auto peakIn = [&] (float lo, float hi, float& outHz)
        {
            float best = -200;
            for (int k = jmax (1, (int) (lo / a.binFrequency (1))); k <= jmin (Analyzer::numBins - 1, (int) (hi / a.binFrequency (1))); ++k)
                if (a.spectrumDb[(size_t) k] > best) { best = a.spectrumDb[(size_t) k]; outHz = a.binFrequency (k); }
            return best;
        };
        float hz = 60;
        auto kdb = peakIn (35, 120, hz);
        if (kdb > -70) callout (hz, kdb, "KICK " + draw::hzText (hz), a.hitFlash[0] > 0.3f ? s.accent : s.text, 0);
        auto hdb = peakIn (6000, 14000, hz);
        if (hdb > -80) callout (hz, hdb, "HATS", a.hitFlash[2] > 0.3f ? s.accent : s.text, 0);
        if (a.chordRoot >= 0 || a.bassMidi >= 0)
        {
            auto ndb = peakIn (180, 1200, hz);
            if (ndb > -70)
            {
                auto midi = (int) std::round (69.0f + 12.0f * std::log2 (hz / 440.0f));
                callout (hz, ndb, "NOTE " + music::noteNameWithOctave (midi, music::keyUsesFlats (a.key)), s.line2, 1);
            }
        }

        // specimen card
        g.setColour (s.grid);
        g.drawRect (card, 1.0f);
        auto row = [&, y = card.getY() + 6] (const juce::String& k, const juce::String& v) mutable
        {
            draw::label (g, k, card.getX() + 7, y, s.dim, 8.0f);
            draw::label (g, v, card.getRight() - 7, y, s.text, 9.5f, juce::Justification::topRight, cardW);
            y += jmax (13.0f, (card.getHeight() - 12) / 4.0f);
        };
        row ("TEMPO", a.bpm > 0 ? juce::String (a.bpm, 1) : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")));
        row ("KEY", music::keyName (a.key));
        row ("LUFS", draw::lufsText (c.loud.integrated > -70 ? c.loud.integrated : c.loud.shortTerm));
        row ("BAR", barBeatText (a));
    }
};

//============================================================================== BAR CLOCK (one turn per bar)
struct BarClockVisual : Visual
{
    static constexpr int slots = 96;
    struct Slot { int root = -1, type = 0, bass = -1; };
    std::array<Slot, slots> cur, prev;
    juce::int64 bar = -1;
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& a = c.a;
        auto ctr = r.getCentre();
        auto radius = jmin (r.getWidth(), r.getHeight()) * 0.44f;
        g.setColour (s.grid);
        g.drawEllipse (Rect (radius * 2, radius * 2).withCentre (ctr), 1.0f);
        for (float f : { 0.45f, 0.62f, 0.8f })
            g.drawEllipse (Rect (radius * 2 * f, radius * 2 * f).withCentre (ctr), 0.6f);
        auto beats = jmax (1, a.beatsPerBar);
        for (int i = 0; i < 16; ++i)
        {
            auto ang = twoPi * (float) i / 16.0f;
            auto len = i % (16 / jmin (16, beats)) == 0 ? 0.12f : 0.05f;
            auto dir = juce::Point<float> (std::sin (ang), -std::cos (ang));
            g.setColour (i == 0 ? s.text : s.dim);
            g.drawLine ({ ctr + dir * radius * (1 - len), ctr + dir * radius }, i == 0 ? 1.6f : 0.8f);
        }
        if (a.barTime < 0)
        {
            draw::label (g, "WAITING FOR TEMPO", ctr.x, ctr.y - 5, s.dim, 8.5f, juce::Justification::centredTop, 200);
            return;
        }
        auto frac = (float) (a.barTime - std::floor (a.barTime));
        auto barIdx = (juce::int64) std::floor (a.barTime);
        if (barIdx > bar || barIdx < bar - 1) { prev = barIdx == bar + 1 ? cur : std::array<Slot, slots> {}; cur = {}; bar = barIdx; }
        auto slot = jlimit (0, slots - 1, (int) (frac * slots));
        cur[(size_t) slot] = { a.chordRoot, a.chordType, a.bassMidi >= 0 ? a.bassMidi % 12 : -1 };

        // chord (outer) and bass (inner) arcs for the bar so far + rest of the previous bar
        auto arcs = [&] (float rad, bool bass, Colour col)
        {
            int startSlot = 0;
            auto valueAt = [&] (int i) { auto& sl = i <= slot ? cur[(size_t) i] : prev[(size_t) i]; return bass ? sl.bass : sl.root * 16 + sl.type; };
            for (int i = 1; i <= slots; ++i)
            {
                if (i < slots && valueAt (i) == valueAt (startSlot) && i != slot + 1) continue;
                auto v = valueAt (startSlot);
                if (v >= 0 && i - startSlot > 1)
                {
                    auto a0 = twoPi * (float) startSlot / slots, a1 = twoPi * (float) i / slots - 0.02f;
                    Path p;
                    p.addCentredArc (ctr.x, ctr.y, rad, rad, 0, a0, a1, true);
                    auto old = startSlot > slot;
                    g.setColour (col.withAlpha (old ? 0.25f : 0.85f));
                    g.strokePath (p, juce::PathStrokeType (bass ? 3.0f : 4.0f));
                    if (i - startSlot > 8 && radius > 50)
                    {
                        auto mid = (a0 + a1) * 0.5f;
                        auto txt = bass ? music::noteName (v, music::keyUsesFlats (a.key))
                                        : music::chordName (v / 16, v % 16, -1, music::keyUsesFlats (a.key));
                        auto p2 = ctr + juce::Point<float> (std::sin (mid), -std::cos (mid)) * (rad + (bass ? -10.0f : 12.0f));
                        draw::label (g, txt, p2.x, p2.y - 5, col.withAlpha (old ? 0.4f : 1.0f), 8.0f, juce::Justification::centredTop, 60);
                    }
                }
                startSlot = i;
            }
        };
        arcs (radius * 0.92f, false, s.line2);
        arcs (radius * 0.36f, true, s.accent);

        // hits of the last bars
        for (auto& h : a.hits)
        {
            if (h.barTime < 0) continue;
            auto age = a.barTime - h.barTime;
            if (age > 4.0 || age < 0) continue;
            auto ang = twoPi * (float) (h.barTime - std::floor (h.barTime));
            auto ringR = radius * (h.kind == 0 ? 0.45f : h.kind == 1 ? 0.62f : 0.8f);
            auto dir = juce::Point<float> (std::sin (ang), -std::cos (ang));
            auto alpha = (float) (1.0 - age / 4.0);
            auto size = (h.kind == 2 ? 3.0f : 5.0f) * (0.6f + h.strength * 0.6f);
            g.setColour ((h.kind == 0 ? s.line : h.kind == 1 ? s.accent : s.line2).withAlpha (alpha));
            g.fillEllipse (Rect (size, size).withCentre (ctr + dir * ringR));
        }
        // needle
        auto ang = twoPi * frac;
        auto dir = juce::Point<float> (std::sin (ang), -std::cos (ang));
        g.setColour (s.text);
        g.drawLine ({ ctr, ctr + dir * radius }, 1.3f);
        g.fillEllipse (Rect (5, 5).withCentre (ctr));
        draw::label (g, barBeatText (a), r.getX() + 6, r.getY() + 4, s.dim, 8.5f);
        draw::label (g, a.bpm > 0 ? juce::String (a.bpm, 1) + " BPM" : juce::String(), r.getRight() - 6, r.getY() + 4, s.dim, 8.5f, juce::Justification::topRight, 100);
    }
};

//============================================================================== STEPS (16 x 9 glyph grid)
struct StepsVisual : Visual
{
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& a = c.a;
        auto area = r.reduced (8, 6);
        auto drums = area.removeFromBottom (jmin (36.0f, area.getHeight() * 0.3f));
        area.removeFromBottom (4);
        auto cw = area.getWidth() / Analyzer::gridSteps, ch = area.getHeight() / Analyzer::numOctaves;
        auto step = a.barTime >= 0 ? (int) ((a.barTime - std::floor (a.barTime)) * Analyzer::gridSteps) : -1;
        static const char* glyphs[] = { "\xc2\xb7", ":", "+", "*", "#", "@" };
        g.setFont (monoFont (jlimit (7.0f, 16.0f, jmin (cw, ch) * 0.9f)));
        for (int st = 0; st < Analyzer::gridSteps; ++st)
        {
            auto useCur = st <= step;
            auto& grid = useCur ? a.currentBar : a.previousBar;
            if (st == step)
            {
                g.setColour (s.line2.withAlpha (0.12f));
                g.fillRect (area.getX() + st * cw, area.getY(), cw, area.getHeight() + drums.getHeight() + 4);
            }
            for (int o = 0; o < Analyzer::numOctaves; ++o)
            {
                auto v = grid.energy[(size_t) st][(size_t) o];
                auto gi = jlimit (0, 5, (int) (v * 6.0f));
                g.setColour ((o < 3 ? s.line2 : o < 6 ? s.line : s.accent).withAlpha ((useCur ? 0.35f : 0.12f) + v * (useCur ? 0.65f : 0.2f)));
                g.drawText (juce::CharPointer_UTF8 (glyphs[gi]), Rect (area.getX() + st * cw, area.getY() + (Analyzer::numOctaves - 1 - o) * ch, cw, ch),
                            juce::Justification::centred, false);
            }
            // drum pattern rows: hat, snare, kick
            auto bits = (useCur ? a.currentBar : a.previousBar).hits[(size_t) st];
            for (int k = 0; k < 3; ++k)
            {
                auto rowH = drums.getHeight() / 3.0f;
                auto cell = Rect (drums.getX() + st * cw, drums.getY() + (2 - k) * rowH, cw, rowH).reduced (cw * 0.18f, rowH * 0.18f);
                auto on = (bits >> k) & 1;
                auto col = k == 0 ? s.line : k == 1 ? s.accent : s.line2;
                g.setColour (on ? col.withAlpha (useCur ? 1.0f : 0.35f) : s.grid);
                if (on) g.fillRect (cell); else g.drawRect (cell, 0.6f);
            }
        }
        if (cw > 12)
        {
            draw::label (g, "H", drums.getX() - 1, drums.getY() - 1, s.dim, 7.0f);
        }
    }
};

//============================================================================== LOW END (kick vs bass)
struct LowEndVisual : Visual
{
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& a = c.a;
        auto area = r.reduced (8, 6);
        auto info = area.removeFromTop (12);
        auto envs = area.removeFromBottom (area.getHeight() * 0.32f);
        auto plot = area.withTrimmedBottom (12).withTrimmedTop (2);
        auto binHz = a.binFrequency (1);
        auto xOf = [&] (int k) { return draw::freqToX (jmax (20.0f, k * binHz), plot, 20.0f, Analyzer::lowBins * binHz); };
        auto yOf = [&] (float db) { return plot.getBottom() - plot.getHeight() * jlimit (0.0f, 1.0f, (db + 72.0f) / 66.0f); };
        for (float f : { 40.0f, 60.0f, 100.0f, 150.0f, 200.0f, 300.0f })
        {
            auto x = draw::freqToX (f, plot, 20.0f, Analyzer::lowBins * binHz);
            g.setColour (s.grid);
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            draw::label (g, draw::hzText (f), x, plot.getBottom() + 1, s.dim, 7.5f, juce::Justification::centredTop, 30);
        }
        Path kp, bp, overlap;
        bool first = true;
        std::vector<juce::Point<float>> lower;
        for (int k = 1; k < Analyzer::lowBins; ++k)
        {
            auto x = xOf (k);
            auto yk = yOf (a.kickSpectrum[(size_t) k]), yb = yOf (a.bassSpectrum[(size_t) k]);
            if (first) { kp.startNewSubPath (x, yk); bp.startNewSubPath (x, yb); overlap.startNewSubPath (x, plot.getBottom()); first = false; }
            else { kp.lineTo (x, yk); bp.lineTo (x, yb); }
            overlap.lineTo (x, jmax (yk, yb));   // the lower curve = shared energy
        }
        overlap.lineTo (xOf (Analyzer::lowBins - 1), plot.getBottom());
        overlap.closeSubPath();
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (overlap);
            g.setColour (s.warn.withAlpha (0.3f));
            for (float x = plot.getX() - plot.getHeight(); x < plot.getRight(); x += 6)
                g.drawLine (x, plot.getBottom(), x + plot.getHeight(), plot.getY(), 0.8f);
        }
        g.setColour (s.accent); g.strokePath (kp, juce::PathStrokeType (1.4f));
        g.setColour (s.line2); g.strokePath (bp, juce::PathStrokeType (1.4f));

        // envelopes for the last two bars
        auto hopSec = 1024.0 / 48000.0;
        auto twoBars = a.bpm > 0 ? 2.0 * a.beatsPerBar * 60.0 / a.bpm : 4.0;
        auto n = jlimit (16, Analyzer::envLength, (int) (twoBars / hopSec));
        auto env = [&] (const std::array<float, Analyzer::envLength>& e, Colour col)
        {
            Path p;
            for (int i = 0; i < n; ++i)
            {
                auto v = e[(size_t) ((a.envWrite - n + i + Analyzer::envLength) % Analyzer::envLength)];
                auto x = envs.getX() + envs.getWidth() * (float) i / (float) (n - 1);
                auto y = envs.getBottom() - envs.getHeight() * v;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (col);
            g.strokePath (p, juce::PathStrokeType (1.0f));
        };
        g.setColour (s.grid);
        g.drawRect (envs, 0.8f);
        env (a.kickEnv, s.accent);
        env (a.bassEnv, s.line2);

        auto peakHz = [&] (const std::array<float, Analyzer::lowBins>& sp)
        {
            int best = 1;
            for (int k = 2; k < Analyzer::lowBins; ++k) if (sp[(size_t) k] > sp[(size_t) best]) best = k;
            return best * binHz;
        };
        auto dot = juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
        draw::label (g, "KICK " + draw::hzText (peakHz (a.kickSpectrum)) + dot + "BASS " + (a.bassMidi >= 0 ? music::noteNameWithOctave (a.bassMidi, music::keyUsesFlats (a.key)) : juce::String ("-"))
                         + dot + "DUCK " + juce::String (a.duckDb, 1) + " DB",
                     info.getX(), info.getY(), s.text, 8.5f, juce::Justification::topLeft, info.getWidth());
    }
};

//============================================================================== WIDTH (per 1/3 octave)
struct WidthVisual : Visual
{
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& a = c.a;
        auto area = r.reduced (8, 6);
        auto corr = area.removeFromBottom (14);
        auto plot = area.withTrimmedBottom (12).withTrimmedTop (12);
        auto bw = plot.getWidth() / Analyzer::numThirds;
        auto monoEdge = plot.getX() + bw * (std::log2 (120.0f / 25.0f) * 3.0f + 0.5f);
        bool wideLow = false;
        for (int t = 0; t < Analyzer::numThirds; ++t)
            if (Analyzer::thirdFrequency (t) < 120.0f && a.thirdDb[(size_t) t] > -60.0f && a.thirdWidth[(size_t) t] > 0.3f) wideLow = true;

        g.setColour (wideLow ? s.warn.withAlpha (0.16f) : s.line2.withAlpha (0.06f));
        g.fillRect (Rect::leftTopRightBottom (plot.getX(), plot.getY(), monoEdge, plot.getBottom()));
        draw::label (g, wideLow ? "LOW END WIDE" : "MONO < 120", plot.getX() + 2, plot.getY() - 12, wideLow ? s.warn : s.dim, 8.0f);
        for (int t = 0; t < Analyzer::numThirds; ++t)
        {
            auto w = a.thirdWidth[(size_t) t];
            auto present = jlimit (0.0f, 1.0f, (a.thirdDb[(size_t) t] + 70.0f) / 30.0f);
            auto h = plot.getHeight() * jlimit (0.0f, 1.0f, w * 1.6f);
            auto x = plot.getX() + t * bw;
            auto low = Analyzer::thirdFrequency (t) < 120.0f;
            auto col = low && w > 0.3f ? s.warn : s.line.interpolatedWith (s.line2, jlimit (0.0f, 1.0f, w * 2.0f));
            g.setColour (col.withAlpha (0.25f + 0.75f * present));
            g.fillRect (x + 1, plot.getBottom() - h, bw - 2, h);
            if (t % 3 == 0) draw::label (g, draw::hzText (Analyzer::thirdFrequency (t)), x + bw * 0.5f, plot.getBottom() + 1, s.dim, 7.0f, juce::Justification::centredTop, 30);
        }
        draw::label (g, "WIDTH", plot.getRight(), plot.getY() - 12, s.dim, 8.0f, juce::Justification::topRight, 60);

        auto bar = corr.reduced (0, 3).withTrimmedRight (44).withTrimmedLeft (30);
        draw::label (g, "CORR", corr.getX(), corr.getY() + 1, s.dim, 7.5f);
        g.setColour (s.grid);
        g.fillRect (bar);
        auto cx = bar.getCentreX(), x = cx + bar.getWidth() * 0.5f * jlimit (-1.0f, 1.0f, a.correlation);
        g.setColour (a.correlation < 0 ? s.warn : s.good);
        g.fillRect (Rect::leftTopRightBottom (jmin (cx, x), bar.getY(), jmax (cx, x), bar.getBottom()));
        draw::label (g, (a.correlation >= 0 ? "+" : "") + juce::String (a.correlation, 2), corr.getRight(), corr.getY() + 1, s.text, 8.5f, juce::Justification::topRight, 44);
    }
};

//============================================================================== FIFTHS (circle of fifths)
struct FifthsVisual : Visual
{
    void paint (Graphics& g, Rect r, const VisualContext& c) override
    {
        auto& s = c.skin;
        auto& a = c.a;
        auto area = r.reduced (6);
        auto strip = area.removeFromBottom (jmin (30.0f, area.getHeight() * 0.22f));
        auto ctr = area.getCentre();
        auto radius = jmin (area.getWidth(), area.getHeight()) * 0.48f;
        auto flats = music::keyUsesFlats (a.key);
        for (int i = 0; i < 12; ++i)
        {
            auto pc = (i * 7) % 12;              // C G D A E B F# C# G# D# A# F
            auto minorPc = (pc + 9) % 12;        // relative minor
            auto a0 = twoPi * ((float) i - 0.5f) / 12.0f, a1 = a0 + twoPi / 12.0f;
            auto seg = [&] (float r0, float r1, float v, bool isKey, bool isChord)
            {
                Path p;
                p.addPieSegment (Rect (r1 * 2, r1 * 2).withCentre (ctr), a0 + 0.02f, a1 - 0.02f, r0 / r1);
                g.setColour (s.line2.withAlpha (0.06f + v * 0.7f));
                g.fillPath (p);
                if (isKey) { g.setColour (s.accent); g.strokePath (p, juce::PathStrokeType (1.6f)); }
                else { g.setColour (s.grid); g.strokePath (p, juce::PathStrokeType (0.6f)); }
                if (isChord)
                {
                    auto mid = (a0 + a1) * 0.5f;
                    auto pt = ctr + juce::Point<float> (std::sin (mid), -std::cos (mid)) * ((r0 + r1) * 0.5f);
                    g.setColour (s.text);
                    g.drawEllipse (Rect (r1 - r0, r1 - r0).reduced ((r1 - r0) * 0.18f).withCentre (pt), 1.2f);
                }
            };
            auto suffix = juce::String (music::chordTypes()[(size_t) jlimit (0, (int) music::chordTypes().size() - 1, a.chordType)].suffix);
            auto chordIsMajor = suffix.startsWith ("maj") || ! (suffix.startsWith ("m") || suffix.startsWith ("dim"));
            seg (radius * 0.66f, radius, a.chroma[(size_t) pc], a.key == pc, a.chordRoot == pc && chordIsMajor);
            seg (radius * 0.38f, radius * 0.64f, a.chroma[(size_t) minorPc] * 0.8f, a.key == minorPc + 12, a.chordRoot == minorPc && ! chordIsMajor);
            auto mid = (a0 + a1) * 0.5f;
            auto dir = juce::Point<float> (std::sin (mid), -std::cos (mid));
            auto fs = jlimit (7.0f, 11.0f, radius * 0.09f);
            auto pOut = ctr + dir * radius * 0.83f, pIn = ctr + dir * radius * 0.51f;
            auto sideFlats = i > 6 || (i == 6 && flats);   // F# / Gb at the bottom follows the key
            draw::label (g, music::noteName (pc, sideFlats), pOut.x, pOut.y - fs * 0.6f, s.text, fs, juce::Justification::centredTop, 40);
            draw::label (g, music::noteName (minorPc, i >= 6 || flats) + "m", pIn.x, pIn.y - fs * 0.55f, s.dim, fs * 0.85f, juce::Justification::centredTop, 40);
        }
        draw::label (g, music::keyName (a.key), ctr.x, ctr.y - 12, s.accent, 9.0f, juce::Justification::centredTop, 100);
        draw::label (g, a.chordText, ctr.x, ctr.y + 1, s.text, 10.0f, juce::Justification::centredTop, 100);

        // chord per bar strip
        auto cell = strip.getWidth() / 8.0f;
        for (int i = 0; i < 8; ++i)
        {
            auto& bc = a.chordStrip[(size_t) i];
            auto box = Rect (strip.getX() + i * cell, strip.getY() + 2, cell - 3, strip.getHeight() - 2);
            g.setColour (s.grid);
            g.drawRect (box, 0.7f);
            if (bc.root < 0) continue;
            draw::label (g, music::chordName (bc.root, bc.type, -1, flats), box.getCentreX(), box.getY() + 2, s.text, 8.5f, juce::Justification::centredTop, cell);
            draw::label (g, music::romanNumeral (a.key, bc.root, bc.type), box.getCentreX(), box.getY() + 13, s.accent, 7.5f, juce::Justification::centredTop, cell);
        }
    }
};

//============================================================================== factory
std::unique_ptr<Visual> Visual::create (VisualType t)
{
    switch (t)
    {
        case VisualType::spectrum:    return std::make_unique<SpectrumVisual>();
        case VisualType::spectrogram: return std::make_unique<SpectrogramVisual>();
        case VisualType::waveform:    return std::make_unique<WaveformVisual>();
        case VisualType::loudness:    return std::make_unique<LoudnessVisual>();
        case VisualType::levels:      return std::make_unique<LevelsVisual>();
        case VisualType::vu:          return std::make_unique<VuVisual>();
        case VisualType::vector:      return std::make_unique<VectorVisual>();
        case VisualType::scope:       return std::make_unique<ScopeVisual>();
        case VisualType::tuner:       return std::make_unique<TunerVisual>();
        case VisualType::starmap:     return std::make_unique<StarmapVisual>();
        case VisualType::rosette:     return std::make_unique<RosetteVisual>();
        case VisualType::ridges:      return std::make_unique<RidgesVisual>();
        case VisualType::ripples:     return std::make_unique<RipplesVisual>();
        case VisualType::nucleus:     return std::make_unique<NucleusVisual>();
        case VisualType::attractor:   return std::make_unique<AttractorVisual>();
        case VisualType::plate:       return std::make_unique<PlateVisual>();
        case VisualType::barClock:    return std::make_unique<BarClockVisual>();
        case VisualType::steps:       return std::make_unique<StepsVisual>();
        case VisualType::lowEnd:      return std::make_unique<LowEndVisual>();
        case VisualType::width:       return std::make_unique<WidthVisual>();
        case VisualType::fifths:      return std::make_unique<FifthsVisual>();
        case VisualType::count:       break;
    }
    return std::make_unique<SpectrumVisual>();
}

juce::String Visual::nameOf (VisualType t)
{
    static const char* names[] = { "SPECTRUM", "SPECTROGRAM", "WAVEFORM", "LOUDNESS", "LEVELS", "VU", "VECTORSCOPE", "SCOPE", "TUNER",
                                   "STARMAP", "ROSETTE", "RIDGES", "RIPPLES", "NUCLEUS", "ATTRACTOR",
                                   "PLATE", "BAR CLOCK", "STEPS", "LOW END", "WIDTH", "FIFTHS" };
    auto i = (int) t;
    return juce::isPositiveAndBelow (i, (int) VisualType::count) ? names[i] : "?";
}

juce::String Visual::groupOf (VisualType t)
{
    auto i = (int) t;
    if (i <= (int) VisualType::vector || t == VisualType::tuner) return i <= 2 ? "SPECTRAL" : "METER";
    if (t == VisualType::scope) return "METER";
    if (i <= (int) VisualType::attractor) return "GENERATIVE";
    return "MUSICAL";
}

float Visual::preferredWeight (VisualType t)
{
    switch (t)
    {
        case VisualType::levels: case VisualType::vector: case VisualType::tuner: case VisualType::barClock:
        case VisualType::fifths: case VisualType::rosette: case VisualType::nucleus: case VisualType::ripples:
            return 0.6f;
        case VisualType::loudness: case VisualType::plate: case VisualType::scope:
            return 1.3f;
        default:
            return 1.0f;
    }
}

} // namespace cosmos
