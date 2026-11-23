#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include <lt_dsp/lt_dsp.h>
#include "Params.h"
#include "CoronaEngine.h"

namespace corona
{

/** LT CORONA: variable-mu tube compressor. */
class Processor : public juce::AudioProcessor
{
public:
    Processor();

    juce::AudioProcessorValueTreeState state;
    lt::presets::ParamPresetLibrary presets { "CORONA", "LT CORONA", factoryPresets() };
    lt::licensing::LicenseManager license { lt::licensing::Products::corona, "LT CORONA" };

    juce::String presetName { "Default" };
    float uiScale = 0.8f;                     // SIZE: 0.64 / 0.8 / 1.0 of the 1376 x 832 design
    void applyPreset (const lt::presets::ParamPreset& preset) { preset.apply (state, false); presetName = preset.name; }
    bool isPresetModified();
    void refreshLicense()                     { license.refresh(); usable = license.isUsable(); }
    std::function<void()> onStateRestored;
    float getGainReductionDb() const noexcept { return engine.getGainReductionDb(); }

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override          { return "LT CORONA"; }
    bool acceptsMidi() const override                    { return false; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 0.1; }

    int getNumPrograms() override                        { return (int) factoryPresets().size(); }
    int getCurrentProgram() override                     { return juce::jmax (0, presets.indexOf (presetName)); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    Engine engine;
    std::map<juce::String, std::atomic<float>*> p;
    float v (const char* id) const { return p.at (id)->load(); }
    std::atomic<bool> usable { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace corona
