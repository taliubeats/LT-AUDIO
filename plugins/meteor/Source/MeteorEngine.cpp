#include "MeteorEngine.h"

namespace meteor
{

namespace { constexpr float twoPi = juce::MathConstants<float>::twoPi; }

const juce::StringArray& Choices::reverseSizes()
{
    static const juce::StringArray n { "1/16", "1/8", "1/4", "1/2", "1 BAR" };
    return n;
}

double Choices::reverseBeats (int i)
{
    static constexpr double b[] = { 0.25, 0.5, 1.0, 2.0, 4.0 };
    return b[juce::jlimit (0, 4, i)];
}

const juce::StringArray& Choices::rates()
{
    static const juce::StringArray n { "1/4", "1/8", "1/16", "1/32" };
    return n;
}

double Choices::rateBeats (int i)
{
    static constexpr double b[] = { 1.0, 0.5, 0.25, 0.125 };
    return b[juce::jlimit (0, 3, i)];
}

const juce::StringArray& Choices::patterns()
{
    static const juce::StringArray n { "STRAIGHT", "OFFBEAT", "TRESILLO", "RANDOM" };
    return n;
}

const juce::StringArray& Choices::voices()
{
    static const juce::StringArray n { "2", "3", "4" };
    return n;
}

const juce::StringArray& Choices::repeats()
{
    static const juce::StringArray n { "2", "3", "4", "6", "8" };
    return n;
}

int Choices::repeatCount (int i)
{
    static constexpr int r[] = { 2, 3, 4, 6, 8 };
    return r[juce::jlimit (0, 4, i)];
}

// ------------------------------------------------------------------

void MeteorEngine::Ring::prepare (int n)
{
    int size = 1;
    while (size < n) size <<= 1;
    buf.assign ((size_t) size, 0.0f);
    mask = size - 1;
    pos = 0;
}

float MeteorEngine::Ring::read (float d) const noexcept
{
    d = juce::jmax (0.0f, d);
    auto i = (int) d;
    auto f = d - (float) i;
    return at (i) + (at (i + 1) - at (i)) * f;
}

bool MeteorEngine::chopOpen (int pattern, juce::int64 step, double phase)
{
    switch (pattern)
    {
        case 0: return phase < 0.5;                                   // STRAIGHT: first half of every step
        case 1: return (step & 1) == 1;                               // OFFBEAT
        case 2: { auto s = ((step % 8) + 8) % 8; return (s == 0 || s == 3 || s == 6) && phase < 0.85; }   // TRESILLO 3-3-2
        default:
        {
            auto h = (juce::uint32) ((juce::uint64) step * 2654435761ull) ^ 0x9e37u;
            h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12;
            return (h & 0xffu) > 100u && phase < 0.9;
        }
    }
}

void MeteorEngine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    for (int c = 0; c < 2; ++c)
    {
        hist[c].prepare ((int) (sr * historySeconds) + 16);
        dblLine[c].prepare ((int) (sr * 0.1) + 16);
    }
    for (auto* s : { &mixS, &outS, &revMixS, &dblMixS })
        s->reset (sr, 0.03);
    reset();
}

void MeteorEngine::reset()
{
    for (int c = 0; c < 2; ++c) { hist[c].clear(); dblLine[c].clear(); }
    freeBeats = 0;
    revSlice = -1;
    gate = 1.0f;
    glitchStep = -1;
    glitchActive = false;
    glitchEnv = 0;
    glitchSamplesLeft = 0;
    primed = false;
}

