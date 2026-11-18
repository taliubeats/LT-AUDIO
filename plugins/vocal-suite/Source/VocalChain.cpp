#include "VocalChain.h"

namespace vocal
{

namespace
{
    inline float dbToGain (float db) noexcept { return juce::Decibels::decibelsToGain (db); }
    inline float gainToDb (float g) noexcept  { return juce::Decibels::gainToDecibels (g, -100.0f); }
    inline float coeffFor (double sr, float ms) noexcept { return std::exp (-1.0f / (float) (sr * 0.001 * juce::jmax (0.01f, ms))); }
}

void VocalChain::prepare (double sampleRate, int maxBlockSize, int voice, bool hq)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    tuneEngine.prepare (sr, maxBlock, voice, hq);
    novaEngine.prepare (sr, maxBlock, 2);
    delayEngine.prepare (sr, maxBlock);
    reverbEngine.prepare (sr, maxBlock);
    limLook = juce::jmax (8, (int) std::lround (sr * 0.0015));
    for (auto& b : limBuf) b.assign ((size_t) limLook, 0.0f);
    limReq.assign ((size_t) limLook, 1.0f);
    limPos = 0; limGain = 1.0f;
    limAtt = 1.0f - std::exp (-1.0f / ((float) limLook * 0.22f));
    limRel = std::exp (-1.0f / (float) (sr * 0.12));
    latency = tuneEngine.getLatency() + novaEngine.getLatencySamples() + limLook;

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
    essSplit.prepare (spec);
    essSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    dryDelay.prepare (spec);
    dryDelay.setMaximumDelayInSamples (juce::jmax (1, latency + 1));
    dryDelay.setDelay ((float) latency);
    doubleLine.prepare (spec);
    dry.setSize (2, maxBlock);
    dbl.setSize (2, maxBlock);
    workBuf.setSize (2, maxBlock);

    outGain.reset (sr, 0.05);
    bypassMix.reset (sr, 0.03);
    outGain.setCurrentAndTargetValue (1.0f);
    bypassMix.setCurrentAndTargetValue (0.0f);
    filtersReady = false;
    reset();
}

void VocalChain::reset()
{
    tuneEngine.reset();
    novaEngine.reset();
    delayEngine.reset();
    reverbEngine.reset();
    dryDelay.reset();
    doubleLine.reset();
    for (auto* f : { &lowCut, &lowCut2, &body, &mud, &presence, &air, &essDetect, &essDetect2, &phoneHp, &phoneLp })
        for (auto& x : *f) x.reset();
    for (auto& lb : limBuf) std::fill (lb.begin(), lb.end(), 0.0f);
    std::fill (limReq.begin(), limReq.end(), 1.0f);
    limGain = 1.0f;
    gateEnv = compEnv = essEnvHi = essEnvAll = 0.0f;
    essGainSm = 1.0f;
    progDb = -24.0f; makeupDb = 0.0f; preCompPow = postCompPow = 1.0e-3f;
    essSplit.reset();
    gateGain = 1.0f;
    lfoPhase = 0.0;
}

