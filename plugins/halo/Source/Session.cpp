#include "Session.h"
#include "Lyrics.h"
#include "AIHelper.h"

namespace halo
{

namespace
{
    constexpr double maxSeconds = 6.0 * 60.0;

    std::vector<float> resample (const std::vector<float>& in, double fromRate, double toRate)
    {
        if (in.empty() || std::abs (fromRate - toRate) < 0.5) return in;
        auto ratio = fromRate / toRate;
        auto outLen = (int) std::ceil ((double) in.size() / ratio);
        std::vector<float> out ((size_t) outLen, 0.0f);
        juce::LagrangeInterpolator interp;
        std::vector<float> padded (in);
        padded.resize (in.size() + 16, 0.0f);
        interp.process (ratio, padded.data(), out.data(), outLen);
        return out;
    }

    Stereo resample (const Stereo& s, double from, double to)
    {
        Stereo o;
        o.l = resample (s.l, from, to);
        o.r = resample (s.r, from, to);
        return o;
    }

    juce::String safeName (juce::String s)
    {
        return s.replaceCharacters ("/\\:*?\"<>|", "---------").trim();
    }

    /** A file name that is not taken yet ("name 2.wav", "name 3.wav"...). Used for drags: a DAW keeps
        referencing (and caching, e.g. Ableton's .asd) a file it already received, so it must never be overwritten. */
    juce::File freshFile (const juce::File& f)
    {
        if (! f.exists()) return f;
        for (int n = 2; n < 10000; ++n)
        {
            auto c = f.getSiblingFile (f.getFileNameWithoutExtension() + " " + juce::String (n) + f.getFileExtension());
            if (! c.exists()) return c;
        }
        return f.getNonexistentSibling();
    }

    bool writeWav (const juce::File& f, const std::vector<float>& l, const std::vector<float>& r, double rate,
                   float* peakOut = nullptr)
    {
        if (peakOut != nullptr) *peakOut = 0.0f;
        if (! f.getParentDirectory().createDirectory()) return false;
        f.deleteFile();
        f.getSiblingFile (f.getFileName() + ".asd").deleteFile();   // stale Ableton analysis of an older take
        std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
        if (os == nullptr) return false;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), rate, 2, 24, {}, 0));
        if (w == nullptr) return false;
        os.release();
        const int n = (int) std::min (l.size(), r.size());
        juce::AudioBuffer<float> buf (2, n);
        float peak = 0;
        for (int i = 0; i < n; ++i)
        {
            buf.setSample (0, i, l[(size_t) i]);
            buf.setSample (1, i, r[(size_t) i]);
            peak = std::max ({ peak, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]) });
        }
        if (peak > 0.98f) buf.applyGain (0.98f / peak);   // never clip the exported file
        if (peakOut != nullptr) *peakOut = peak;
        bool ok = w->writeFromAudioSampleBuffer (buf, 0, n);
        w.reset();   // finishes the header and closes the file before anyone (a DAW) reads it
        return ok && f.getSize() > 44;
    }

}

Session::Session()
{
    formats.registerBasicFormats();
    auto threads = juce::jlimit (1, 6, juce::SystemStats::getNumCpus() - 1);
    renderPool = std::make_unique<juce::ThreadPool> (threads);
}

Session::~Session()
{
    cancelFlag = true;
    worker.removeAllJobs (true, 20000);
    renderPool->removeAllJobs (true, 20000);
    cancelPendingUpdate();
}

juce::File Session::dataFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("LT Audio").getChildFile ("HALO");
}

juce::File Session::aiLauncher()  { return AIHelper::launcher(); }
juce::File Session::lyricsModel() { return Lyrics::modelFile(); }

void Session::setStatus (const juce::String& s)
{
    {
        const juce::ScopedLock sl (lock);
        status = s;
    }
    triggerAsyncUpdate();
}

juce::String Session::getStatus() const { const juce::ScopedLock sl (lock); return status; }
juce::String Session::getError() const  { const juce::ScopedLock sl (lock); return error; }

