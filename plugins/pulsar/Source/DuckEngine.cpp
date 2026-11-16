#include "DuckEngine.h"

namespace pulsar
{

DuckEngine::DuckEngine()
    : table ((size_t) tableSize + 1, 1.0f), pendingTable ((size_t) tableSize + 1, 1.0f)
{
    for (auto& b : inBins) b = 0.0f;
    for (auto& b : outBins) b = 0.0f;
    setCurve (Curve());
}

void DuckEngine::prepare (double sr, int numChannels)
{
    sampleRate = sr;
    channels = juce::jmax (1, numChannels);
    focusBuffer.setSize (channels, chunk, false, true, false);
    splitter.prepare (sr, chunk, channels);
    spectrum.setSampleRate (sr);
    mixSmoothed.reset (sr, 0.05);
    gainCoeff = 1.0f - std::exp (-1.0f / (0.0007f * (float) sr));   // ~0.7 ms: hard cuts stay sharp, without clicks
    reset();
}

void DuckEngine::reset()
{
    splitter.reset();
    gain = 1.0f;
    freePhase = 0.0;
    midiPhase = 1.0;
    midiRunning = false;
    currentBin = -1;
}

void DuckEngine::setCurve (const Curve& c)
{
    std::vector<float> t ((size_t) tableSize + 1);
    c.fillTable (t.data(), tableSize);
    {
        const juce::SpinLock::ScopedLockType sl (tableLock);
        pendingTable.swap (t);
    }
    tableDirty = true;
}

float DuckEngine::lookup (float phase) const noexcept
{
    auto pos = juce::jlimit (0.0f, (float) tableSize, phase * (float) tableSize);
    auto i = juce::jmin (tableSize - 1, (int) pos);
    auto frac = pos - (float) i;
    return table[(size_t) i] + (table[(size_t) i + 1] - table[(size_t) i]) * frac;
}

void DuckEngine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const Settings& s,
                          const Transport& t, const lt::dsp::FocusSettings& focus)
{
    if (tableDirty.load())
    {
        const juce::SpinLock::ScopedTryLockType sl (tableLock);
        if (sl.isLocked())
        {
            std::copy (pendingTable.begin(), pendingTable.end(), table.begin());
            tableDirty = false;
        }
    }

    mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, s.mix));
    spectrum.push (buffer);

    auto n = buffer.getNumSamples();
    for (int start = 0; start < n; start += chunk)
    {
        auto len = juce::jmin (chunk, n - start);
        auto chunkTransport = t;
        if (t.hasPosition)
            chunkTransport.ppq = t.ppq + (double) start / (sampleRate * 60.0 / juce::jmax (1.0, t.bpm));
        processChunk (buffer, start, len, midi, s, chunkTransport, focus, start);
    }
}

void DuckEngine::processChunk (juce::AudioBuffer<float>& buffer, int start, int n, const juce::MidiBuffer& midi,
                               const Settings& s, const Transport& t, const lt::dsp::FocusSettings& focus, int blockOffset)
{
    auto chans = juce::jmin (buffer.getNumChannels(), focusBuffer.getNumChannels());
    auto bpm = juce::jlimit (20.0, 999.0, t.bpm > 0.0 ? t.bpm : 120.0);
    auto samplesPerBeat = sampleRate * 60.0 / bpm;
    auto cycleBeats = Rates::beats (s.rate);
    auto inc = 1.0 / (cycleBeats * samplesPerBeat);

    // 1) where in the cycle every sample is
    auto midiIt = midi.findNextSamplePosition (blockOffset);
    for (int i = 0; i < n; ++i)
    {
        double phase;
        if (s.midiTrigger)
        {
            for (; midiIt != midi.cend() && (*midiIt).samplePosition <= blockOffset + i; ++midiIt)
                if ((*midiIt).getMessage().isNoteOn())
                {
                    midiPhase = 0.0;
                    midiRunning = true;
                    ++cycleCount;
                }

            phase = midiPhase;
            if (midiRunning)
            {
                midiPhase += inc;
                if (midiPhase >= 1.0) { midiPhase = 1.0; midiRunning = false; }
            }
        }
        else if (t.playing && t.hasPosition)
        {
            auto pos = (t.ppq + (double) i / samplesPerBeat) / cycleBeats;
            phase = pos - std::floor (pos);
            freePhase = phase;
        }
        else
        {
            phase = freePhase;
            freePhase += inc;
            freePhase -= std::floor (freePhase);
        }

        if (! s.midiTrigger && phase < lastPhase - 0.5)
            ++cycleCount;
        lastPhase = phase;
        phases[(size_t) i] = (float) phase;

        float peak = 0.0f;
        for (int c = 0; c < chans; ++c)
            peak = juce::jmax (peak, std::abs (buffer.getSample (c, start + i)));
        inAbs[(size_t) i] = peak;
    }

    // 2) split off the part we work on (all of it when Frequency Focus is FULL)
    juce::AudioBuffer<float> io (buffer.getArrayOfWritePointers(), chans, start, n);
    juce::AudioBuffer<float> target (focusBuffer.getArrayOfWritePointers(), chans, 0, n);
    splitter.split (io, target, focus);

    // 3) volume shape
    for (int i = 0; i < n; ++i)
    {
        auto mix = mixSmoothed.getNextValue();
        auto shape = lookup (phases[(size_t) i]);
        auto wanted = 1.0f - mix * (1.0f - shape);
        gain += (wanted - gain) * gainCoeff;
        for (int c = 0; c < chans; ++c)
            target.getWritePointer (c)[i] *= gain;
    }

    lt::dsp::FocusSplitter::combine (io, target, focus);

    // 4) what the visualiser shows
    for (int i = 0; i < n; ++i)
    {
        float peak = 0.0f;
        for (int c = 0; c < chans; ++c)
            peak = juce::jmax (peak, std::abs (io.getSample (c, i)));
        recordDisplay (phases[(size_t) i], inAbs[(size_t) i], peak);
        levelFollower = juce::jmax (inAbs[(size_t) i], levelFollower * 0.99995f);
    }

    displayPhase = phases[(size_t) n - 1];
    inputLevel = levelFollower;
    currentGain = gain;
}

void DuckEngine::recordDisplay (float phase, float in, float out) noexcept
{
    auto bin = juce::jlimit (0, displayBins - 1, (int) (phase * (float) displayBins));
    if (bin != currentBin)
    {
        if (currentBin >= 0)
        {
            inBins[(size_t) currentBin].store (binIn, std::memory_order_relaxed);
            outBins[(size_t) currentBin].store (binOut, std::memory_order_relaxed);
        }
        currentBin = bin;
        binIn = binOut = 0.0f;
    }
    binIn = juce::jmax (binIn, in);
    binOut = juce::jmax (binOut, out);
}

} // namespace pulsar
