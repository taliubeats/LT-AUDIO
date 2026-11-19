#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include "Params.h"
#include "SynthEngine.h"
#include "Fx.h"
#include "Presets.h"
#include "LfoShape.h"

namespace quasar
{

/** LT QUASAR: wavetable / analog / sample synth with a Serum-style mixer, matrix and effect buses. */
class Processor : public juce::AudioProcessor,
                  private juce::Timer,
                  private juce::AudioProcessorParameter::Listener
{
public:
    Processor();
    ~Processor() override;

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState state;
    LfoTables lfoTables;
    SynthEngine engine { state, lfoTables };
    std::array<std::unique_ptr<FxChain>, numBuses> chains;
    PresetLibrary presets;
    lt::licensing::LicenseManager license { lt::licensing::Products::quasar, "LT QUASAR" };

    // ---- extra state (message thread)
    juce::String presetName { "Init" }, presetBank { "QUASAR FACTORY" };
    juce::StringArray macroNames;
    std::array<juce::String, numOscs> samplePaths, userTablePaths;
    juce::String lastPage { "OSC" };
    bool simpleView = false;

    void setLfoShape (int lfo, const LfoShape&);
    bool loadSample (int osc, const juce::File&, juce::String& error);
    bool loadUserTable (int osc, const juce::File&, juce::String& error);
    void clearSample (int osc);

    Preset capturePreset (const juce::String& name) const;
    void applyPreset (const Preset&);
    void initPatch();
    bool isPresetModified() const;
    std::function<void()> onStateRestored;   // called on the message thread
    void refreshLicense() { license.refresh(); usable = license.isUsable(); }

    /** Adds a modulation route (drag and drop). Returns the slot or -1 if the matrix is full. */
    int addModulation (int source, int dest, float amountPct);
    void removeModulation (int slot);
    int findModulation (int source, int dest) const;
    struct Route { int slot, source; float amount; bool bipolar; };
    /** Routes that modulate a destination (message thread, cheap: reads cached parameter pointers). */
    std::vector<Route> routesFor (int dest) const;
    juce::RangedAudioParameter* param (const juce::String& id) const { return dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)); }
    void setParam (const juce::String& id, float realValue);

    // meters (any thread)
    float getOutputLevel (int ch) const noexcept { return outLevel[(size_t) (ch & 1)].load(); }
    float getBusLevel (int bus, int ch) const noexcept { return busLevel[(size_t) bus][(size_t) (ch & 1)].load(); }
    float getCpuLoad() const noexcept { return cpu.load(); }
    float getLimiterReduction() const noexcept { return limiterGr.load(); }

    // juce::AudioProcessor
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); for (auto& c : chains) c->reset(); }
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                  { return true; }
    const juce::String getName() const override      { return "LT QUASAR"; }
    bool acceptsMidi() const override                { return true; }
    bool producesMidi() const override               { return false; }
    bool isMidiEffect() const override               { return false; }
    double getTailLengthSeconds() const override     { return 4.0; }
    int getNumPrograms() override                    { return 1; }
    int getCurrentProgram() override                 { return 0; }
    void setCurrentProgram (int) override            {}
    const juce::String getProgramName (int) override { return presetName; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    /** For offline renders and tests: makes effect changes take effect now. */
    void serviceNow();

private:
    void timerCallback() override;
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int, bool starting) override;
    juce::ValueTree extraState() const;
    void restoreExtra (const juce::ValueTree&);

    std::array<juce::AudioBuffer<float>, numBuses> buses;
    juce::AudioBuffer<float> chunk;
    std::array<std::atomic<float>*, numBuses> busLevelP {}, busPanP {}, busMuteP {}, busSoloP {};
    std::atomic<float>* masterP = nullptr, *limiterP = nullptr;
    std::array<juce::RangedAudioParameter*, numBuses> busLevelParam {};
    std::array<std::atomic<float>*, numModSlots> modSrcP {}, modDstP {}, modAmtP {}, modBipP {}, modOnP {};
    juce::RangedAudioParameter* masterParameter = nullptr;
    int masterDest = 0;
    std::array<int, numBuses> busLevelDest {}, busPanDest {};
    std::array<dsp::Ramp, numBuses> busGainL, busGainR;
    dsp::Ramp masterGain;
    float limEnv = 0;
    std::array<dsp::DcBlocker, 2> dc;
    double sampleRate = 44100.0;
    std::atomic<bool> usable { true };
    std::array<std::atomic<float>, 2> outLevel {};
    std::array<std::array<std::atomic<float>, 2>, numBuses> busLevel {};
    std::atomic<float> cpu { 0 }, limiterGr { 0 };
    Preset loadedPreset;

    JUCE_DECLARE_WEAK_REFERENCEABLE (Processor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace quasar
