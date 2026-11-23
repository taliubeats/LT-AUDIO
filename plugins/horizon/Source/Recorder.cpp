#include "Recorder.h"

namespace horizon
{

namespace
{
    // The oldest part of the ring can be overwritten by the audio thread while we copy it,
    // so reads stay this far away from it.
    constexpr juce::int64 safetyMargin = 16384;
}

Recorder::Recorder()
{
    allocate (44100.0, defaultSeconds);
}

void Recorder::allocate (double sr, double seconds)
{
    seconds = juce::jlimit (minSeconds, maxSeconds, seconds);
    auto s = std::make_unique<Store>();
    auto wanted = (juce::int64) std::ceil (sr * seconds) + safetyMargin;
    s->capacity = (wanted + peakBlock - 1) / peakBlock * peakBlock;
    s->audio.setSize (2, (int) s->capacity);
    s->audio.clear();
    s->peaks.assign ((size_t) (s->capacity / peakBlock), 0.0f);

    const juce::SpinLock::ScopedLockType lock (swapLock);
    store = std::move (s);
    capacity.store (store->capacity);
    sampleRate.store (sr);
    lengthSeconds.store (seconds);
    written.store (0);
    pendingPeak = 0.0f;
}

void Recorder::prepare (double sr, double seconds)
{
    if (sr <= 0.0) sr = 44100.0;
    if (store != nullptr && juce::approximatelyEqual (sr, sampleRate.load()) && juce::approximatelyEqual (seconds, lengthSeconds.load()))
        return;                                         // hosts call prepareToPlay often: keep what we have
    allocate (sr, seconds);
}

void Recorder::setLength (double seconds)
{
    seconds = juce::jlimit (minSeconds, maxSeconds, seconds);
    if (juce::approximatelyEqual (seconds, lengthSeconds.load())) return;

    auto sr = sampleRate.load();
    auto s = std::make_unique<Store>();
    auto wanted = (juce::int64) std::ceil (sr * seconds) + safetyMargin;
    s->capacity = (wanted + peakBlock - 1) / peakBlock * peakBlock;
    s->audio.setSize (2, (int) s->capacity);
    s->audio.clear();
    s->peaks.assign ((size_t) (s->capacity / peakBlock), 0.0f);

    // Keep the newest audio that fits. Done under the lock so nothing is written meanwhile
    // (the audio thread skips those blocks; it never waits).
    const juce::SpinLock::ScopedLockType lock (swapLock);
    auto end = written.load();
    auto keepFrom = juce::jmax ((juce::int64) 0, end - juce::jmin (store->capacity, s->capacity) + peakBlock);
    keepFrom = keepFrom / peakBlock * peakBlock;
    for (auto pos = keepFrom; pos < end; )
    {
        auto from = (int) (pos % store->capacity), to = (int) (pos % s->capacity);
        auto n = (int) juce::jmin (end - pos, store->capacity - from, s->capacity - to);
        for (int ch = 0; ch < 2; ++ch)
            s->audio.copyFrom (ch, to, store->audio, ch, from, n);
        pos += n;
    }
    for (auto c = keepFrom / peakBlock; c < end / peakBlock; ++c)
        s->peaks[(size_t) (c % (juce::int64) s->peaks.size())] = store->peaks[(size_t) (c % (juce::int64) store->peaks.size())];

    store = std::move (s);
    capacity.store (store->capacity);
    lengthSeconds.store (seconds);
}

void Recorder::push (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (paused.load() || numChannels <= 0 || numSamples <= 0) return;
    const juce::SpinLock::ScopedTryLockType lock (swapLock);
    if (! lock.isLocked() || store == nullptr) return;

    auto& s = *store;
    auto pos = written.load (std::memory_order_relaxed);
    auto* l = channels[0];
    auto* r = channels[numChannels > 1 ? 1 : 0];
    auto* outL = s.audio.getWritePointer (0);
    auto* outR = s.audio.getWritePointer (1);
    auto numCols = (juce::int64) s.peaks.size();

    for (int i = 0; i < numSamples; ++i, ++pos)
    {
        auto idx = (size_t) (pos % s.capacity);
        outL[idx] = l[i];
        outR[idx] = r[i];
        pendingPeak = juce::jmax (pendingPeak, std::abs (l[i]), std::abs (r[i]));
        if ((pos + 1) % peakBlock == 0)
        {
            s.peaks[(size_t) ((pos / peakBlock) % numCols)] = pendingPeak;
            pendingPeak = 0.0f;
        }
    }
    written.store (pos, std::memory_order_release);
}

juce::int64 Recorder::getStart() const noexcept
{
    auto start = getEnd() - capacity.load() + safetyMargin;
    return juce::jmax ((juce::int64) 0, start);
}

juce::Range<juce::int64> Recorder::clampToAvailable (juce::Range<juce::int64> r) const noexcept
{
    return r.getIntersectionWith ({ getStart(), getEnd() });
}

juce::AudioBuffer<float> Recorder::read (juce::Range<juce::int64> range) const
{
    range = clampToAvailable (range);
    if (range.isEmpty() || store == nullptr) return {};
    auto& s = *store;
    juce::AudioBuffer<float> out (2, (int) range.getLength());
    int done = 0;
    for (auto pos = range.getStart(); pos < range.getEnd(); )
    {
        auto from = (int) (pos % s.capacity);
        auto n = (int) juce::jmin (range.getEnd() - pos, s.capacity - from);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, done, s.audio, ch, from, n);
        pos += n;
        done += n;
    }
    return out;
}

