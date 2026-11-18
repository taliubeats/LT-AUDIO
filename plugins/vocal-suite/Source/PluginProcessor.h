#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include <lt_dsp/lt_dsp.h>
#include "Params.h"
#include "Assistant.h"

namespace vocal
{

/** LT VOCAL SUITE: a complete vocal chain with an AI assistant. */
class Processor : public juce::AudioProcessor,
                  private juce::AudioProcessorValueTreeState::Listener,
                  private juce::AsyncUpdater
{
public:
    Processor();
    ~Processor() override;

    juce::AudioProcessorValueTreeState state;
    lt::presets::ParamPresetLibrary presets { "VOCALSUITE", "LT VOCAL SUITE", factoryPresets() };
    VocalChain chain;
    Assistant assistant;
    lt::licensing::LicenseManager license { lt::licensing::Products::vocalSuite, "LT VOCAL SUITE" };

    juce::String presetName { "Default" };
    bool lightTheme = false;
    juce::String lastAssistantMessage;
    bool isPresetModified();
    void refreshLicense()                    { license.refresh(); usable = license.isUsable(); }
    std::function<void()> onStateRestored;

    // ---- values / undo (message thread)
    std::map<juce::String, float> currentValues() const;
    void applyValues (const std::map<juce::String, float>&, bool addUndo = true);
    void applyPreset (const lt::presets::ParamPreset&);
    void pushUndo();
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    void undo();
    void redo();

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override          { return "LT VOCAL SUITE"; }
    bool acceptsMidi() const override                    { return false; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 6.0; }

    int getNumPrograms() override                        { return (int) factoryPresets().size(); }
    int getCurrentProgram() override                     { return juce::jmax (0, presets.indexOf (presetName)); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    void parameterChanged (const juce::String&, float) override;
    void reconfigure();
    void handleAsyncUpdate() override { reconfigure(); }
    ChainSettings readSettings (double bpm) const;

    std::map<juce::String, std::atomic<float>*> p;
    float v (const char* id) const { return p.at (id)->load(); }
    double sampleRate = 44100.0;
    int blockSize = 512;
    std::atomic<bool> usable { true };
    std::vector<std::map<juce::String, float>> undoStack, redoStack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace vocal
