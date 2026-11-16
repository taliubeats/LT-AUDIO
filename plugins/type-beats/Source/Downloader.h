#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace typebeats
{

/**
    Downloads the audio of a YouTube link with yt-dlp and saves it as WAV.
    yt-dlp is fetched automatically the first time (and can be updated from Settings).
    No ffmpeg needed: we ask for the m4a stream and decode it with the OS decoder.
*/
class Downloader : private juce::Thread
{
public:
    explicit Downloader (juce::AudioFormatManager&);
    ~Downloader() override;

    void download (const juce::String& url, const juce::File& destinationFolder);
    void updateTool();
    void cancel()                                { signalThreadShouldExit(); }
    bool isBusy() const                          { return isThreadRunning(); }

    // Called on the message thread
    std::function<void (const juce::String& status, float progress)> onProgress;
    std::function<void (const juce::File& wav)> onFinished;
    std::function<void (const juce::String& error)> onError;

    static juce::File getToolFile();
    static bool looksLikeYouTubeLink (const juce::String&);

private:
    void run() override;
    bool ensureTool (bool forceUpdate);
    juce::File convertToWav (const juce::File& source, const juce::File& folder);
    void post (std::function<void()>);
    void progress (const juce::String&, float);

    juce::AudioFormatManager& formats;
    juce::String url;
    juce::File folder;
    bool updateOnly = false;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};

} // namespace typebeats
