// Renders every LT QUASAR factory preset with a phrase that suits its category, writes WAVs and a report
// (loudness, peak, brightness, finite), and a levels.json with the master gain that brings each preset
// to the same loudness. Usage: quasar_render <out folder> [current levels.json]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

using namespace quasar;

namespace
{
    /** The plugin icon: a quasar (tilted accretion disk, two jets, bright core) in an LT rounded tile. */
    void writeIcon (const juce::File& file)
    {
        const int S = 512;
        juce::Image img (juce::Image::ARGB, S, S, true);
        juce::Graphics g (img);
        auto tile = juce::Rectangle<float> (8, 8, S - 16, S - 16);
        juce::Path clip; clip.addRoundedRectangle (tile, 92.0f);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (clip);
            g.fillAll (juce::Colour (0xff07050d));
            auto neb = lt::ui::NebulaBackground::renderNebula (S, S, lt::ui::Palette::dark(), 31, 0.35f, 0.9f);
            g.drawImageAt (neb, 0, 0);
            juce::Random r (3);
            for (int i = 0; i < 220; ++i)
            {
                auto x = r.nextFloat() * S, y = r.nextFloat() * S, sz = 0.6f + r.nextFloat() * 1.8f;
                g.setColour (juce::Colours::white.withAlpha (0.25f + 0.6f * r.nextFloat())); g.fillEllipse (x, y, sz, sz);
            }
            juce::Point<float> c (S * 0.5f, S * 0.52f);
            auto tilt = juce::AffineTransform::rotation (-0.35f, c.x, c.y);
            // jets
            for (int k = 0; k < 2; ++k)
            {
                auto dir = k == 0 ? -1.0f : 1.0f;
                juce::Path jet; jet.addTriangle (c.x - 10, c.y, c.x + 10, c.y, c.x, c.y + dir * 230);
                juce::ColourGradient jg (juce::Colour (0xeee6d6ff), c.x, c.y, juce::Colour (0x00b98cff), c.x, c.y + dir * 230, false);
                g.setGradientFill (jg);
                g.fillPath (jet, juce::AffineTransform::rotation (-0.35f + 0.0f, c.x, c.y));
                lt::ui::Glass::drawGlow (g, c.transformedBy (juce::AffineTransform::translation (0, dir * 120).followedBy (tilt)), 60, juce::Colour (0xff9b5cff), 0.25f);
            }
            // accretion disk: thousands of particles on tilted ellipses, brighter inside
            for (int i = 0; i < 5000; ++i)
            {
                auto rad = 40.0f + std::pow (r.nextFloat(), 0.75f) * 170.0f;
                auto a = r.nextFloat() * juce::MathConstants<float>::twoPi;
                juce::Point<float> pt (c.x + std::cos (a) * rad, c.y + std::sin (a) * rad * 0.3f);
                pt = pt.transformedBy (tilt);
                auto t = 1.0f - (rad - 40.0f) / 170.0f;
                auto col = juce::Colour (0xff7b2cff).interpolatedWith (juce::Colour (0xfffff0ff), t * 0.9f);
                auto sz = 0.8f + r.nextFloat() * 1.8f;
                g.setColour (col.withAlpha (0.15f + 0.6f * t * r.nextFloat() + 0.15f));
                g.fillEllipse (pt.x - sz * 0.5f, pt.y - sz * 0.5f, sz, sz);
            }
            lt::ui::Glass::drawGlow (g, c, 150, juce::Colour (0xff7b2cff), 0.55f);
            lt::ui::Glass::drawGlow (g, c, 70, juce::Colour (0xffd9c4ff), 0.9f);
            lt::ui::Glass::drawGlow (g, c, 22, juce::Colours::white, 1.0f);
        }
        juce::ColourGradient rim (juce::Colour (0xffc9a8ff), 0, 0, juce::Colour (0xff6b2cdc), 0, (float) S, false);
        g.setGradientFill (rim);
        g.strokePath (clip, juce::PathStrokeType (10.0f));
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String (argv[1]) == "--icon") { writeIcon (juce::File (juce::String (argv[2]))); return 0; }
    juce::File out (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("renders").getFullPathName());
    out.createDirectory();
    const double sr = 48000.0; const int block = 256;
    std::unique_ptr<Processor> p (new Processor());
    p->setRateAndBufferSizeDetails (sr, block);
    p->prepareToPlay (sr, block);

    auto targetDb = -17.0f;
    auto levels = std::make_unique<juce::DynamicObject>();
    juce::Array<juce::var> report;
    int bad = 0, count = 0;
    juce::WavAudioFormat wav;

    for (auto* preset : p->presets.allPresets())
    {
        if (! preset->factory) continue;
        p->applyPreset (*preset);
        p->serviceNow();
        {
            // pre-roll: let the new effects take over (they crossfade in) before the phrase starts
            juce::AudioBuffer<float> pre (2, block); juce::MidiBuffer none;
            for (int i = 0; i < 40; ++i) p->processBlock (pre, none);
        }
        p->reset();
        auto cat = preset->category;
        std::vector<std::pair<double, juce::MidiMessage>> ev;
        double len = 4.0;
        auto chord = [&] (double t, std::initializer_list<int> notes, double dur)
        { for (auto n : notes) { ev.push_back ({ t, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) }); ev.push_back ({ t + dur, juce::MidiMessage::noteOff (1, n) }); } };
        if (cat == "BASS" || cat == "808" || cat == "SUB" || cat == "REESE") { chord (0.0, { 36 }, 0.9); chord (1.0, { 43 }, 0.9); chord (2.0, { 39 }, 1.2); len = 4.0; }
        else if (cat == "PAD" || cat == "FX") { chord (0.0, { 48, 51, 55, 58 }, 3.5); len = 6.0; }
        else if (cat == "ARP") { chord (0.0, { 48, 51, 55 }, 4.0); len = 5.5; }
        else { int mel[] = { 60, 63, 67, 70, 67, 63 }; for (int i = 0; i < 6; ++i) chord (i * 0.35, { mel[i] }, 0.3); chord (2.2, { 48, 55, 60, 63 }, 1.2); len = 5.0; }
        std::sort (ev.begin(), ev.end(), [] (auto& a, auto& b) { return a.first < b.first; });

        auto total = (int) (len * sr);
        juce::AudioBuffer<float> all (2, total), buf (2, block);
        size_t e = 0;
        for (int pos = 0; pos < total; pos += block)
        {
            auto n = juce::jmin (block, total - pos);
            buf.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;
            while (e < ev.size() && (int) (ev[e].first * sr) < pos + n) { midi.addEvent (ev[e].second, juce::jmax (0, (int) (ev[e].first * sr) - pos)); ++e; }
            p->processBlock (buf, midi);
            all.copyFrom (0, pos, buf, 0, 0, n); all.copyFrom (1, pos, buf, 1, 0, n);
        }
        // loudness: RMS over the louder half of 50 ms windows (ignores silence and tails)
        std::vector<float> win;
        bool finite = true;
        for (int pos = 0; pos + 2400 <= total; pos += 2400)
        {
            double s = 0;
            for (int i = pos; i < pos + 2400; ++i)
            {
                auto l = all.getSample (0, i), r = all.getSample (1, i);
                finite &= std::isfinite (l) && std::isfinite (r);
                s += l * l + r * r;
            }
            win.push_back ((float) std::sqrt (s / 4800.0));
        }
        std::sort (win.begin(), win.end());
        double acc = 0; int k = 0;
        for (size_t i = win.size() / 2; i < win.size(); ++i) { acc += win[i] * win[i]; ++k; }
        auto rmsDb = 10.0f * std::log10 ((float) (acc / juce::jmax (1, k)) + 1.0e-12f);
        auto peak = juce::jmax (all.getMagnitude (0, 0, total), all.getMagnitude (1, 0, total));
        // brightness (spectral centroid of the whole render)
        constexpr int order = 15; const int nfft = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> d ((size_t) nfft * 2, 0.0f);
        for (int i = 0; i < juce::jmin (nfft, total); ++i) d[(size_t) i] = 0.5f * (all.getSample (0, i) + all.getSample (1, i)) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) nfft));
        fft.performFrequencyOnlyForwardTransform (d.data());
        double num = 0, den = 0;
        for (int b = 1; b < nfft / 2; ++b) { auto f = b * sr / nfft; num += f * d[(size_t) b]; den += d[(size_t) b]; }
        auto centroid = den > 0 ? num / den : 0.0;

        // clicks: sample-to-sample jumps right after each note-on, compared with the note body
        float clickRatio = 0;
        for (auto& evt : ev)
        {
            if (! evt.second.isNoteOn()) continue;
            auto t0 = (int) (evt.first * sr);
            auto stepIn = [&] (int a, int b2) { float m = 0; for (int c = 0; c < 2; ++c) for (int i = juce::jmax (1, a); i < juce::jmin (total, b2); ++i) m = juce::jmax (m, std::abs (all.getSample (c, i) - all.getSample (c, i - 1))); return m; };
            auto onset = stepIn (t0, t0 + (int) (0.003 * sr));
            auto body = stepIn (t0 + (int) (0.02 * sr), t0 + (int) (0.12 * sr));
            if (body > 1.0e-4f) clickRatio = juce::jmax (clickRatio, onset / body);
        }
        auto currentMaster = p->state.getRawParameterValue (IDs::master)->load();
        auto gain = juce::jlimit (-18.0f, 18.0f, currentMaster + (targetDb - rmsDb));
        levels->setProperty (preset->name, std::round (gain * 10.0f) / 10.0f);

        auto file = out.getChildFile (juce::File::createLegalFileName (preset->bank + " - " + preset->name) + ".wav");
        file.deleteFile();
        if (auto os = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0)))
            {
                os.release();
                w->writeFromAudioSampleBuffer (all, 0, total);
            }

        auto r = std::make_unique<juce::DynamicObject>();
        r->setProperty ("name", preset->name); r->setProperty ("bank", preset->bank); r->setProperty ("category", cat);
        r->setProperty ("rms_db", std::round (rmsDb * 10) / 10); r->setProperty ("peak", std::round (peak * 1000) / 1000);
        r->setProperty ("centroid", (int) centroid); r->setProperty ("finite", finite);
        r->setProperty ("click", std::round (clickRatio * 100) / 100);
        report.add (juce::var (r.release()));
        if (! finite || rmsDb < -60.0f) { ++bad; std::cout << "PROBLEM: " << preset->name << " rms " << rmsDb << " finite " << (int) finite << std::endl; }
        ++count;
    }
    out.getChildFile ("report.json").replaceWithText (juce::JSON::toString (juce::var (report)));
    out.getChildFile ("levels.json").replaceWithText (juce::JSON::toString (juce::var (levels.release())));
    std::cout << "rendered " << count << " presets, problems: " << bad << std::endl;
    return bad == 0 ? 0 : 1;
}
