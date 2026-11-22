#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include "Loudness.h"
#include "SystemAudio.h"

namespace cosmos
{

/**
    LT COSMOS: pass-through meter. The audio is never changed (in the standalone app the output is
    silenced so the input never feeds back). Loudness is measured here on the audio thread; everything
    else is analysed by the editor from the audio copied into a lock-free FIFO.
*/
class Processor : public juce::AudioProcessor
{
public:
    Processor();
    ~Processor() override { system.stop(); }   // the capture thread writes into members below

    LoudnessMeter meter;
    lt::licensing::LicenseManager license { lt::licensing::Products::cosmos, "LT COSMOS" };

    // ------------------------------------------------------------------ saved with the project
    juce::String layout;               // PanelStrip layout string
    int skin = 0;
    int target = -14;                  // LUFS target (0 = off)
    int width = 0, height = 0;         // editor size (0 = default)
    bool pinned = false;
    bool wantSystemAudio = true;       // app: listen to the whole Mac by default

    // ------------------------------------------------------------------ audio -> editor
    /** Reads up to maxSamples into l / r. Returns the number read. */
    int readAudio (float* l, float* r, int maxSamples);

    /** App only: listen to everything the computer plays instead of the audio input. */
    juce::String startSystemAudio();
    void stopSystemAudio();
    bool isSystemAudio() const noexcept { return externalSource.load(); }
    /** Discards whatever is waiting (call when an editor opens). */
    void flushAudio();
    juce::int64 getSamplesRead() const noexcept { return samplesRead; }

    struct Timing
    {
        bool valid = false, playing = false;
        double bpm = 120, ppq = 0, barStart = 0;
        int num = 4, den = 4;
        juce::int64 atWritten = 0;
    };
    Timing getTiming() const;
    double getCurrentSampleRate() const noexcept { return currentRate.load(); }
    bool isStandalone() const noexcept { return wrapperType == wrapperType_Standalone; }
    void refreshLicense() { license.refresh(); usable = license.isUsable(); }

    // ------------------------------------------------------------------ AudioProcessor
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override          { return "LT COSMOS"; }
    bool acceptsMidi() const override                    { return false; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 0.0; }
    int getNumPrograms() override                        { return 1; }
    int getCurrentProgram() override                     { return 0; }
    void setCurrentProgram (int) override                {}
    const juce::String getProgramName (int) override     { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    std::function<void()> onStateRestored;

private:
    void feed (const float* const* channels, int numChannels, int numSamples);
    SystemAudio system;
    std::atomic<bool> externalSource { false };
    static constexpr int fifoSize = 1 << 17;
    juce::AbstractFifo fifo { fifoSize };
    juce::AudioBuffer<float> fifoBuffer { 2, fifoSize };
    std::atomic<juce::int64> samplesWritten { 0 };
    juce::int64 samplesRead = 0;   // message thread only

    mutable juce::SpinLock timingLock;
    Timing timing;
    std::atomic<double> currentRate { 48000.0 };
    std::atomic<bool> usable { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace cosmos
