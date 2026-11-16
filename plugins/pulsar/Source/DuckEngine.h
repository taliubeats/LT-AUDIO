#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <lt_dsp/lt_dsp.h>
#include "Curve.h"
#include "Presets.h"

namespace pulsar
{

/**
    The audio side of LT PULSAR: tempo-synced (or MIDI-triggered) volume shaping, optionally
    only inside the Frequency Focus band. No latency. Also records what the visualiser shows.
*/
class DuckEngine
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int displayBins = 400;
    static constexpr int chunk = 256;

    struct Settings
    {
        float mix = 0.75f;          // 0..1
        int rate = Rates::quarter;
        bool midiTrigger = false;
    };

    struct Transport
    {
        bool playing = false, hasPosition = false;
        double ppq = 0.0, bpm = 120.0;
    };

    DuckEngine();

    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Message thread: the curve to use from now on. */
    void setCurve (const Curve&);

    void process (juce::AudioBuffer<float>&, const juce::MidiBuffer&, const Settings&, const Transport&,
                  const lt::dsp::FocusSettings&);

    // ---- for the visualiser (any thread)
    float getDisplayPhase() const noexcept   { return displayPhase.load(); }
    int getCycleCount() const noexcept       { return cycleCount.load(); }
    float getInputLevel() const noexcept     { return inputLevel.load(); }
    float getCurrentGain() const noexcept    { return currentGain.load(); }
    float getInputPeak (int bin) const noexcept  { return inBins[(size_t) bin].load (std::memory_order_relaxed); }
    float getOutputPeak (int bin) const noexcept { return outBins[(size_t) bin].load (std::memory_order_relaxed); }

    lt::dsp::SpectrumAnalyser spectrum;

private:
    void processChunk (juce::AudioBuffer<float>&, int start, int n, const juce::MidiBuffer&, const Settings&,
                       const Transport&, const lt::dsp::FocusSettings&, int blockOffset);
    float lookup (float phase) const noexcept;
    void recordDisplay (float phase, float in, float out) noexcept;

    double sampleRate = 44100.0;
    int channels = 2;

    // curve table: written on the message thread, copied on the audio thread when it changes
    std::vector<float> table, pendingTable;
    juce::SpinLock tableLock;
    std::atomic<bool> tableDirty { false };

    juce::AudioBuffer<float> focusBuffer;
    lt::dsp::FocusSplitter splitter;
    juce::SmoothedValue<float> mixSmoothed { 0.75f };
    float gain = 1.0f, gainCoeff = 0.03f;

    double freePhase = 0.0, midiPhase = 1.0, lastPhase = 0.0;
    bool midiRunning = false;
    std::array<float, chunk> phases {}, inAbs {};

    // display
    std::array<std::atomic<float>, displayBins> inBins, outBins;
    int currentBin = -1;
    float binIn = 0, binOut = 0, levelFollower = 0;
    std::atomic<float> displayPhase { 0.0f }, inputLevel { 0.0f }, currentGain { 1.0f };
    std::atomic<int> cycleCount { 0 };
};

} // namespace pulsar
