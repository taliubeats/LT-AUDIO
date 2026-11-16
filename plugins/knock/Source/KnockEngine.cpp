#include "KnockEngine.h"

namespace knock
{

namespace
{
    constexpr int chunk = 512;
    constexpr float pi = juce::MathConstants<float>::pi;
    inline float onePoleCoeff (double sr, float seconds) { return 1.0f - std::exp (-1.0f / (seconds * (float) sr)); }
}

const juce::StringArray& DistTypes::names()  { static const juce::StringArray n { "SOFT", "HARD", "FUZZ", "TUBE" }; return n; }
const juce::StringArray& DelayTimes::names() { static const juce::StringArray n { "1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2" }; return n; }
double DelayTimes::beats (int i)             { static constexpr double b[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 }; return b[juce::jlimit (0, 5, i)]; }

void Biquad::shelf (double sr, float hz, float db, bool high)
{
    // RBJ cookbook shelf, S = 1
    auto A = std::pow (10.0, db / 40.0);
    auto w0 = 2.0 * juce::MathConstants<double>::pi * hz / sr;
    auto cs = std::cos (w0), sn = std::sin (w0);
    auto alpha = sn / 2.0 * std::sqrt (2.0);
    auto sq = 2.0 * std::sqrt (A) * alpha;
    double B0, B1, B2, A0, A1, A2;
    if (high)
    {
        B0 = A * ((A + 1) + (A - 1) * cs + sq);
        B1 = -2 * A * ((A - 1) + (A + 1) * cs);
        B2 = A * ((A + 1) + (A - 1) * cs - sq);
        A0 = (A + 1) - (A - 1) * cs + sq;
        A1 = 2 * ((A - 1) - (A + 1) * cs);
        A2 = (A + 1) - (A - 1) * cs - sq;
    }
    else
    {
        B0 = A * ((A + 1) - (A - 1) * cs + sq);
        B1 = 2 * A * ((A - 1) - (A + 1) * cs);
        B2 = A * ((A + 1) - (A - 1) * cs - sq);
        A0 = (A + 1) + (A - 1) * cs + sq;
        A1 = -2 * ((A - 1) + (A + 1) * cs);
        A2 = (A + 1) + (A - 1) * cs - sq;
    }
    b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
    a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
}

void KnockEngine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    target.setSize (2, chunk); dry.setSize (2, chunk);
    verbBuf.setSize (2, chunk); revVerbBuf.setSize (2, chunk);
    splitter.prepare (sr, chunk, 2);
    spectrum.setSampleRate (sr);

    for (int c = 0; c < 2; ++c)
    {
        wowBuf[c].assign ((size_t) (0.02 * sr) + 8, 0.0f);
        stutBuf[c].assign ((size_t) (2.0 * sr), 0.0f);
        revBuf[c].assign ((size_t) (1.0 * sr), 0.0f);
        delayBuf[c].assign ((size_t) (4.5 * sr), 0.0f);
    }

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) chunk, 2 };
    lofiLp.prepare (spec);  lofiLp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    sweep.prepare (spec);
    delayLp.prepare (spec); delayLp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    delayLp.setCutoffFrequency (5000.0f);

    reverb.setSampleRate (sr);
    reverseVerb.setSampleRate (sr);
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.88f; rp.damping = 0.35f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f;
    reverseVerb.setParameters (rp);
    reset();
}

void KnockEngine::reset()
{
    splitter.reset();
    lowShelf.reset(); highShelf.reset();
    lofiLp.reset(); sweep.reset(); delayLp.reset();
    reverb.reset(); reverseVerb.reset();
    for (int c = 0; c < 2; ++c)
    {
        envFast[c] = envSlow[c] = 0;
        std::fill (wowBuf[c].begin(), wowBuf[c].end(), 0.0f);
        std::fill (stutBuf[c].begin(), stutBuf[c].end(), 0.0f);
        std::fill (revBuf[c].begin(), revBuf[c].end(), 0.0f);
        std::fill (delayBuf[c].begin(), delayBuf[c].end(), 0.0f);
    }
    compEnv = 0; compGain = 1; punchGain = 1;
    stutActive = false; stutLen = stutWrite = stutRead = 0; lastSlice = -1;
    lastLowDb = lastHighDb = -100;
}

