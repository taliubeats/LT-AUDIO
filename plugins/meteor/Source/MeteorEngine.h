#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace meteor
{

struct Choices
{
    static const juce::StringArray& reverseSizes();   // 1/16 .. 1 BAR
    static double reverseBeats (int index);
    static const juce::StringArray& rates();          // 1/4 .. 1/32
    static double rateBeats (int index);
    static const juce::StringArray& patterns();       // STRAIGHT, OFFBEAT, TRESILLO, RANDOM
    static const juce::StringArray& voices();         // 2, 3, 4
    static const juce::StringArray& repeats();        // 2, 3, 4, 6, 8
    static int repeatCount (int index);
};

struct LiveSettings
{
    double bpm = 120.0;
    bool hostPlaying = false;
    double ppq = 0.0;                  // host position in beats (valid when hostPlaying)

    bool reverseOn = false;  int reverseSize = 2;  float reverseMix = 1.0f;
    bool doubleOn = false;   int doubleVoices = 1; float doubleDetune = 0.4f, doubleWidth = 0.8f, doubleMix = 0.6f;
    bool chopOn = false;     int chopRate = 2;     int chopPattern = 0;  float chopDepth = 1.0f;
    bool glitchOn = false;   int glitchRate = 2;   float glitchChance = 0.3f; int glitchRepeats = 2;
    float mix = 1.0f;
    float outputDb = 0.0f;
};

/**
    LT METEOR live voice effects: REVERSE (previous slice played backwards), DOUBLE (ADT-style
    doubled voices), CHOP (rhythmic gate) and GLITCH (random stutters). Tempo-synced, no latency.
*/
class MeteorEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>&, const LiveSettings&);

    float getInputLevel() const noexcept     { return inLevel.load(); }
    float getOutputLevel() const noexcept    { return outLevel.load(); }
    float getGate() const noexcept           { return gateNow.load(); }
    int getGlitchCount() const noexcept      { return glitches.load(); }
    bool isReversing() const noexcept        { return reversing.load(); }

    /** True when step `step` of the CHOP pattern is open (also used by the visuals and tests). */
    static bool chopOpen (int pattern, juce::int64 step, double phaseInStep);

private:
    static constexpr double historySeconds = 8.0;

    struct Ring
    {
        std::vector<float> buf;
        int mask = 0, pos = 0;
        void prepare (int n);
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
        void push (float v) noexcept { buf[(size_t) pos] = v; pos = (pos + 1) & mask; }
        float at (int samplesAgo) const noexcept { return buf[(size_t) ((pos - 1 - samplesAgo) & mask)]; }
        float read (float samplesAgo) const noexcept;
    };

    double sr = 44100.0;
    Ring hist[2], dblLine[2];

    // free-running clock (beats) used when the host is stopped
    double freeBeats = 0.0;

    // reverse
    juce::int64 revSlice = -1;

    // chop
    float gate = 1.0f;

    // glitch
    juce::int64 glitchStep = -1;
    bool glitchActive = false;
    int glitchSliceLen = 0, glitchStartAgo = 0;
    float glitchRate = 1.0f, glitchReadPos = 0.0f, glitchEnv = 0.0f;
    juce::int64 glitchSamplesLeft = 0;
    juce::Random rng { 0x3e7e0 };

    // double
    float dblPhase[3] { 0.0f, 2.2f, 4.1f };

    juce::SmoothedValue<float> mixS, outS, revMixS, dblMixS;
    bool primed = false;
    std::atomic<float> inLevel { 0 }, outLevel { 0 }, gateNow { 1 };
    std::atomic<int> glitches { 0 };
    std::atomic<bool> reversing { false };
};

} // namespace meteor
