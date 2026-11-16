#include "VoidEngine.h"

namespace lt_void
{

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    // delay line lengths at size 100 %, in ms (mutually prime-ish)
    constexpr float lineMs[VoidEngine::numLines] = { 41.3f, 47.9f, 53.7f, 61.1f, 67.9f, 73.3f, 83.9f, 97.1f };
    constexpr float diffMs[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
    constexpr float lineSignL[VoidEngine::numLines] = { 1, -1, 1, -1, 1, 1, -1, -1 };
    constexpr float lineSignR[VoidEngine::numLines] = { 1, 1, -1, -1, -1, 1, 1, -1 };

    inline void hadamard8 (float* x) noexcept
    {
        for (int h = 1; h < 8; h <<= 1)
            for (int i = 0; i < 8; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    auto a = x[j], b = x[j + h];
                    x[j] = a + b;
                    x[j + h] = a - b;
                }
        for (int i = 0; i < 8; ++i) x[i] *= 0.35355339f;   // 1 / sqrt (8)
    }

    inline int nextPow2 (int n) { int p = 1; while (p < n) p <<= 1; return p; }
}

void VoidEngine::Delay::prepare (int maxSamples)
{
    auto n = nextPow2 (juce::jmax (4, maxSamples + 4));
    buf.assign ((size_t) n, 0.0f);
    mask = n - 1;
    pos = 0;
}

float VoidEngine::Delay::read (float d) const noexcept
{
    d = juce::jmax (1.0f, d);
    auto i = (int) d;
    auto f = d - (float) i;
    auto xm1 = tap (i - 1), x0 = tap (i), x1 = tap (i + 1), x2 = tap (i + 2);
    // cubic Hermite
    auto c1 = 0.5f * (x1 - xm1);
    auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

float VoidEngine::coeffFor (float hz) const noexcept
{
    return 1.0f - std::exp (-twoPi * juce::jlimit (5.0f, (float) sr * 0.45f, hz) / (float) sr);
}

void VoidEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    auto s = (float) sr / 1000.0f;

    preL.prepare ((int) (s * 1100.0f));   // pre-delay up to 500 ms + bloom up to 600 ms
    preR.prepare ((int) (s * 1100.0f));

    for (int i = 0; i < 4; ++i)
    {
        diffL[(size_t) i].len = juce::jmax (1, (int) (diffMs[i] * s));
        diffR[(size_t) i].len = juce::jmax (1, (int) (diffMs[i] * 1.13f * s));
        diffL[(size_t) i].d.prepare (diffL[(size_t) i].len + 2);
        diffR[(size_t) i].d.prepare (diffR[(size_t) i].len + 2);
    }

    for (int i = 0; i < numLines; ++i)
    {
        baseLen[(size_t) i] = lineMs[i] * s;
        lines[(size_t) i].prepare ((int) (lineMs[i] * 2.1f * s) + (int) (0.01 * sr) + 8);
        lfoPhase[(size_t) i] = (float) i / (float) numLines;
    }

    juce::Random r (17);
    for (int i = 0; i < 12; ++i)
    {
        auto t = 0.004f + 0.075f * std::pow ((float) (i + 1) / 12.0f, 1.3f);
        erTimeL[(size_t) i] = t * (0.9f + 0.2f * r.nextFloat());
        erTimeR[(size_t) i] = t * (0.9f + 0.2f * r.nextFloat());
        erGain[(size_t) i] = (r.nextBool() ? 1.0f : -1.0f) * std::pow (0.82f, (float) i) * 0.5f;
    }

    shimBuf.prepare ((int) (0.12 * sr));
    revL.prepare ((int) (4.5 * sr));
    revR.prepare ((int) (4.5 * sr));

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, maxBlockSize), 2 };
    lowCutF.prepare (spec);  lowCutF.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    highCutF.prepare (spec); highCutF.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    mixS.reset (sr, 0.05);
    preS.reset (sr, 0.15);
    widthS.reset (sr, 0.05);
    wet.setSize (2, juce::jmax (1, maxBlockSize), false, true, false);
    reset();
}

