#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include <lt_dsp/lt_dsp.h>
#include "Params.h"
#include "VoidEngine.h"

namespace lt_void
{

/** LT VOID: high-end algorithmic reverb with 12 types. */
class Processor : public juce::AudioProcessor
{
public:
    Processor();

    juce::AudioProcessorValueTreeState state;
    lt::presets::ParamPresetLibrary presets { "VOID", "LT VOID", factoryPresets() };
    VoidEngine engine;
    lt::licensing::LicenseManager license { lt::licensing::Products::voidVerb, "LT VOID" };

    juce::String presetName { "Infinite Cloud" };
    void applyPreset (const lt::presets::ParamPreset&);
    bool isPresetModified();
    void refreshLicense()                                { license.refresh(); usable = license.isUsable(); }
    std::function<void()> onStateRestored;
    float getHostBpm() const noexcept                    { return hostBpm.load(); }

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override          { return "LT VOID"; }
    bool acceptsMidi() const override                    { return false; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 30.0; }

    int getNumPrograms() override                        { return (int) factoryPresets().size(); }
    int getCurrentProgram() override                     { return juce::jmax (0, presets.indexOf (presetName)); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    std::map<juce::String, std::atomic<float>*> p;
    float v (const char* id) const { return p.at (id)->load(); }
    std::atomic<float> hostBpm { 120.0f };
    std::atomic<bool> usable { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace lt_void