void VocalChain::updateFilters (const ChainSettings& s)
{
    using C = juce::dsp::IIR::Coefficients<float>;
    auto nyq = sr * 0.45;
    auto lc  = C::makeHighPass (sr, juce::jlimit (10.0, nyq, (double) s.lowCutHz), 0.54f);    // two stages = 4th-order Butterworth-ish
    auto lc2 = C::makeHighPass (sr, juce::jlimit (10.0, nyq, (double) s.lowCutHz), 1.31f);
    auto bd  = C::makeLowShelf (sr, 180.0, 0.7f, dbToGain (s.body));
    auto md  = C::makePeakFilter (sr, 350.0, 1.0f, dbToGain (-juce::jmax (0.0f, s.mudCut)));
    auto pr  = C::makePeakFilter (sr, 3200.0, 0.9f, dbToGain (s.presence));
    auto ar  = C::makeHighShelf (sr, juce::jmin (12000.0, nyq), 0.7f, dbToGain (s.air));
    auto es  = C::makeHighPass (sr, juce::jlimit (1000.0, nyq, (double) s.deEssHz * 0.85), 0.7f);    // sibilance detector band
    auto es2 = C::makeLowPass (sr, juce::jlimit (2000.0, nyq, (double) s.deEssHz * 1.8), 0.7f);
    auto phH = C::makeHighPass (sr, 450.0, 0.9f);
    auto phL = C::makeLowPass (sr, juce::jmin (3200.0, nyq), 0.9f);

    for (int c = 0; c < 2; ++c)
    {
        lowCut[(size_t) c].coefficients = lc;  lowCut2[(size_t) c].coefficients = lc2;
        body[(size_t) c].coefficients = bd;    mud[(size_t) c].coefficients = md;
        presence[(size_t) c].coefficients = pr; air[(size_t) c].coefficients = ar;
        essDetect[(size_t) c].coefficients = es; essDetect2[(size_t) c].coefficients = es2;
        phoneHp[(size_t) c].coefficients = phH; phoneLp[(size_t) c].coefficients = phL;
    }
    essSplit.setCutoffFrequency ((float) juce::jlimit (1000.0, nyq, (double) s.deEssHz));
    lastFilters = s;
    filtersReady = true;
}

// ------------------------------------------------------------------ CLEANUP: low cut, gate, de-esser

void VocalChain::cleanup (juce::AudioBuffer<float>& b, const ChainSettings& s)
{
    auto n = b.getNumSamples(), nc = juce::jmin (2, b.getNumChannels());
    auto gateOn = s.gateDb > -79.0f;
    auto gateOpenThr = dbToGain (s.gateDb), gateCloseThr = dbToGain (s.gateDb - 4.0f);    // 4 dB hysteresis
    auto gateRange = dbToGain (-40.0f);
    auto envA = coeffFor (sr, 1.0f), envR = coeffFor (sr, 60.0f);
    auto gA = coeffFor (sr, 0.8f), gR = coeffFor (sr, 140.0f);

    // De-esser (split-band, like the classic studio units):
    //   detector: RMS of the sibilance band vs RMS of the whole voice (so it follows the singer's level),
    //   action: only the band above the de-ess frequency is turned down, through a flat-summing crossover.
    auto essOn = s.deEss > 0.001f;
    auto thr = 0.30f - 0.16f * s.deEss;                 // sibilance share that counts as "too much"
    auto maxCutDb = 4.0f + 10.0f * s.deEss;
    auto eA = coeffFor (sr, 2.0f), eR = coeffFor (sr, 35.0f);
    auto gsA = coeffFor (sr, 0.5f), gsR = coeffFor (sr, 25.0f);
    float maxRed = 0.0f;
    bool open = false;

    for (int i = 0; i < n; ++i)
    {
        float peak = 0.0f, sib = 0.0f, all = 0.0f;
        float x[2];
        for (int c = 0; c < nc; ++c)
        {
            auto v = lowCut2[(size_t) c].processSample (lowCut[(size_t) c].processSample (b.getSample (c, i)));
            x[c] = v;
            auto d = essDetect2[(size_t) c].processSample (essDetect[(size_t) c].processSample (v));
            sib += d * d; all += v * v;
            peak = juce::jmax (peak, std::abs (v));
        }

        if (gateOn)
        {
            gateEnv = peak > gateEnv ? envA * gateEnv + (1 - envA) * peak : envR * gateEnv + (1 - envR) * peak;
            auto target = gateEnv > gateOpenThr ? 1.0f : gateEnv < gateCloseThr ? gateRange : (gateGain > 0.5f ? 1.0f : gateRange);
            gateGain = target > gateGain ? gA * gateGain + (1 - gA) * target : gR * gateGain + (1 - gR) * target;
            open = open || target > 0.5f;
        }
        else
        {
            gateGain = 1.0f;
            open = true;
        }

        float target = 1.0f;
        if (essOn)
        {
            essEnvHi  = sib > essEnvHi  ? eA * essEnvHi  + (1 - eA) * sib : eR * essEnvHi  + (1 - eR) * sib;
            essEnvAll = all > essEnvAll ? eA * essEnvAll + (1 - eA) * all : eR * essEnvAll + (1 - eR) * all;
            auto share = std::sqrt (essEnvHi / juce::jmax (1.0e-12f, essEnvAll));     // 0..1 (RMS ratio)
            if (share > thr && essEnvAll > 1.0e-8f)
            {
                auto overDb = gainToDb (share / thr);
                auto cutDb = juce::jmin (maxCutDb, overDb * (2.0f + 3.0f * s.deEss));
                target = dbToGain (-cutDb);
            }
        }
        essGainSm = target < essGainSm ? gsA * essGainSm + (1 - gsA) * target : gsR * essGainSm + (1 - gsR) * target;
        maxRed = juce::jmax (maxRed, -gainToDb (essGainSm));

        for (int c = 0; c < nc; ++c)
        {
            float low = 0.0f, high = 0.0f;
            essSplit.processSample (c, x[c], low, high);        // low + high == allpass(x): flat, no comb
            b.setSample (c, i, (low + high * essGainSm) * gateGain);
        }
    }
    deEssDb.store (maxRed);
    gateOpen.store (open);
}

