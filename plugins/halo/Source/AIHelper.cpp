#include "AIHelper.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace halo
{

juce::File AIHelper::launcher()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("LT Audio/HALO/ai/halo-ai");
}

bool AIHelper::available()
{
    return launcher().existsAsFile();
}

namespace
{
    bool writeMono (const juce::File& f, const std::vector<float>& x, int sr)
    {
        f.deleteFile();
        std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
        if (os == nullptr) return false;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 1, 32, {}, 0));
        if (w == nullptr) return false;
        os.release();
        const float* ch[] = { x.data() };
        return w->writeFromFloatArrays (ch, 1, (int) x.size());
    }

    bool readMono (const juce::File& f, std::vector<float>& out, size_t len)
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (f.createInputStream().release(), true));
        if (r == nullptr) return false;
        juce::AudioBuffer<float> b (1, (int) r->lengthInSamples);
        r->read (&b, 0, (int) r->lengthInSamples, 0, true, false);
        out.assign (len, 0.0f);
        for (size_t i = 0; i < len && (int) i < b.getNumSamples(); ++i) out[i] = b.getSample (0, (int) i);
        return true;
    }

    double rms (const std::vector<float>& v)
    {
        double s = 0;
        for (auto x : v) s += (double) x * x;
        return std::sqrt (s / std::max<size_t> (1, v.size())) + 1e-12;
    }
}

bool AIHelper::render (const Analysis& an, std::vector<Item>& items, const std::function<void (float)>& progress,
                       const std::function<bool()>& shouldStop)
{
    if (! available()) return false;
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LT_HALO_AI_" + juce::String (juce::Random().nextInt64()));
    dir.createDirectory();
    const int sr = an.sampleRate;

    std::vector<float> lead (an.audio.begin(), an.audio.end());
    auto ref = dir.getChildFile ("reference.wav");
    if (! writeMono (ref, lead, sr)) return false;

    juce::Array<juce::var> jobItems;
    std::vector<int> map;
    for (size_t k = 0; k < items.size(); ++k)
    {
        if (! items[k].eligible || items[k].stereo == nullptr || items[k].stereo->empty()) continue;
        auto& st = *items[k].stereo;
        std::vector<float> mono (st.size());
        for (size_t i = 0; i < st.size(); ++i) mono[i] = 0.5f * (st.l[i] + st.r[i]);
        auto src = dir.getChildFile ("src_" + juce::String ((int) k) + ".wav");
        if (! writeMono (src, mono, sr)) continue;
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("source", src.getFullPathName());
        obj->setProperty ("out", dir.getChildFile ("ai_" + juce::String ((int) k) + ".wav").getFullPathName());
        jobItems.add (juce::var (obj));
        map.push_back ((int) k);
    }
    if (map.empty()) { dir.deleteRecursively(); return true; }

    auto* job = new juce::DynamicObject();
    job->setProperty ("reference", ref.getFullPathName());
    job->setProperty ("steps", 25);
    job->setProperty ("cfg", 0.7);
    job->setProperty ("items", jobItems);
    auto jobFile = dir.getChildFile ("job.json");
    jobFile.replaceWithText (juce::JSON::toString (juce::var (job)));

    juce::ChildProcess proc;
    if (! proc.start (juce::StringArray { launcher().getFullPathName(), jobFile.getFullPathName() },
                      juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        dir.deleteRecursively();
        return false;
    }
    int finished = 0;
    juce::String pending;
    while (proc.isRunning())
    {
        if (shouldStop && shouldStop()) { proc.kill(); dir.deleteRecursively(); return false; }
        char buf[512];
        auto n = proc.readProcessOutput (buf, sizeof (buf) - 1);
        if (n > 0)
        {
            pending += juce::String::fromUTF8 (buf, n);
            while (pending.containsChar ('\n'))
            {
                auto line = pending.upToFirstOccurrenceOf ("\n", false, false);
                pending = pending.fromFirstOccurrenceOf ("\n", false, false);
                if (line.contains (".wav") && line.contains ("for")) ++finished;   // "[seedvc] ai_0.wav  40s for 30s of audio"
                if (progress) progress ((float) finished / (float) map.size());
            }
        }
        else juce::Thread::sleep (50);
    }
    bool ok = proc.getExitCode() == 0;
    for (size_t j = 0; j < map.size() && ok; ++j)
    {
        auto& st = *items[(size_t) map[j]].stereo;
        std::vector<float> ai;
        if (! readMono (dir.getChildFile ("ai_" + juce::String (map[j]) + ".wav"), ai, st.size())) { ok = false; break; }
        auto gl = (float) (rms (st.l) / rms (ai)), gr = (float) (rms (st.r) / rms (ai));
        for (size_t i = 0; i < st.size(); ++i) { st.l[i] = ai[i] * gl; st.r[i] = ai[i] * gr; }
    }
    dir.deleteRecursively();
    return ok;
}

} // namespace halo
