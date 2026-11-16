#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "NebulaFx.h"

namespace nebula
{

struct NebulaSettings
{
    // macros (0..1, PITCH in cents)
    float motion = 0.45f, texture = 0.72f, space = 0.8f, dirt = 0.15f, mix = 0.5f;
    float pitchCents = 0.0f;

    // grain engine
    float sizeMs = 120.0f, density = 20.0f /* grains / s */, position = 0.1f, spray = 0.3f;
    float grainPitch = 0.0f /* semitones */, stereo = 0.6f, shape = 0.5f, reverse = 0.0f;
    bool freeze = false;

    // pitch step
    int stepMode = 0;               // 0 OFF, 1 +5, 2 +7, 3 +12, 4 -12, 5 CUSTOM (8 steps)
    std::array<int, 8> steps { 0, 7, 12, 5, 0, -5, 12, 7 };
    double stepBeats = 2.0;         // length of one step in quarter notes
    float glide = 0.0f, chance = 1.0f, stepAmount = 1.0f;
    int scaleLock = 0, scaleKey = 9; // 0 off, 1 major, 2 minor, 3 pentatonic; key 0 = C

    // tape stop
    double tapeEveryBeats = 0.0;    // 0 = off
    double tapeLengthBeats = 1.0;
    float tapeCurve = 0.5f, tapeStart = 0.0f, tapeChance = 1.0f, tapePosition = 0.75f;
    bool tapeManual = false;        // MANUAL / MIDI: stop when triggerTapeStop() is called

    // grain delay
    float gdTimeMs = 350.0f, gdFeedback = 0.35f, gdHighPass = 120.0f, gdLowPass = 8000.0f, gdMix = 0.25f;

    // FX
    bool particles = false, grainCloud = false, reverseFx = false, shimmer = false;
    float input = 1.0f, output = 1.0f;

    // FX CHAIN (8 slots, processed in order on the grains)
    std::array<int, 8> fxTypes {};
    std::array<float, 8> fxAmounts {};
};

struct Transport { bool playing = false, hasPosition = false; double ppq = 0, bpm = 120; };

/**
    LT NEBULA's engine: records the input into an 8 s buffer and plays it back as clouds of grains
    (pitch, position, spray, stereo, shape, reverse), with a pitch-step sequencer, tape stop,
    grain delay, shimmer reverb and DIRT. No latency on the dry path.
*/
class NebulaEngine
{
public:
    static constexpr int maxGrains = 96;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>&, const NebulaSettings&, const Transport&);

    /** For MANUAL / MIDI tape stop. */
    void triggerTapeStop() noexcept { manualTrigger = true; }

    // visuals (any thread)
    int getGrainCount() const noexcept        { return grainsStarted.load(); }
    int getActiveGrains() const noexcept      { return activeGrains.load(); }
    float getTapeSpeed() const noexcept       { return tapeSpeedNow.load(); }
    int getStepFlash() const noexcept         { return stepFlash.load(); }
    float getCurrentStep() const noexcept     { return currentSemis.load(); }
    int getStepIndex() const noexcept         { return stepIndexNow.load(); }
    float getInputLevel (int c) const noexcept  { return inLevel[(size_t) (c & 1)].load(); }
    float getOutputLevel (int c) const noexcept { return outLevel[(size_t) (c & 1)].load(); }
    float getWetLevel() const noexcept        { return wetLevel.load(); }

private:
    struct Grain
    {
        bool active = false;
        double pos = 0, inc = 1;      // read position in the record buffer, step per sample
        int length = 0, age = 0;
        float panL = 1, panR = 1, gain = 1, shape = 0.5f;
    };

    void startGrain (const NebulaSettings&, float semis, bool sparkle);
    float window (float x, float shape) const noexcept;
    float readRecord (int ch, double pos) const noexcept;
    float quantise (float semis, int scale, int key) const noexcept;

    double sr = 44100.0;
    juce::AudioBuffer<float> record;     // input history
    int recordLen = 0, writePos = 0;
    std::array<Grain, maxGrains> grains;
    double grainClock = 0, sparkleClock = 0;
    juce::Random rnd;

    // pitch step
    double lastStep = -1, freeBeats = 0;
    float semisNow = 0, semisTarget = 0;

    // tape stop
    juce::AudioBuffer<float> tapeBuf;
    int tapePos = 0;
    double tapeLag = 0, lastTapeCycle = -1;
    float tapePhase = -1;           // < 0 = not stopping
    float tapeRelease = 0;
    std::atomic<bool> manualTrigger { false };

    // grain delay
    std::vector<float> gdBuf[2]; int gdPos = 0;
    juce::dsp::StateVariableTPTFilter<float> gdHp, gdLp;
    float gdTimeNow = 0;

    // space & dirt
    juce::Reverb reverb;
    juce::AudioBuffer<float> wet, verb;
    std::vector<float> shimBuf[2]; int shimPos = 0; float shimPhase = 0;
    juce::dsp::StateVariableTPTFilter<float> dirtLp;
    int crushCount = 0; float crushHold[2] {};
    juce::SmoothedValue<float> mixS;
    std::array<FxUnit, 8> fx;

    std::atomic<int> grainsStarted { 0 }, activeGrains { 0 }, stepFlash { 0 }, stepIndexNow { 0 };
    std::atomic<float> tapeSpeedNow { 1.0f }, currentSemis { 0.0f }, wetLevel { 0.0f };
    std::array<std::atomic<float>, 2> inLevel {}, outLevel {};
};

} // namespace nebula