void VoidEngine::reset()
{
    preL.clear(); preR.clear();
    for (auto& a : diffL) a.d.clear();
    for (auto& a : diffR) a.d.clear();
    for (auto& l : lines) l.clear();
    for (auto& d : damp) d.z = 0;
    for (auto& d : lowDamp) d.z = 0;
    for (auto& d : loopHp) d.z = 0;
    for (auto& d : dispState) d.fill (0.0f);
    shimBuf.clear(); revL.clear(); revR.clear();
    lowCutF.reset(); highCutF.reset();
    gateEnv = gateGain = 0; gateHold = 0;
    duckEnv = 0; inEnv = 0;
    lastType = -1;
}

void VoidEngine::updateBlock (const VoidSettings& s)
{
    if (s.type != lastType)
    {
        ch = TypeInfo::character (s.type);
        lastType = s.type;
    }
    cur = s;

    sizeTarget = ch.sizeScale * (0.25f + 0.75f * juce::jlimit (0.0f, 1.0f, s.size));
    sizeTarget = juce::jmin (sizeTarget, 2.0f);

    // gated: the tank rings long, the gate decides the length
    auto t60 = ch.gated ? 3.0f : juce::jlimit (0.1f, 60.0f, s.decay);
    for (int i = 0; i < numLines; ++i)
    {
        auto len = baseLen[(size_t) i] * sizeTarget;
        gains[(size_t) i] = std::pow (10.0f, -3.0f * len / (t60 * (float) sr));
    }

    dampCoeff = coeffFor (juce::jmin (s.highFreq, ch.maxDamp));
    lowDampCoeff = coeffFor (juce::jmax (10.0f, s.lowFreq));
    loopHpCoeff = ch.loopLowCut > 0.0f ? coeffFor (ch.loopLowCut) : 0.0f;
    modDepthSamples = s.modDepth * ch.modScale * 0.0011f * (float) sr;
    modInc = s.modRate / (float) sr;
    diffG = 0.25f + 0.5f * juce::jlimit (0.0f, 1.0f, s.diffusion);
    bloomSamples = s.attack * ch.bloom * 0.35f * (float) sr;

    auto lc = s.lowCut, hc = s.highCut;
    if (ch.crush == 2) { lc = juce::jmax (lc, 250.0f); hc = juce::jmin (hc, 3800.0f); }
    lowCutF.setCutoffFrequency (juce::jlimit (10.0f, (float) sr * 0.45f, lc));
    highCutF.setCutoffFrequency (juce::jlimit (200.0f, (float) sr * 0.45f, hc));

    mixS.setTargetValue (juce::jlimit (0.0f, 1.0f, s.mix));
    preS.setTargetValue (juce::jlimit (0.0f, 500.0f, s.preDelayMs) * 0.001f * (float) sr);
    widthS.setTargetValue (juce::jlimit (0.0f, 1.5f, s.width));
}

