#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"

namespace halo
{

/** Immutable audio handed to the audio thread for the preview (already at the host sample rate). */
struct PlaybackData
{
    double rate = 44100.0;
    size_t length = 0;
    std::vector<float> lead;
    std::array<Stereo, numStrips> strips;
    std::array<bool, numStrips> stripValid {};
    std::array<Stereo, 4> choirSections;
    bool choirValid = false;
};

/** What the UI needs to draw a strip's moon / line. */
struct StripInfo
{
    bool valid = false, dirty = true;
    juce::String label;
    std::vector<int> notes;      // harmony MIDI notes (one per lead note)
};

/**
    LT HALO session: the dropped vocal, its analysis and lyrics, the generated voices and the choir.
    Heavy work runs on a background thread; results are swapped in atomically for the audio thread.
*/
class Session : public juce::ChangeBroadcaster,
                private juce::AsyncUpdater
{
public:
    Session();
    ~Session() override;

    // ---- loading (message thread)
    bool loadFile (const juce::File&);
    void loadBuffer (const juce::AudioBuffer<float>&, double sampleRate, const juce::String& name);
    void restore (const juce::File& vocal);     // from saved state: reload + analyse quietly

    bool hasVocal() const;
    juce::File getVocalFile() const;
    juce::String getVocalName() const;
    std::shared_ptr<const Analysis> getAnalysis() const;
    Key getDetectedKey() const;
    double getBpm() const;

    /** Use another key (KEY & SCALE or DETECT). Recomputes the implied chords; voices become dirty. */
    void setKey (const Key&);

    // ---- lyrics (refuerzo)
    std::vector<Word> getWords() const;
    void toggleWordReinforce (int index);
    bool lyricsAvailable() const;

    // ---- generation
    struct Request
    {
        std::array<StripSpec, numStrips> specs;
        std::array<bool, numStrips> enabled {};
        std::array<bool, numStrips> force {};     // REGEN
        GlobalSpec global;
        bool best = false;
    };
    void generate (const Request&);
    void generateChoir (const ChoirSpec&);
    void cancel();

    StripInfo getStripInfo (int index, const StripSpec& current) const;
    bool isStripDirty (int index, const StripSpec& current) const;
    bool choirReady() const;
    bool choirDirty (const ChoirSpec&) const;

    // ---- status (any thread)
    bool isBusy() const noexcept               { return busy.load(); }
    float getProgress() const noexcept         { return progress.load(); }
    juce::String getStatus() const;
    juce::String getError() const;

    // ---- playback
    void setPlaybackRate (double hostRate);
    std::shared_ptr<const PlaybackData> getPlayback() const { return std::atomic_load (&playback); }

    // ---- export (message thread). Gains are what the user hears (level/pan/width applied).
    struct StripGains { float gl = 1, gr = 1; bool audible = true; };
    // fresh = never overwrite (a new name per drag); peak = loudest sample written (0 = silent file)
    bool stripReady (int index) const;
    juce::File writeStrip (int index, StripGains, const juce::File& folder = {}, bool fresh = false, float* peak = nullptr);
    juce::File writeMix (const std::array<StripGains, numStrips>&, const juce::File& folder = {}, bool fresh = false,
                         float* peak = nullptr);
    juce::Array<juce::File> writeChoir (float gain, bool separateSections, const juce::File& folder = {}, bool fresh = false);
    juce::File defaultExportFolder() const;

    juce::AudioFormatManager formats;

    /** BEST quality helper (Seed-VC). Returns empty if not installed. */
    static juce::File aiLauncher();
    /** Whisper model for lyrics. Returns empty if not installed. */
    static juce::File lyricsModel();
    static juce::File dataFolder();

private:
    void handleAsyncUpdate() override;
    void runJob (std::function<void()>);
    void analyseNow (std::vector<double> mono, int sr, juce::String name, juce::File file);
    void transcribeNow();
    void rebuildPlayback();
    bool stopRequested() const { return cancelFlag.load(); }
    void setStatus (const juce::String&);
    bool renderBest (const Request&, const std::vector<int>& stripIndices, std::vector<Stereo>& buffers);

    mutable juce::CriticalSection lock;
    juce::File vocalFile;
    juce::String vocalName;
    std::shared_ptr<Analysis> analysis;
    Key detectedKey;
    std::vector<Word> words;
    double bpm = 0;

    struct StripResult
    {
        bool valid = false;
        StripSpec spec;
        GlobalSpec global;
        bool best = false;
        Stereo audio;
        std::vector<int> notes;
        int analysisVersion = 0;
    };
    std::array<StripResult, numStrips> results;
    int analysisVersion = 0;

    ChoirSpec choirSpec;
    bool choirValid = false;
    int choirVersion = -1;
    std::array<Stereo, 4> choirSections;

    double playRate = 44100.0;
    std::shared_ptr<const PlaybackData> playback;
    std::vector<std::shared_ptr<const PlaybackData>> retired;   // freed on the message thread, never on the audio thread

    juce::String status, error;
    std::atomic<bool> busy { false }, cancelFlag { false };
    std::atomic<float> progress { 0.0f };
    juce::ThreadPool worker { 1 };
    std::unique_ptr<juce::ThreadPool> renderPool;
};

} // namespace halo
