#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include <lt_dsp/lt_dsp.h>
#include "Params.h"

namespace saturn
{

/** LT SATURN: vintage tape / Mellotron ageing. */
class Processor : public juce::AudioProcessor
{
public:
    Processor();

    juce::AudioProcessorValueTreeState state;
    lt::presets::ParamPresetLibrary presets { "SATURN", "LT SATURN", factoryPresets() };
    SaturnEngine engine;
    lt::licensing::LicenseManager license { lt::licensing::Products::saturn, "LT SATURN" };

    juce::String presetName { "Default" };
    bool night = false;                      // THEME: false = CREAM panels, true = NIGHT
    void applyPreset (const lt::presets::ParamPreset& preset) { preset.apply (state); presetName = preset.name; }
    bool isPresetModified();
    void refreshLicense()                    { license.refresh(); usable = license.isUsable(); }
    std::function<void()> onStateRestored;
    float v (const char* id) const           { return p.at (id)->load(); }
    SaturnSettings currentSettings() const;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override          { return "LT SATURN v2"; }
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
    std::map<juce::String, std::atomic<float>*> p;
    std::atomic<bool> usable { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace saturn
