#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

namespace warp
{

enum class Kind { audio, video, image, unknown };

enum class Target { mp3, wav16, wav24, flac, aiff, m4a, opus, jpg, png, webp, tiff, heic, count };

struct Options
{
    int mp3Kbps = 320, aacKbps = 256, opusKbps = 160;
    int sampleRate = 0;          // 0 = keep the source rate
    int imageQuality = 90;       // 1..100 (JPG / WEBP / HEIC)
    double trimStart = 0;        // seconds; 0 / 0 = whole file
    double trimEnd = 0;
};

/** The command line tools the converter drives. Bundled copies win over system ones. */
struct Tools
{
    juce::File ffmpeg, ffprobe, sips;
    bool hasAacAt = false;       // Apple AudioToolbox AAC encoder inside ffmpeg
    static Tools locate();
    bool ready() const { return ffmpeg.existsAsFile() && ffprobe.existsAsFile(); }
};

Kind kindOf (const juce::File&);
bool isAudioTarget (Target);
juce::String targetLabel (Target);       // "WAV 24", "M4A"...
juce::String targetExtension (Target);   // "wav", "m4a"...
/** Can this kind of input become this target? (video -> audio extracts the sound; video -> image grabs a frame) */
bool canConvert (Kind, Target, const Tools&);

/** A free output path: "name.ext", else "name (2).ext", ... */
juce::File uniqueOutput (const juce::File& dir, const juce::String& stem, const juce::String& ext);

/** The commands that turn one file into one target. Steps run in order; temp files are deleted afterwards. */
struct Plan
{
    std::vector<juce::StringArray> steps;
    juce::File output;
    juce::Array<juce::File> temps;
    juce::String error;          // set when the conversion is not possible
};
Plan makePlan (const juce::File& input, Target, const Options&, const Tools&, const juce::File& outputDir);

/** Duration in seconds (ffprobe), or 0. */
double probeDuration (const juce::File&, const Tools&);

/** Runs one plan; progress 0..1. Returns an empty string on success, else the error. */
juce::String runPlan (const Plan&, double durationSeconds, std::function<void (float)> progress, std::atomic<bool>& cancel);

//==============================================================================
/** The queue: files in, conversions out, on a small pool of worker threads. */
class Queue
{
public:
    enum class State { waiting, running, done, failed, cancelled };
    struct Item
    {
        int id = 0;
        juce::File input, output;
        Kind kind = Kind::unknown;
        Target target = Target::mp3;
        State state = State::waiting;
        float progress = 0;
        juce::String message;
    };

    explicit Queue (int threads = 2);
    ~Queue();

    Tools tools;
    Options options;
    juce::File outputDir;        // empty = next to each input

    /** Adds files (folders are scanned one level deep). Returns how many were added. */
    int add (const juce::StringArray& paths, Target audioTarget, Target imageTarget);
    void setTarget (int id, Target);
    void remove (int id);
    void clearFinished();
    void startAll();
    void cancelAll();
    bool isBusy() const;

    std::vector<Item> snapshot() const;
    std::function<void()> onChange;   // called on the message thread

private:
    struct Worker;
    void notify();
    mutable juce::CriticalSection lock;
    std::vector<Item> items;
    std::map<int, std::shared_ptr<std::atomic<bool>>> cancels;
    int nextId = 1;
    juce::ThreadPool pool;
};

} // namespace warp
