#include "Downloader.h"
#include <lt_shared/lt_shared.h>

namespace typebeats
{

namespace
{
    const char* toolUrl()
    {
       #if JUCE_MAC
        return "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_macos";
       #elif JUCE_WINDOWS
        return "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe";
       #else
        return "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_linux";
       #endif
    }
}

Downloader::Downloader (juce::AudioFormatManager& f) : juce::Thread ("LT TYPE BEATS download"), formats (f) {}

Downloader::~Downloader()
{
    *alive = false;
    stopThread (5000);
}

juce::File Downloader::getToolFile()
{
   #if JUCE_WINDOWS
    auto name = "yt-dlp.exe";
   #else
    auto name = "yt-dlp";
   #endif
    return lt::licensing::LicenseManager::getDataDirectory().getChildFile ("Tools").getChildFile (name);
}

bool Downloader::looksLikeYouTubeLink (const juce::String& s)
{
    auto t = s.trim().toLowerCase();
    return t.contains ("youtube.com/") || t.contains ("youtu.be/") || t.contains ("music.youtube.com/");
}

void Downloader::download (const juce::String& u, const juce::File& dest)
{
    if (isThreadRunning()) return;
    url = u.trim();
    folder = dest;
    updateOnly = false;
    startThread();
}

void Downloader::updateTool()
{
    if (isThreadRunning()) return;
    updateOnly = true;
    startThread();
}

void Downloader::post (std::function<void()> fn)
{
    juce::MessageManager::callAsync ([fn = std::move (fn), token = alive] { if (*token) fn(); });
}

void Downloader::progress (const juce::String& s, float p)
{
    post ([this, s, p] { if (onProgress) onProgress (s, p); });
}

bool Downloader::ensureTool (bool forceUpdate)
{
    auto tool = getToolFile();
    if (tool.existsAsFile() && ! forceUpdate)
        return true;

    progress (forceUpdate ? "Updating downloader..." : "Installing downloader (first time only)...", 0.02f);
    tool.getParentDirectory().createDirectory();
    auto temp = tool.getSiblingFile (tool.getFileName() + ".download");

    auto stream = juce::URL (toolUrl()).createInputStream (
        juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs (20000)
            .withNumRedirectsToFollow (10));

    if (stream == nullptr)
        return false;

    {
        temp.deleteFile();
        juce::FileOutputStream out (temp);
        if (! out.openedOk()) return false;

        auto total = stream->getTotalLength();
        juce::HeapBlock<char> buf (65536);
        juce::int64 done = 0;
        while (! stream->isExhausted() && ! threadShouldExit())
        {
            auto n = stream->read (buf, 65536);
            if (n <= 0) break;
            out.write (buf, (size_t) n);
            done += n;
            if (total > 0)
                progress ("Installing downloader... " + juce::String ((int) (100 * done / total)) + "%", 0.02f + 0.08f * (float) done / (float) total);
        }
        out.flush();
        if (done < 1000000) { temp.deleteFile(); return false; }   // the real binary is several MB
    }

    tool.deleteFile();
    if (! temp.moveFileTo (tool)) return false;
    tool.setExecutePermission (true);
    return true;
}

juce::File Downloader::convertToWav (const juce::File& source, const juce::File& dest)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (source));
    if (reader == nullptr)
        return {};

    dest.createDirectory();
    auto wav = dest.getNonexistentChildFile (juce::File::createLegalFileName (source.getFileNameWithoutExtension()), ".wav", false);

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::OutputStream> stream (wav.createOutputStream());
    if (stream == nullptr) return {};

    std::unique_ptr<juce::AudioFormatWriter> writer (wavFormat.createWriterFor (stream.get(), reader->sampleRate,
                                                     juce::jlimit (1u, 2u, reader->numChannels), 24, {}, 0));
    if (writer == nullptr) return {};
    stream.release(); // owned by the writer now

    if (! writer->writeFromAudioReader (*reader, 0, reader->lengthInSamples))
        return {};

    writer.reset();
    return wav;
}

void Downloader::run()
{
    if (updateOnly)
    {
        auto ok = ensureTool (true);
        post ([this, ok] { if (ok) { if (onProgress) onProgress ("Downloader updated.", 1.0f); }
                           else if (onError) onError ("Could not update the downloader. Check your internet connection."); });
        return;
    }

    if (! ensureTool (false))
    {
        post ([this] { if (onError) onError ("Could not install the downloader. Check your internet connection."); });
        return;
    }

    auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LT Type Beats");
    temp.createDirectory();

    juce::StringArray args;
    args.add (getToolFile().getFullPathName());
    args.addArray ({ "--no-playlist", "--newline", "--no-part", "--restrict-filenames",
                     "-f", "bestaudio[ext=m4a]/bestaudio[acodec^=mp4a]/bestaudio",
                     "-o", temp.getChildFile ("%(title).80s.%(ext)s").getFullPathName(),
                     "--print", "after_move:filepath",
                     url });

    juce::ChildProcess proc;
    if (! proc.start (args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        post ([this] { if (onError) onError ("Could not start the downloader."); });
        return;
    }

    progress ("Connecting to YouTube...", 0.1f);

    juce::String pending, lastLine, downloadedPath, errors;
    char buf[1024];
    while (! threadShouldExit())
    {
        auto n = proc.readProcessOutput (buf, sizeof (buf));
        if (n <= 0)
        {
            if (! proc.isRunning()) break;
            wait (50);
            continue;
        }

        pending += juce::String::fromUTF8 (buf, n);
        while (pending.containsChar ('\n'))
        {
            auto line = pending.upToFirstOccurrenceOf ("\n", false, false).trim();
            pending = pending.fromFirstOccurrenceOf ("\n", false, false);
            if (line.isEmpty()) continue;

            if (line.startsWith ("[download]") && line.contains ("%"))
            {
                auto pct = line.fromFirstOccurrenceOf ("]", false, false).trim().upToFirstOccurrenceOf ("%", false, false).getFloatValue();
                progress ("Downloading... " + juce::String ((int) pct) + "%", 0.1f + 0.75f * pct / 100.0f);
            }
            else if (line.startsWithIgnoreCase ("ERROR"))
            {
                errors << line.fromFirstOccurrenceOf (":", false, false).trim() << "\n";
            }
            else if (juce::File::isAbsolutePath (line) && juce::File (line).existsAsFile())
            {
                downloadedPath = line;
            }
            lastLine = line;
        }
    }

    if (threadShouldExit())
    {
        proc.kill();
        return;
    }

    if (pending.trim().isNotEmpty() && juce::File::isAbsolutePath (pending.trim()))
        downloadedPath = pending.trim();

    if (downloadedPath.isEmpty() || ! juce::File (downloadedPath).existsAsFile())
    {
        auto msg = errors.isNotEmpty() ? errors.trim() : juce::String ("The download failed. Try updating the downloader in Settings.");
        post ([this, msg] { if (onError) onError (msg); });
        return;
    }

    progress ("Converting to WAV...", 0.9f);
    auto source = juce::File (downloadedPath);
    auto wav = convertToWav (source, folder);
    source.deleteFile();

    if (wav == juce::File())
    {
        post ([this] { if (onError) onError ("Downloaded, but could not convert the audio to WAV."); });
        return;
    }

    post ([this, wav] { if (onFinished) onFinished (wav); });
}

} // namespace typebeats
