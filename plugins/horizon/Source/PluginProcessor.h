#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include "Recorder.h"

namespace horizon
{

/** Settings shared by every LT HORIZON instance (export folder, defaults). */
struct GlobalSettings
{
    static juce::PropertiesFile& get();
    static juce::File exportFolder();
    static void setExportFolder (const juce::File&);
    static bool bpmInName();
    static void setBpmInName (bool);
    static double defaultLength();
    static void setDefaultLength (double);
};

/** LT HORIZON: a rolling sampler. Audio passes through untouched; the last N seconds are always kept. */
class Processor : public juce::AudioProcessor,
                  private juce::AsyncUpdater
{
public:
    Processor();

    Recorder recorder;
    lt::licensing::LicenseManager license { lt::licensing::Products::horizon, "LT HORIZON" };

    void refreshLicense()                         { license.refresh(); usable = license.isUsable(); }
    double getHostBpm() const noexcept            { return hostBpm.load(); }

    /** Message thread: plays [start, end) through the plugin output. */
    void startPreview (juce::Range<juce::int64>);
    void stopPreview()                            { previewPos.store (-1); }
    juce::int64 getPreviewPosition() const        { return previewPos.load(); }
    bool isPreviewing() const                     { return previewPos.load() >= 0; }

    /** Writes [start, end) to the export folder. Returns the file, or {} on failure. */
    juce::File exportRange (juce::Range<juce::int64>, juce::String& error);

    void setLength (double seconds);

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override          { return "LT HORIZON"; }
    bool acceptsMidi() const override                    { return false; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 0.0; }

    int getNumPrograms() override                        { return 1; }
    int getCurrentProgram() override                     { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override     { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    void handleAsyncUpdate() override             { setLength (wantedLength.load()); }

    std::atomic<bool> usable { true };
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<juce::int64> previewPos { -1 }, previewEnd { 0 };
    std::atomic<double> wantedLength { Recorder::defaultSeconds };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace horizon
