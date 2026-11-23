#include "DrumSeparator.h"
#include "Kit.h"
#include <juce_audio_formats/juce_audio_formats.h>

#include "model.hpp"     // demucs.cpp

namespace eclipse
{

namespace
{
const char* drumsModelName = "ggml-model-htdemucs_ft_drums-4s-f16.bin";
const char* partsModelName = "ggml-model-drumsep-v3-f16.bin";
constexpr double modelRate = 44100.0;

struct Cancelled {};

juce::String audioKey (const juce::AudioBuffer<float>& b, double sr)
{
    // cheap fingerprint of the content (not the file name: the same song from anywhere hits the cache)
    juce::MemoryOutputStream m;
    m.writeInt (b.getNumChannels()); m.writeInt (b.getNumSamples()); m.writeDouble (sr);
    const int step = juce::jmax (1, b.getNumSamples() / 20000);
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); i += step) m.writeFloat (b.getSample (c, i));
    return juce::String::toHexString ((juce::int64) juce::DefaultHashFunctions::generateHash (juce::String::toHexString (m.getData(), (int) m.getDataSize(), 0), std::numeric_limits<int>::max()))
         + "-" + juce::String (b.getNumSamples()) + "-v2";
}

std::shared_ptr<juce::AudioBuffer<float>> resample (const juce::AudioBuffer<float>& in, double from, double to)
{
    if (std::abs (from - to) < 0.5) return std::make_shared<juce::AudioBuffer<float>> (in);
    const double ratio = from / to;
    const int outLen = (int) std::ceil (in.getNumSamples() / ratio);
    auto out = std::make_shared<juce::AudioBuffer<float>> (in.getNumChannels(), outLen);
    for (int c = 0; c < in.getNumChannels(); ++c)
    {
        juce::WindowedSincInterpolator interp;
        interp.process (ratio, in.getReadPointer (c), out->getWritePointer (c), outLen, in.getNumSamples(), 0);
    }
    return out;
}

