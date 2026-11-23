#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include "Params.h"
#include "Kit.h"
#include "Sampler.h"
#include "CloneEngine.h"
#include "Patterns.h"
#include "SimilarSearch.h"

namespace eclipse
{

/** LT ECLIPSE: drum-kit cloner + 64-pad sampler. */
class Processor : public juce::AudioProcessor,
                  private juce::Thread,
                  private juce::AsyncUpdater
{
public:
    Processor();
    ~Processor() override;

    juce::AudioProcessorValueTreeState state;
    lt::licensing::LicenseManager license { lt::licensing::Products::eclipse, "LT ECLIPSE" };
    Kit kit;

    // ---------------------------------------------------------------- UI state (saved)
    juce::String skinId { "negra" };
    float uiScale = 0.8f;
    int selectedPad = 0;
    juce::String kitName { "New kit" };
    bool splitKits = true;                    // a song with several kits: one bank per kit
    std::function<void()> onStateRestored, onKitChanged;   // message thread

    const Skin& skin() const { return skinById (skinId); }
    void setSkin (const juce::String& id);

    // ---------------------------------------------------------------- clone
    void cloneFile (const juce::File&, bool ai);
    void recloneSource (bool ai);
    bool isCloning() const { return cloning.load(); }
    float cloneProgress() const { return progress.load(); }
    juce::String cloneStatus() const { const juce::ScopedLock sl (statusLock); return status; }
    juce::String lastError;

    // ---------------------------------------------------------------- pads
    void selectHit (int pad, int hitIndex);           // HIT < n of N >
    int hitCount (int pad) const;
    void loadSampleToPad (int pad, const juce::File&);
    void movePad (int from, int to, bool copy);
    void setPadType (int pad, Family);
    void clearPad (int pad);
    void triggerPad (int pad, float velocity);        // UI click / preview
    juce::File exportPad (int pad);                   // temp WAV for drag & drop
    uint32_t padHitCount (int pad) const { return sampler.hits[(size_t) pad].load(); }

    // ---------------------------------------------------------------- patterns
    double bpm() const { return bpmValue.load(); }
    void setBpm (double b) { bpmValue = juce::jlimit (40.0, 240.0, b); }
    void setPreviewPlaying (bool);
    bool isPreviewPlaying() const { return previewOn.load(); }
    double previewTick() const { return previewPos.load(); }          // 1/64 ticks into the pattern
    void patternEdited();                                             // after the UI changed kit.patterns
    juce::File exportPattern (int index);                             // temp MIDI file
    std::vector<Pattern> history;                                     // VARY history of the current pattern (max 30)
    std::array<Family, numPads> padFamilies() const;

    // ---------------------------------------------------------------- FIND SIMILAR
    SimilarSearch similar;
    void previewFile (const juce::File&);             // listen to a sample of the library
    void stopPreview() { std::atomic_store (&preview, std::shared_ptr<const juce::AudioBuffer<float>>()); }

    // ---------------------------------------------------------------- kits on disk
    juce::File kitsFolder() const;
    bool saveKit (const juce::String& name);
    bool loadKit (const juce::File&);

    void refreshLicense() { license.refresh(); usable = license.isUsable(); }

    // ---------------------------------------------------------------- AudioProcessor
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override          { return "LT ECLIPSE"; }
    bool acceptsMidi() const override                    { return true; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 2.0; }

    int getNumPrograms() override                        { return 1; }
    int getCurrentProgram() override                     { return 0; }
    void setCurrentProgram (int) override                {}
    const juce::String getProgramName (int) override     { return kitName; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    double sampleRateForUi() const { return sourceRate; }
    float getOutputLevel (int ch) const { return meter[(size_t) juce::jlimit (0, 1, ch)].load(); }

private:
    static BusesProperties makeBuses();
    void run() override;                              // clone thread
    void handleAsyncUpdate() override;                // results back on the message thread
    void applyClone (CloneResult&&, const juce::String& name, const juce::File& file,
                     std::shared_ptr<juce::AudioBuffer<float>> source, double sr, bool ai);
    void readPadSettings();
    void kitChanged();

    Sampler sampler;
    std::array<PadParamPtrs, numPads> padPtrs;
    std::array<PadSettings, numPads> padSettings;
    std::atomic<float>* outputDb = nullptr;
    std::atomic<float>* sensitivity = nullptr;
    std::atomic<bool> usable { true };
    std::array<std::atomic<float>, 2> meter {};

    // pattern preview
    std::atomic<bool> previewOn { false };
    std::atomic<double> previewPos { 0 }, bpmValue { 120 };
    std::shared_ptr<const Pattern> previewPattern;   // swapped atomically
    juce::SpinLock uiMidiLock;
    juce::MidiBuffer uiMidi;                          // pad clicks from the UI

    // clone job
    struct Job { juce::File file; std::shared_ptr<juce::AudioBuffer<float>> audio; double sr = 44100; bool ai = false; float sens = 0.5f; juce::String name; bool restoreOnly = false;
                 std::shared_ptr<juce::AudioBuffer<float>> stem; std::array<std::shared_ptr<juce::AudioBuffer<float>>, 4> parts {}; };
    struct Pending { CloneResult res; juce::String name; juce::File file; std::shared_ptr<juce::AudioBuffer<float>> source; double sr = 44100; bool ai = false, restoreOnly = false;
                     std::shared_ptr<juce::AudioBuffer<float>> stem; std::array<std::shared_ptr<juce::AudioBuffer<float>>, 4> parts {};
                     std::array<std::shared_ptr<juce::AudioBuffer<float>>, 3> cutParts {}; };
    std::unique_ptr<Pending> pending;
    std::vector<int> padOfGroup;
    double sourceRate = 44100;
    std::array<std::atomic<Family>, numPads> familyOfPad {};
    std::atomic<int> vibeType { 0 };
    juce::CriticalSection jobLock, statusLock;
    std::unique_ptr<Job> job;
    std::atomic<bool> cloning { false }, cancelJob { false };
    std::atomic<float> progress { 0 };
    juce::String status;
    std::shared_ptr<juce::AudioBuffer<float>> extractSource;
    std::shared_ptr<juce::AudioBuffer<float>> drumStem;                    // v2: the neural drum stem of the source (re-clone reuses it)
    std::array<std::shared_ptr<juce::AudioBuffer<float>>, 4> drumParts {};   // v3: its kick / snare / cymbals / toms
    std::array<std::shared_ptr<juce::AudioBuffer<float>>, 3> cutParts {};    // the parts at the analysis gain (HIT cuts from them)
    std::shared_ptr<const juce::AudioBuffer<float>> preview;    // FIND SIMILAR audition (at the host rate)
    std::atomic<int> previewPosSamples { 0 };        // v2: the neural drum stem of the source (re-clone reuses it)   // the audio the one-shots come from (for HIT)

    double sampleRate = 44100;

    JUCE_DECLARE_WEAK_REFERENCEABLE (Processor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace eclipse
