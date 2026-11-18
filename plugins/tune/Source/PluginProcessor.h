#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include <lt_dsp/lt_dsp.h>
#include "Params.h"
#include "TuneEngine.h"
#include "MatchAnalysis.h"

namespace tune
{

/** LT TUNE: real-time vocal tuner with formant preservation and MATCH REFERENCE. */
class Processor : public juce::AudioProcessor
{
public:
    Processor();

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState state;
    lt::presets::ParamPresetLibrary presets { "TUNE", "LT TUNE", factoryPresets() };
    TuneEngine engine;
    lt::licensing::LicenseManager license { lt::licensing::Products::tune, "LT TUNE" };

    juce::String presetName { "Default" };
    bool lightTheme = false;
    /** VOICE, MODE and the notes are not part of presets (they belong to the singer / session). */
    void applyPreset (const lt::presets::ParamPreset& preset) { preset.apply (state, false); presetName = preset.name; }
    bool isPresetModified();
    void refreshLicense()                    { license.refresh(); usable = license.isUsable(); }
    std::function<void()> onStateRestored;

    /** Last MATCH REFERENCE analysis (kept while the plugin is open, not saved). */
    std::shared_ptr<MatchResult> lastMatch;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override          { return "LT TUNE"; }
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
    TuneSettings readSettings();
    void configureEngine (int voice, bool hq);

    std::map<juce::String, std::atomic<float>*> p;
    float v (const char* id) const { return p.at (id)->load(); }
    std::atomic<bool> usable { true };
    double sampleRate = 44100.0;
    int blockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace tune
