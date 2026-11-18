#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include <lt_dsp/lt_dsp.h>
#include "Params.h"

namespace nova
{

/** LT NOVA: saturation with 8 analog characters, Frequency Focus and auto gain. */
class Processor : public juce::AudioProcessor
{
public:
    Processor();

    juce::AudioProcessorValueTreeState state;
    lt::presets::ParamPresetLibrary presets { "NOVA", "LT NOVA", factoryPresets() };
    NovaEngine engine;
    lt::dsp::SpectrumAnalyser spectrum;
    lt::licensing::LicenseManager license { lt::licensing::Products::nova, "LT NOVA" };

    juce::String presetName { "Default" };
    bool lightTheme = false;
    void applyPreset (const lt::presets::ParamPreset& preset) { preset.apply (state); presetName = preset.name; }
    bool isPresetModified();
    void refreshLicense()                    { license.refresh(); usable = license.isUsable(); }
    std::function<void()> onStateRestored;

    float getInputLevel (int ch) const noexcept  { return inLevel[(size_t) (ch & 1)].load(); }
    float getOutputLevel (int ch) const noexcept { return outLevel[(size_t) (ch & 1)].load(); }

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override          { return "LT NOVA"; }
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

private:
    std::map<juce::String, std::atomic<float>*> p;
    float v (const char* id) const { return p.at (id)->load(); }
    lt::dsp::FocusParams::Handles focus;
    lt::dsp::FocusSplitter splitter;

    /** Delays the un-processed band by the engine latency so both bands stay aligned. */
    struct RestDelay
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> line { 1 << 14 };
        void prepare (double sr, int block, int ch)
        {
            line.prepare ({ sr, (juce::uint32) block, (juce::uint32) ch });
            line.reset();
        }
        void process (juce::AudioBuffer<float>& b, int samples)
        {
            line.setDelay ((float) samples);
            for (int c = 0; c < b.getNumChannels(); ++c)
            {
                auto* x = b.getWritePointer (c);
                for (int i = 0; i < b.getNumSamples(); ++i) { line.pushSample (c, x[i]); x[i] = line.popSample (c); }
            }
        }
    } restDelay;
    juce::AudioBuffer<float> target;
    std::atomic<bool> usable { true };
    std::array<std::atomic<float>, 2> inLevel {}, outLevel {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace nova
