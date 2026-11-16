#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include <lt_dsp/lt_dsp.h>
#include "DuckEngine.h"
#include "Presets.h"

namespace pulsar
{

namespace IDs
{
    inline constexpr const char* mix = "mix";
    inline constexpr const char* rate = "rate";
    inline constexpr const char* midi = "midiTrigger";
    inline constexpr const char* audio = "audioTrigger";
}

/** LT PULSAR: sidechain-style ducking synced to the host tempo (or triggered by MIDI notes). */
class Processor : public juce::AudioProcessor
{
public:
    Processor();
    ~Processor() override = default;

    // ---- message thread API
    const Curve& getCurve() const noexcept               { return curve; }
    void setCurve (const Curve&);
    void applyPreset (const Preset&);
    const juce::String& getPresetName() const noexcept   { return loaded.name; }
    bool isPresetModified() const;
    void refreshLicense()                                { license.refresh(); usable = license.isUsable(); }
    Preset makePreset (const juce::String& name) const;

    /** Which of the 8 shape buttons matches the curve, or -1. */
    int getMatchingShape() const;

    std::function<void()> onStateRestored;

    bool lightTheme = false, showWave = true;
    PresetLibrary presets;
    juce::AudioProcessorValueTreeState state;
    DuckEngine engine;
    lt::licensing::LicenseManager license { lt::licensing::Products::pulsar, "LT PULSAR" };

    float getHostBpm() const noexcept                    { return hostBpm.load(); }
    bool isHostPlaying() const noexcept                  { return hostPlaying.load(); }

    // ---- AudioProcessor
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }

    const juce::String getName() const override          { return "LT PULSAR"; }
    bool acceptsMidi() const override                    { return true; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    Curve curve;
    Preset loaded;        // the preset last loaded/saved, to show "modified"

    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* rateParam = nullptr;
    std::atomic<float>* midiParam = nullptr;
    std::atomic<float>* audioParam = nullptr;
    lt::dsp::FocusParams::Handles focus;
    std::atomic<float> hostBpm { 120.0f };
    std::atomic<bool> hostPlaying { false };
    std::atomic<bool> usable { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace pulsar