void VoidEngine::process (juce::AudioBuffer<float>& buffer, const VoidSettings& s)
{
    updateBlock (s);

    auto n = buffer.getNumSamples();
    auto chans = buffer.getNumChannels();
    if (n == 0 || chans == 0) return;
    if (wet.getNumSamples() < n) wet.setSize (2, n, false, true, true);

    auto* outL = buffer.getWritePointer (0);
    auto* outR = chans > 1 ? buffer.getWritePointer (1) : nullptr;

    const auto releaseCoeff = std::exp (-1.0f / (0.25f * (float) sr));
    const auto followCoeff = std::exp (-1.0f / (0.08f * (float) sr));
    const auto freezeStep = 1.0f / (0.05f * (float) sr);
    const auto revLen = juce::jlimit (0.2f, 2.0f, s.decay * 0.25f) * (float) sr;
    const auto gateHoldSamples = (int) (juce::jlimit (0.05f, 1.2f, s.decay * 0.08f) * (float) sr);
    const auto shimLen = 0.05f * (float) sr;
    float inPeak = 0, outPeak = 0, wetPeak = 0;

    for (int i = 0; i < n; ++i)
    {
        auto dryL = outL[i];
        auto dryR = outR != nullptr ? outR[i] : dryL;
        auto absIn = juce::jmax (std::abs (dryL), std::abs (dryR));
        inPeak = juce::jmax (inPeak, absIn);

        // transient detector for the visuals
        auto prevEnv = inEnv;
        inEnv = juce::jmax (absIn, inEnv * followCoeff);
        if (inEnv > 0.05f && inEnv > prevEnv * 1.6f + 0.02f) ++hits;

        freezeAmt = juce::jlimit (0.0f, 1.0f, freezeAmt + (s.freeze ? freezeStep : -freezeStep));
        auto feedIn = 1.0f - freezeAmt;

        // ---- pre-delay + bloom
        preL.push (dryL);
        preR.push (dryR);
        auto pd = preS.getNextValue();
        float xl = 0, xr = 0;
        if (bloomSamples < 16.0f)
        {
            xl = preL.read (pd); xr = preR.read (pd);
        }
        else
        {
            // rising taps spread over the attack time -> slow swell
            float norm = 0;
            for (int k = 0; k < 6; ++k)
            {
                auto w = (float) (k + 1);
                auto t = pd + bloomSamples * (float) k / 5.0f;
                xl += w * preL.read (t);
                xr += w * preR.read (t);
                norm += w;
            }
            xl /= norm * 0.45f; xr /= norm * 0.45f;
        }

        // ---- input diffusion
        for (auto& a : diffL) { a.g = diffG; xl = a.process (xl); }
        for (auto& a : diffR) { a.g = diffG; xr = a.process (xr); }

        // ---- tank
        sizeNow += (sizeTarget - sizeNow) * 0.0004f;
        float y[numLines];
        for (int l = 0; l < numLines; ++l)
        {
            auto& ph = lfoPhase[(size_t) l];
            ph += modInc * (1.0f + 0.13f * (float) l);
            if (ph >= 1.0f) ph -= 1.0f;
            auto mod = modDepthSamples * (0.5f + 0.5f * std::sin (twoPi * ph));
            auto v = lines[(size_t) l].read (baseLen[(size_t) l] * sizeNow + mod + 2.0f);

            // damping (bypassed while frozen so the sound never dies)
            auto dc = dampCoeff + (1.0f - dampCoeff) * freezeAmt;
            v = onePoleLP (damp[(size_t) l], v, dc);
            auto low = onePoleLP (lowDamp[(size_t) l], v, lowDampCoeff);
            v -= low * 0.65f * (1.0f - freezeAmt);
            if (loopHpCoeff > 0.0f)
                v -= onePoleLP (loopHp[(size_t) l], v, loopHpCoeff);

            if (ch.dispersion > 0.0f)
                for (auto& z : dispState[(size_t) l])
                {
                    // first-order allpass chain = spring chirp
                    auto a = ch.dispersion;
                    auto out = -a * v + z;
                    z = v + a * out;
                    v = out;
                }

            y[l] = v;
        }

        float wl = 0, wr = 0;
        for (int l = 0; l < numLines; ++l)
        {
            wl += y[l] * lineSignL[l];
            wr += y[l] * lineSignR[l];
        }
        wl *= 0.38f; wr *= 0.38f;

        // shimmer: octave up of the tail, fed back in
        float shim = 0;
        if (ch.shimmer > 0.0f)
        {
            shimBuf.push (0.5f * (wl + wr));
            shimPhase += 1.0f / shimLen;            // reading twice as fast = +12 st
            if (shimPhase >= 1.0f) shimPhase -= 1.0f;
            auto p2 = shimPhase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
            auto a = shimBuf.read (2.0f + (1.0f - shimPhase) * shimLen);
            auto b = shimBuf.read (2.0f + (1.0f - p2) * shimLen);
            auto wa = std::sin (juce::MathConstants<float>::pi * shimPhase);
            auto wb = std::sin (juce::MathConstants<float>::pi * p2);
            shim = onePoleLP (shimLp, a * wa * wa + b * wb * wb, coeffFor (7000.0f)) * ch.shimmer;
        }

        hadamard8 (y);
        for (int l = 0; l < numLines; ++l)
        {
            auto g = gains[(size_t) l] + (1.0f - gains[(size_t) l]) * freezeAmt;
            auto inject = ((l & 1) ? xr : xl) * 0.5f * feedIn + shim * feedIn;
            auto v = y[l] * g + inject;
            lines[(size_t) l].push (juce::jlimit (-8.0f, 8.0f, v));
        }

        // early reflections
        if (ch.erGain > 0.0f)
        {
            float el = 0, er = 0;
            auto spread = ch.erSpread * (0.4f + 0.6f * s.size) * (float) sr;
            for (int k = 0; k < 12; ++k)
            {
                el += erGain[(size_t) k] * preL.read (pd + erTimeL[(size_t) k] * spread);
                er += erGain[(size_t) k] * preR.read (pd + erTimeR[(size_t) k] * spread);
            }
            wl += el * ch.erGain * feedIn;
            wr += er * ch.erGain * feedIn;
        }

        // reverse: play the tail backwards in overlapping chunks
        if (ch.reverse)
        {
            revL.push (wl); revR.push (wr);
            revPhase += 1.0f / revLen;
            if (revPhase >= 1.0f) revPhase -= 1.0f;
            float rl = 0, rr = 0;
            for (auto off : { 0.0f, 0.5f })
            {
                auto p = revPhase + off; if (p >= 1.0f) p -= 1.0f;
                // newest sample at p = 0 is (p * revLen) old; reading backwards through the last chunk
                auto age = 2.0f * p * revLen + 1.0f;
                auto w = std::sin (juce::MathConstants<float>::pi * p);
                rl += revL.read (age) * w * w;
                rr += revR.read (age) * w * w;
            }
            wl = rl; wr = rr;
        }

        // gate
        if (ch.gated)
        {
            if (absIn > 0.03f) gateHold = gateHoldSamples;
            auto open = gateHold > 0;
            if (gateHold > 0) --gateHold;
            gateGain += ((open ? 1.0f : 0.0f) - gateGain) * (open ? 0.02f : 0.0015f);
            wl *= gateGain; wr *= gateGain;
        }

        // vintage / lo-fi crush
        if (ch.crush > 0)
        {
            auto factor = ch.crush == 1 ? 2 : 4;
            auto levels = ch.crush == 1 ? 2048.0f : 128.0f;
            if (++crushCount >= factor) { crushCount = 0; crushL = std::round (wl * levels) / levels; crushR = std::round (wr * levels) / levels; }
            wl = crushL; wr = crushR;
            if (ch.crush == 2) { wl += (noise.nextFloat() - 0.5f) * 0.002f; wr += (noise.nextFloat() - 0.5f) * 0.002f; }
        }

        // wet EQ
        wl = highCutF.processSample (0, lowCutF.processSample (0, wl));
        wr = highCutF.processSample (1, lowCutF.processSample (1, wr));

        // width
        auto width = widthS.getNextValue();
        auto mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
        wl = (mid + side) * ch.outputGain;
        wr = (mid - side) * ch.outputGain;

        // duck: the tail steps aside while you play
        duckEnv = juce::jmax (absIn, duckEnv * releaseCoeff);
        auto duckDb = juce::Decibels::gainToDecibels (duckEnv, -80.0f);
        auto duckGain = 1.0f - s.duck * 0.9f * juce::jlimit (0.0f, 1.0f, (duckDb + 45.0f) / 35.0f);

        auto mix = mixS.getNextValue();
        auto dryG = std::cos (mix * juce::MathConstants<float>::halfPi);
        auto wetG = std::sin (mix * juce::MathConstants<float>::halfPi) * duckGain;

        wetPeak = juce::jmax (wetPeak, std::abs (wl), std::abs (wr));
        auto ol = dryL * dryG + wl * wetG;
        auto orr = dryR * dryG + wr * wetG;
        outL[i] = ol;
        if (outR != nullptr) outR[i] = orr;
        outPeak = juce::jmax (outPeak, std::abs (ol), std::abs (orr));
    }

    auto fall = [] (std::atomic<float>& a, float v) { a = juce::jmax (v, a.load() * 0.85f); };
    fall (inputLevel, inPeak);
    fall (outputLevel, outPeak);
    fall (wetLevel, wetPeak);
}

} // namespace lt_void
