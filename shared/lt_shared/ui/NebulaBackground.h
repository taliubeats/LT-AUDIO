#pragma once

namespace lt::ui
{

/**
    Animated deep-space background: drifting nebula clouds, twinkling stars and
    floating particles. Purely visual - plugins feed it `energy` (0..1) from the
    audio level and call `pulse()` on beats so the space reacts to the sound.
*/
class NebulaBackground : public juce::Component,
                         private juce::Timer
{
public:
    explicit NebulaBackground (int seed = 7);
    ~NebulaBackground() override;

    void setPalette (const Palette&);
    void setEnergy (float newEnergy) noexcept     { energy = juce::jlimit (0.0f, 1.0f, newEnergy); }
    void pulse (float amount = 1.0f) noexcept     { flash = juce::jmax (flash, amount); }
    void setDriftSpeed (float s) noexcept         { driftSpeed = s; }
    void setFrozen (bool f) noexcept              { frozen = f; }
    void setParticleAmount (float a) noexcept     { particleAmount = juce::jlimit (0.0f, 1.0f, a); }

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Reusable helpers so other components can draw matching nebula images. */
    static juce::Image renderNebula (int w, int h, const Palette&, int seed, float bandTilt, float density);

private:
    void timerCallback() override;
    void createStars();

    struct Star { juce::Point<float> pos; float size, phase, speed; };
    struct Particle { juce::Point<float> pos, vel; float life, size; };

    Palette palette = Palette::dark();
    juce::Image layerFar, layerNear;
    std::vector<Star> stars;
    std::vector<Particle> particles;
    juce::Random random;
    int seed;
    double time = 0.0;
    float energy = 0.0f, flash = 0.0f, driftSpeed = 1.0f, particleAmount = 0.5f;
    bool frozen = false;
};

} // namespace lt::ui
