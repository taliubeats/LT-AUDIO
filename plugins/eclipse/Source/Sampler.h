#pragma once

#include "Kit.h"
#include "Vibe.h"

namespace eclipse
{

struct PadSettings
{
    float gainDb = 0, pan = 0, tune = 0, attackMs = 0, fade = 0.25f, hp = 20, lp = 20000, clean = 0.7f, vibe = 0;
    int choke = 0, out = 1;
    bool mute = false, solo = false;
    float start = 0, end = 1;
    Family family = Family::sample;
};

/**
    Plays the 64 pads. Main stereo out + one stereo out per family.
    Each pad has its own bus: voices -> HP/LP (12 dB/oct) -> VIBE -> gain/pan -> outs.
*/
class Sampler
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    /** outs[0] = main, outs[1 + family] = family outs (may be null). Each is a stereo pair. */
    void process (std::array<float*, 2>* outs, int numOuts, int numSamples, const juce::MidiBuffer&,
                  const Kit::Snapshot&, const std::array<PadSettings, numPads>&, Vibe vibe);

    void trigger (int pad, float velocity, const Kit::Snapshot&, const std::array<PadSettings, numPads>&, int offset = 0);
    void stopAll();

    /** Increments every time a pad starts (the UI flashes the pads). */
    std::array<std::atomic<uint32_t>, numPads> hits {};

private:
    struct Voice
    {
        bool active = false;
        int pad = 0;
        AudioPtr raw, clean;
        double pos = 0, inc = 1, end = 0, start = 0;
        float gain = 1, cleanMix = 0, fade = 0;
        int attack = 0, age = 0, startOffset = 0;
        float releaseGain = 1, releaseStep = 0;
    };
    struct Svf
    {
        float ic1[2] {}, ic2[2] {};
        float process (int c, float v0, float g, float k, bool high);
    };
    struct Bus
    {
        Svf hp, lp;
        VibeSaturator vibe;
        int tail = 0;          // samples to keep processing after the last voice
    };

    double sr = 44100;
    int maxBlock = 512;
    std::array<Voice, 96> voices;
    std::array<Bus, numPads> buses;
    juce::AudioBuffer<float> scratch;   // 2 channels per pad
    std::array<bool, numPads> busActive {};

    void renderVoice (Voice&, float* l, float* r, int from, int to);
};

} // namespace eclipse
