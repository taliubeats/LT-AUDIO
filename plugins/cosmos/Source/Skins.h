#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cosmos
{

/** A skin repaints the whole window: bar, panels and every visual. */
struct Skin
{
    juce::String name;
    bool light = false;
    juce::Colour bg, panel, grid, text, dim, line, line2, accent, warn, good;
    std::array<juce::Colour, 5> heat;   // spectrogram / density colour map, quiet -> loud

    juce::Colour heatAt (float v) const
    {
        v = juce::jlimit (0.0f, 1.0f, v) * 4.0f;
        auto i = juce::jmin (3, (int) v);
        return heat[(size_t) i].interpolatedWith (heat[(size_t) i + 1], v - (float) i);
    }

    static const std::vector<Skin>& all()
    {
        static const std::vector<Skin> skins = []
        {
            std::vector<Skin> s;
            auto make = [&s] (const char* name, bool light, juce::uint32 bg, juce::uint32 panel, juce::uint32 grid, juce::uint32 text,
                              juce::uint32 line, juce::uint32 line2, juce::uint32 accent, juce::uint32 warn, juce::uint32 good,
                              std::array<juce::uint32, 5> heat)
            {
                Skin k;
                k.name = name; k.light = light;
                k.bg = juce::Colour (bg); k.panel = juce::Colour (panel); k.grid = juce::Colour (grid);
                k.text = juce::Colour (text); k.dim = k.text.withAlpha (0.48f);
                k.line = juce::Colour (line); k.line2 = juce::Colour (line2); k.accent = juce::Colour (accent);
                k.warn = juce::Colour (warn); k.good = juce::Colour (good);
                for (size_t i = 0; i < 5; ++i) k.heat[i] = juce::Colour (heat[i]);
                s.push_back (k);
            };
            make ("MIDNIGHT", false, 0xff0c0c0d, 0xff141415, 0x16ffffff, 0xffe8e2d4, 0xfff3ecda, 0xffa99cff, 0xffff9a3c, 0xffff5449, 0xff35c768,
                  { 0xff141415, 0xff2a1f6b, 0xff7d6cf0, 0xffff9f4a, 0xfffff3da });
            make ("PAPER", true, 0xffefeae0, 0xfff8f5ee, 0x1a000000, 0xff1c1b18, 0xff1c1b18, 0xff2f62d0, 0xffd9480f, 0xffd62828, 0xff2b9348,
                  { 0xfff8f5ee, 0xffc9d6f2, 0xff5b7fd8, 0xff26357a, 0xff111111 });
            make ("NEBULA", false, 0xff07050d, 0xff0f0b19, 0x18ffffff, 0xffeae6f5, 0xffeae6f5, 0xffa78bfa, 0xffd17bff, 0xfff87171, 0xff34d399,
                  { 0xff0f0b19, 0xff3b0d7a, 0xff7b2cff, 0xffa78bfa, 0xffffffff });
            make ("PHOSPHOR", false, 0xff040904, 0xff081109, 0x1c7dff9b, 0xffa8ffbb, 0xff7dff9b, 0xff36b85a, 0xffd8ff6a, 0xffff6b4a, 0xff7dff9b,
                  { 0xff081109, 0xff0e3a1b, 0xff2b9f4a, 0xff7dff9b, 0xffeaffee });
            make ("BLUEPRINT", false, 0xff0c2850, 0xff10315e, 0x26ffffff, 0xffe4eeff, 0xffffffff, 0xff8fd3ff, 0xffffd166, 0xffff7b7b, 0xff7ef0b0,
                  { 0xff10315e, 0xff1f5b9c, 0xff5fa8e6, 0xffbfe3ff, 0xffffffff });
            make ("NEON", false, 0xff110721, 0xff190b2e, 0x1cff4fd8, 0xfff6e8ff, 0xffff4fd8, 0xff3df5ff, 0xffffc94d, 0xffff3b5c, 0xff3df5a0,
                  { 0xff190b2e, 0xff4a1170, 0xffd6249f, 0xffff9e3d, 0xfffff3b0 });
            make ("ROSE", true, 0xfff8dde7, 0xfffcedf2, 0x1f3a1022, 0xff3a1022, 0xffd6336c, 0xff7b2d8b, 0xffff7a00, 0xffc9184a, 0xff2b9348,
                  { 0xfffcedf2, 0xfff6b3c9, 0xffe0568a, 0xff8f1d5c, 0xff2e0a1d });
            make ("GLACIER", true, 0xffdff0f8, 0xffeff8fc, 0x1f0b2a3a, 0xff0b2a3a, 0xff0a72b5, 0xff009c9c, 0xffff6b4a, 0xffd62828, 0xff1b9e5a,
                  { 0xffeff8fc, 0xffaedcf0, 0xff4aa8d8, 0xff12608f, 0xff06243a });
            return s;
        }();
        return skins;
    }

    static const Skin& get (int index) { return all()[(size_t) juce::jlimit (0, (int) all().size() - 1, index)]; }
};

/** The monospaced type used everywhere in COSMOS. */
inline juce::Font monoFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height,
                                          bold ? juce::Font::bold : juce::Font::plain))
        .withExtraKerningFactor (0.06f);
}

} // namespace cosmos
