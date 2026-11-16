#pragma once

namespace lt::dsp
{

/** The "Frequency Focus" settings every LT plugin with an EQ button shares. */
struct FocusSettings
{
    enum Band { full = 0, low, mid, high, custom };

    int band = full;
    float lowHz = 30.0f, highHz = 150.0f;
    bool invert = false, solo = false;

    bool isActive() const noexcept { return band != full; }

    static constexpr float minHz = 20.0f, maxHz = 20000.0f;

    /** The cut-offs the LOW / MID / HIGH buttons jump to. */
    static juce::Range<float> rangeFor (int band) noexcept
    {
        switch (band)
        {
            case low:  return { 30.0f, 150.0f };
            case mid:  return { 150.0f, 2500.0f };
            case high: return { 2500.0f, 20000.0f };
            default:   return { minHz, maxHz };
        }
    }
};

/** Parameter IDs and helpers so every plugin stores Frequency Focus the same way. */
struct FocusParams
{
    static constexpr const char* band   = "focusBand";
    static constexpr const char* lowCut = "focusLow";
    static constexpr const char* highCut = "focusHigh";
    static constexpr const char* invert = "focusInvert";
    static constexpr const char* solo   = "focusSolo";

    static void addTo (juce::AudioProcessorValueTreeState::ParameterLayout&);

    /** Cached raw pointers, read on the audio thread. */
    struct Handles
    {
        std::atomic<float>* band = nullptr;
        std::atomic<float>* lowCut = nullptr;
        std::atomic<float>* highCut = nullptr;
        std::atomic<float>* invert = nullptr;
        std::atomic<float>* solo = nullptr;

        void attach (juce::AudioProcessorValueTreeState&);
        FocusSettings read() const noexcept;
    };

    static juce::NormalisableRange<float> frequencyRange();
};

/**
    Splits audio into the focused band and everything else, with Linkwitz-Riley crossovers
    (the two parts add back up flat, no latency).

    Usage per block:
        splitter.split (buffer, target, settings);   // buffer keeps "the rest", target gets the part to process
        ... process target ...
        splitter.combine (buffer, target, settings); // buffer = rest + target (or only target with SOLO)
*/
class FocusSplitter
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void split (juce::AudioBuffer<float>& io, juce::AudioBuffer<float>& target, const FocusSettings&);
    static void combine (juce::AudioBuffer<float>& io, const juce::AudioBuffer<float>& target, const FocusSettings&);

private:
    void updateCutoffs (const FocusSettings&);

    double sampleRate = 44100.0;
    juce::dsp::LinkwitzRileyFilter<float> lowSplit, highSplit, lowAllpass;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> lowHz { 30.0f }, highHz { 150.0f };
    bool wasActive = false;
};

} // namespace lt::dsp
