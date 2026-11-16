#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace nebula
{

/**
    The 4 modulation sources and up to 16 routes (source -> knob, depth -1..1).
    Routes are read on the audio thread (atomics) and edited on the message thread.
*/
class Modulation
{
public:
    enum Source { mod1 = 0, mod2, envelope, random, numSources };
    static constexpr int maxRoutes = 16;

    static const juce::StringArray& sourceNames();     // MOD 1, MOD 2, ENV, RND
    static juce::Colour sourceColour (int);
    static const juce::StringArray& mod2Shapes();       // SINE TRIANGLE SAW UP SAW DOWN SQUARE SMOOTH RND

    /** Knobs that can be modulated (parameter IDs). */
    static const juce::StringArray& targets();
    static int targetIndex (const juce::String& id)    { return targets().indexOf (id); }

    struct Route { std::atomic<int> source { -1 }, target { -1 }; std::atomic<float> depth { 0.5f }; };

    // ---- message thread
    void addRoute (int source, int target, float depth = 0.5f);
    void removeRoute (int source, int target);
    void setDepth (int source, int target, float depth);
    float getDepth (int source, int target) const;      // 0 when not routed
    juce::var toVar() const;
    void fromVar (const juce::var&);

    // ---- audio thread: advance the sources for one block, then read the modulated value of a parameter
    struct Inputs
    {
        std::array<float, 8> steps {};   // MOD 1 step values 0..1
        double mod1Beats = 0.5, mod2Beats = 4.0, rndBeats = 0.25;
        float mod1Swing = 0, mod2Swing = 0;
        int mod2Shape = 0;
        double ppq = 0, bpm = 120;
        bool playing = false;
        float inputLevel = 0;
        int numSamples = 0;
        double sampleRate = 44100;
    };
    void advance (const Inputs&);
    /** Normalised (0..1) parameter value plus all its routes. */
    float modulated (int target, float normalisedBase) noexcept;

    // ---- UI
    float getSourceValue (int s) const noexcept     { return values[(size_t) s].load(); }
    float getOffset (int target) const noexcept     { return juce::isPositiveAndBelow (target, 64) ? offsets[(size_t) target].load() : 0.0f; }
    float getMod2Phase() const noexcept             { return mod2Phase.load(); }
    int getMod1Step() const noexcept                { return mod1Step.load(); }

private:
    std::array<Route, maxRoutes> routes;
    std::array<std::atomic<float>, numSources> values {};
    std::array<std::atomic<float>, 64> offsets {};
    std::atomic<float> mod2Phase { 0 };
    std::atomic<int> mod1Step { 0 };
    double freeBeats = 0, lastRnd = -1;
    float env = 0, rndNow = 0, rndPrev = 0, smoothRnd = 0;
    juce::Random rnd;
};

} // namespace nebula
