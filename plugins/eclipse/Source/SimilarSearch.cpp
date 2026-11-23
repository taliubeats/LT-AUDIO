#include "SimilarSearch.h"
#include <regex>
#include <mutex>
#include <map>

namespace eclipse
{

int familyFromPath (const juce::String& pathLower)
{
    const auto dir = pathLower.upToLastOccurrenceOf ("/", false, false).toStdString();
    const auto file = pathLower.fromLastOccurrenceOf ("/", false, false).toStdString();
    struct R { const char* rx; Family f; };
    static const R rules[] = {
        { "open.?hat|/oh/|/oh |/ohs/|openhat", Family::openHat }, { "hi.?hat|hihat|/hats?/|/hh/|/hh |closed.?hat", Family::hat },
        { "clap", Family::clap }, { "snare|/sn/|/rims?/|rimshot", Family::snare }, { "kick|/bd/|808|bombo", Family::kick },
        { "/toms?/|/tom |timbal", Family::tom }, { "crash|/ride|cymbal", Family::cymbal },
        { "perc|conga|bongo|shaker|cowbell|guira|guiro|tamb|block|clave", Family::perc } };
    try
    {
        for (auto& r : rules) if (std::regex_search (dir, std::regex (r.rx))) return (int) r.f;
        for (auto& r : rules) if (std::regex_search (file, std::regex (r.rx))) return (int) r.f;
    }
    catch (...) {}
    return -1;
}

SimilarSearch::SimilarSearch() : juce::Thread ("LT Eclipse library index") { load(); }

SimilarSearch::~SimilarSearch() { stopThread (5000); }

juce::File SimilarSearch::indexFile() const
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("LT Audio/LT Eclipse");
    d.createDirectory();
    return d.getChildFile ("Library Index");
}

juce::File SimilarSearch::folder() const { const juce::ScopedLock sl (lock); return root; }
int SimilarSearch::size() const { const juce::ScopedLock sl (lock); return (int) entries.size(); }

void SimilarSearch::setFolder (const juce::File& f)
{
    stopThread (5000);
    { const juce::ScopedLock sl (lock); if (f != root) entries.clear(); root = f; }
    scanning = true; progress = 0;
    startThread (juce::Thread::Priority::low);
}

juce::AudioBuffer<float> SimilarSearch::loadOneShot (const juce::File& f, double& sr, double maxSeconds)
{
    static juce::AudioFormatManager fm;
    static std::once_flag once;
    std::call_once (once, [] { fm.registerBasicFormats(); });
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples < 64) return {};
    const int len = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * maxSeconds));
    juce::AudioBuffer<float> b ((int) juce::jlimit (1u, 2u, r->numChannels), len);
    r->read (&b, 0, len, 0, true, b.getNumChannels() > 1);
    sr = r->sampleRate;
    return b;
}

static juce::int64 onsetOf (const juce::AudioBuffer<float>& b)
{
    const float pk = b.getMagnitude (0, b.getNumSamples());
    if (pk <= 0) return 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
        for (int c = 0; c < b.getNumChannels(); ++c)
            if (std::abs (b.getSample (c, i)) > pk * 0.05f) return juce::jmax (0, i - 16);
    return 0;
}

