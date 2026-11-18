#include "TuneEngine.h"
#include <cmath>

namespace tune
{

namespace
{
    int nextPow2 (int n)
    {
        int p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    int hopFor (double sr)        { return juce::jmax (1, (int) std::round (sr / 11025.0)) * 32; }
    int maxPeriodFor (double sr, int voice) { return (int) std::ceil (sr / Voices::minHz (voice)); }
}

int TuneEngine::latencyFor (double sampleRate, int v, bool highQuality)
{
    auto p = maxPeriodFor (sampleRate, v);
    return (highQuality ? 3 : 2) * p + hopFor (sampleRate) + 8;
}

void TuneEngine::prepare (double sampleRate, int maxBlockSize, int v, bool highQuality)
{
    sr = sampleRate;
    voice = v;
    hq = highQuality;
    hop = hopFor (sr);
    margin = hop + 8;
    maxPeriod = maxPeriodFor (sr, voice);
    latency = latencyFor (sr, voice, hq);
    unvoicedPeriod = (float) (sr * 0.0045);

    detector.prepare (sr, Voices::minHz (voice), Voices::maxHz (voice), hq ? 2.0f : 1.0f);

    auto size = nextPow2 (2 * (latency + 4 * maxPeriod) + juce::jmax (1024, maxBlockSize));
    mask = size - 1;
    for (int c = 0; c < maxCh; ++c)
    {
        in[c].assign ((size_t) size, 0.0f);
        acc[c].assign ((size_t) size, 0.0f);
    }
    wsum.assign ((size_t) size, 0.0f);
    mixS.reset (sr, 0.05);
    reset();
}

void TuneEngine::reset()
{
    for (int c = 0; c < maxCh; ++c)
    {
        std::fill (in[c].begin(), in[c].end(), 0.0f);
        std::fill (acc[c].begin(), acc[c].end(), 0.0f);
    }
    std::fill (wsum.begin(), wsum.end(), 0.0f);
    detector.reset();
    marks.clear();
    now = 0;
    nextMark = nextSynth = 0.0;
    hopCounter = 0;
    ratio = formantRatio = 1.0;
    voiced = false;
    unvoicedHops = 0;
    target = -1;
    corr = trend = noteAge = vibPhase = voicedAge = 0.0f;
    periodNow = unvoicedPeriod;
    inPeak = outPeak = 0.0f;
}

float TuneEngine::readInput (int ch, double pos) const noexcept
{
    auto i = (juce::int64) std::floor (pos);
    auto f = (float) (pos - (double) i);
    const auto& b = in[ch];
    auto y0 = b[(size_t) ((i - 1) & mask)], y1 = b[(size_t) (i & mask)],
         y2 = b[(size_t) ((i + 1) & mask)], y3 = b[(size_t) ((i + 2) & mask)];
    // cubic Hermite
    auto c1 = 0.5f * (y2 - y0);
    auto c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

void TuneEngine::placeGrain (double s, const Mark& a, double rf, double radius, int numChannels)
{
    auto k0 = (juce::int64) std::ceil (s - radius), k1 = (juce::int64) std::floor (s + radius);
    const auto invR = 1.0 / radius;
    for (auto k = k0; k <= k1; ++k)
    {
        auto u = ((double) k - s) * invR;
        auto w = (float) (0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * u));
        auto src = a.pos + ((double) k - s) * rf;
        auto idx = (size_t) (k & mask);
        wsum[idx] += w;
        for (int c = 0; c < numChannels; ++c)
            acc[c][idx] += w * readInput (c, src);
    }
}

void TuneEngine::analyseHop (const TuneSettings& s)
{
    const auto dt = (float) hop / (float) sr;
    auto r = detector.analyse();

    if (r.voiced)
    {
        // a sudden octave jump with low clarity is usually a detection error: keep the previous pitch
        auto jump = lastHz > 0.0f ? std::abs (std::log2 (r.hz / lastHz)) : 0.0f;
        auto suspicious = voiced && jump > 0.9f && jump < 1.1f && r.clarity < 0.9f;
        if (! suspicious) lastHz = r.hz;
        unvoicedHops = 0;
        if (! voiced)
        {
            voiced = true;
            trend = 69.0f + 12.0f * std::log2 (lastHz / s.refHz);
            noteAge = voicedAge = 0.0f;
            target = -1;
        }
    }
    else if (voiced && ++unvoicedHops > 3)
    {
        voiced = false;
    }

    formantRatio = std::pow (2.0, (double) s.formant / 12.0);

    bool anyNote = false;
    for (auto b : s.mask) anyNote = anyNote || b;
    auto allowed = [&] (int note) { return ! anyNote || s.mask[(size_t) (((note % 12) + 12) % 12)]; };

    if (voiced && lastHz > 0.0f)
    {
        periodNow = (float) (sr / lastHz);
        auto m = 69.0f + 12.0f * std::log2 (lastHz / s.refHz);
        trend += (m - trend) * (1.0f - std::exp (-dt * juce::MathConstants<float>::twoPi * 2.5f));
        auto vib = m - trend;
        auto mv = s.vibrato < 0.0f ? m + s.vibrato * vib : m;

        // nearest allowed note, with hysteresis so it does not flicker between two notes
        int best = -1;
        float bestD = 1e9f;
        auto centre = (int) std::round (mv);
        for (int d = -7; d <= 7; ++d)
            if (allowed (centre + d) && std::abs (mv - (float) (centre + d)) < bestD)
            {
                bestD = std::abs (mv - (float) (centre + d));
                best = centre + d;
            }
        if (target < 0 || ! allowed (target) || std::abs (mv - (float) target) > bestD + 0.3f)
        {
            if (target != best)
            {
                target = best;
                noteAge = 0.0f;
                retunes.fetch_add (1);
            }
        }
        noteAge += dt;
        voicedAge += dt;

        auto dev = (float) target - mv;
        // FLEX TUNE: only notes close to the target are corrected; big expressive moves stay natural
        auto inv = 1.0f - s.flex;
        auto zone = 0.05f + inv * 0.75f + inv * inv * inv * inv * 6.0f;   // FLEX 0 = always corrected
        auto weight = juce::jlimit (0.0f, 1.0f, 1.0f - (std::abs (dev) - zone) / 0.15f);

        // RETUNE SPEED + HUMANIZE (sustained notes get a slower retune)
        auto sustain = juce::jlimit (0.0f, 1.0f, (noteAge - 0.15f) / 0.5f);
        auto tau = s.retuneMs * 0.001f * (1.0f + 3.0f * s.humanize * sustain) + 0.12f * s.humanize * sustain;
        auto goal = dev * weight;
        corr = tau < 0.0005f ? goal : corr + (goal - corr) * (1.0f - std::exp (-dt / tau));

        float vibAdd = 0.0f;
        if (s.vibrato > 0.0f)
        {
            vibPhase = std::fmod (vibPhase + juce::MathConstants<float>::twoPi * 5.5f * dt, juce::MathConstants<float>::twoPi);
            vibAdd = s.vibrato * 0.5f * std::sin (vibPhase) * juce::jlimit (0.0f, 1.0f, (noteAge - 0.1f) / 0.3f);
        }

        auto shift = (mv - m) + corr + vibAdd + s.transpose;
        ratio = std::pow (2.0, (double) juce::jlimit (-24.0f, 24.0f, shift) / 12.0);

        uiInputMidi = m;
        uiOutputMidi = m + shift;
        uiTarget = target;
        uiCorrection = (mv - m) + corr;
    }
    else
    {
        corr *= std::exp (-dt / 0.05f);
        ratio = 1.0;
        uiTarget = -1;
        uiCorrection = 0.0f;
    }
    uiVoiced = voiced;
}

void TuneEngine::process (juce::AudioBuffer<float>& buffer, const TuneSettings& s)
{
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (maxCh, buffer.getNumChannels());
    if (numCh == 0 || n == 0) return;

    mixS.setTargetValue (s.bypass ? 0.0f : juce::jlimit (0.0f, 1.0f, s.mix));
    float blockIn = 0.0f, blockOut = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float mono = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            auto x = buffer.getSample (c, i);
            in[c][(size_t) (now & mask)] = x;
            mono += x;
        }
        mono /= (float) numCh;
        blockIn = juce::jmax (blockIn, std::abs (mono));
        detector.push (mono);
        ++now;

