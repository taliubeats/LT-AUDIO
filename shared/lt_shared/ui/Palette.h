#pragma once

namespace lt::ui
{

/** All colours used by the LT family. Every plugin draws from one of these. */
struct Palette
{
    juce::Colour background;      // deep space base
    juce::Colour nebulaA;         // main nebula colour
    juce::Colour nebulaB;         // secondary nebula colour
    juce::Colour nebulaC;         // highlights inside the nebula
    juce::Colour star;
    juce::Colour accent;          // arcs, glows, selected buttons
    juce::Colour accentText;      // text on top of accent-filled buttons
    juce::Colour text;            // labels
    juce::Colour textDim;
    juce::Colour value;           // big values (75%, 140, A MINOR...)
    juce::Colour glassFill;
    juce::Colour glassEdge;
    juce::Colour success;
    juce::Colour danger;
    bool isLight = false;

    /** Black + purple + white. */
    static Palette dark()
    {
        Palette p;
        p.background = juce::Colour (0xff07050d);
        p.nebulaA    = juce::Colour (0xff5b21b6);
        p.nebulaB    = juce::Colour (0xff3b0d7a);
        p.nebulaC    = juce::Colour (0xffa78bfa);
        p.star       = juce::Colours::white;
        p.accent     = juce::Colour (0xff7b2cff);
        p.accentText = juce::Colours::white;
        p.text       = juce::Colour (0xffeae6f5);
        p.textDim    = juce::Colour (0x99d6cfee);
        p.value      = juce::Colours::white;
        p.glassFill  = juce::Colour (0x14ffffff);
        p.glassEdge  = juce::Colour (0x33ffffff);
        p.success    = juce::Colour (0xff34d399);
        p.danger     = juce::Colour (0xfff87171);
        return p;
    }

    /** Sky blue + yellow + white, black small text. */
    static Palette light()
    {
        Palette p;
        p.isLight    = true;
        p.background = juce::Colour (0xff4fc3f7);
        p.nebulaA    = juce::Colour (0xffffffff);
        p.nebulaB    = juce::Colour (0xffb3ecff);
        p.nebulaC    = juce::Colour (0xfffff3b0);
        p.star       = juce::Colours::white;
        p.accent     = juce::Colours::white;
        p.accentText = juce::Colour (0xff111111);
        p.text       = juce::Colour (0xff111111);
        p.textDim    = juce::Colour (0xaa111111);
        p.value      = juce::Colour (0xffffc300);
        p.glassFill  = juce::Colour (0x26ffffff);
        p.glassEdge  = juce::Colour (0x8cffffff);
        p.success    = juce::Colour (0xff0f9d58);
        p.danger     = juce::Colour (0xffd93025);
        return p;
    }

    /** A dark space palette tinted with one accent colour (used by LT VOID reverb types). */
    static Palette tinted (juce::Colour accentColour, juce::Colour nebula1, juce::Colour nebula2, juce::Colour bg)
    {
        auto p = dark();
        p.accent = accentColour;
        p.nebulaA = nebula1;
        p.nebulaB = nebula2;
        p.nebulaC = accentColour.brighter (0.6f);
        p.background = bg;
        return p;
    }

    /** Smooth blend between two palettes (for animated theme changes). */
    static Palette blend (const Palette& a, const Palette& b, float t)
    {
        auto mix = [t] (juce::Colour x, juce::Colour y) { return x.interpolatedWith (y, t); };
        Palette p;
        p.background = mix (a.background, b.background);
        p.nebulaA = mix (a.nebulaA, b.nebulaA);
        p.nebulaB = mix (a.nebulaB, b.nebulaB);
        p.nebulaC = mix (a.nebulaC, b.nebulaC);
        p.star = mix (a.star, b.star);
        p.accent = mix (a.accent, b.accent);
        p.accentText = mix (a.accentText, b.accentText);
        p.text = mix (a.text, b.text);
        p.textDim = mix (a.textDim, b.textDim);
        p.value = mix (a.value, b.value);
        p.glassFill = mix (a.glassFill, b.glassFill);
        p.glassEdge = mix (a.glassEdge, b.glassEdge);
        p.success = mix (a.success, b.success);
        p.danger = mix (a.danger, b.danger);
        p.isLight = t < 0.5f ? a.isLight : b.isLight;
        return p;
    }
};

} // namespace lt::ui