float Recorder::peakAt (juce::int64 pos) const noexcept
{
    if (store == nullptr || pos < getStart() || pos >= getEnd() - (getEnd() % peakBlock)) return 0.0f;
    auto& p = store->peaks;
    return p[(size_t) ((pos / peakBlock) % (juce::int64) p.size())];
}

float Recorder::peakOver (juce::int64 start, juce::int64 end) const noexcept
{
    float m = 0.0f;
    start = start / peakBlock * peakBlock;
    for (auto pos = start; pos < end; pos += peakBlock)
        m = juce::jmax (m, peakAt (pos));
    return m;
}

int Recorder::readForPreview (juce::int64 pos, float* const* out, int numChannels, int numSamples) const noexcept
{
    const juce::SpinLock::ScopedTryLockType lock (swapLock);
    if (! lock.isLocked() || store == nullptr) return 0;
    auto avail = clampToAvailable ({ pos, pos + numSamples });
    if (avail.getStart() != pos || avail.isEmpty()) return 0;
    auto& s = *store;
    auto n = (int) avail.getLength();
    for (int i = 0; i < n; ++i)
    {
        auto idx = (int) ((pos + i) % s.capacity);
        for (int ch = 0; ch < numChannels; ++ch)
            out[ch][i] += s.audio.getSample (juce::jmin (ch, 1), idx);
    }
    return n;
}

bool Recorder::writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double sr)
{
    if (audio.getNumSamples() == 0) return false;
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
    if (stream == nullptr) return false;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), sr, (unsigned int) audio.getNumChannels(), 24, {}, 0));
    if (writer == nullptr) return false;
    stream.release();                                   // the writer owns it now
    return writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
}

juce::String Recorder::makeFileName (double bpm, bool withBpm, juce::Time when)
{
    juce::String name ("LT Horizon");
    if (withBpm && bpm > 0.0)
    {
        auto rounded = std::round (bpm * 100.0) / 100.0;
        name << " " << (juce::approximatelyEqual (rounded, std::round (rounded)) ? juce::String ((int) rounded) : juce::String (rounded, 2)) << " BPM";
    }
    name << " " << when.formatted ("%Y-%m-%d %H-%M-%S");
    return name + ".wav";
}

} // namespace horizon
