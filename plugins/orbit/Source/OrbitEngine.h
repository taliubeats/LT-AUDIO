#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace orbit
{

struct Modes
{
    enum { digital = 0, tape, analog, pingPong, reverse, shimmer, lofi, dub, count };
    static const juce::StringArray& names();
};

struct OrbitSettings
{
    int mode = Modes::tape;
    double delaySeconds = 0.5;
    float feedback = 0.45f;     // 0..1
    float mix = 0.3f;           // 0..1
    bool freeze = false;
    float lowCut = 120.0f, highCut = 9000.0f;
    float width = 1.0f;         // 0..1.5
    float duck = 0.0f;          // 0..1
    float wobble = 0.2f;        // 0..1
    float drive = 0.2f;         // 0..1
};

/** LT ORBIT's delay: one stereo line with mode-specific colour in the feedback loop. No latency. */
class OrbitEngine
{
public:
    static constexpr double maxSeconds = 6.0;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>&, const OrbitSettings&);

    float getInputLevel (int ch) const noexcept  { return inLevel[(size_t) (ch & 1)].load(); }
    float getOutputLevel (int ch) const noexcept { return outLevel[(size_t) (ch & 1)].load(); }
    float getWetLevel() const noexcept           { return wetLevel.load(); }
    int getHitCount() const noexcept             { return hits.load(); }

private:
    struct Line
    {
        std::vector<float> buf;
        int mask = 0, pos = 0;
        void prepare (int n);
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        void push (float v) noexcept { buf[(size_t) pos] = v; pos = (pos + 1) & mask; }
        float tap (int d) const noexcept { return buf[(size_t) ((pos - 1 - d) & mask)]; }
        float read (float d) const noexcept;
    };

    float saturate (float x, float drive) const noexcept;

    double sr = 44100.0;
    Line lineL, lineR, revL, revR, shimL, shimR;
    juce::dsp::StateVariableTPTFilter<float> hp, lp, colourLp;
    float delayNow = 0, wowPhase = 0, flutterPhase = 0, revPhase = 0, shimPhase = 0;
    float duckEnv = 0, inEnv = 0, freezeAmt = 0;
    int crushCount = 0;
    float crushL = 0, crushR = 0;
    juce::Random noise;
    juce::SmoothedValue<float> mixS, fbS, widthS;
    int lastMode = -1;

    std::array<std::atomic<float>, 2> inLevel {}, outLevel {};
    std::atomic<float> wetLevel { 0 };
    std::atomic<int> hits { 0 };
};

} // namespace orbit
