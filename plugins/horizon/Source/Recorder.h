#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace horizon
{

/**
    LT HORIZON engine: always records the last N seconds of what reaches the plugin.

    Threading:
      - push() runs on the audio thread and is lock-free (one try-lock, never waits).
      - setLength(), read(), peaks() run on the message thread.
      - Positions are absolute ("sample number since the plugin started recording"), so a
        selection stays on the same audio while the buffer keeps rolling.
*/
class Recorder
{
public:
    static constexpr int peakBlock = 256;               // samples per waveform column
    static constexpr double minSeconds = 5.0, maxSeconds = 600.0, defaultSeconds = 45.0;

    Recorder();

    /** (Re)allocates for this rate/length. Keeps nothing (positions restart at 0). Not concurrent with push(). */
    void prepare (double sampleRate, double seconds);

    /** Message thread: changes the length. Safe while the audio thread is pushing. */
    void setLength (double seconds);
    double getLengthSeconds() const noexcept     { return lengthSeconds.load(); }
    double getSampleRate() const noexcept        { return sampleRate.load(); }

    /** Audio thread. Records `numSamples` from up to two channels (mono is duplicated). */
    void push (const float* const* channels, int numChannels, int numSamples) noexcept;

    void setPaused (bool p) noexcept             { paused.store (p); }
    bool isPaused() const noexcept               { return paused.load(); }

    /** One past the newest recorded sample. */
    juce::int64 getEnd() const noexcept          { return written.load (std::memory_order_acquire); }
    /** Oldest sample that is still in memory. */
    juce::int64 getStart() const noexcept;
    juce::int64 getCapacity() const noexcept     { return capacity.load(); }

    /** Clamps [start, end) to what is available (keeps a small safety margin from the write head). */
    juce::Range<juce::int64> clampToAvailable (juce::Range<juce::int64>) const noexcept;

    /** Copies a range out (stereo). Returns an empty buffer if nothing of it is available. */
    juce::AudioBuffer<float> read (juce::Range<juce::int64>) const;

    /** Peak column (max |x| of both channels) covering absolute sample `pos`. 0 if unavailable. */
    float peakAt (juce::int64 pos) const noexcept;
    /** Max of the peak columns covering [start, end). */
    float peakOver (juce::int64 start, juce::int64 end) const noexcept;

    /** Audio thread: reads a block for preview playback (adds into `out`). Returns how many samples it read. */
    int readForPreview (juce::int64 pos, float* const* out, int numChannels, int numSamples) const noexcept;

    /** Writes a 24-bit WAV. */
    static bool writeWav (const juce::File&, const juce::AudioBuffer<float>&, double sampleRate);

    /** "LT Horizon 128 BPM 14-03-22.wav" (BPM part only when known and wanted). */
    static juce::String makeFileName (double bpm, bool withBpm, juce::Time when);

private:
    struct Store
    {
        juce::AudioBuffer<float> audio;     // 2 x capacity
        std::vector<float> peaks;           // capacity / peakBlock
        juce::int64 capacity = 0;
    };

    void allocate (double sr, double seconds);

    std::unique_ptr<Store> store;
    juce::SpinLock swapLock;               // held by the message thread only while swapping `store`
    std::atomic<juce::int64> written { 0 }, capacity { 0 };
    std::atomic<double> sampleRate { 44100.0 }, lengthSeconds { defaultSeconds };
    std::atomic<bool> paused { false };
    float pendingPeak = 0.0f;              // audio thread: peak of the column being filled
};

} // namespace horizon