juce::File findModel (const char* name, const char* env)
{
    if (auto* e = std::getenv (env)) { juce::File f (e); if (f.existsAsFile()) return f; }
    // the installer puts them in /Library/Application Support; a copy in the user's Application Support works too
    // (note: JUCE's userApplicationDataDirectory is ~/Library on the Mac, not ~/Library/Application Support)
   #if JUCE_WINDOWS
    // Windows: C:\ProgramData (installer) or %APPDATA% (user copy)
    for (auto base : { juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory),
                       juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory) })
   #else
    for (auto base : { juce::File ("/Library/Application Support"),
                       juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Application Support") })
   #endif
    {
        auto f = base.getChildFile ("LT Audio").getChildFile ("LT Eclipse").getChildFile ("Models").getChildFile (name);
        if (f.existsAsFile()) return f;
    }
    return {};
}

std::shared_ptr<const juce::AudioBuffer<float>> loadCached (const juce::File& f, int numSamples, double sr)
{
    if (! f.existsAsFile()) return nullptr;
    juce::MemoryBlock mb;
    double r = sr;
    if (! f.loadFileAsData (mb)) return nullptr;
    auto a = decodeAudio (mb, r);
    return a != nullptr && a->getNumSamples() == numSamples ? a : nullptr;
}

void saveCached (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    auto mb = encodeFlac (b, sr);
    f.replaceWithData (mb.getData(), mb.getSize());
    // keep the cache small: the 120 most recent files (about 30 songs with their parts)
    auto files = DrumSeparator::cacheFolder().findChildFiles (juce::File::findFiles, false, "*.flac");
    if (files.size() > 120)
    {
        std::sort (files.begin(), files.end(), [] (const juce::File& x, const juce::File& y) { return x.getLastModificationTime() > y.getLastModificationTime(); });
        for (int i = 120; i < files.size(); ++i) files[i].deleteFile();
    }
}

/**
    Runs a Demucs model on the audio in pieces of 12 s (+0.75 s on each side, crossfaded), shared by a
    few workers (each needs ~1.2 GB, so the memory sets how many). Returns the wanted targets at 44.1 kHz.
*/
template <typename Infer>
std::vector<juce::AudioBuffer<float>> runChunked (const juce::AudioBuffer<float>& in44, const std::vector<int>& targets,
                                                  Infer infer, const std::atomic<bool>* cancel,
                                                  const std::function<void (float, const juce::String&)>& report, const juce::String& what, bool& failed, double gbPerWorker)
{
    const int n = in44.getNumSamples();
    const int cores = juce::jmax (1, juce::SystemStats::getNumCpus() - 1);
    const double ramGB = juce::SystemStats::getMemorySizeInMegabytes() / 1024.0;
    int workers = juce::jlimit (1, juce::jmin (cores, 8), (int) std::floor ((ramGB - 3.5) / gbPerWorker));
    if (auto* t = std::getenv ("ECLIPSE_AI_THREADS")) workers = juce::jlimit (1, 16, juce::String (t).getIntValue());
    const int chunk = (int) (modelRate * 12.0), ov = (int) (modelRate * 0.75);
    const int numChunks = juce::jmax (1, (n + chunk - 1) / chunk);
    workers = juce::jmin (workers, numChunks);
    const size_t T = targets.size();
    std::vector<std::vector<std::vector<float>>> outs ((size_t) numChunks, std::vector<std::vector<float>> (T * 2));
    std::vector<std::atomic<float>> done ((size_t) numChunks);
    std::atomic<int> next { 0 }, finished { 0 };
    std::atomic<bool> bad { false };
    std::vector<std::thread> pool;
    for (int wkr = 0; wkr < workers; ++wkr)
    {
        pool.emplace_back ([&]
        {
            for (int c = next++; c < numChunks && ! bad.load(); c = next++)
            {
                const int start = c * chunk, end = juce::jmin (n, start + chunk);
                const int a = juce::jmax (0, start - ov), b = juce::jmin (n, end + ov);
                Eigen::MatrixXf seg (2, b - a);
                for (int ch = 0; ch < 2; ++ch)
                {
                    const auto* x = in44.getReadPointer (juce::jmin (ch, in44.getNumChannels() - 1));
                    for (int i = a; i < b; ++i) seg (ch, i - a) = x[i];
                }
                demucscpp::ProgressCallback cb = [&, c] (float p, const std::string&)
                {
                    done[(size_t) c] = p;
                    if (cancel != nullptr && cancel->load()) throw Cancelled();
                };
                try
                {
                    auto o = infer (seg, cb);
                    for (size_t t = 0; t < T; ++t)
                        for (int ch = 0; ch < 2; ++ch)
                        {
                            auto& v = outs[(size_t) c][t * 2 + (size_t) ch];
                            v.resize ((size_t) (b - a));
                            for (int i = 0; i < b - a; ++i) v[(size_t) i] = o (targets[t], ch, i);
                        }
                    done[(size_t) c] = 1.0f;
                }
                catch (...) { bad = true; }
            }
            ++finished;
        });
    }
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    while (finished.load() < workers)
    {
        float sum = 0;
        for (auto& d : done) sum += d.load();
        const float frac = sum / (float) numChunks;
        juce::String txt (what);
        const double el = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        if (frac > 0.04f)
        {
            const int left = (int) (el / frac * (1.0 - frac));
            txt << juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) << (left >= 60 ? juce::String (left / 60) + " min " : juce::String()) << juce::String (left % 60).paddedLeft ('0', left >= 60 ? 2 : 1) << " s left";
        }
        report (0.98f * frac, txt);
        juce::Thread::sleep (200);
    }
    for (auto& th : pool) th.join();
    failed = bad.load() || (cancel != nullptr && cancel->load());
    std::vector<juce::AudioBuffer<float>> res;
    if (failed) return res;
    for (size_t t = 0; t < T; ++t)
    {
        juce::AudioBuffer<float> o (2, n);
        o.clear();
        std::vector<float> weight ((size_t) n, 0.0f);
        for (int c = 0; c < numChunks; ++c)
        {
            const int start = c * chunk, end = juce::jmin (n, start + chunk);
            const int a = juce::jmax (0, start - ov), b = juce::jmin (n, end + ov);
            for (int i = a; i < b; ++i)
            {
                float w = 1.0f;
                if (i < start) w = (float) (i - a) / (float) juce::jmax (1, start - a);
                else if (i >= end) w = (float) (b - i) / (float) juce::jmax (1, b - end);
                w = juce::jmax (w, 1.0e-3f);
                o.addSample (0, i, outs[(size_t) c][t * 2][(size_t) (i - a)] * w);
                o.addSample (1, i, outs[(size_t) c][t * 2 + 1][(size_t) (i - a)] * w);
                weight[(size_t) i] += w;
            }
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* x = o.getWritePointer (ch);
            for (int i = 0; i < n; ++i) x[i] /= juce::jmax (1.0e-3f, weight[(size_t) i]);
        }
        res.push_back (std::move (o));
    }
    return res;
}

std::shared_ptr<juce::AudioBuffer<float>> backToSong (juce::AudioBuffer<float>& b44, const juce::AudioBuffer<float>& like, double sr)
{
    if (like.getNumChannels() == 1)
    {
        juce::AudioBuffer<float> mono (1, b44.getNumSamples());
        for (int i = 0; i < b44.getNumSamples(); ++i) mono.setSample (0, i, 0.5f * (b44.getSample (0, i) + b44.getSample (1, i)));
        b44 = std::move (mono);
    }
    auto out = resample (b44, modelRate, sr);
    out->setSize (like.getNumChannels(), like.getNumSamples(), true, true);
    return out;
}
}

juce::String DrumSeparator::keyOf (const juce::AudioBuffer<float>& b, double sr) { return audioKey (b, sr); }

