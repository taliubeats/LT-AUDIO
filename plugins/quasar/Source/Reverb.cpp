#include "Reverb.h"

namespace quasar
{

namespace
{
    struct TypeShape { float minMs, maxMs, diffMs, er, bright, modScale; };
    // per type: FDN delay range, diffusion spread, early energy, input brightness, modulation
    constexpr TypeShape shapes[] = {
        { 12.0f, 48.0f, 9.0f, 0.0f, 1.0f, 1.0f },     // PLATE: dense at once
        { 35.0f, 120.0f, 22.0f, 0.25f, 0.85f, 1.0f },  // HALL
        { 8.0f, 32.0f, 6.0f, 0.5f, 0.9f, 0.5f },       // ROOM
        { 18.0f, 70.0f, 12.0f, 0.35f, 0.8f, 0.7f },    // CHAMBER
        { 60.0f, 220.0f, 35.0f, 0.15f, 0.9f, 1.8f },   // SPACE
        { 20.0f, 80.0f, 14.0f, 0.2f, 0.55f, 1.5f },    // VINTAGE: darker, wobbly
    };
}

void QuasarVerb::hadamard (float* x) noexcept
{
    for (int h = 1; h < N; h <<= 1)
        for (int i = 0; i < N; i += h * 2)
            for (int j = i; j < i + h; ++j)
            {
                auto a = x[j], b = x[j + h];
                x[j] = a + b; x[j + h] = a - b;
            }
    for (int i = 0; i < N; ++i) x[i] *= 0.25f;   // 1 / sqrt (16)
}

void QuasarVerb::prepare (double sampleRate)
{
    sr = sampleRate;
    auto ms = [this] (float m) { return (int) std::ceil (m * 0.001 * sr); };
    preL.prepare (ms (260.0f)); preR.prepare (ms (260.0f));
    juce::Random r (4242);
    for (int s = 0; s < 4; ++s)
        for (int i = 0; i < N; ++i)
        {
            diff[(size_t) s][(size_t) i].prepare (ms (90.0f));
            diffSign[(size_t) s][(size_t) i] = r.nextBool() ? 1.0f : -1.0f;
        }
    for (int i = 0; i < N; ++i)
    {
        fdn[(size_t) i].prepare (ms (500.0f));
        lfoPhase[(size_t) i] = r.nextFloat();
        lfoInc[(size_t) i] = (0.07f + 0.9f * r.nextFloat()) / (float) sr;
        outSignL[(size_t) i] = (i % 2 == 0) ? (r.nextBool() ? 1.0f : -1.0f) : 0.0f;
        outSignR[(size_t) i] = (i % 2 == 1) ? (r.nextBool() ? 1.0f : -1.0f) : 0.0f;
    }
    preRamp.setLength ((int) (0.05 * sr));
    widthS.reset (sr, 0.05); compS.reset (sr, 0.2);
    sizeNow = decayNow = -1; typeNow = -1;
    reset();
}

void QuasarVerb::reset()
{
    preL.clear(); preR.clear();
    for (auto& s : diff) for (auto& l : s) l.clear();
    for (auto& l : fdn) l.clear();
    dampZ.fill (0); hpZ.fill (0);
    inLpZL = inLpZR = 0; wetHpL1 = wetHpR1 = wetHpL2 = wetHpR2 = 0;
}

void QuasarVerb::updateLengths (const Settings& s)
{
    auto& sh = shapes[juce::jlimit (0, 5, s.type)];
    auto scale = 0.45f + 1.35f * s.size;
    juce::Random r (777 + s.type);
    // diffusion (depends only on the type: integer taps must not jump while playing)
    if (s.type != typeNow)
    for (int st = 0; st < 4; ++st)
    {
        auto range = sh.diffMs * (0.4f + 0.3f * (float) st);
        for (int i = 0; i < N; ++i)
        {
            auto lo = range * (float) i / N, hi = range * (float) (i + 1) / N;
            diffLen[(size_t) st][(size_t) i] = juce::jmax (1, (int) ((lo + r.nextFloat() * (hi - lo)) * 0.001f * (float) sr));
        }
    }
    // FDN: exponentially spread, slightly randomised lengths (no common factors)
    for (int i = 0; i < N; ++i)
    {
        auto t = (float) i / (float) (N - 1);
        auto len = sh.minMs * std::pow (sh.maxMs / sh.minMs, t) * (0.93f + 0.14f * r.nextFloat()) * scale;
        fdnTarget[(size_t) i] = juce::jmin ((float) fdn[(size_t) i].mask - 64.0f - modDepth, len * 0.001f * (float) sr);
        if (typeNow < 0 || s.type != typeNow) fdnLen[(size_t) i] = fdnTarget[(size_t) i];
    }
    sizeNow = s.size; typeNow = s.type;
    decayNow = -1;
}

void QuasarVerb::process (const float* inL, const float* inR, float* wetL, float* wetR, int n, const Settings& s)
{
    if (s.type != typeNow || std::abs (s.size - sizeNow) > 0.002f) updateLengths (s);
    auto& sh = shapes[juce::jlimit (0, 5, s.type)];

    auto t60 = juce::jlimit (0.1f, 30.0f, s.decay * (s.type == room ? 0.6f : 1.0f));
    if (std::abs (t60 - decayNow) > 1.0e-4f || std::abs (fdnLen[0] - fdnTarget[0]) > 1.0f)
    {
        float meanG = 0;
        for (int i = 0; i < N; ++i)
        {
            fdnGain[(size_t) i] = std::pow (10.0f, -3.0f * fdnLen[(size_t) i] / (t60 * (float) sr));
            meanG += fdnGain[(size_t) i];
        }
        meanG /= N;
        decayNow = t60;
        // long decays build up more energy: compensate part of it so 100 % wet stays at a sane level
        compS.setTargetValue (std::pow (juce::jmax (1.0e-4f, 1.0f - meanG * meanG), 0.32f) * 1.6f);
    }
    auto damp = juce::jlimit (500.0f, 0.45f * (float) sr, s.damp * (s.type == vintage ? 0.55f : 1.0f));
    dampCoeff = dsp::OnePole::coeff (damp, (float) sr);
    inCoeff = dsp::OnePole::coeff (juce::jmin (0.45f * (float) sr, 18000.0f * sh.bright), (float) sr);
    auto hpC = dsp::OnePole::coeff (12.0f, (float) sr);
    modDepth = s.mod * sh.modScale * 0.0012f * (float) sr;   // up to ~1.2 ms
    preRamp.set (juce::jlimit (0.0f, 250.0f, s.preDelayMs) * 0.001f * (float) sr);
    widthS.setTargetValue (juce::jlimit (0.0f, 1.0f, s.width));
    auto lcC = dsp::OnePole::coeff (juce::jlimit (10.0f, 2000.0f, s.lowCut), (float) sr);

    std::array<float, N> x {}, y {};
    for (int i = 0; i < n; ++i)
    {
        // input: pre-delay and bandwidth
        auto pd = juce::jmax (0.0f, preRamp.next());
        preL.push (inL[i]); preR.push (inR[i]);
        auto l = preL.read (pd), r = preR.read (pd);
        inLpZL += inCoeff * (l - inLpZL); inLpZR += inCoeff * (r - inLpZR);
        l = inLpZL; r = inLpZR;

        // spread over 16 channels and diffuse
        for (int c = 0; c < N; ++c) x[(size_t) c] = (c % 2 == 0 ? l : r) * ((c & 2) ? -1.0f : 1.0f);
        float early = 0, earlyR = 0;
        for (int st = 0; st < 4; ++st)
        {
            auto& lines = diff[(size_t) st];
            for (int c = 0; c < N; ++c)
            {
                lines[(size_t) c].push (x[(size_t) c]);
                x[(size_t) c] = lines[(size_t) c].tap (diffLen[(size_t) st][(size_t) c]) * diffSign[(size_t) st][(size_t) c];
            }
            hadamard (x.data());
            if (st == 1) { early = x[0] + x[2] - x[4]; earlyR = x[1] + x[3] - x[5]; }
        }

        // feedback delay network
        for (int c = 0; c < N; ++c)
        {
            fdnLen[(size_t) c] += 0.0005f * (fdnTarget[(size_t) c] - fdnLen[(size_t) c]);   // size changes glide
            auto& ph = lfoPhase[(size_t) c];
            ph += lfoInc[(size_t) c]; if (ph >= 1.0f) ph -= 1.0f;
            auto mod = modDepth * (0.5f + 0.5f * std::sin (dsp::twoPi * ph));
            auto v = fdn[(size_t) c].read (fdnLen[(size_t) c] - 1.0f - mod);
            dampZ[(size_t) c] += dampCoeff * (v - dampZ[(size_t) c]);
            v = dampZ[(size_t) c];
            hpZ[(size_t) c] += hpC * (v - hpZ[(size_t) c]);
            y[(size_t) c] = (v - hpZ[(size_t) c]) * fdnGain[(size_t) c];
        }
        // Householder: y - 2/N * sum
        float sum = 0;
        for (auto v : y) sum += v;
        sum *= 2.0f / N;
        float outL = 0, outR = 0;
        for (int c = 0; c < N; ++c)
        {
            auto fb = y[(size_t) c] - sum;
            fdn[(size_t) c].push (fb + x[(size_t) c] * 0.5f);
            outL += y[(size_t) c] * outSignL[(size_t) c];
            outR += y[(size_t) c] * outSignR[(size_t) c];
        }
        auto comp = compS.getNextValue();
        outL = outL * 0.5f * comp + early * sh.er * 0.35f;
        outR = outR * 0.5f * comp + earlyR * sh.er * 0.35f;

        // width (M/S) and the wet low cut (2 one-pole stages)
        auto w = widthS.getNextValue();
        auto mid = 0.5f * (outL + outR), side = 0.5f * (outL - outR) * w;
        outL = mid + side; outR = mid - side;
        wetHpL1 += lcC * (outL - wetHpL1); outL -= wetHpL1;
        wetHpR1 += lcC * (outR - wetHpR1); outR -= wetHpR1;
        wetHpL2 += lcC * (outL - wetHpL2); outL -= wetHpL2;
        wetHpR2 += lcC * (outR - wetHpR2); outR -= wetHpR2;
        if (s.type == vintage) { outL = dsp::softClip (outL * 1.2f) / 1.2f; outR = dsp::softClip (outR * 1.2f) / 1.2f; }
        wetL[i] = outL; wetR[i] = outR;
    }
}

} // namespace quasar