void Session::handleAsyncUpdate()
{
    // free retired playback snapshots here (message thread), once the audio thread no longer holds them
    retired.erase (std::remove_if (retired.begin(), retired.end(), [] (auto& p) { return p.use_count() == 1; }), retired.end());
    sendChangeMessage();
}

void Session::runJob (std::function<void()> fn)
{
    cancelFlag = true;
    renderPool->removeAllJobs (true, 20000);
    worker.removeAllJobs (true, 20000);
    cancelFlag = false;
    busy = true;
    progress = 0.0f;
    worker.addJob ([this, fn]
    {
        fn();
        busy = false;
        triggerAsyncUpdate();
    });
}

void Session::cancel()
{
    cancelFlag = true;
    setStatus ("Cancelled");
}

// ------------------------------------------------------------------ loading

bool Session::loadFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        const juce::ScopedLock sl (lock);
        error = "Can't read " + file.getFileName();
        return false;
    }
    auto sr = reader->sampleRate;
    auto len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (maxSeconds * sr));
    juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
    reader->read (&buf, 0, len, 0, true, true);
    loadBuffer (buf, sr, file.getFileNameWithoutExtension());
    const juce::ScopedLock sl (lock);
    vocalFile = file;
    return true;
}

void Session::restore (const juce::File& f)
{
    if (f.existsAsFile()) loadFile (f);
}

void Session::loadBuffer (const juce::AudioBuffer<float>& buf, double sampleRate, const juce::String& name)
{
    // mono, and a rate WORLD is happy with (44.1 / 48 kHz kept as they are)
    std::vector<float> mono ((size_t) buf.getNumSamples(), 0.0f);
    for (int c = 0; c < buf.getNumChannels(); ++c)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            mono[(size_t) i] += buf.getSample (c, i) / (float) buf.getNumChannels();
    int sr = (int) std::lround (sampleRate);
    if (sr != 44100 && sr != 48000)
    {
        mono = resample (mono, sampleRate, 48000.0);
        sr = 48000;
    }
    std::vector<double> d (mono.begin(), mono.end());
    {
        const juce::ScopedLock sl (lock);
        vocalFile = juce::File();
        vocalName = name;
        error.clear();
        words.clear();
    }
    runJob ([this, d = std::move (d), sr, name] () mutable { analyseNow (std::move (d), sr, name, {}); });
}

void Session::analyseNow (std::vector<double> mono, int sr, juce::String name, juce::File)
{
    setStatus ("Analyzing voice...");
    AnalysisOptions opt;
    auto an = analyse (std::move (mono), sr, opt, [this] (float p) { progress = p * 0.7f; triggerAsyncUpdate(); },
                       [this] { return stopRequested(); });
    if (an == nullptr)
    {
        if (! stopRequested()) { const juce::ScopedLock sl (lock); error = "The vocal is too short or silent"; }
        setStatus (stopRequested() ? "Cancelled" : "Could not analyse the vocal");
        return;
    }
    {
        const juce::ScopedLock sl (lock);
        analysis = an;
        detectedKey = an->key;
        ++analysisVersion;
        bpm = bpmFromName (name.toStdString());
        choirValid = false;
        for (auto& r : results) r.valid = false;
    }
    rebuildPlayback();
    transcribeNow();
    progress = 1.0f;
    setStatus ("Voice learned");
}

void Session::transcribeNow()
{
    auto an = getAnalysis();
    if (an == nullptr || ! Lyrics::available()) return;
    setStatus ("Reading lyrics...");
    auto w = Lyrics::transcribe (an->audio, an->sampleRate, [this] (float p) { progress = 0.7f + 0.3f * p; triggerAsyncUpdate(); },
                                 [this] { return stopRequested(); });
    const juce::ScopedLock sl (lock);
    words = std::move (w);
}