void KnockEngine::process (juce::AudioBuffer<float>& buffer, const KnockSettings& s, const Transport& t,
                           const lt::dsp::FocusSettings& focus)
{
    auto n = buffer.getNumSamples();
    auto chans = juce::jmin (2, buffer.getNumChannels());
    if (n == 0 || chans == 0) return;

    buffer.applyGain (s.input);
    spectrum.push (buffer);

    float inPk[2] {}, outPk[2] {};
    const auto follow = std::exp (-1.0f / (0.08f * (float) sr));
    float e = 0;
    for (int i = 0; i < n; ++i)
    {
        auto a = 0.0f;
        for (int c = 0; c < chans; ++c)
        {
            auto v = std::abs (buffer.getSample (c, i));
            inPk[c] = juce::jmax (inPk[c], v);
            a = juce::jmax (a, v);
        }
        auto prev = inEnvHit;
        inEnvHit = juce::jmax (a, inEnvHit * follow);
        if (inEnvHit > 0.05f && inEnvHit > prev * 1.6f + 0.02f) ++hits;
        e = juce::jmax (e, a);
    }

    for (int start = 0; start < n; start += chunk)
    {
        auto len = juce::jmin (chunk, n - start);
        juce::AudioBuffer<float> io (buffer.getArrayOfWritePointers(), chans, start, len);
        juce::AudioBuffer<float> tgt (target.getArrayOfWritePointers(), chans, 0, len);
        splitter.split (io, tgt, focus);

        for (int c = 0; c < chans; ++c) dry.copyFrom (c, 0, tgt, c, 0, len);

        auto ct = t;
        if (t.hasPosition) ct.ppq = t.ppq + (double) start / (sr * 60.0 / juce::jmax (20.0, t.bpm));
        processTarget (tgt, s, ct, len);

        // KNOCK = how much of the processed sound you hear
        for (int c = 0; c < chans; ++c)
        {
            auto* w = tgt.getWritePointer (c);
            auto* d = dry.getReadPointer (c);
            for (int i = 0; i < len; ++i)
                w[i] = d[i] + (w[i] - d[i]) * s.knock;
        }
        lt::dsp::FocusSplitter::combine (io, tgt, focus);
    }

    buffer.applyGain (s.output);
    for (int c = 0; c < chans; ++c)
        outPk[c] = buffer.getMagnitude (c, 0, n);
    if (chans == 1) { inPk[1] = inPk[0]; outPk[1] = outPk[0]; }

    for (int c = 0; c < 2; ++c)
    {
        inLevel[(size_t) c] = juce::jmax (inPk[c], inLevel[(size_t) c].load() * 0.85f);
        outLevel[(size_t) c] = juce::jmax (outPk[c], outLevel[(size_t) c].load() * 0.85f);
    }
    energy = juce::jmax (e, energy.load() * 0.85f);
}

