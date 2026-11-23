#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <lt_shared/lt_shared.h>
#include <lt_dsp/lt_dsp.h>
#include "Params.h"
#include "Strokes.h"

namespace meteor
{

using Buffer = juce::AudioBuffer<float>;
using BufferPtr = std::shared_ptr<const Buffer>;

/** LT METEOR: live voice FX (reverse, doubles, chops, glitch) + strokes rendered to drag into the DAW. */
class Processor : public juce::AudioProcessor,
                  private juce::Thread
{
public:
    Processor();
    ~Processor() override;

    juce::AudioProcessorValueTreeState state;
    lt::presets::ParamPresetLibrary presets { "METEOR", "LT METEOR", factoryPresets() };
    MeteorEngine engine;
    lt::licensing::LicenseManager license { lt::licensing::Products::meteor, "LT METEOR" };

    juce::String presetName { "Default" };
    bool lightTheme = false;
    int page = 0;                                    // 0 LIVE, 1 STROKES (remembered in the session)
    void applyPreset (const lt::presets::ParamPreset& preset) { preset.apply (state, false); presetName = preset.name; }
    bool isPresetModified();
    void refreshLicense()                    { license.refresh(); usable = license.isUsable(); }
    std::function<void()> onStateRestored;
    float v (const char* id) const           { return p.at (id)->load(); }
    double getBpm() const noexcept           { return bpmNow.load(); }

    // ---- strokes: source
    bool loadSourceFile (const juce::File&, juce::String& error);
    void startCapture();
    void stopCapture();                       // keeps what was recorded as the source
    bool isCapturing() const noexcept         { return capturing.load(); }
    double getCaptureSeconds() const;
    bool hasSource() const;
    juce::String getSourceName() const;
    BufferPtr getSourceForDisplay() const;

    // ---- strokes: rendering (background thread)
    void requestRender();
    bool isRendering() const noexcept         { return rendering.load(); }
    int getRenderGeneration() const noexcept  { return generation.load(); }
    BufferPtr getStroke (int index) const;

    // ---- strokes: preview + export
    void preview (int index);                 // -1 stops
    int getPreviewIndex() const noexcept      { return previewIndex.load(); }
    float getPreviewProgress() const;
    juce::File exportStroke (int index);      // WAV in Music/LT METEOR (empty file on failure)
    static juce::File outputFolder();

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (Buffer&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override          { return "LT METEOR"; }
    bool acceptsMidi() const override                    { return false; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 0.2; }

    int getNumPrograms() override                        { return (int) factoryPresets().size(); }
    int getCurrentProgram() override                     { return juce::jmax (0, presets.indexOf (presetName)); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    void run() override;
    LiveSettings liveSettings() const;
    void setSource (Buffer original, double rate, const juce::String& name, const juce::File& file);

    std::map<juce::String, std::atomic<float>*> p;
    std::atomic<bool> usable { true };
    std::atomic<double> bpmNow { 120.0 }, sampleRate { 44100.0 };

    // source (message thread + render thread)
    mutable juce::CriticalSection sourceLock;
    BufferPtr originalSource, displaySource;
    double originalRate = 44100.0;
    juce::String sourceName;
    juce::File sourceFile;

    // capture (audio thread writes, message thread reads after stop)
    Buffer captureBuffer;
    std::atomic<int> captureLength { 0 };
    std::atomic<bool> capturing { false };
    double captureRate = 44100.0;

    // results
    mutable juce::CriticalSection resultLock;
    std::array<BufferPtr, Strokes::count> results;
    std::atomic<int> generation { 0 }, requested { 0 };
    std::atomic<bool> rendering { false };
    juce::WaitableEvent wake;

    // preview (message thread sets, audio thread plays)
    juce::SpinLock previewLock;
    BufferPtr previewBuffer;
    std::atomic<int> previewIndex { -1 }, previewPos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace meteor
