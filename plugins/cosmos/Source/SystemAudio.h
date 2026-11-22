#pragma once

#include <functional>
#include <memory>
#include <string>

namespace cosmos
{

/**
    Listens to everything the computer plays (Spotify, YouTube, a DAW...) without extra drivers.
    macOS 14.2+: a Core Audio process tap on the whole system mix. Elsewhere: not available.
    The callback runs on a real-time audio thread with de-interleaved stereo float audio.
*/
class SystemAudio
{
public:
    using Callback = std::function<void (const float* left, const float* right, int numSamples, double sampleRate)>;

    SystemAudio();
    ~SystemAudio();

    static bool isSupported();
    /** Starts capturing. Returns an empty string on success, otherwise the reason it could not start. */
    std::string start (Callback);
    void stop();
    bool isRunning() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace cosmos
