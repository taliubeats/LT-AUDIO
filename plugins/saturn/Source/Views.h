#pragma once

#include <lt_shared/lt_shared.h>

namespace saturn
{

/**
    LT SATURN v2: a vintage tape machine (cream enamel panels, wooden cheeks, bakelite knobs,
    VU meters) with a CRT-like window where two Saturns are the tape reels.
    THEME switches between CREAM (default) and NIGHT (dark graphite panels).
*/
struct Vintage
{
    juce::Colour panel, panelShade, grime, text, textSoft, wood, woodDark, key, keyShade, keyText,
                 accent, accentGlow, bezel, screwHead;
    bool night = false;

    static Vintage cream();
    static Vintage nightPanel();

    // ---- drawing helpers
    /** Enamel panel with speckles, worn edges and a soft bevel (rendered once into an image). */
    static juce::Image renderPanel (int w, int h, const Vintage&, int seed);
    static juce::Image renderWood (int w, int h, const Vintage&, int seed);
    static void drawScrew (juce::Graphics&, juce::Point<float> c, float r, float angle, const Vintage&);
    static void drawKeycap (juce::Graphics&, juce::Rectangle<float>, const Vintage&, bool selected, bool hover, bool down);
    static juce::Font serif (float height);
};

/** A cream "tape type" key, like the program keys of an old machine. */
class KeyButton : public juce::Button
{
public:
    explicit KeyButton (const juce::String& name, bool squareKey = false) : juce::Button (name), square (squareKey) {}
    const Vintage* look = nullptr;
    void paintButton (juce::Graphics&, bool hover, bool down) override;

private:
    bool square;
};

/** Bakelite knob with a chrome cap (small) or the big fluted AGE knob. */
class VintageKnob : public juce::Slider
{
public:
    enum class Style { small, age, mini };   // mini: small knob with the label beside it
    VintageKnob (const juce::String& label, Style);
    const Vintage* look = nullptr;
    juce::String label;
    bool alwaysShowArc = false;

    juce::Rectangle<float> knobArea() const;
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;

private:
    void drawSmall (juce::Graphics&, juce::Rectangle<float>, float proportion, bool hot);
    void drawAge (juce::Graphics&, juce::Rectangle<float>, float proportion, bool hot);
    Style style;
};

/** Classic VU meter with real VU ballistics (~300 ms). */
class VUMeter : public juce::Component
{
public:
    const Vintage* look = nullptr;
    void setLevel (float peak, float dt);
    void paint (juce::Graphics&) override;

private:
    float vu = 0.0f, needle = 0.0f, needleVel = 0.0f;
};

struct TapeState
{
    int tape = 0;
    float age = 0.4f, wow = 0.3f, flutter = 0.2f, drift = 0.2f, hiss = 0.15f, saturation = 0.3f, ensemble = 0.3f;
    float speed = 1.0f, dropout = 0.0f, level = 0.0f;
};

/**
    The window: deep space, two Saturns that turn like reels, the tape running from their rings
    to a tape head at the bottom (sparks = signal hitting the head), a small Saturn above.
    WOW/FLUTTER make the tape sway, DRIFT tilts the reels, AGE yellows the picture, HISS adds
    dust, BROKEN makes everything jump. Visual only.
*/
class TapeScreen : public juce::Component
{
public:
    TapeScreen() { setInterceptsMouseClicks (false, false); }
    const Vintage* look = nullptr;
    void tick (const TapeState&);
    void paint (juce::Graphics&) override;
    void resized() override { space = {}; }

private:
    void drawSaturn (juce::Graphics&, juce::Point<float> c, float r, float ringTilt, float spinPhase, float alpha);
    void drawHead (juce::Graphics&, juce::Rectangle<float> screen);

    struct Spark { juce::Point<float> pos, vel; float life, size; };
    struct Dust { juce::Point<float> pos; float life, size; };
    TapeState st;
    std::vector<Spark> sparks;
    std::vector<Dust> dust;
    juce::Random random;
    juce::Image space;
    double time = 0;
    float spin = 0, tilt = 0, glow = 0, jump = 0;
};

} // namespace saturn
