#include "SaturnEngine.h"

namespace saturn
{

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    constexpr float baseDelayMs = 12.0f;
    constexpr float maxDelayMs = 60.0f;
}

const juce::StringArray& Tapes::names()
{
    static const juce::StringArray n { "MELLO STRINGS", "MELLO FLUTE", "MELLO CHOIR", "CASSETTE", "REEL", "BROKEN" };
    return n;
}

const SaturnEngine::Character& SaturnEngine::character (int t)
{
    //                     wowHz flutHz wowMs flutMs  lowHz  highHz  bumpHz bumpDb drive asym  ensV ensMs  hissDb rumble drop
    static const Character c[Tapes::count] = {
        { 0.55f, 7.0f,  2.4f, 0.25f,  80.0f, 8000.0f,  250.0f,  2.0f, 1.0f, 0.10f, 3.0f, 3.5f, -58.0f, 0.3f, 0.0f },  // strings
        { 0.80f, 8.5f,  3.4f, 0.30f, 200.0f, 6000.0f,  900.0f,  2.5f, 0.8f, 0.06f, 2.0f, 2.5f, -54.0f, 0.3f, 0.0f },  // flute
        { 0.45f, 6.0f,  2.2f, 0.22f, 120.0f, 7000.0f, 1100.0f,  4.0f, 0.9f, 0.08f, 3.0f, 4.5f, -58.0f, 0.3f, 0.0f },  // choir
        { 0.90f, 12.0f, 2.8f, 0.45f,  60.0f, 9000.0f,  110.0f,  1.5f, 1.3f, 0.15f, 2.0f, 1.5f, -48.0f, 0.4f, 0.0f },  // cassette
        { 0.35f, 5.5f,  1.2f, 0.12f,  25.0f, 18000.0f,  80.0f,  3.0f, 1.1f, 0.05f, 2.0f, 1.2f, -64.0f, 0.2f, 0.0f },  // reel
        { 1.10f, 13.0f, 4.0f, 0.60f, 150.0f, 5000.0f,  700.0f,  3.0f, 1.6f, 0.25f, 2.0f, 2.0f, -46.0f, 1.0f, 1.0f },  // broken
    };
    return c[juce::jlimit (0, Tapes::count - 1, t)];
}

void SaturnEngine::Line::prepare (int n)
{
    int size = 1;
    while (size < n) size <<= 1;
    buf.assign ((size_t) size, 0.0f);
    mask = size - 1;
    pos = 0;
}

float SaturnEngine::Line::read (float d) const noexcept
{
    d = juce::jmax (1.0f, d);
    auto i = (int) d;
    auto f = d - (float) i;
    auto at = [this] (int k) { return buf[(size_t) ((pos - 1 - k) & mask)]; };
    auto y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
    // Catmull-Rom
    auto c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + c0;
}

void SaturnEngine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    baseDelay = (float) (sr * baseDelayMs * 0.001);
    latency = (int) std::round (baseDelay);
    for (int c = 0; c < 2; ++c)
    {
        tape[c].prepare ((int) (sr * maxDelayMs * 0.001) + 8);
        dry[c].prepare (latency + 8);
    }
    juce::dsp::ProcessSpec spec { sr, 4096, 1 };
    for (int c = 0; c < 2; ++c)
        for (auto* f : { &hp[c], &lp[c], &lp2[c], &bump[c], &tiltLo[c], &tiltHi[c], &hissHp[c], &hissLp[c], &rumbleLp[c] })
            f->prepare (spec);
    for (int c = 0; c < 2; ++c)
    {
        hissHp[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 1800.0f);
        hissLp[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, (float) juce::jmin (12000.0, sr * 0.45));
        rumbleLp[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 70.0f);
    }
    mixS.reset (sr, 0.05);
    outS.reset (sr, 0.05);
    lastTape = -1;
    reset();
}

void SaturnEngine::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        tape[c].clear();
        dry[c].clear();
        for (auto* f : { &hp[c], &lp[c], &lp2[c], &bump[c], &tiltLo[c], &tiltHi[c], &hissHp[c], &hissLp[c], &rumbleLp[c] })
            f->reset();
    }
    wowPh = flutPh = 0;
    driftNow = driftTarget = driftTimer = 0;
    wowRand = wowRandTarget = 0;
    dropGain = dropTarget = 1;
    dropTimer = 0;
    jerk = 0;
    clickCountdown = 0;
    primed = false;
}

