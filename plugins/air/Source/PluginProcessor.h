#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include <lt_dsp/lt_dsp.h>
#include "Params.h"

namespace air
{

/** LT AIR: vocal enhancer (BODY, MID AIR, HIGH AIR, four character models each). */
class Processor : public juce::AudioProcessor
{
public:
    Processor();

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState state;
    lt::presets::ParamPresetLibrary presets { "AIR", "LT AIR", factoryPresets() };
    AirEngine engine;
    lt::dsp::SpectrumAnalyser spectrum;     // output, for the atmosphere display
    lt::licensing::LicenseManager license { lt::licensing::Products::air, "LT AIR" };

    juce::String presetName { "Silky Pop Vocal" };
    void applyPreset (const lt::presets::ParamPreset& preset) { preset.apply (state, false); presetName = preset.name; }
    bool isPresetModified();
    void refreshLicense()                    { license.refresh(); usable = license.isUsable(); }
    std::function<void()> onStateRestored;
    float v (const char* id) const           { return p.at (id)->load(); }
    AirSettings currentSettings() const;

    // A | B: two complete settings to compare (saved with the project)
    int abSlot() const noexcept              { return abActive; }
    void switchAB (int slot);

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override          { return "LT AIR"; }
    bool acceptsMidi() const override                    { return false; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 0.0; }

    int getNumPrograms() override                        { return (int) factoryPresets().size(); }
    int getCurrentProgram() override                     { return juce::jmax (0, presets.indexOf (presetName)); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioParameterBool* getBypassParameter() const override;

private:
    using Snapshot = std::map<juce::String, float>;
    Snapshot capture() const;
    void restore (const Snapshot&);
    static juce::String toString (const Snapshot&);
    static Snapshot fromString (const juce::String&);

    std::map<juce::String, std::atomic<float>*> p;
    std::atomic<bool> usable { true };
    std::array<Snapshot, 2> ab;
    std::array<juce::String, 2> abPreset;
    int abActive = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace air