// ------------------------------------------------------------------ DYNAMICS: vocal compressor

void VocalChain::compress (juce::AudioBuffer<float>& b, const ChainSettings& s)
{
    if (s.comp < 0.001f) { grDb.store (0.0f); return; }
    auto n = b.getNumSamples(), nc = juce::jmin (2, b.getNumChannels());

    // Level-adaptive "one knob" vocal compressor: the threshold follows the singer's programme level
    // (slow RMS while the voice is present), so AMOUNT behaves the same on quiet or hot recordings.
    // Make-up gain is loudness-matched: it compares the programme power before and after the gain
    // reduction (only while the voice is present) and restores it, plus a little "density" lift,
    // so turning AMOUNT up makes the vocal more even, not quieter.
    auto ratio = 2.0f + 4.0f * s.comp;
    auto knee = 9.0f;
    auto a = coeffFor (sr, s.compAttackMs), r = coeffFor (sr, s.compReleaseMs);
    auto progA = coeffFor (sr, 1500.0f);
    auto powA = coeffFor (sr, 1200.0f);
    auto mkA = coeffFor (sr, 250.0f);
    auto bonusDb = 1.0f * s.comp;
    float worst = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float peak = 0.0f, pow2 = 0.0f;
        for (int c = 0; c < nc; ++c) { auto v = b.getSample (c, i); peak = juce::jmax (peak, std::abs (v)); pow2 += v * v; }
        auto lvl = gainToDb (peak);
        auto rmsDb = 10.0f * std::log10 (pow2 / (float) nc + 1.0e-12f);
        if (rmsDb > -60.0f) progDb = progA * progDb + (1 - progA) * rmsDb;

        auto thr = juce::jlimit (-50.0f, -4.0f, progDb + 9.0f - 14.0f * s.comp);
        auto over = lvl - thr;
        float grTarget = 0.0f;
        if (2.0f * over < -knee)      grTarget = 0.0f;
        else if (2.0f * over > knee)  grTarget = over * (1.0f - 1.0f / ratio);
        else                          grTarget = (1.0f - 1.0f / ratio) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);

        compEnv = grTarget > compEnv ? a * compEnv + (1 - a) * grTarget : r * compEnv + (1 - r) * grTarget;
        auto grLin = dbToGain (-compEnv);
        if (rmsDb > -60.0f)
        {
            preCompPow  = powA * preCompPow  + (1 - powA) * pow2;
            postCompPow = powA * postCompPow + (1 - powA) * pow2 * grLin * grLin;
            auto target = juce::jlimit (0.0f, 18.0f, 10.0f * std::log10 ((preCompPow + 1.0e-12f) / (postCompPow + 1.0e-12f)) + bonusDb);
            makeupDb = mkA * makeupDb + (1 - mkA) * target;
        }
        worst = juce::jmax (worst, compEnv);
        auto g = grLin * dbToGain (makeupDb);
        for (int c = 0; c < nc; ++c) b.setSample (c, i, b.getSample (c, i) * g);
    }
    grDb.store (worst);
}

