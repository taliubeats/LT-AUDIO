#include "Converter.h"

namespace warp
{

//============================================================================== tools
static juce::File findTool (const juce::String& name)
{
    // 1. next to the app (Contents/Resources on macOS, the exe folder on Windows)
    auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
   #if JUCE_WINDOWS
    auto file = name + ".exe";
    for (auto dir : { exe.getParentDirectory(), exe.getParentDirectory().getChildFile ("tools") })
        if (dir.getChildFile (file).existsAsFile()) return dir.getChildFile (file);
   #else
    auto file = name;
    auto res = exe.getParentDirectory().getParentDirectory().getChildFile ("Resources");
    if (res.getChildFile (file).existsAsFile()) return res.getChildFile (file);
    for (auto dir : { "/opt/homebrew/bin", "/usr/local/bin", "/usr/bin" })
        if (juce::File (dir).getChildFile (file).existsAsFile()) return juce::File (dir).getChildFile (file);
   #endif
    // 2. PATH
    auto path = juce::SystemStats::getEnvironmentVariable ("PATH", {});
   #if JUCE_WINDOWS
    const char* sep = ";";
   #else
    const char* sep = ":";
   #endif
    for (auto& d : juce::StringArray::fromTokens (path, sep, ""))
        if (d.isNotEmpty() && juce::File (d).getChildFile (file).existsAsFile()) return juce::File (d).getChildFile (file);
    return {};
}

Tools Tools::locate()
{
    Tools t;
    auto overrideDir = juce::SystemStats::getEnvironmentVariable ("LT_WARP_TOOLS", {});
    if (overrideDir.isNotEmpty())
    {
        t.ffmpeg = juce::File (overrideDir).getChildFile ("ffmpeg");
        t.ffprobe = juce::File (overrideDir).getChildFile ("ffprobe");
    }
    if (! t.ffmpeg.existsAsFile()) t.ffmpeg = findTool ("ffmpeg");
    if (! t.ffprobe.existsAsFile()) t.ffprobe = findTool ("ffprobe");
   #if JUCE_MAC
    if (juce::File ("/usr/bin/sips").existsAsFile()) t.sips = juce::File ("/usr/bin/sips");
   #endif
    if (t.ffmpeg.existsAsFile())
    {
        juce::ChildProcess p;
        if (p.start (juce::StringArray { t.ffmpeg.getFullPathName(), "-hide_banner", "-encoders" }))
            t.hasAacAt = p.readAllProcessOutput().contains ("aac_at");
    }
    return t;
}

//============================================================================== formats
Kind kindOf (const juce::File& f)
{
    auto e = f.getFileExtension().toLowerCase().trimCharactersAtStart (".");
    static const juce::StringArray audio { "mp3", "wav", "wave", "flac", "aif", "aiff", "aifc", "m4a", "aac", "opus", "ogg", "oga", "wma", "alac", "caf", "mp2", "amr", "ac3" };
    static const juce::StringArray video { "mp4", "mov", "m4v", "mkv", "webm", "avi", "wmv", "flv", "mpg", "mpeg", "3gp", "ts", "mts" };
    static const juce::StringArray image { "jpg", "jpeg", "png", "webp", "tif", "tiff", "heic", "heif", "bmp", "gif", "avif" };
    if (audio.contains (e)) return Kind::audio;
    if (video.contains (e)) return Kind::video;
    if (image.contains (e)) return Kind::image;
    return Kind::unknown;
}

bool isAudioTarget (Target t) { return (int) t <= (int) Target::opus; }

juce::String targetLabel (Target t)
{
    static const char* n[] = { "MP3", "WAV 16", "WAV 24", "FLAC", "AIFF", "M4A", "OPUS", "JPG", "PNG", "WEBP", "TIFF", "HEIC" };
    return n[juce::jlimit (0, (int) Target::count - 1, (int) t)];
}

juce::String targetExtension (Target t)
{
    static const char* n[] = { "mp3", "wav", "wav", "flac", "aiff", "m4a", "opus", "jpg", "png", "webp", "tiff", "heic" };
    return n[juce::jlimit (0, (int) Target::count - 1, (int) t)];
}

bool canConvert (Kind k, Target t, const Tools& tools)
{
    if (k == Kind::unknown) return false;
    if (t == Target::heic && ! tools.sips.existsAsFile()) return false;   // HEIC is written by macOS
    if (isAudioTarget (t)) return k == Kind::audio || k == Kind::video;
    return k == Kind::image || k == Kind::video;                         // video -> still frame
}

juce::File uniqueOutput (const juce::File& dir, const juce::String& stem, const juce::String& ext)
{
    auto f = dir.getChildFile (stem + "." + ext);
    for (int i = 2; f.exists() && i < 10000; ++i)
        f = dir.getChildFile (stem + " (" + juce::String (i) + ")." + ext);
    return f;
}

//============================================================================== plans
static juce::StringArray ffmpegBase (const Tools& tools)
{
    return { tools.ffmpeg.getFullPathName(), "-hide_banner", "-nostdin", "-loglevel", "error", "-y" };
}

static void addTrim (juce::StringArray& a, const Options& o, bool beforeInput)
{
    if (beforeInput && o.trimStart > 0) a.addArray ({ "-ss", juce::String (o.trimStart, 3) });
    if (! beforeInput && o.trimEnd > o.trimStart && o.trimEnd > 0) a.addArray ({ "-t", juce::String (o.trimEnd - o.trimStart, 3) });
}

Plan makePlan (const juce::File& in, Target t, const Options& o, const Tools& tools, const juce::File& outDirIn)
{
    Plan plan;
    auto kind = kindOf (in);
    if (! tools.ready()) { plan.error = "ffmpeg not found"; return plan; }
    if (! in.existsAsFile()) { plan.error = "file not found"; return plan; }
    if (! canConvert (kind, t, tools)) { plan.error = "can't make " + targetLabel (t) + " from this file"; return plan; }

    auto outDir = outDirIn.isDirectory() ? outDirIn : in.getParentDirectory();
    plan.output = uniqueOutput (outDir, in.getFileNameWithoutExtension(), targetExtension (t));
    auto out = plan.output.getFullPathName();

    if (isAudioTarget (t))
    {
        auto a = ffmpegBase (tools);
        addTrim (a, o, true);
        a.addArray ({ "-i", in.getFullPathName() });
        addTrim (a, o, false);
        a.addArray ({ "-map", "0:a:0", "-vn", "-sn", "-dn", "-map_metadata", "0" });
        auto rate = o.sampleRate > 0 ? o.sampleRate : 0;
        switch (t)
        {
            case Target::mp3:   a.addArray ({ "-c:a", "libmp3lame", "-b:a", juce::String (o.mp3Kbps) + "k", "-id3v2_version", "3" }); break;
            case Target::wav16: a.addArray ({ "-c:a", "pcm_s16le", "-af", "aresample=dither_method=triangular" }); break;
            case Target::wav24: a.addArray ({ "-c:a", "pcm_s24le" }); break;
            case Target::flac:  a.addArray ({ "-c:a", "flac", "-compression_level", "8" }); break;
            case Target::aiff:  a.addArray ({ "-c:a", "pcm_s24be", "-f", "aiff" }); break;
            case Target::m4a:   a.addArray ({ "-c:a", tools.hasAacAt ? "aac_at" : "aac", "-b:a", juce::String (o.aacKbps) + "k", "-movflags", "+faststart" }); break;
            case Target::opus:  a.addArray ({ "-c:a", "libopus", "-b:a", juce::String (o.opusKbps) + "k", "-vbr", "on" });
                                if (rate == 0 || (rate != 48000 && rate != 24000 && rate != 16000)) rate = 48000; break;
            default: break;
        }
        if (rate > 0) a.addArray ({ "-ar", juce::String (rate) });
        a.addArray ({ "-progress", "pipe:1", "-nostats", out });
        plan.steps.push_back (a);
        return plan;
    }

    // ------------------------------------------------------------ images
    const auto inExt = in.getFileExtension().toLowerCase();
    const bool heicIn = inExt == ".heic" || inExt == ".heif";
    const bool useSipsIn = heicIn && tools.sips.existsAsFile();
    auto q = juce::jlimit (1, 100, o.imageQuality);

    auto source = in;
    if (kind == Kind::video || (useSipsIn && (t == Target::webp)))
    {
        // first make a PNG we can work from (a video frame, or a HEIC that ffmpeg should encode further)
        auto tmp = juce::File::createTempFile (".png");
        plan.temps.add (tmp);
        if (kind == Kind::video)
        {
            auto a = ffmpegBase (tools);
            a.addArray ({ "-ss", juce::String (juce::jmax (0.0, o.trimStart), 3), "-i", in.getFullPathName(), "-frames:v", "1", "-update", "1", tmp.getFullPathName() });
            plan.steps.push_back (a);
        }
        else plan.steps.push_back ({ tools.sips.getFullPathName(), "-s", "format", "png", in.getFullPathName(), "--out", tmp.getFullPathName() });
        source = tmp;
    }

    const bool sourceIsHeic = source == in && heicIn;
    if (t == Target::heic || (sourceIsHeic && useSipsIn && (t == Target::jpg || t == Target::png || t == Target::tiff)))
    {
        // macOS ImageIO reads and writes HEIC natively; WEBP input goes through a PNG first
        if (source.getFileExtension().equalsIgnoreCase (".webp"))
        {
            auto tmp = juce::File::createTempFile (".png");
            plan.temps.add (tmp);
            auto a = ffmpegBase (tools);
            a.addArray ({ "-i", source.getFullPathName(), "-frames:v", "1", "-update", "1", tmp.getFullPathName() });
            plan.steps.push_back (a);
            source = tmp;
        }
        auto fmt = t == Target::heic ? "heic" : t == Target::jpg ? "jpeg" : t == Target::png ? "png" : "tiff";
        juce::StringArray s { tools.sips.getFullPathName(), "-s", "format", fmt };
        if (t == Target::heic || t == Target::jpg) s.addArray ({ "-s", "formatOptions", juce::String (q) });
        s.addArray ({ source.getFullPathName(), "--out", out });
        plan.steps.push_back (s);
        return plan;
    }

    auto a = ffmpegBase (tools);
    a.addArray ({ "-i", source.getFullPathName(), "-frames:v", "1", "-update", "1" });
    switch (t)
    {
        case Target::jpg:  a.addArray ({ "-c:v", "mjpeg", "-q:v", juce::String (juce::roundToInt (2 + (100 - q) * 29 / 99.0)), "-pix_fmt", "yuvj444p" }); break;
        case Target::png:  a.addArray ({ "-c:v", "png" }); break;
        case Target::webp: a.addArray ({ "-c:v", "libwebp", "-quality", juce::String (q) }); break;
        case Target::tiff: a.addArray ({ "-c:v", "tiff", "-compression_algo", "lzw" }); break;
        default: break;
    }
    a.add (out);
    plan.steps.push_back (a);
    return plan;
}

//============================================================================== running
double probeDuration (const juce::File& f, const Tools& tools)
{
    if (! tools.ffprobe.existsAsFile()) return 0;
    juce::ChildProcess p;
    if (! p.start (juce::StringArray { tools.ffprobe.getFullPathName(), "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", f.getFullPathName() }))
        return 0;
    return p.readAllProcessOutput().trim().getDoubleValue();
}

juce::String runPlan (const Plan& plan, double duration, std::function<void (float)> progress, std::atomic<bool>& cancel)
{
    if (plan.error.isNotEmpty()) return plan.error;
    juce::String result;
    const auto steps = (int) plan.steps.size();
    for (int si = 0; si < steps && result.isEmpty(); ++si)
    {
        juce::ChildProcess p;
        if (! p.start (plan.steps[(size_t) si], juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        {
            result = "could not start " + juce::File (plan.steps[(size_t) si][0]).getFileName();
            break;
        }
        juce::String pending;
        juce::StringArray tail;
        char buf[4096];
        while (true)
        {
            if (cancel.load()) { p.kill(); result = "cancelled"; break; }
            auto n = p.readProcessOutput (buf, (int) sizeof (buf));
            if (n <= 0)
            {
                if (! p.isRunning()) break;
                juce::Thread::sleep (5);
                continue;
            }
            pending += juce::String::fromUTF8 (buf, n);
            int nl;
            while ((nl = pending.indexOfChar ('\n')) >= 0)
            {
                auto line = pending.substring (0, nl).trim();
                pending = pending.substring (nl + 1);
                if (line.startsWith ("out_time_us=") || line.startsWith ("out_time_ms="))
                {
                    auto secs = line.fromFirstOccurrenceOf ("=", false, false).getLargeIntValue() / 1.0e6;
                    if (duration > 0 && progress) progress ((float) juce::jlimit (0.0, 1.0, ((double) si + secs / duration) / steps));
                }
                else if (line.isNotEmpty() && ! line.containsChar ('=')) { tail.add (line); if (tail.size() > 6) tail.remove (0); }
            }
        }
        if (result.isNotEmpty()) break;
        if (p.getExitCode() != 0)
            result = tail.isEmpty() ? "conversion failed (" + juce::String ((int) p.getExitCode()) + ")" : tail.joinIntoString (" ").substring (0, 200);
        else if (progress) progress ((float) (si + 1) / steps);
    }
    for (auto& t : plan.temps) t.deleteFile();
    if (result.isNotEmpty()) plan.output.deleteFile();
    else if (! plan.output.existsAsFile()) result = "no output was written";
    return result;
}

//============================================================================== queue
struct Queue::Worker : public juce::ThreadPoolJob
{
    Worker (Queue& q, int i, std::shared_ptr<std::atomic<bool>> c) : juce::ThreadPoolJob ("warp"), queue (q), id (i), cancel (std::move (c)) {}

    JobStatus runJob() override
    {
        juce::File input;
        Target target;
        Options options;
        juce::File outDir;
        {
            const juce::ScopedLock sl (queue.lock);
            auto it = std::find_if (queue.items.begin(), queue.items.end(), [this] (const Item& x) { return x.id == id; });
            if (it == queue.items.end() || it->state != State::waiting) return jobHasFinished;
            it->state = State::running;
            it->progress = 0;
            input = it->input; target = it->target; options = queue.options; outDir = queue.outputDir;
        }
        queue.notify();

        auto plan = makePlan (input, target, options, queue.tools, outDir);
        auto duration = plan.error.isEmpty() && isAudioTarget (target) ? probeDuration (input, queue.tools) : 0.0;
        if (duration > 0 && options.trimEnd > options.trimStart) duration = juce::jmin (duration, options.trimEnd - options.trimStart);
        else if (duration > 0 && options.trimStart > 0) duration = juce::jmax (0.01, duration - options.trimStart);

        auto lastNotify = juce::Time::getMillisecondCounter();
        auto err = runPlan (plan, duration, [this, &lastNotify] (float p)
        {
            {
                const juce::ScopedLock sl (queue.lock);
                for (auto& x : queue.items) if (x.id == id) x.progress = p;
            }
            auto now = juce::Time::getMillisecondCounter();
            if (now - lastNotify > 80) { lastNotify = now; queue.notify(); }
        }, *cancel);

        {
            const juce::ScopedLock sl (queue.lock);
            for (auto& x : queue.items)
                if (x.id == id)
                {
                    x.state = err.isEmpty() ? State::done : err == "cancelled" ? State::cancelled : State::failed;
                    x.message = err;
                    x.output = err.isEmpty() ? plan.output : juce::File();
                    x.progress = err.isEmpty() ? 1.0f : x.progress;
                }
        }
        queue.notify();
        return jobHasFinished;
    }

    Queue& queue;
    int id;
    std::shared_ptr<std::atomic<bool>> cancel;
};

Queue::Queue (int threads) : pool (juce::ThreadPoolOptions().withNumberOfThreads (threads).withThreadName ("LT WARP")) {}

Queue::~Queue()
{
    cancelAll();
    pool.removeAllJobs (true, 5000);
}

void Queue::notify()
{
    if (onChange) juce::MessageManager::callAsync ([fn = onChange] { fn(); });
}

int Queue::add (const juce::StringArray& paths, Target audioTarget, Target imageTarget)
{
    juce::Array<juce::File> files;
    for (auto& p : paths)
    {
        juce::File f (p);
        if (f.isDirectory()) files.addArray (f.findChildFiles (juce::File::findFiles, false));
        else files.add (f);
    }
    int added = 0;
    {
        const juce::ScopedLock sl (lock);
        for (auto& f : files)
        {
            auto k = kindOf (f);
            if (k == Kind::unknown || f.getFileName().startsWithChar ('.')) continue;
            Item it;
            it.id = nextId++;
            it.input = f;
            it.kind = k;
            it.target = k == Kind::image ? imageTarget : audioTarget;
            if (! canConvert (k, it.target, tools)) { it.state = State::failed; it.message = "can't make " + targetLabel (it.target) + " from this file"; }
            items.push_back (it);
            ++added;
        }
    }
    notify();
    return added;
}

void Queue::setTarget (int id, Target t)
{
    {
        const juce::ScopedLock sl (lock);
        for (auto& x : items)
            if (x.id == id && x.state != State::running)
            {
                x.target = t;
                auto ok = canConvert (x.kind, t, tools);
                x.state = ok ? State::waiting : State::failed;
                x.message = ok ? juce::String() : "can't make " + targetLabel (t) + " from this file";
                x.progress = 0;
            }
    }
    notify();
}

void Queue::remove (int id)
{
    {
        const juce::ScopedLock sl (lock);
        if (auto c = cancels.find (id); c != cancels.end()) c->second->store (true);
        items.erase (std::remove_if (items.begin(), items.end(), [id] (const Item& x) { return x.id == id && x.state != State::running; }), items.end());
    }
    notify();
}

void Queue::clearFinished()
{
    {
        const juce::ScopedLock sl (lock);
        items.erase (std::remove_if (items.begin(), items.end(), [] (const Item& x) { return x.state == State::done || x.state == State::failed || x.state == State::cancelled; }), items.end());
    }
    notify();
}

void Queue::startAll()
{
    const juce::ScopedLock sl (lock);
    for (auto& x : items)
    {
        if (x.state == State::cancelled) { x.state = State::waiting; x.progress = 0; }
        if (x.state != State::waiting) continue;
        auto c = std::make_shared<std::atomic<bool>> (false);
        cancels[x.id] = c;
        pool.addJob (new Worker (*this, x.id, c), true);
    }
}

void Queue::cancelAll()
{
    const juce::ScopedLock sl (lock);
    for (auto& [id, c] : cancels) c->store (true);
    cancels.clear();
    for (auto& x : items) if (x.state == State::waiting) x.state = State::cancelled;
}

bool Queue::isBusy() const
{
    const juce::ScopedLock sl (lock);
    return std::any_of (items.begin(), items.end(), [] (const Item& x) { return x.state == State::running; });
}

std::vector<Queue::Item> Queue::snapshot() const
{
    const juce::ScopedLock sl (lock);
    return items;
}

} // namespace warp