void SimilarSearch::run()
{
    const auto dir = folder();
    std::map<juce::String, Entry> old;
    { const juce::ScopedLock sl (lock); for (auto& e : entries) old[e.file.getFullPathName()] = e; }
    juce::Array<juce::File> files;
    for (const auto& it : juce::RangedDirectoryIterator (dir, true, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.WAV;*.AIF", juce::File::findFiles, juce::File::FollowSymlinks::no))
    {
        if (threadShouldExit()) { scanning = false; return; }
        if (! it.getFile().getFileName().startsWith ("._") && it.getFileSize() < 4 * 1024 * 1024) files.add (it.getFile());
    }
    std::vector<Entry> next;
    for (int i = 0; i < files.size(); ++i)
    {
        if (threadShouldExit()) { scanning = false; return; }
        const auto& f = files.getReference (i);
        const auto mod = f.getLastModificationTime().toMilliseconds();
        auto it = old.find (f.getFullPathName());
        if (it != old.end() && it->second.modified == mod) { next.push_back (it->second); continue; }
        double sr = 44100;
        auto b = loadOneShot (f, sr);
        if (b.getNumSamples() == 0) continue;
        Entry e;
        e.file = f; e.modified = mod;
        e.f = classifier::features (b, onsetOf (b), sr);
        e.p = classifier::predict (e.f);
        e.label = familyFromPath (f.getFullPathName().toLowerCase());
        next.push_back (e);
        progress = (float) (i + 1) / (float) files.size();
        if (i % 200 == 199) { const juce::ScopedLock sl (lock); entries = next; }
    }
    { const juce::ScopedLock sl (lock); entries = std::move (next); }
    save();
    progress = 1.0f;
    scanning = false;
}

std::vector<SimilarSearch::Match> SimilarSearch::find (const juce::AudioBuffer<float>& b, double sr, Family family, bool sameFamily, int count) const
{
    const auto q = classifier::features (b, onsetOf (b), sr);
    const auto& net = classifier::net();
    // standardised features: every band counts the same
    auto z = [&] (const classifier::Features& f, int i) { return net.ready() ? (f[(size_t) i] - net.mean[(size_t) i]) / net.std[(size_t) i] : f[(size_t) i]; };
    std::vector<float> qz ((size_t) classifier::numFeatures);
    float qn = 0;
    for (int i = 0; i < classifier::numFeatures; ++i) { qz[(size_t) i] = z (q, i); qn += qz[(size_t) i] * qz[(size_t) i]; }
    qn = std::sqrt (qn) + 1.0e-6f;
    std::vector<Match> out;
    const juce::ScopedLock sl (lock);
    for (auto& e : entries)
    {
        // what this sample is: its folder name if it says, otherwise what the network hears
        int fam = e.label;
        if (fam < 0) { fam = 0; for (int k = 1; k < classifier::numClasses; ++k) if (e.p[(size_t) k] > e.p[(size_t) fam]) fam = k; }
        if (sameFamily && family != Family::sample && fam != (int) family) continue;
        float dot = 0, en = 0;
        for (int i = 0; i < classifier::numFeatures; ++i) { const float v = z (e.f, i); dot += v * qz[(size_t) i]; en += v * v; }
        const float cosine = dot / (qn * (std::sqrt (en) + 1.0e-6f));
        out.push_back ({ e.file, juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * cosine), (Family) fam });
    }
    std::sort (out.begin(), out.end(), [] (const Match& x, const Match& y) { return x.score > y.score; });
    if ((int) out.size() > count) out.resize ((size_t) count);
    return out;
}

void SimilarSearch::save() const
{
    juce::MemoryOutputStream m;
    const juce::ScopedLock sl (lock);
    m.writeInt (0x4C545331);  // "LTS1"
    m.writeInt (classifier::numFeatures);
    m.writeString (root.getFullPathName());
    m.writeInt ((int) entries.size());
    for (auto& e : entries)
    {
        m.writeString (e.file.getFullPathName());
        m.writeInt64 (e.modified);
        m.writeInt (e.label);
        m.write (e.f.data(), sizeof (float) * e.f.size());
        m.write (e.p.data(), sizeof (float) * e.p.size());
    }
    indexFile().replaceWithData (m.getData(), m.getDataSize());
}

void SimilarSearch::load()
{
    juce::MemoryBlock mb;
    if (! indexFile().loadFileAsData (mb)) return;
    juce::MemoryInputStream in (mb, false);
    if (in.readInt() != 0x4C545331 || in.readInt() != classifier::numFeatures) return;
    const juce::ScopedLock sl (lock);
    root = juce::File (in.readString());
    const int n = in.readInt();
    for (int i = 0; i < n && ! in.isExhausted(); ++i)
    {
        Entry e;
        e.file = juce::File (in.readString());
        e.modified = in.readInt64();
        e.label = in.readInt();
        in.read (e.f.data(), (int) (sizeof (float) * e.f.size()));
        in.read (e.p.data(), (int) (sizeof (float) * e.p.size()));
        entries.push_back (e);
    }
}

} // namespace eclipse