// ------------------------------------------------------------------ output safety limiter

void VocalChain::limit (juce::AudioBuffer<float>& b)
{
    // Look-ahead peak limiter: the gain starts falling limLook samples before a peak arrives,
    // so normal material passes untouched and overs are caught without clicks.
    constexpr float ceiling = 0.944f;   // -0.5 dBFS
    auto n = b.getNumSamples();
    auto* l = b.getWritePointer (0);
    auto* r = b.getWritePointer (1);
    float worst = 1.0f;
    for (int i = 0; i < n; ++i)
    {
        auto pk = juce::jmax (std::abs (l[i]), std::abs (r[i]));
        limReq[(size_t) limPos] = pk > ceiling ? ceiling / pk : 1.0f;

        auto target = 1.0f;
        for (auto v : limReq) target = juce::jmin (target, v);
        limGain = target < limGain ? limGain + (target - limGain) * limAtt
                                   : target + (limGain - target) * limRel;

        auto dl = limBuf[0][(size_t) limPos], dr = limBuf[1][(size_t) limPos];
        limBuf[0][(size_t) limPos] = l[i];
        limBuf[1][(size_t) limPos] = r[i];
        limPos = (limPos + 1) % limLook;

        // last line of defence: gentle clip of anything the envelope still let through
        auto clip = [] (float x) { return std::abs (x) <= ceiling ? x : std::copysign (ceiling + (1.0f - ceiling) * std::tanh ((std::abs (x) - ceiling) / (1.0f - ceiling)), x); };
        l[i] = clip (dl * limGain);
        r[i] = clip (dr * limGain);
        worst = juce::jmin (worst, limGain);
    }
    limitDb.store (-gainToDb (worst));
}

// ------------------------------------------------------------------ FX: doubler, phone, width

void VocalChain::effects (juce::AudioBuffer<float>& b, const ChainSettings& s)
{
    auto n = b.getNumSamples(), nc = juce::jmin (2, b.getNumChannels());

    if (s.doubler > 0.001f && nc == 2)
    {
        auto lfoInc = 2.0 * juce::MathConstants<double>::pi * 0.37 / sr;
        for (int i = 0; i < n; ++i)
        {
            auto mono = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
            doubleLine.pushSample (0, mono);
            doubleLine.pushSample (1, mono);
            auto mod = (float) std::sin (lfoPhase);
            auto dl = (float) (sr * (0.0140 + 0.0011 * mod));
            auto dr = (float) (sr * (0.0215 - 0.0013 * mod));
            auto l = doubleLine.popSample (0, dl, true);
            auto r = doubleLine.popSample (1, dr, true);
            lfoPhase += lfoInc;
            if (lfoPhase > juce::MathConstants<double>::twoPi) lfoPhase -= juce::MathConstants<double>::twoPi;
            auto amt = 0.55f * s.doubler;
            b.setSample (0, i, b.getSample (0, i) * (1.0f - 0.15f * s.doubler) + l * amt);
            b.setSample (1, i, b.getSample (1, i) * (1.0f - 0.15f * s.doubler) + r * amt);
        }
    }

    if (s.phone > 0.001f)
        for (int c = 0; c < nc; ++c)
            for (int i = 0; i < n; ++i)
            {
                auto x = b.getSample (c, i);
                auto t = phoneLp[(size_t) c].processSample (phoneHp[(size_t) c].processSample (x));
                t = std::tanh (t * 2.5f) * 0.6f;
                b.setSample (c, i, x * (1.0f - s.phone) + t * s.phone * 1.6f);
            }

    if (nc == 2 && std::abs (s.width - 1.0f) > 0.01f)
        for (int i = 0; i < n; ++i)
        {
            auto l = b.getSample (0, i), r = b.getSample (1, i);
            auto m = 0.5f * (l + r), sd = 0.5f * (l - r) * s.width;
            b.setSample (0, i, m + sd);
            b.setSample (1, i, m - sd);
        }
}

