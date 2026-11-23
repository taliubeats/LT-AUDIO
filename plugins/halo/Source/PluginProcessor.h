#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include <lt_dsp/lt_dsp.h>
#include "Params.h"
#include "Session.h"

namespace halo
{

/**
    LT HALO: drop a vocal, the plugin composes harmonies / production layers / a choir and sings them with the same
    voice. Everything is previewed inside the plugin (its output) and dragged or exported to the DAW as WAV.
    The track's own audio always passes through untouched.
*/
class Processor : public juce::AudioProcessor
{
public:
    Processor();
    ~Processor() override;

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState state;
    lt::presets::ParamPresetLibrary presets { "HALO", "LT HALO", factoryPresets() };
    lt::licensing::LicenseManager license { lt::licensing::Products::halo, "LT HALO" };
    Session session;

    juce::String presetName { "Pop Thirds" };
    std::atomic<int> tab { 0 };                   // 0 HARMONY, 1 CHOIR
    std::array<unsigned long long, numStrips> seeds {};
    unsigned long long choirSeed = 1;

    void applyPreset (const lt::presets::ParamPreset& p) { p.apply (state, false); presetName = p.name; }
    bool isPresetModified();
    void refreshLicense()                         { license.refresh(); usable = license.isUsable(); }
    std::function<void()> onStateRestored;

    // ---- specs from the current parameters
    StripSpec stripSpec (int index) const;
    bool stripEnabled (int index) const;
    ChoirSpec choirSpec() const;
    GlobalSpec globalSpec() const;
    /** level / pan / width as heard; asHeard = also apply power / mute / solo (false = this voice on its own). */
    Session::StripGains stripGains (int index, bool asHeard = true) const;
    Key currentKey() const;

    void generateHarmony (int onlyStrip = -1, bool regenerate = false);
    void generateChoir (bool regenerate = false);

    // ---- preview transport (message thread writes, audio thread reads)
    void play (bool fromStart = false);
    void stop();
    bool isPlaying() const noexcept               { return playing.load(); }
    double getPlayPositionSeconds() const;
    void setPlayPositionSeconds (double);
    std::atomic<int> soloStrip { -1 };            // moon click: listen to one voice

    // ---- USE TRACK: record the incoming audio as the vocal
    void startCapture();
    void stopCapture();
    bool isCapturing() const noexcept             { return capturing.load(); }
    double getCaptureSeconds() const;

    float getInputLevel() const noexcept          { return inLevel.load(); }
    float getOutputLevel() const noexcept         { return outLevel.load(); }
    double getHostBpm() const noexcept            { return hostBpm.load(); }

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override          { return "LT HALO"; }
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
    float v (const juce::String& id) const;

    std::atomic<bool> usable { true }, playing { false }, capturing { false };
    std::atomic<juce::int64> playPos { 0 };
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f };
    std::atomic<double> hostBpm { 0.0 };
    double sampleRate = 44100.0;

    juce::AudioBuffer<float> captureBuffer;
    std::atomic<int> captureLength { 0 };
    double captureRate = 44100.0;

    // smoothed per-strip gains used by the audio thread
    std::array<juce::LinearSmoothedValue<float>, numStrips> gainL, gainR;
    juce::LinearSmoothedValue<float> leadGain, choirGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace halo