bool Session::hasVocal() const                          { const juce::ScopedLock sl (lock); return analysis != nullptr; }
juce::File Session::getVocalFile() const                { const juce::ScopedLock sl (lock); return vocalFile; }
juce::String Session::getVocalName() const              { const juce::ScopedLock sl (lock); return vocalName; }
std::shared_ptr<const Analysis> Session::getAnalysis() const { const juce::ScopedLock sl (lock); return analysis; }
Key Session::getDetectedKey() const                     { const juce::ScopedLock sl (lock); return detectedKey; }
double Session::getBpm() const                          { const juce::ScopedLock sl (lock); return bpm; }
std::vector<Word> Session::getWords() const             { const juce::ScopedLock sl (lock); return words; }
bool Session::lyricsAvailable() const                   { return Lyrics::available(); }

void Session::toggleWordReinforce (int i)
{
    {
        const juce::ScopedLock sl (lock);
        if (! juce::isPositiveAndBelow (i, (int) words.size())) return;
        words[(size_t) i].reinforce = ! words[(size_t) i].reinforce;
        for (auto& r : results)
            if (r.valid && partInfo (r.spec.part).refuerzo) r.valid = false;   // refuerzo strips must regenerate
    }
    sendChangeMessage();
}

void Session::setKey (const Key& k)
{
    {
        const juce::ScopedLock sl (lock);
        if (analysis == nullptr) return;
        if (analysis->key.tonic == k.tonic && analysis->key.minor == k.minor) return;
        auto copy = std::make_shared<Analysis> (*analysis);   // never mutate an analysis a job may be reading
        copy->key = k;
        estimateChords (copy->notes, k);
        analysis = copy;
        ++analysisVersion;
    }
    sendChangeMessage();
}

// ------------------------------------------------------------------ generation

bool Session::isStripDirty (int i, const StripSpec& cur) const
{
    const juce::ScopedLock sl (lock);
    auto& r = results[(size_t) i];
    return ! r.valid || r.spec != cur || r.analysisVersion != analysisVersion;
}

StripInfo Session::getStripInfo (int i, const StripSpec& cur) const
{
    StripInfo info;
    std::shared_ptr<Analysis> an;
    {
        const juce::ScopedLock sl (lock);
        auto& r = results[(size_t) i];
        info.valid = r.valid;
        info.dirty = ! r.valid || r.spec != cur || r.analysisVersion != analysisVersion;
        an = analysis;
    }
    info.label = partInfo (cur.part).name;
    if (an != nullptr)
        for (auto& h : stripLine (*an, cur)) info.notes.push_back (h.midi);
    return info;
}

void Session::generate (const Request& req)
{
    auto an = getAnalysis();
    if (an == nullptr) return;

    runJob ([this, req, an]
    {
        int version;
        std::vector<Word> w;
        { const juce::ScopedLock sl (lock); version = analysisVersion; w = words; }

        auto global = req.global;
        if (global.bpm <= 0) global.bpm = bpm > 0 ? bpm : 90.0;
        auto regions = fx::refuerzoRegions (w, *an, global.refuerzoWords);

        std::vector<int> todo;
        for (int i = 0; i < numStrips; ++i)
        {
            bool qualityChanged;
            { const juce::ScopedLock sl (lock); qualityChanged = results[(size_t) i].best != req.best; }
            if (req.enabled[(size_t) i] && (req.force[(size_t) i] || isStripDirty (i, req.specs[(size_t) i]) || qualityChanged))
                todo.push_back (i);
        }
        if (todo.empty()) { setStatus ("Up to date"); return; }

        setStatus ("Generating harmonies...");
        std::vector<Stereo> out (todo.size());
        std::atomic<int> done { 0 };
        for (size_t k = 0; k < todo.size(); ++k)
        {
            renderPool->addJob ([&, k]
            {
                auto spec = req.specs[(size_t) todo[k]];
                out[k] = renderStrip (*an, spec, global, regions, [this] { return stopRequested(); });
                ++done;
                progress = (req.best ? 0.3f : 1.0f) * (float) done.load() / (float) todo.size();
                triggerAsyncUpdate();
            });
        }
        while (renderPool->getNumJobs() > 0)
            juce::Thread::sleep (20);
        if (stopRequested()) { setStatus ("Cancelled"); return; }

        if (req.best)
        {
            setStatus ("BEST: AI is singing the harmonies...");
            if (! renderBest (req, todo, out))
                setStatus ("BEST not available - kept FAST version");
        }

        {
            const juce::ScopedLock sl (lock);
            for (size_t k = 0; k < todo.size(); ++k)
            {
                auto& r = results[(size_t) todo[k]];
                r.valid = ! out[k].empty();
                r.spec = req.specs[(size_t) todo[k]];
                r.global = global;
                r.best = req.best;
                r.audio = std::move (out[k]);
                r.analysisVersion = version;
                r.notes.clear();
                for (auto& h : stripLine (*an, r.spec)) r.notes.push_back (h.midi);
            }
        }
        rebuildPlayback();
        progress = 1.0f;
        setStatus ("Harmonies ready");
    });
}