void MeteorEngine::process (juce::AudioBuffer<float>& buffer, const LiveSettings& s)
{
    auto n = buffer.getNumSamples();
    auto numCh = juce::jmin (2, buffer.getNumChannels());
    if (n == 0 || numCh == 0) return;

    auto bpm = juce::jlimit (30.0, 300.0, s.bpm);
    auto beatsPerSample = bpm / 60.0 / sr;
    auto samplesPerBeat = sr * 60.0 / bpm;
    auto startBeat = s.hostPlaying ? s.ppq : freeBeats;
    if (! s.hostPlaying) freeBeats += beatsPerSample * n;
    else freeBeats = s.ppq + beatsPerSample * n;

    auto setT = [this] (juce::SmoothedValue<float>& sv, float target)
    {
        if (primed) sv.setTargetValue (target); else sv.setCurrentAndTargetValue (target);
    };
    setT (mixS, s.mix);
    setT (outS, juce::Decibels::decibelsToGain (s.outputDb));
    setT (revMixS, s.reverseOn ? s.reverseMix : 0.0f);
    setT (dblMixS, s.doubleOn ? s.doubleMix : 0.0f);
    primed = true;

    // reverse slice: at most half of the history (we read up to two slices back)
    auto revBeats = Choices::reverseBeats (s.reverseSize);
    auto revLen = juce::jmin (revBeats * samplesPerBeat, sr * (historySeconds * 0.5 - 0.05));
    revBeats = revLen / samplesPerBeat;
    auto fade = (float) (sr * 0.006);

    auto chopBeats = Choices::rateBeats (s.chopRate);
    auto glitchBeats = Choices::rateBeats (s.glitchRate);
    auto voices = juce::jlimit (1, 3, s.doubleVoices + 1);
    const float dblRate[3] = { 0.61f, 0.97f, 0.43f };
    const float dblDelayMs[3] = { 13.0f, 22.0f, 31.0f };
    const float dblPan[3] = { -1.0f, 1.0f, -0.35f };
    auto msToS = (float) sr * 0.001f;
    auto gateCoef = 1.0f - std::exp (-1.0f / ((float) sr * 0.0015f));
    auto envCoef = 1.0f - std::exp (-1.0f / ((float) sr * 0.003f));

    auto* d0 = buffer.getWritePointer (0);
    auto* d1 = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float inPk = 0, outPk = 0, gateMin = 1;
    bool revActive = false;

    for (int i = 0; i < n; ++i)
    {
        auto beat = startBeat + beatsPerSample * i;
        float x[2] = { d0[i], d1 != nullptr ? d1[i] : d0[i] };
        float dry[2] = { x[0], x[1] };
        inPk = juce::jmax (inPk, std::abs (x[0]), std::abs (x[1]));
        hist[0].push (x[0]);
        hist[1].push (x[1]);

        // ---- REVERSE: the previous slice, backwards
        auto rm = revMixS.getNextValue();
        if (rm > 0.0001f)
        {
            auto slice = (juce::int64) std::floor (beat / revBeats);
            auto k = (float) ((beat - (double) slice * revBeats) * samplesPerBeat);
            auto L = (float) revLen;
            auto w = juce::jlimit (0.0f, 1.0f, juce::jmin (k / fade, (L - k) / fade));
            if (slice != revSlice) revSlice = slice;
            for (int c = 0; c < 2; ++c)
                x[c] = x[c] * (1.0f - rm) + hist[c].read (2.0f * k + 1.0f) * w * rm;
            revActive = true;
        }

        // ---- GLITCH: at each step, maybe repeat the last slice (sometimes an octave up/down)
        if (s.glitchOn)
        {
            auto step = (juce::int64) std::floor (beat / glitchBeats);
            if (step != glitchStep)
            {
                glitchStep = step;
                if (rng.nextFloat() < s.glitchChance)
                {
                    auto stepLen = glitchBeats * samplesPerBeat;
                    glitchSliceLen = juce::jmax (64, (int) (stepLen / Choices::repeatCount (s.glitchRepeats)));
                    glitchStartAgo = glitchSliceLen;
                    glitchReadPos = 0;
                    auto r = rng.nextFloat();
                    glitchRate = r < 0.2f ? 2.0f : r < 0.32f ? 0.5f : 1.0f;
                    glitchSamplesLeft = (juce::int64) stepLen;
                    glitchActive = true;
                    glitches = glitches.load() + 1;
                }
                else glitchActive = false;
            }
        }
        else glitchActive = false;

        glitchEnv += ((glitchActive ? 1.0f : 0.0f) - glitchEnv) * envCoef;
        if (glitchEnv > 0.0001f && glitchSliceLen > 0)
        {
            auto rp = glitchReadPos;
            auto L = (float) glitchSliceLen;
            auto w = juce::jlimit (0.0f, 1.0f, juce::jmin (rp / (fade * 0.4f), (L - rp) / (fade * 0.4f)));
            auto ago = (float) glitchStartAgo - rp;
            for (int c = 0; c < 2; ++c)
                x[c] = x[c] * (1.0f - glitchEnv) + hist[c].read (ago) * w * glitchEnv;
        }
        if (glitchActive)
        {
            ++glitchStartAgo;
            glitchReadPos += glitchRate;
            if (glitchReadPos >= (float) glitchSliceLen) glitchReadPos -= (float) glitchSliceLen;
            if (--glitchSamplesLeft <= 0) glitchActive = false;
            glitchStartAgo = juce::jmin (glitchStartAgo, (int) (sr * historySeconds) - 4);
        }
        else if (glitchEnv > 0.0001f)
            ++glitchStartAgo;

        // ---- CHOP: rhythmic gate
        if (s.chopOn)
        {
            auto pos = beat / chopBeats;
            auto step = (juce::int64) std::floor (pos);
            auto open = chopOpen (s.chopPattern, step, pos - (double) step);
            auto target = open ? 1.0f : 1.0f - s.chopDepth;
            gate += (target - gate) * gateCoef;
        }
        else gate += (1.0f - gate) * gateCoef;
        x[0] *= gate;
        x[1] *= gate;
        gateMin = juce::jmin (gateMin, gate);

        // ---- DOUBLE: ADT-style copies, slightly detuned and delayed, spread in stereo
        dblLine[0].push (x[0]);
        dblLine[1].push (x[1]);
        auto dm = dblMixS.getNextValue();
        if (dm > 0.0001f)
        {
            float add[2] { 0, 0 };
            for (int v = 0; v < voices; ++v)
            {
                dblPhase[v] += twoPi * dblRate[v] / (float) sr;
                if (dblPhase[v] > twoPi) dblPhase[v] -= twoPi;
                auto delay = (dblDelayMs[v] + s.doubleDetune * 1.6f * std::sin (dblPhase[v])) * msToS;
                auto mono = 0.5f * (dblLine[0].read (delay) + dblLine[1].read (delay));
                auto pan = juce::jlimit (-1.0f, 1.0f, dblPan[v] * s.doubleWidth);
                add[0] += mono * std::sqrt (0.5f * (1.0f - pan));
                add[1] += mono * std::sqrt (0.5f * (1.0f + pan));
            }
            auto g = dm * 0.9f / std::sqrt ((float) voices);
            x[0] += add[0] * g;
            x[1] += add[1] * g;
        }

        auto mix = mixS.getNextValue();
        auto outG = outS.getNextValue();
        for (int c = 0; c < numCh; ++c)
        {
            auto y = (dry[c] * (1.0f - mix) + x[c] * mix) * outG;
            if (! std::isfinite (y)) y = 0.0f;
            (c == 0 ? d0 : d1)[i] = y;
            outPk = juce::jmax (outPk, std::abs (y));
        }
    }

    reversing = revActive;
    gateNow = gateMin;
    inLevel = juce::jmax (inPk, inLevel.load() * 0.85f);
    outLevel = juce::jmax (outPk, outLevel.load() * 0.85f);
}

} // namespace meteor