        if (++hopCounter >= hop)
        {
            hopCounter = 0;
            analyseHop (s);
        }

        // synthesis grains that can be placed now
        for (;;)
        {
            while (nextMark <= nextSynth)
            {
                auto P = (double) (voiced ? periodNow : unvoicedPeriod);
                marks.push_back ({ nextMark, P });
                nextMark += P;
            }
            while (marks.size() > 1 && marks[1].pos <= nextSynth)
                marks.pop_front();
            const auto& a = marks.front();

            auto r = voiced ? ratio : 1.0;
            auto rf = formantRatio * (s.preserve ? 1.0 : r);
            auto pOut = a.period / r;
            auto radius = juce::jmin (a.period, (double) (latency - margin) / (1.0 + rf));   // two-period grains
            radius = juce::jmax (4.0, radius);
            if (nextSynth + radius * rf + 3.0 > (double) (now - 1))
                break;
            placeGrain (nextSynth, a, rf, radius, numCh);
            nextSynth += juce::jmax (2.0, pOut);
        }

        // output (analysis time now - 1 - latency)
        auto t = (size_t) ((now - 1 - latency) & mask);
        auto norm = 1.0f / juce::jmax (wsum[t], 0.35f);
        auto mixNow = mixS.getNextValue();
        float o = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            auto wet = acc[c][t] * norm;
            auto dry = in[c][t];
            auto y = dry + mixNow * (wet - dry);
            acc[c][t] = 0.0f;
            buffer.setSample (c, i, y);
            o += y;
        }
        wsum[t] = 0.0f;
        blockOut = juce::jmax (blockOut, std::abs (o / (float) numCh));
    }

    auto fall = std::exp (-(float) n / (float) (sr * 0.25));
    inPeak = juce::jmax (blockIn, inPeak * fall);
    outPeak = juce::jmax (blockOut, outPeak * fall);
    inLevel = inPeak;
    outLevel = outPeak;
}

} // namespace tune