bool Session::renderBest (const Request& req, const std::vector<int>& idx, std::vector<Stereo>& buffers)
{
    auto an = getAnalysis();
    if (an == nullptr || ! AIHelper::available()) return false;
    std::vector<AIHelper::Item> items;
    for (size_t k = 0; k < idx.size(); ++k)
    {
        auto& spec = req.specs[(size_t) idx[k]];
        auto& info = partInfo (spec.part);
        AIHelper::Item it;
        it.stereo = &buffers[k];
        it.eligible = info.takes <= 1 && spec.voice != VoiceType::chipmunk && spec.voice != VoiceType::demon;
        items.push_back (it);
    }
    return AIHelper::render (*an, items, [this] (float p) { progress = 0.3f + 0.7f * p; triggerAsyncUpdate(); },
                             [this] { return stopRequested(); });
}

bool Session::choirReady() const { const juce::ScopedLock sl (lock); return choirValid; }

bool Session::choirDirty (const ChoirSpec& c) const
{
    const juce::ScopedLock sl (lock);
    auto& o = choirSpec;
    bool same = o.voices == c.voices && o.type == c.type && o.harmony == c.harmony && o.seed == c.seed
             && std::abs (o.spread - c.spread) < 1e-4 && std::abs (o.loose - c.loose) < 1e-4
             && std::abs (o.width - c.width) < 1e-4 && std::abs (o.room - c.room) < 1e-4 && std::abs (o.breath - c.breath) < 1e-4;
    for (int i = 0; i < 4; ++i) same = same && std::abs (o.sections[i] - c.sections[i]) < 1e-4;
    return ! choirValid || ! same || choirVersion != analysisVersion;
}