void KnockEngine::processTarget (juce::AudioBuffer<float>& b, const KnockSettings& s, const Transport& t, int n)
{
    auto chans = b.getNumChannels();
    auto* L = b.getWritePointer (0);
    auto* R = chans > 1 ? b.getWritePointer (1) : nullptr;
    auto mel = s.melody;

    // ---- coefficients for this block
    auto lowDb = s.low * 12.0f, highDb = s.bright * 12.0f;
    if (std::abs (lowDb - lastLowDb) > 0.05f) { lowShelf.lowShelf (sr, mel ? 160.0f : 90.0f, lowDb); lastLowDb = lowDb; }
    if (std::abs (highDb - lastHighDb) > 0.05f) { highShelf.highShelf (sr, mel ? 5000.0f : 6500.0f, highDb); lastHighDb = highDb; }

    const auto fastAtt = onePoleCoeff (sr, 0.0005f), fastRel = onePoleCoeff (sr, mel ? 0.05f : 0.025f);
    const auto slowAtt = onePoleCoeff (sr, mel ? 0.03f : 0.015f), slowRel = onePoleCoeff (sr, mel ? 0.25f : 0.12f);
    const auto punchSmooth = onePoleCoeff (sr, 0.001f);
    const auto punchAmt = s.punch * (mel ? 1.0f : 1.8f);

    const auto thrDb = -6.0f - s.glue * 24.0f;
    const auto compAtt = onePoleCoeff (sr, mel ? 0.015f : 0.005f), compRel = onePoleCoeff (sr, mel ? 0.3f : 0.12f);
    const auto makeup = juce::Decibels::decibelsToGain (s.glue * 7.0f);

    const auto crushLevels = std::pow (2.0f, 15.0f - s.crush * 12.0f);
    const auto crushFactor = 1 + (int) (s.crush * s.crush * 24.0f);

    lofiLp.setCutoffFrequency (18000.0f * std::pow (0.17f, s.lofi));
    const auto wowDepth = s.lofi * 0.002f * (float) sr;
    const auto wowCentre = 0.004f * (float) sr;

    auto f = s.filter;
    auto filterOn = std::abs (f) > 0.02f;
    if (filterOn)
    {
        sweep.setType (f < 0 ? juce::dsp::StateVariableTPTFilterType::lowpass : juce::dsp::StateVariableTPTFilterType::highpass);
        sweep.setCutoffFrequency (f < 0 ? 20000.0f * std::pow (0.01f, -f) : 20.0f * std::pow (250.0f, f));
        sweep.setResonance (0.7f + std::abs (f) * 0.9f);
    }

    const auto ringHz = (mel ? 20.0f : 30.0f) * std::pow (40.0f, s.ring);
    const auto ringMix = juce::jmin (1.0f, s.ring * 1.6f);

    auto bpm = juce::jlimit (20.0, 999.0, t.bpm);
    auto spb = sr * 60.0 / bpm;
    auto sliceBeats = mel ? 0.5 : 0.25;
    auto sliceSamples = (int) juce::jmin ((double) stutBuf[0].size() - 1, sliceBeats * spb);

    auto distK = 1.0f + s.distortion * 30.0f;
    auto distWet = juce::jmin (1.0f, s.distortion * 3.0f);
    auto satK = 1.0f + s.saturation * 6.0f;
    auto satWet = juce::jmin (1.0f, s.saturation * 2.0f);

    for (int i = 0; i < n; ++i)
    {
        float x[2] = { L[i], R != nullptr ? R[i] : L[i] };

        // PUNCH: boost the attack of every hit
        if (punchAmt > 0.001f)
        {
            auto a = juce::jmax (std::abs (x[0]), std::abs (x[1]));
            envFast[0] += (a - envFast[0]) * (a > envFast[0] ? fastAtt : fastRel);
            envSlow[0] += (a - envSlow[0]) * (a > envSlow[0] ? slowAtt : slowRel);
            auto tr = juce::jlimit (0.0f, 3.0f, (envFast[0] - envSlow[0]) / (envSlow[0] + 1.0e-3f));
            auto want = (1.0f + punchAmt * tr) * (1.0f - s.punch * 0.2f);
            punchGain += (want - punchGain) * punchSmooth;
            x[0] *= punchGain; x[1] *= punchGain;
        }

        for (int c = 0; c < 2; ++c)
        {
            auto v = x[c];
            if (satWet > 0.0f)
                v += (std::tanh (v * satK) / std::sqrt (satK) * 1.2f - v) * satWet;

            if (distWet > 0.0f)
            {
                auto k = v * distK;
                float d;
                switch (s.distType)
                {
                    case DistTypes::hard: d = juce::jlimit (-1.0f, 1.0f, k); break;
                    case DistTypes::fuzz: { auto m = 1.0f - std::exp (-std::abs (k) * 1.5f); d = (k >= 0 ? m : -0.7f * m); break; }
                    case DistTypes::tube: d = k >= 0 ? 1.0f - std::exp (-k) : -(1.0f - std::exp (k * 0.7f)) / 1.2f; break;
                    default:              d = std::tanh (k); break;
                }
                v += (d * 0.6f / (1.0f + s.distortion * 0.8f) - v) * distWet;
            }

            if (lastLowDb > 0.05f) v = lowShelf.process (c, v);
            if (lastHighDb > 0.05f) v = highShelf.process (c, v);
            x[c] = v;
        }

        // GLUE: gentle bus compressor
        if (s.glue > 0.001f)
        {
            auto a = juce::jmax (std::abs (x[0]), std::abs (x[1]));
            compEnv += (a - compEnv) * (a > compEnv ? compAtt : compRel);
            auto over = juce::Decibels::gainToDecibels (compEnv, -100.0f) - thrDb;
            auto want = over > 0 ? juce::Decibels::decibelsToGain (-over * 0.75f) : 1.0f;
            compGain += (want - compGain) * 0.01f;
            x[0] *= compGain * makeup; x[1] *= compGain * makeup;
        }

        // CRUSH
        if (s.crush > 0.001f)
        {
            if (++crushCount >= crushFactor)
            {
                crushCount = 0;
                for (int c = 0; c < 2; ++c) crushHold[c] = std::round (x[c] * crushLevels) / crushLevels;
            }
            for (int c = 0; c < 2; ++c) x[c] += (crushHold[c] - x[c]) * juce::jmin (1.0f, s.crush * 3.0f);
        }

        // LO-FI: wobble, dull top, hiss and crackle
        if (s.lofi > 0.001f)
        {
            wowPhase += 0.8f / (float) sr; if (wowPhase >= 1.0f) wowPhase -= 1.0f;
            auto d = wowCentre + wowDepth * std::sin (2.0f * pi * wowPhase);
            auto size = (int) wowBuf[0].size();
            for (int c = 0; c < 2; ++c)
            {
                wowBuf[c][(size_t) wowPos] = x[c];
                auto rp = (float) wowPos - d;
                while (rp < 0) rp += (float) size;
                auto i0 = (int) rp, i1 = (i0 + 1) % size;
                auto fr = rp - (float) i0;
                auto w = wowBuf[c][(size_t) i0] * (1.0f - fr) + wowBuf[c][(size_t) i1] * fr;
                w = lofiLp.processSample (c, w);
                w += (rnd.nextFloat() - 0.5f) * 0.006f * s.lofi;
                x[c] += (w - x[c]) * juce::jmin (1.0f, s.lofi * 2.0f);
            }
            wowPos = (wowPos + 1) % size;
            if (rnd.nextFloat() < s.lofi * 6.0f / (float) sr)
            {
                auto pop = (rnd.nextFloat() - 0.5f) * 0.35f * s.lofi;
                x[0] += pop; x[1] += pop;
            }
        }

        // STUTTER: repeat the last slice (synced to the song)
        if (s.stutter > 0.001f && sliceSamples > 64)
        {
            double beat;
            if (t.playing && t.hasPosition) beat = t.ppq + (double) i / spb;
            else { beat = freeBeats; freeBeats += 1.0 / spb; }
            auto slice = std::floor (beat / sliceBeats);
            if (std::abs (slice - lastSlice) > 0.5)
            {
                lastSlice = slice;
                if (stutActive) stutRead = 0;
                auto repeat = stutLen >= sliceSamples - 2 && rnd.nextFloat() < s.stutter * 0.85f;
                if (repeat) { stutActive = true; stutRead = 0; }
                else { stutActive = false; stutWrite = 0; stutLen = 0; }
            }
            if (stutActive)
            {
                auto pos = stutRead;
                auto fade = juce::jmin (1.0f, (float) pos / 48.0f, (float) (stutLen - pos) / 48.0f);
                fade = juce::jlimit (0.0f, 1.0f, fade);
                for (int c = 0; c < 2; ++c)
                {
                    auto rep = stutBuf[c][(size_t) juce::jmin (pos, stutLen - 1)];
                    x[c] = rep * fade + x[c] * (1.0f - fade);
                }
                if (++stutRead >= stutLen) stutRead = 0;
            }
            else if (stutWrite < (int) stutBuf[0].size())
            {
                for (int c = 0; c < 2; ++c) stutBuf[c][(size_t) stutWrite] = x[c];
                ++stutWrite; stutLen = stutWrite;
            }
        }

        // FILTER sweep: left = low-pass, right = high-pass
        if (filterOn)
            for (int c = 0; c < 2; ++c) x[c] = sweep.processSample (c, x[c]);

        // RING mod
        if (ringMix > 0.0f)
        {
            ringPhase += ringHz / (float) sr; if (ringPhase >= 1.0f) ringPhase -= 1.0f;
            auto car = std::sin (2.0f * pi * ringPhase);
            for (int c = 0; c < 2; ++c) x[c] += (x[c] * car - x[c]) * ringMix;
        }

        L[i] = x[0];
        if (R != nullptr) R[i] = x[1];
    }

    // ---- SPACE (and REVERSE reverb): block based
    auto* outR = R != nullptr ? R : L;

    if (s.reverse > 0.001f)
    {
        revVerbBuf.copyFrom (0, 0, L, n);
        revVerbBuf.copyFrom (1, 0, outR, n);
        reverseVerb.processStereo (revVerbBuf.getWritePointer (0), revVerbBuf.getWritePointer (1), n);
        auto size = (int) revBuf[0].size();
        auto chunkLen = (float) juce::jmin ((double) size * 0.45, spb * (mel ? 1.0 : 0.5));
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c) revBuf[c][(size_t) revPos] = revVerbBuf.getSample (c, i);
            revPhase += 1.0f / chunkLen; if (revPhase >= 1.0f) revPhase -= 1.0f;
            float o[2] {};
            for (auto off : { 0.0f, 0.5f })
            {
                auto p = revPhase + off; if (p >= 1.0f) p -= 1.0f;
                auto age = (int) (2.0f * p * chunkLen) + 1;
                auto idx = (revPos - age + size * 4) % size;
                auto w = std::sin (pi * p);
                for (int c = 0; c < 2; ++c) o[c] += revBuf[c][(size_t) idx] * w * w;
            }
            revPos = (revPos + 1) % size;
            L[i] += o[0] * s.reverse * 1.4f;
            if (R != nullptr) R[i] += o[1] * s.reverse * 1.4f;
        }
    }

    if (s.reverb > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = mel ? 0.86f : 0.7f; rp.damping = 0.45f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f;
        reverb.setParameters (rp);
        verbBuf.copyFrom (0, 0, L, n);
        verbBuf.copyFrom (1, 0, outR, n);
        reverb.processStereo (verbBuf.getWritePointer (0), verbBuf.getWritePointer (1), n);
        auto g = s.reverb * 0.9f;
        juce::FloatVectorOperations::addWithMultiply (L, verbBuf.getReadPointer (0), g, n);
        if (R != nullptr) juce::FloatVectorOperations::addWithMultiply (R, verbBuf.getReadPointer (1), g, n);
    }

    if (s.delay > 0.001f)
    {
        auto size = (int) delayBuf[0].size();
        auto want = (float) juce::jmin ((double) size - 4, DelayTimes::beats (s.delayTime) * spb);
        if (delayNow <= 0) delayNow = want;
        auto fb = 0.3f + 0.3f * s.delay;
        for (int i = 0; i < n; ++i)
        {
            delayNow += (want - delayNow) * 0.0005f;
            auto rp = (float) delayPos - delayNow;
            while (rp < 0) rp += (float) size;
            auto i0 = (int) rp, i1 = (i0 + 1) % size;
            auto fr = rp - (float) i0;
            float y[2];
            for (int c = 0; c < 2; ++c) y[c] = delayBuf[c][(size_t) i0] * (1.0f - fr) + delayBuf[c][(size_t) i1] * fr;
            float in[2] = { L[i], R != nullptr ? R[i] : L[i] };
            // a little cross-feed so the repeats spread across the stereo field
            delayBuf[0][(size_t) delayPos] = in[0] + delayLp.processSample (0, y[1]) * fb;
            delayBuf[1][(size_t) delayPos] = in[1] + delayLp.processSample (1, y[0]) * fb;
            delayPos = (delayPos + 1) % size;
            L[i] += y[0] * s.delay * 0.7f;
            if (R != nullptr) R[i] += y[1] * s.delay * 0.7f;
        }
    }

    // WIDTH
    if (R != nullptr && std::abs (s.width - 1.0f) > 0.01f)
        for (int i = 0; i < n; ++i)
        {
            auto m = 0.5f * (L[i] + R[i]), sd = 0.5f * (L[i] - R[i]) * s.width;
            L[i] = m + sd; R[i] = m - sd;
        }

    for (int i = 0; i < n; ++i)
    {
        L[i] = juce::jlimit (-4.0f, 4.0f, L[i]);
        if (R != nullptr) R[i] = juce::jlimit (-4.0f, 4.0f, R[i]);
    }
}

} // namespace knock
