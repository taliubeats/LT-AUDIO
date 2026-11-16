#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <lt_shared/lt_shared.h>
#include "Analysis.h"

namespace typebeats
{

/**
    LT TYPE BEATS: analyses a song file (BPM, key, chords). Audio from the track passes
    through untouched; a loaded song can be previewed through the plugin's output.
*/
class Processor : public juce::AudioProcessor,
                  private juce::Thread
{
public:
    Processor();
    ~Processor() override;

    // ---- analysis (message thread API)
    void loadFile (const juce::File&);
    const juce::File& getLoadedFile() const noexcept     { return loadedFile; }
    const AnalysisResult& getResult() const noexcept     { return result; }
    AnalysisResult& getEditableResult() noexcept         { return result; }
    bool isAnalysing() const noexcept                    { return analysing.load(); }
    float getProgress() const noexcept                   { return progress.load(); }
    juce::String getStatus() const                       { const juce::ScopedLock sl (statusLock); return status; }
    void setStatus (const juce::String&);

    std::function<void()> onResultChanged;

    // ---- preview playback
    void togglePreview (double fromSeconds);
    void stopPreview()                                   { previewing = false; }
    bool isPreviewing() const noexcept                   { return previewing.load(); }
    double getPreviewPosition() const noexcept;

    // ---- settings
    bool lightTheme = false;
    juce::File getDownloadFolder() const;
    void setDownloadFolder (const juce::File& f)         { downloadFolder = f; }

    lt::licensing::LicenseManager license { lt::licensing::Products::typeBeats, "LT TYPE BEATS" };
    juce::AudioFormatManager formats;

    // ---- AudioProcessor
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }

    const juce::String getName() const override          { return "LT TYPE BEATS"; }
    bool acceptsMidi() const override                    { return false; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 0.0; }

    int getNumPrograms() override                        { return 1; }
    int getCurrentProgram() override                     { return 0; }
    void setCurrentProgram (int) override                {}
    const juce::String getProgramName (int) override     { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    void run() override;
    void preparePreview();

    std::shared_ptr<bool> alive = std::make_shared<bool> (true);  // guards async callbacks after deletion
    juce::File loadedFile, pendingFile, downloadFolder;
    AnalysisResult result;
    std::atomic<bool> analysing { false };
    std::atomic<float> progress { 0.0f };
    juce::CriticalSection statusLock;
    juce::String status;

    // preview: the song resampled to the host rate
    juce::AudioBuffer<float> fileAudio;     // at file rate (kept for re-preparing)
    double fileRate = 44100.0;
    juce::AudioBuffer<float> previewAudio;
    juce::CriticalSection previewLock;
    std::atomic<bool> previewing { false };
    std::atomic<juce::int64> previewPos { 0 };
    double hostRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace typebeats
