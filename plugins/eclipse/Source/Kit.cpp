#include "Kit.h"

namespace eclipse
{

juce::String noteName (int n)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[n % 12]) + juce::String (n / 12 - 2);   // 36 = C1 (Ableton naming)
}

int Analysis::count (Family f) const
{
    int n = 0;
    for (auto& h : hits) n += h.family == f ? 1 : 0;
    return n;
}

PadSound Kit::pad (int i) const
{
    const juce::ScopedLock sl (lock);
    return pads[(size_t) i];
}

void Kit::setPad (int i, PadSound p)
{
    {
        const juce::ScopedLock sl (lock);
        pads[(size_t) i] = std::move (p);
        rebuildSnapshot();
    }
}

void Kit::clearAll()
{
    const juce::ScopedLock sl (lock);
    for (auto& p : pads) p = {};
    rebuildSnapshot();
}

void Kit::rebuildSnapshot()
{
    auto s = std::make_shared<Snapshot>();
    for (int i = 0; i < numPads; ++i)
    {
        s->raw[(size_t) i] = pads[(size_t) i].raw;
        s->clean[(size_t) i] = pads[(size_t) i].clean;
        s->rate[(size_t) i] = pads[(size_t) i].sampleRate;
    }
    std::atomic_store (&snap, std::shared_ptr<const Snapshot> (s));
}

std::shared_ptr<const Kit::Snapshot> Kit::snapshot() const { return std::atomic_load (&snap); }

// ------------------------------------------------------------------ audio <-> memory
juce::MemoryBlock encodeFlac (const juce::AudioBuffer<float>& b, double sr)
{
    juce::MemoryBlock mb;
    juce::FlacAudioFormat flac;
    auto* os = new juce::MemoryOutputStream (mb, false);
    std::unique_ptr<juce::AudioFormatWriter> w (flac.createWriterFor (os, sr, (unsigned) b.getNumChannels(), 24, {}, 5));
    if (w == nullptr) { delete os; return {}; }
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    w.reset();
    return mb;
}

AudioPtr decodeAudio (const juce::MemoryBlock& mb, double& sr)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (std::make_unique<juce::MemoryInputStream> (mb, false)));
    if (r == nullptr) return nullptr;
    auto b = std::make_shared<juce::AudioBuffer<float>> ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (b.get(), 0, (int) r->lengthInSamples, 0, true, true);
    sr = r->sampleRate;
    return b;
}

bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    if (os == nullptr) return false;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned) b.getNumChannels(), 24, {}, 0));
    if (w == nullptr) return false;
    os.release();
    return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

// ------------------------------------------------------------------ state
static juce::ValueTree patternTree (const Pattern& p)
{
    juce::ValueTree t ("PATTERN");
    t.setProperty ("bars", p.bars, nullptr);
    t.setProperty ("name", p.name, nullptr);
    t.setProperty ("kit", p.kit, nullptr);
    juce::String s;
    for (auto& n : p.notes) s << n.pad << ',' << n.tick << ',' << n.length << ',' << juce::roundToInt (n.velocity * 127) << ';';
    t.setProperty ("notes", s, nullptr);
    return t;
}

static Pattern patternFrom (const juce::ValueTree& t)
{
    Pattern p;
    p.bars = juce::jlimit (1, 8, (int) t.getProperty ("bars", 1));
    p.name = t.getProperty ("name").toString();
    p.kit = juce::jlimit (0, numBanks - 1, (int) t.getProperty ("kit", 0));
    for (auto& tok : juce::StringArray::fromTokens (t.getProperty ("notes").toString(), ";", ""))
    {
        auto v = juce::StringArray::fromTokens (tok, ",", "");
        if (v.size() == 4) p.notes.push_back ({ v[0].getIntValue(), v[1].getIntValue(), v[2].getIntValue(), v[3].getIntValue() / 127.0f });
    }
    return p;
}

juce::ValueTree Kit::toValueTree() const
{
    juce::ValueTree t ("KIT");
    t.setProperty ("source", sourceName, nullptr);
    t.setProperty ("sourceFile", sourceFile.getFullPathName(), nullptr);
    t.setProperty ("ai", usedAI, nullptr);
    t.setProperty ("pattern", currentPattern, nullptr);
    if (analysis) t.setProperty ("bpm", analysis->bpm, nullptr);
    const juce::ScopedLock sl (lock);
    for (int i = 0; i < numPads; ++i)
    {
        auto& p = pads[(size_t) i];
        if (p.empty()) continue;
        juce::ValueTree pt ("PAD");
        pt.setProperty ("i", i, nullptr);
        pt.setProperty ("name", p.name, nullptr);
        pt.setProperty ("type", (int) p.type, nullptr);
        pt.setProperty ("tag", p.tag, nullptr);
        pt.setProperty ("group", p.group, nullptr);
        pt.setProperty ("hit", p.hitIndex, nullptr);
        pt.setProperty ("raw", juce::var (encodeFlac (*p.raw, p.sampleRate)), nullptr);
        if (p.clean != nullptr && p.clean != p.raw) pt.setProperty ("clean", juce::var (encodeFlac (*p.clean, p.sampleRate)), nullptr);
        t.appendChild (pt, nullptr);
    }
    for (auto& p : patterns) t.appendChild (patternTree (p), nullptr);
    return t;
}

void Kit::fromValueTree (const juce::ValueTree& t)
{
    if (! t.hasType ("KIT")) return;
    sourceName = t.getProperty ("source").toString();
    sourceFile = juce::File (t.getProperty ("sourceFile").toString());
    usedAI = t.getProperty ("ai", false);
    patterns.clear();
    {
        const juce::ScopedLock sl (lock);
        for (auto& p : pads) p = {};
        for (auto c : t)
        {
            if (c.hasType ("PATTERN")) { patterns.push_back (patternFrom (c)); continue; }
            if (! c.hasType ("PAD")) continue;
            const int i = c.getProperty ("i", -1);
            if (! juce::isPositiveAndBelow (i, numPads)) continue;
            PadSound p;
            p.name = c.getProperty ("name").toString();
            p.type = (Family) juce::jlimit (0, 8, (int) c.getProperty ("type", 8));
            p.tag = c.getProperty ("tag").toString();
            p.group = c.getProperty ("group", -1);
            p.hitIndex = c.getProperty ("hit", 0);
            if (auto* mb = c.getProperty ("raw").getBinaryData()) p.raw = decodeAudio (*mb, p.sampleRate);
            double sr2 = p.sampleRate;
            if (auto* mb = c.getProperty ("clean").getBinaryData()) p.clean = decodeAudio (*mb, sr2);
            if (p.clean == nullptr) p.clean = p.raw;
            if (p.raw != nullptr) pads[(size_t) i] = std::move (p);
        }
        rebuildSnapshot();
    }
    currentPattern = juce::jlimit (0, juce::jmax (0, (int) patterns.size() - 1), (int) t.getProperty ("pattern", 0));
    // the analysis (for HIT browsing) is rebuilt by the processor if the source file is still there
    auto a = std::make_shared<Analysis>();
    a->bpm = t.getProperty ("bpm", 120.0);
    analysis = a;
}

} // namespace eclipse