void SaturnEngine::updateFilters (const Character& ch, float bandwidth, float tone, float age)
{
    using C = juce::dsp::IIR::Coefficients<float>;
    auto b = juce::jlimit (0.0f, 1.0f, bandwidth + 0.35f * age);
    auto low = juce::jlimit (20.0f, 700.0f, ch.lowHz * std::pow (5.0f, b));
    auto high = juce::jlimit (1200.0f, (float) sr * 0.45f, ch.highHz * std::pow (0.2f, b));
    auto hiTone = tone * 6.0f - age * 2.0f;                       // old tape loses top end
    auto loTone = -tone * 3.0f;
    auto nyq = (float) sr * 0.45f;

    auto hpC = C::makeHighPass (sr, low, 0.6f);
    auto lpC = C::makeLowPass (sr, high, 0.55f);
    auto lp2C = C::makeLowPass (sr, high, 0.8f);
    auto bumpC = C::makePeakFilter (sr, juce::jmin (ch.bumpHz, nyq), 0.9f, juce::Decibels::decibelsToGain (ch.bumpDb * (0.6f + 0.6f * age)));
    auto loC = C::makeLowShelf (sr, 300.0f, 0.7f, juce::Decibels::decibelsToGain (loTone));
    auto hiC = C::makeHighShelf (sr, juce::jmin (3000.0f, nyq), 0.7f, juce::Decibels::decibelsToGain (hiTone));
    for (int c = 0; c < 2; ++c)
    {
        hp[c].coefficients = hpC;
        lp[c].coefficients = lpC;
        lp2[c].coefficients = lp2C;
        bump[c].coefficients = bumpC;
        tiltLo[c].coefficients = loC;
        tiltHi[c].coefficients = hiC;
    }
}