void Session::generateChoir (const ChoirSpec& spec)
{
    auto an = getAnalysis();
    if (an == nullptr) return;
    runJob ([this, spec, an]
    {
        int version;
        { const juce::ScopedLock sl (lock); version = analysisVersion; }
        auto singers = planChoir (*an, spec);
        setStatus ("Generating choir... " + juce::String ((int) singers.size()) + " voices");
        const size_t n = an->audio.size();
        std::array<Stereo, 4> sections;
        for (auto& s : sections) s.resize (n);
        juce::CriticalSection mixLock;
        std::atomic<int> done { 0 };
        for (auto& singer : singers)
        {
            renderPool->addJob ([&, singer]
            {
                if (stopRequested()) return;
                auto y = renderSinger (*an, singer);
                double a = (singer.pan + 1) * juce::MathConstants<double>::pi / 4;
                auto gl = (float) std::cos (a), gr = (float) std::sin (a);
                {
                    const juce::ScopedLock sl (mixLock);
                    auto& sec = sections[(size_t) singer.section];
                    for (size_t i = 0; i < n && i < y.size(); ++i) { sec.l[i] += y[i] * gl; sec.r[i] += y[i] * gr; }
                }
                ++done;
                progress = 0.95f * (float) done.load() / (float) singers.size();
                triggerAsyncUpdate();
            });
        }
        while (renderPool->getNumJobs() > 0) juce::Thread::sleep (20);
        if (stopRequested()) { setStatus ("Cancelled"); return; }

        // ROOM: one shared space for the whole choir
        if (spec.room > 0.01)
        {
            juce::Reverb verb;
            juce::Reverb::Parameters p;
            p.roomSize = (float) (0.55 + 0.4 * spec.room);
            p.damping = 0.45f;
            p.width = 1.0f;
            p.wetLevel = (float) (0.15 + 0.35 * spec.room);
            p.dryLevel = (float) (1.0 - 0.35 * spec.room);
            verb.setSampleRate (an->sampleRate);
            for (auto& sec : sections)
            {
                verb.setParameters (p);
                verb.reset();
                std::vector<float> l = sec.l, r = sec.r;
                l.resize (n + (size_t) an->sampleRate * 2, 0.0f);   // room tail is cut at the end of the vocal
                r.resize (l.size(), 0.0f);
                verb.processStereo (l.data(), r.data(), (int) l.size());
                std::copy (l.begin(), l.begin() + (long) n, sec.l.begin());
                std::copy (r.begin(), r.begin() + (long) n, sec.r.begin());
            }
        }
        {
            const juce::ScopedLock sl (lock);
            choirSections = std::move (sections);
            choirSpec = spec;
            choirValid = true;
            choirVersion = version;
        }
        rebuildPlayback();
        progress = 1.0f;
        setStatus ("Choir ready - " + juce::String ((int) singers.size()) + " voices");
    });
}

// ------------------------------------------------------------------ playback

void Session::setPlaybackRate (double hostRate)
{
    if (std::abs (hostRate - playRate) < 0.5) return;
    playRate = hostRate;
    if (hasVocal())
        worker.addJob ([this] { rebuildPlayback(); triggerAsyncUpdate(); });
}

void Session::rebuildPlayback()
{
    auto pd = std::make_shared<PlaybackData>();
    std::shared_ptr<Analysis> an;
    std::array<Stereo, numStrips> strips;
    std::array<bool, numStrips> valid {};
    std::array<Stereo, 4> choir;
    bool choirOk;
    {
        const juce::ScopedLock sl (lock);
        an = analysis;
        for (int i = 0; i < numStrips; ++i)
        {
            valid[(size_t) i] = results[(size_t) i].valid;
            if (valid[(size_t) i]) strips[(size_t) i] = results[(size_t) i].audio;
        }
        choirOk = choirValid;
        if (choirOk) choir = choirSections;
    }
    if (an == nullptr) return;
    double from = an->sampleRate, to = playRate;
    pd->rate = to;
    std::vector<float> lead (an->audio.begin(), an->audio.end());
    pd->lead = resample (lead, from, to);
    pd->length = pd->lead.size();
    for (int i = 0; i < numStrips; ++i)
        if (valid[(size_t) i]) { pd->strips[(size_t) i] = resample (strips[(size_t) i], from, to); pd->stripValid[(size_t) i] = true; }
    if (choirOk)
    {
        for (int s = 0; s < 4; ++s) pd->choirSections[(size_t) s] = resample (choir[(size_t) s], from, to);
        pd->choirValid = true;
    }
    auto old = std::atomic_exchange (&playback, std::shared_ptr<const PlaybackData> (pd));
    if (old != nullptr)
    {
        const juce::ScopedLock sl (lock);
        retired.push_back (old);
    }
}

// ------------------------------------------------------------------ export

juce::File Session::defaultExportFolder() const
{
    auto base = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("LT HALO");
    auto name = getVocalName();
    return base.getChildFile (safeName (name.isEmpty() ? juce::String ("Vocal") : name));
}

bool Session::stripReady (int i) const
{
    const juce::ScopedLock sl (lock);
    return analysis != nullptr && results[(size_t) i].valid;
}

