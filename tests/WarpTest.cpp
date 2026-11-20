// LT WARP: every conversion on real files, checked with ffprobe.
#include "Converter.h"
#include <iostream>

using namespace warp;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

static juce::String run (const juce::StringArray& args)
{
    juce::ChildProcess p;
    if (! p.start (args)) return "START FAILED";
    return p.readAllProcessOutput();
}

static juce::String probe (const Tools& t, const juce::File& f, const juce::String& entries, const juce::String& stream)
{
    juce::StringArray a { t.ffprobe.getFullPathName(), "-v", "error" };
    if (stream.isNotEmpty()) a.addArray ({ "-select_streams", stream });
    a.addArray ({ "-show_entries", entries, "-of", "default=nw=1", f.getFullPathName() });
    return run (a).trim();
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto tools = Tools::locate();
    check (tools.ready(), "ffmpeg found: " + tools.ffmpeg.getFullPathName());
    if (! tools.ready()) return 1;

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("lt-warp-test");
    dir.deleteRecursively();
    dir.createDirectory();
    auto ff = [&] (juce::StringArray extra) { juce::StringArray a { tools.ffmpeg.getFullPathName(), "-hide_banner", "-loglevel", "error", "-y" }; a.addArray (extra); return run (a); };

    // fixtures
    auto wav = dir.getChildFile ("tone.wav"), mp4 = dir.getChildFile ("clip.mp4"), png = dir.getChildFile ("pic.png"), heic = dir.getChildFile ("photo.heic");
    ff ({ "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000:duration=4", "-ac", "2", "-c:a", "pcm_s24le", wav.getFullPathName() });
    ff ({ "-f", "lavfi", "-i", "testsrc=size=320x240:rate=25:duration=3", "-f", "lavfi", "-i", "sine=frequency=660:sample_rate=44100:duration=3",
          "-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac", "-shortest", mp4.getFullPathName() });
    ff ({ "-f", "lavfi", "-i", "testsrc=size=200x120", "-frames:v", "1", png.getFullPathName() });
    if (tools.sips.existsAsFile()) run ({ tools.sips.getFullPathName(), "-s", "format", "heic", png.getFullPathName(), "--out", heic.getFullPathName() });
    check (wav.existsAsFile() && mp4.existsAsFile() && png.existsAsFile(), "fixtures written");

    Options o;
    std::atomic<bool> cancel { false };
    auto convert = [&] (const juce::File& in, Target t, const Options& opts) -> juce::File
    {
        auto plan = makePlan (in, t, opts, tools, dir);
        float last = 0;
        auto err = runPlan (plan, probeDuration (in, tools), [&last] (float p) { last = p; }, cancel);
        if (err.isNotEmpty()) std::cout << "        error: " << err << std::endl;
        return err.isEmpty() ? plan.output : juce::File();
    };

    std::cout << "audio" << std::endl;
    struct A { Target t; const char* codec; const char* extra; };
    const A audio[] = { { Target::mp3, "codec_name=mp3", "" }, { Target::wav16, "codec_name=pcm_s16le", "" }, { Target::wav24, "codec_name=pcm_s24le", "" },
                        { Target::flac, "codec_name=flac", "" }, { Target::aiff, "codec_name=pcm_s24be", "" }, { Target::m4a, "codec_name=aac", "" },
                        { Target::opus, "codec_name=opus", "sample_rate=48000" } };
    for (auto& c : audio)
    {
        auto out = convert (wav, c.t, o);
        auto info = out.existsAsFile() ? probe (tools, out, "stream=codec_name,sample_rate,channels", "a:0") : juce::String();
        check (info.contains (c.codec) && info.contains (c.extra) && info.contains ("channels=2"), "WAV 24 -> " + targetLabel (c.t) + "  [" + info.replace ("\n", " ") + "]");
    }
    {
        auto opts = o; opts.sampleRate = 44100;
        auto out = convert (wav, Target::wav16, opts);
        check (probe (tools, out, "stream=sample_rate", "a:0").contains ("44100"), "resample to 44.1 kHz");
        auto mp3 = convert (wav, Target::mp3, o);
        auto back = convert (mp3, Target::flac, o);
        check (probe (tools, back, "stream=codec_name", "a:0").contains ("flac"), "MP3 -> FLAC (any input format)");
    }

    std::cout << "video" << std::endl;
    {
        auto out = convert (mp4, Target::wav24, o);
        auto info = probe (tools, out, "stream=codec_name,sample_rate", "a:0");
        auto streams = run ({ tools.ffprobe.getFullPathName(), "-v", "error", "-show_entries", "stream=codec_type", "-of", "csv=p=0", out.getFullPathName() }).trim();
        check (info.contains ("pcm_s24le") && info.contains ("44100") && streams == "audio", "MP4 -> WAV 24 extracts only the audio  [" + info.replace ("\n", " ") + "]");
        auto mp3 = convert (mp4, Target::mp3, o);
        check (probe (tools, mp3, "stream=codec_name", "a:0").contains ("mp3"), "MP4 -> MP3");
        auto frame = convert (mp4, Target::jpg, o);
        check (probe (tools, frame, "stream=width,height", "v:0").contains ("width=320"), "MP4 -> JPG still frame");
    }

    std::cout << "trim" << std::endl;
    {
        auto opts = o; opts.trimStart = 1.0; opts.trimEnd = 2.5;
        auto out = convert (wav, Target::wav24, opts);
        auto d = probeDuration (out, tools);
        check (std::abs (d - 1.5) < 0.02, "trim 1.0 .. 2.5 s -> " + juce::String (d, 3) + " s");
    }

    std::cout << "images" << std::endl;
    struct I { Target t; const char* codec; };
    const I images[] = { { Target::jpg, "codec_name=mjpeg" }, { Target::webp, "codec_name=webp" }, { Target::tiff, "codec_name=tiff" }, { Target::heic, "" }, { Target::png, "codec_name=png" } };
    for (auto& c : images)
    {
        if (c.t == Target::heic && ! tools.sips.existsAsFile()) continue;
        auto out = convert (png, c.t, o);
        auto info = out.existsAsFile() ? probe (tools, out, "stream=codec_name,width,height", "v:0") : juce::String();
        auto okSize = c.t == Target::heic ? out.existsAsFile() && out.getSize() > 200 : info.contains ("width=200") && info.contains ("height=120");
        check (okSize && info.contains (c.codec), "PNG -> " + targetLabel (c.t) + "  [" + info.replace ("\n", " ") + "]");
    }
    if (heic.existsAsFile())
    {
        for (auto t : { Target::jpg, Target::png, Target::webp })
        {
            auto out = convert (heic, t, o);
            auto info = out.existsAsFile() ? probe (tools, out, "stream=width,height", "v:0") : juce::String();
            check (info.contains ("width=200"), "HEIC -> " + targetLabel (t) + "  [" + info.replace ("\n", " ") + "]");
        }
        auto webp = dir.getChildFile ("pic.webp");
        auto back = convert (webp, Target::heic, o);
        check (back.existsAsFile() && back.getSize() > 200, "WEBP -> HEIC");
    }

    std::cout << "rules" << std::endl;
    check (makePlan (png, Target::mp3, o, tools, dir).error.isNotEmpty(), "image -> MP3 is refused");
    check (makePlan (wav, Target::png, o, tools, dir).error.isNotEmpty(), "audio -> PNG is refused");
    {
        auto a = uniqueOutput (dir, "tone", "wav");
        check (a.getFileName() == "tone (2).wav" || a.getFileName().startsWith ("tone ("), "never overwrites: " + a.getFileName());
    }
    {
        // cancel a long job
        auto big = dir.getChildFile ("long.wav");
        ff ({ "-f", "lavfi", "-i", "anoisesrc=d=600:r=48000", "-ac", "2", "-c:a", "pcm_s24le", big.getFullPathName() });
        std::atomic<bool> stop { false };
        auto plan = makePlan (big, Target::opus, o, tools, dir);
        std::thread killer ([&stop] { juce::Thread::sleep (400); stop = true; });
        auto err = runPlan (plan, 600, nullptr, stop);
        killer.join();
        check (err == "cancelled" && ! plan.output.existsAsFile(), "cancel stops the job and removes the partial file");
    }

    std::cout << "queue" << std::endl;
    {
        auto outDir = dir.getChildFile ("out");
        outDir.createDirectory();
        Queue q (2);
        q.tools = tools;
        q.outputDir = outDir;
        auto added = q.add ({ wav.getFullPathName(), mp4.getFullPathName(), png.getFullPathName(), dir.getChildFile ("notes.txt").getFullPathName() }, Target::flac, Target::webp);
        q.startAll();
        auto until = juce::Time::getMillisecondCounter() + 30000;
        auto finished = [&q]
        {
            for (auto& i : q.snapshot()) if (i.state == Queue::State::waiting || i.state == Queue::State::running) return false;
            return true;
        };
        while (! finished() && juce::Time::getMillisecondCounter() < until) juce::Thread::sleep (20);
        int done = 0;
        for (auto& i : q.snapshot()) done += i.state == Queue::State::done;
        check (added == 3 && done == 3, "queue: 3 files on 2 threads (txt ignored) -> " + juce::String (done) + " done");
        check (outDir.getChildFile ("tone.flac").existsAsFile() && outDir.getChildFile ("clip.flac").existsAsFile() && outDir.getChildFile ("pic.webp").existsAsFile(),
               "queue wrote tone.flac, clip.flac, pic.webp into the chosen folder");
    }

    dir.deleteRecursively();
    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