void SaturnEngine::process (juce::AudioBuffer<float>& buffer, const SaturnSettings& s)
{
    auto n = buffer.getNumSamples();
    auto numCh = juce::jmin (2, buffer.getNumChannels());
    if (n == 0 || numCh == 0) return;
    auto& ch = character (s.tape);

    auto mixT = juce::jlimit (0.0f, 1.0f, s.mix), outT = juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 12.0f, s.outputDb));
    if (primed) { mixS.setTargetValue (mixT); outS.setTargetValue (outT); }
    else { mixS.setCurrentAndTargetValue (mixT); outS.setCurrentAndTargetValue (outT); primed = true; }

    auto age = juce::jlimit (0.0f, 1.0f, s.age);
    if (s.tape != lastTape || std::abs (s.bandwidth - lastBand) > 0.002f || std::abs (s.tone - lastTone) > 0.002f
        || std::abs (age - lastAge) > 0.002f)
    {
        updateFilters (ch, s.bandwidth, s.tone, age);
        lastTape = s.tape; lastBand = s.bandwidth; lastTone = s.tone; lastAge = age;
    }

    auto ageMul = 0.5f + age;
    auto effWow = juce::jlimit (0.0f, 1.5f, s.wow * ageMul);
    auto effFlut = juce::jlimit (0.0f, 1.5f, s.flutter * ageMul);
    auto effDrift = juce::jlimit (0.0f, 1.5f, s.drift * ageMul);
    auto effHiss = juce::jlimit (0.0f, 1.5f, s.hiss * ageMul);
    auto effSat = juce::jlimit (0.0f, 1.0f, s.saturation * (0.7f + 0.6f * age));
    auto ens = juce::jlimit (0.0f, 1.0f, s.ensemble);
    auto drop = juce::jlimit (0.0f, 1.0f, ch.dropouts * (0.4f + 0.6f * age) + (age > 0.8f ? (age - 0.8f) * 0.5f : 0.0f));

    auto drive = 1.0f + effSat * 7.0f * ch.drive;
    auto bias = ch.asym * effSat;
    auto tb = std::tanh (bias);                                   // asymmetry applied after the drive: bounded
    auto satNorm = 1.0f / (drive * (1.0f - tb * tb));            // unity gain for small signals

    auto msToS = (float) sr * 0.001f;
    auto wowInc = twoPi * ch.wowHz / (float) sr;
    auto flutInc = twoPi * ch.flutterHz / (float) sr;
    const float ensRate[3] = { 0.31f, 0.47f, 0.63f };
    auto voices = juce::jlimit (1, 3, (int) ch.ensembleVoices);
    auto hissGain = juce::Decibels::decibelsToGain (ch.hissDb) * effHiss;
    auto rumbleGain = hissGain * ch.rumble * 6.0f;
    auto prevDelay = baseDelay + driftNow * 3.0f * msToS * effDrift;

    float inPk = 0, outPk = 0;
    auto* data0 = buffer.getWritePointer (0);
    auto* data1 = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        // slow random parts: drift (motor) and wow irregularity
        if ((driftTimer -= 1.0f) <= 0)
        {
            driftTimer = (float) sr * (0.6f + rng.nextFloat() * 1.8f);
            driftTarget = rng.nextFloat() * 2.0f - 1.0f;
            wowRandTarget = rng.nextFloat() * 2.0f - 1.0f;
        }
        driftNow += (driftTarget - driftNow) * (1.0f / ((float) sr * 1.5f));
        wowRand += (wowRandTarget - wowRand) * (1.0f / ((float) sr * 0.8f));

        // BROKEN: dropouts and speed jerks
        if (drop > 0.0f && (dropTimer -= 1.0f) <= 0)
        {
            auto r = rng.nextFloat();
            if (r < 0.35f * drop) { dropTarget = 0.15f + rng.nextFloat() * 0.45f; dropTimer = (float) sr * (0.03f + rng.nextFloat() * 0.12f); }
            else { dropTarget = 1.0f; dropTimer = (float) sr * (0.2f + rng.nextFloat() * 0.9f); }
            if (rng.nextFloat() < 0.25f * drop) jerk = (rng.nextFloat() * 2.0f - 1.0f) * 2.0f * msToS;
        }
        else if (drop <= 0.0f) dropTarget = 1.0f;
        dropGain += (dropTarget - dropGain) * 0.004f;
        jerk *= 0.9993f;

        wowPh += wowInc * (1.0f + 0.3f * wowRand);
        flutPh += flutInc;
        if (wowPh > twoPi) wowPh -= twoPi;
        if (flutPh > twoPi) flutPh -= twoPi;

        auto mod = ch.wowDepth * effWow * (std::sin (wowPh) + 0.35f * wowRand)
                 + ch.flutterDepth * effFlut * (std::sin (flutPh) + 0.3f * std::sin (flutPh * 2.7f));
        auto delay = baseDelay + (mod + driftNow * 3.0f * effDrift) * msToS + jerk;
        delay = juce::jlimit (2.0f, (maxDelayMs - 25.0f) * msToS, delay);

        auto mix = mixS.getNextValue();
        auto outG = outS.getNextValue();

        float noiseL = rng.nextFloat() * 2.0f - 1.0f, noiseR = rng.nextFloat() * 2.0f - 1.0f;
        float click = 0;
        if (drop > 0.0f && --clickCountdown <= 0)
        {
            clickCountdown = (int) (sr * (0.05 + rng.nextDouble() * 0.6 / (0.2 + drop)));
            click = (rng.nextFloat() * 2.0f - 1.0f) * 0.06f * drop * juce::jmin (1.0f, effHiss * 2.0f + 0.2f);
        }

        for (int c = 0; c < numCh; ++c)
        {
            auto* d = c == 0 ? data0 : data1;
            auto x = d[i];
            inPk = juce::jmax (inPk, std::abs (x));
            dry[c].push (x);

            // tape saturation (unity small-signal gain)
            auto y = (std::tanh (drive * x + bias) - tb) * satNorm;
            tape[c].push (y);

            auto wet = tape[c].read (delay);
            if (ens > 0.001f)
            {
                float sum = 0;
                for (int v = 0; v < voices; ++v)
                {
                    auto ph = ensPh[v] + (c == 1 ? 1.6f : 0.0f);
                    auto dv = delay + (6.0f + 3.0f * (float) v + ch.ensembleDepthMs * (0.5f + 0.5f * std::sin (ph))) * msToS;
                    sum += tape[c].read (dv);
                }
                wet = wet * (1.0f - 0.35f * ens) + sum * ens * 0.75f / std::sqrt ((float) voices);
            }

            wet = hp[c].processSample (wet);
            wet = lp[c].processSample (wet);
            wet = lp2[c].processSample (wet);
            wet = bump[c].processSample (wet);
            wet = tiltLo[c].processSample (wet);
            wet = tiltHi[c].processSample (wet);
            wet *= dropGain;

            auto nz = c == 0 ? noiseL : noiseR;
            wet += hissLp[c].processSample (hissHp[c].processSample (nz)) * hissGain;
            wet += rumbleLp[c].processSample (nz) * rumbleGain + click;

            auto out = (dry[c].read ((float) latency) * (1.0f - mix) + wet * mix) * outG;
            if (! std::isfinite (out)) out = 0.0f;
            d[i] = out;
            outPk = juce::jmax (outPk, std::abs (out));
        }

        for (int v = 0; v < 3; ++v)
        {
            ensPh[v] += twoPi * ensRate[v] / (float) sr;
            if (ensPh[v] > twoPi) ensPh[v] -= twoPi;
        }
    }

    auto curDelay = baseDelay + driftNow * 3.0f * msToS * effDrift;
    speedNow = 1.0f - (curDelay - prevDelay) / (float) n * 40.0f
                    - (ch.wowDepth * effWow * std::cos (wowPh) * wowInc * msToS) * 40.0f;
    dropoutNow = 1.0f - dropGain;
    inLevel = juce::jmax (inPk, inLevel.load() * 0.85f);
    outLevel = juce::jmax (outPk, outLevel.load() * 0.85f);
}

} // namespace saturn