juce::File Session::writeStrip (int i, StripGains g, const juce::File& folderIn, bool fresh, float* peak)
{
    std::shared_ptr<Analysis> an;
    Stereo audio;
    StripSpec spec;
    {
        const juce::ScopedLock sl (lock);
        if (! results[(size_t) i].valid || analysis == nullptr) return {};
        an = analysis;
        audio = results[(size_t) i].audio;
        spec = results[(size_t) i].spec;
    }
    for (auto& v : audio.l) v *= g.gl;
    for (auto& v : audio.r) v *= g.gr;
    auto folder = folderIn == juce::File() ? defaultExportFolder() : folderIn;
    auto f = folder.getChildFile (safeName (getVocalName() + " HALO " + juce::String (i + 1) + " " + partInfo (spec.part).name) + ".wav");
    if (fresh) f = freshFile (f);
    return writeWav (f, audio.l, audio.r, an->sampleRate, peak) ? f : juce::File();
}

juce::File Session::writeMix (const std::array<StripGains, numStrips>& gains, const juce::File& folderIn, bool fresh, float* peak)
{
    std::shared_ptr<Analysis> an;
    Stereo mix;
    {
        const juce::ScopedLock sl (lock);
        if (analysis == nullptr) return {};
        an = analysis;
        mix.resize (an->audio.size());
        bool any = false;
        for (int i = 0; i < numStrips; ++i)
        {
            auto& r = results[(size_t) i];
            if (! r.valid || ! gains[(size_t) i].audible) continue;
            any = true;
            for (size_t k = 0; k < mix.size() && k < r.audio.size(); ++k)
            {
                mix.l[k] += r.audio.l[k] * gains[(size_t) i].gl;
                mix.r[k] += r.audio.r[k] * gains[(size_t) i].gr;
            }
        }
        if (! any) return {};
    }
    auto folder = folderIn == juce::File() ? defaultExportFolder() : folderIn;
    auto f = folder.getChildFile (safeName (getVocalName() + " HALO harmonies") + ".wav");
    if (fresh) f = freshFile (f);
    return writeWav (f, mix.l, mix.r, an->sampleRate, peak) ? f : juce::File();
}

juce::Array<juce::File> Session::writeChoir (float gain, bool separate, const juce::File& folderIn, bool fresh)
{
    juce::Array<juce::File> files;
    std::shared_ptr<Analysis> an;
    std::array<Stereo, 4> secs;
    ChoirSpec spec;
    {
        const juce::ScopedLock sl (lock);
        if (! choirValid || analysis == nullptr) return files;
        an = analysis;
        secs = choirSections;
        spec = choirSpec;
    }
    auto folder = folderIn == juce::File() ? defaultExportFolder() : folderIn;
    auto base = getVocalName() + " HALO choir " + choirTypeName (spec.type) + " " + juce::String (spec.voices);
    const char* names[] = { "sopranos", "altos", "tenors", "basses" };
    auto counts = sectionCounts (spec);
    if (separate)
    {
        for (int s = 0; s < 4; ++s)
        {
            if (counts[(size_t) s] == 0) continue;
            auto l = secs[(size_t) s].l, r = secs[(size_t) s].r;
            for (auto& v : l) v *= gain;
            for (auto& v : r) v *= gain;
            auto f = folder.getChildFile (safeName (base + " " + names[s]) + ".wav");
            if (fresh) f = freshFile (f);
            if (writeWav (f, l, r, an->sampleRate)) files.add (f);
        }
        return files;
    }
    Stereo mix;
    mix.resize (an->audio.size());
    for (auto& s : secs)
        for (size_t k = 0; k < mix.size() && k < s.size(); ++k) { mix.l[k] += s.l[k] * gain; mix.r[k] += s.r[k] * gain; }
    auto f = folder.getChildFile (safeName (base) + ".wav");
    if (fresh) f = freshFile (f);
    if (writeWav (f, mix.l, mix.r, an->sampleRate)) files.add (f);
    return files;
}

} // namespace halo
