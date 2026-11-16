#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include <lt_dsp/lt_dsp.h>
#include "Params.h"
#include "Banks.h"

namespace nebula
{

/** LT NEBULA: granular processor (a simple alternative to Portal). */
class Processor : public juce::AudioProcessor
{
public:
    Processor();

    juce::AudioProcessorValueTreeState state;
    lt::presets::ParamPresetLibrary presets { "NEBULA", "LT NEBULA", factoryPresets() };
    NebulaEngine engine;
    Modulation mod;
    lt::licensing::LicenseManager license { lt::licensing::Products::nebula, "LT NEBULA" };

    BankLibrary banks { factoryPresets() };
    juce::String presetName { "Stardust Keys" }, bankName { "LT COSMOS" };
    bool lightTheme = false, advanced = false;
    void applyPreset (const lt::presets::ParamPreset& preset) { preset.apply (state); presetName = preset.name; }
    bool isPresetModified();
    const lt::presets::ParamPreset* findCurrent() const;
    void refreshLicense()                    { license.refresh(); usable = license.isUsable(); }
    std::function<void()> onStateRestored;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override          { return "LT NEBULA"; }
    bool acceptsMidi() const override                    { return true; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 8.0; }

    int getNumPrograms() override                        { return (int) factoryPresets().size(); }
    int getCurrentProgram() override                     { return juce::jmax (0, presets.indexOf (presetName)); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    std::map<juce::String, std::atomic<float>*> p;
    float v (const juce::String& id) const { return p.at (id)->load(); }
    float ev (const juce::String& id);   // value with modulation
    std::map<juce::String, juce::RangedAudioParameter*> params;
    std::atomic<bool> usable { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace nebula