// ------------------------------------------------------------------ main

void VocalChain::process (juce::AudioBuffer<float>& buffer, const ChainSettings& s)
{
    auto n = buffer.getNumSamples();
    auto nc = juce::jmin (2, buffer.getNumChannels());
    if (n == 0 || nc == 0) return;

    // make mono tracks behave: everything below runs on 2 channels
    workBuf.setSize (2, n, false, false, true);   // no allocation: capacity reserved in prepare()
    auto& work = workBuf;
    for (int c = 0; c < 2; ++c) work.copyFrom (c, 0, buffer, juce::jmin (c, nc - 1), 0, n);

    float inPk = 0.0f;
    for (int c = 0; c < nc; ++c) inPk = juce::jmax (inPk, buffer.getMagnitude (c, 0, n));
    inLevel.store (inPk);

    // latency-aligned dry for BYPASS
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < n; ++i)
        {
            dryDelay.pushSample (c, work.getSample (c, i));
            dry.setSample (c, i, dryDelay.popSample (c));
        }

    if (! filtersReady || s.lowCutHz != lastFilters.lowCutHz || s.body != lastFilters.body || s.mudCut != lastFilters.mudCut
        || s.presence != lastFilters.presence || s.air != lastFilters.air || s.deEssHz != lastFilters.deEssHz)
        updateFilters (s);

    cleanup (work, s);

    if (s.tuneOn)
        tuneEngine.process (work, s.tune);
    else
    {
        // keep latency constant when TUNE is off
        auto t = s.tune; t.bypass = true;
        tuneEngine.process (work, t);
    }

    for (int c = 0; c < 2; ++c)
    {
        auto* x = work.getWritePointer (c);
        for (int i = 0; i < n; ++i)
            x[i] = air[(size_t) c].processSample (presence[(size_t) c].processSample (mud[(size_t) c].processSample (body[(size_t) c].processSample (x[i]))));
    }

    compress (work, s);
    novaEngine.process (work, s.sat);       // always runs (fixed latency); mix 0 = transparent
    effects (work, s);

    if (s.delayOn && s.delay.mix > 0.001f)  delayEngine.process (work, s.delay);
    if (s.reverbOn && s.reverb.mix > 0.001f) reverbEngine.process (work, s.reverb);

    outGain.setTargetValue (dbToGain (s.outputDb));
    for (int i = 0; i < n; ++i)
    {
        auto g = outGain.getNextValue();
        for (int c = 0; c < 2; ++c) work.setSample (c, i, work.getSample (c, i) * g);
    }
    limit (work);

    bypassMix.setTargetValue (s.bypass ? 1.0f : 0.0f);
    float outPk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        auto bp = bypassMix.getNextValue();
        for (int c = 0; c < 2; ++c)
        {
            auto v = work.getSample (c, i) * (1.0f - bp) + dry.getSample (c, i) * bp;
            work.setSample (c, i, v);
            outPk = juce::jmax (outPk, std::abs (v));
        }
    }
    outLevel.store (outPk);

    if (buffer.getNumChannels() == 1)
    {
        buffer.copyFrom (0, 0, work, 0, 0, n);
        buffer.addFrom (0, 0, work, 1, 0, n);
        buffer.applyGain (0, 0, n, 0.5f);
    }
    else
        for (int c = 0; c < nc; ++c) buffer.copyFrom (c, 0, work, c, 0, n);
}

} // namespace vocal