juce::File DrumSeparator::modelFile() { return findModel (drumsModelName, "ECLIPSE_MODEL"); }
juce::File DrumSeparator::partsModelFile() { return findModel (partsModelName, "ECLIPSE_PARTS_MODEL"); }

juce::File DrumSeparator::cacheFolder()
{
   #if JUCE_WINDOWS
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("LT Audio/LT Eclipse/Stems");
   #else
    auto d = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Caches/LT Eclipse/Stems");
   #endif
    d.createDirectory();
    return d;
}

std::shared_ptr<juce::AudioBuffer<float>> DrumSeparator::separate (const juce::AudioBuffer<float>& src, double sr,
                                                                    const std::atomic<bool>* cancel, Progress progress,
                                                                    juce::String& error, bool useCache)
{
    auto report = [&] (float p, const juce::String& s) { if (progress) progress (p, s); };
    const auto model = modelFile();
    if (! model.existsAsFile()) { error = "The AI model is not installed."; return nullptr; }

    // each song is separated once
    const auto cacheFile = cacheFolder().getChildFile (audioKey (src, sr) + ".flac");
    if (useCache)
        if (auto a = loadCached (cacheFile, src.getNumSamples(), sr))
        {
            report (1.0f, "AI drum stem (cached)");
            return std::const_pointer_cast<juce::AudioBuffer<float>> (a);
        }

    report (0.0f, "AI: loading the model");
    demucscpp::demucs_model m;
    if (! demucscpp::load_demucs_model (model.getFullPathName().toStdString(), &m)) { error = "The AI model can't be loaded."; return nullptr; }
    auto in44 = resample (src, sr, modelRate);
    if (in44->getNumSamples() < (int) modelRate) { error = "Too short for the AI (less than 1 s)."; return nullptr; }

    bool failed = false;
    auto outs = runChunked (*in44, { 0 }, [&m] (const Eigen::MatrixXf& seg, demucscpp::ProgressCallback& cb) { return demucscpp::demucs_inference (m, seg, cb); },
                            cancel, report, "AI: separating the drums", failed, 1.3);
    if (failed || outs.empty()) { if (error.isEmpty()) error = "AI separation stopped."; return nullptr; }
    auto drums = backToSong (outs[0], src, sr);
    if (useCache) saveCached (cacheFile, *drums, sr);
    report (1.0f, "AI drum stem ready");
    return drums;
}

std::array<std::shared_ptr<juce::AudioBuffer<float>>, 4> DrumSeparator::separateParts (const juce::AudioBuffer<float>& drums, double sr,
                                                                                       const std::atomic<bool>* cancel, Progress progress,
                                                                                       juce::String& error, bool useCache, const juce::String& songKey)
{
    std::array<std::shared_ptr<juce::AudioBuffer<float>>, 4> parts {};
    auto report = [&] (float p, const juce::String& s) { if (progress) progress (p, s); };
    const auto model = partsModelFile();
    if (! model.existsAsFile()) { error = "The drum parts model is not installed."; return parts; }

    const auto key = (songKey.isNotEmpty() ? songKey : audioKey (drums, sr)) + "-parts";
    if (useCache)
    {
        bool all = true;
        for (int p = 0; p < 4; ++p)
        {
            auto a = loadCached (cacheFolder().getChildFile (key + "-part" + juce::String (p) + ".flac"), drums.getNumSamples(), sr);
            if (a == nullptr) { all = false; break; }
            parts[(size_t) p] = std::const_pointer_cast<juce::AudioBuffer<float>> (a);
        }
        if (all) { report (1.0f, "AI drum parts (cached)"); return parts; }
        parts = {};
    }

    report (0.0f, "AI: loading the drum parts model");
    auto m = std::make_unique<demucscpp_v3::demucs_v3_model>();
    if (! demucscpp_v3::load_demucs_v3_model (model.getFullPathName().toStdString(), m.get())) { error = "The drum parts model can't be loaded."; return parts; }
    auto in44 = resample (drums, sr, modelRate);
    bool failed = false;
    auto outs = runChunked (*in44, { 0, 1, 2, 3 }, [&m] (const Eigen::MatrixXf& seg, demucscpp::ProgressCallback& cb) { return demucscpp_v3::demucs_v3_inference (*m, seg, cb); },
                            cancel, report, "AI: kick / snare / hats", failed, 2.2);
    if (failed || outs.size() != 4) { if (error.isEmpty()) error = "AI separation stopped."; return {}; }
    for (int p = 0; p < 4; ++p)
    {
        parts[(size_t) p] = backToSong (outs[(size_t) p], drums, sr);
        if (useCache) saveCached (cacheFolder().getChildFile (key + "-part" + juce::String (p) + ".flac"), *parts[(size_t) p], sr);
    }
    report (1.0f, "AI drum parts ready");
    return parts;
}

} // namespace eclipse
