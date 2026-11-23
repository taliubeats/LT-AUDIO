#include "CoronaEngine.h"

namespace corona
{
namespace
{
constexpr double kPi = juce::MathConstants<double>::pi;

// ---- the model constants (LT's own; see tests/CoronaTest.cpp for what each one is held to) ----
constexpr float kVcut      = 12.0f;   // control "volts" where the remote-cutoff cell closes
constexpr float kAlpha     = 1.6f;    // gm vs bias exponent of the cell
constexpr float kLoopGain  = 60.0f;   // rectifier gain: sets how hard the feedback loop limits
constexpr float kKnee      = 0.25f;   // rectifier conduction softness (normalised amplitude)
constexpr float kHoldShare = 0.30f;   // share of the slow memory cap in the control voltage
constexpr float kHoldCharge = 0.9f;   // s, slow cap charge
constexpr float kHoldDischarge = 1.2f;// s, slow cap discharge (program-dependent release tail)

inline float softplus (float x) noexcept { return x > 20.0f ? x : std::log1p (std::exp (x)); }

inline float ironSat (float x, float limit) noexcept
{
    const float l = limit * (x >= 0.0f ? 0.92f : 1.08f);   // slight asymmetry of the core
    const float r = std::abs (x / l);
    return x / std::pow (1.0f + std::pow (r, 2.5f), 0.4f);
}

inline float triode (float x) noexcept
{
    // driver stage: unity small-signal gain, 2nd-order curvature (warm H2), smooth ceiling
    return std::tanh (0.6f * x + 0.05f * x * x) / 0.6f;
}

inline float coef (double seconds, double fsOs) noexcept
{
    return (float) (1.0 - std::exp (-1.0 / (seconds * fsOs)));
}
} // namespace

static constexpr float kAttackTau[3]  = { 2.40f, 0.36f, 0.066f };
static constexpr float kReleaseTau[5] = { 0.840f, 0.300f, 0.105f, 0.037f, 0.014f };
float attackSeconds  (int s) noexcept { return kAttackTau[juce::jlimit (0, 2, s)]; }
float limitDb (float threshold) noexcept { return 6.0f - 0.75f * juce::jlimit (0.0f, 40.0f, threshold); }
float releaseSeconds (int s) noexcept { return kReleaseTau[juce::jlimit (0, 4, s)]; }

float Engine::cellGain (float vc) noexcept
{
    return std::pow (std::max (1.0f - vc / kVcut, 0.01f), kAlpha);
}

// -------------------------------------------------------------------- filters
void Engine::Biquad::setHighPass (double fs, double f, double q) noexcept
{
    const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
    b0 = (float) ((1 + c) / 2 / a0); b1 = (float) (-(1 + c) / a0); b2 = b0;
    a1 = (float) (-2 * c / a0); a2 = (float) ((1 - al) / a0);
}
void Engine::Biquad::setLowPass (double fs, double f, double q) noexcept
{
    f = std::min (f, fs * 0.45);
    const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
    b0 = (float) ((1 - c) / 2 / a0); b1 = (float) ((1 - c) / a0); b2 = b0;
    a1 = (float) (-2 * c / a0); a2 = (float) ((1 - al) / a0);
}
void Engine::Biquad::setPeak (double fs, double f, double q, double g) noexcept
{
    const double A = std::pow (10.0, g / 40.0), w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al / A;
    b0 = (float) ((1 + al * A) / a0); b1 = (float) (-2 * c / a0); b2 = (float) ((1 - al * A) / a0);
    a1 = b1; a2 = (float) ((1 - al / A) / a0);
}

void Engine::Channel::clear() noexcept
{
    for (auto* b : { &inHp, &bumpPeak, &inLp, &outLp, &scHp1, &scHp2 }) b->clear();
    inIronLf.z = outIronLf.z = outDc.z = driverDc.z = 0;
    lastOut = 0;
}

// -------------------------------------------------------------------- setup
void Engine::prepare (double sampleRate, int block, int channels)
{
    fs = sampleRate; maxBlock = std::max (1, block); numChannels = juce::jlimit (1, 2, channels);
    using OS = juce::dsp::Oversampling<float>;
    for (int k = 1; k <= 2; ++k)
    {
        os[(size_t) k] = std::make_unique<OS> ((size_t) numChannels, (size_t) k, OS::filterHalfBandFIREquiripple, true, true);
        os[(size_t) k]->initProcessing ((size_t) maxBlock);
        osLatency[(size_t) k] = (int) std::round (os[(size_t) k]->getLatencyInSamples());
    }
    latency = osLatency[2];
    const juce::dsp::ProcessSpec spec { fs, (juce::uint32) maxBlock, 1 };
    for (int c = 0; c < 2; ++c)
    {
        padDelay[(size_t) c].prepare (spec); padDelay[(size_t) c].setMaximumDelayInSamples (latency + 1);
        dryDelay[(size_t) c].prepare (spec); dryDelay[(size_t) c].setMaximumDelayInSamples (latency + 1);
        dryDelay[(size_t) c].setDelay ((float) latency);
    }
    dry.setSize (2, maxBlock); work.setSize (2, maxBlock);
    for (auto* s : { &inGain, &outGain, &wetMix, &powerMix }) s->reset (fs, 0.02);
    reset();
}

void Engine::reset()
{
    for (auto& o : os) if (o) o->reset();
    for (auto& d : padDelay) d.reset();
    for (auto& d : dryDelay) d.reset();
    for (auto& c : chans) c.clear();
    for (auto& d : dets) d.clear();
    currentOs = -1; filtersKey = -1; filtersFs = 0; autoSlowGr = 0;
    inGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.inputDb));
    outGain.setCurrentAndTargetValue (1.0f);
    wetMix.setCurrentAndTargetValue (params.mix / 10.0f);
    powerMix.setCurrentAndTargetValue (params.power ? 1.0f : 0.0f);
    grDb.store (0.0f);
}

void Engine::updateFilters (double fsOs)
{
    const float bump = 1.0f - juce::jlimit (0.0f, 10.0f, params.debump) / 10.0f;
    const int key = params.filter;
    if (fsOs == filtersFs && key == filtersKey && std::abs (bump - filtersBump) < 1e-4f) return;
    filtersFs = fsOs; filtersKey = key; filtersBump = bump;
    for (auto& c : chans)
    {
        // input iron: resonant low cut. The resonance is the low "bump"; DE-BUMP flattens it.
        c.inHp.setHighPass (fsOs, 22.0, 0.707 + 0.95 * bump);
        c.bumpPeak.setPeak (fsOs, 78.0, 0.8, 3.2 * bump);
        c.inLp.setLowPass (fsOs, 21000.0, 0.707);
        c.outLp.setLowPass (fsOs, 22000.0, 0.6);
        c.inIronLf.set (fsOs, 90.0);
        c.outIronLf.set (fsOs, 45.0);
        c.outDc.set (fsOs, 6.0);
        c.driverDc.set (fsOs, 4.0);
        const double scf = 50.0 * std::max (1, key);
        c.scHp1.setHighPass (fsOs, scf, 0.707);
        c.scHp2.setHighPass (fsOs, scf, 0.707);
    }
}

// -------------------------------------------------------------------- stages
float Engine::inputStage (Channel& c, float x, float) noexcept
{
    x = c.inHp.process (x);
    const float lf = c.inIronLf.lp (x);
    x += ironSat (lf, 1.8f) - lf;              // the core saturates on low frequencies first
    x = c.bumpPeak.process (x);
    x = c.inLp.process (x);
    x = triode (x);
    return c.driverDc.hp (x);
}

float Engine::gainStage (float u, float g, float vc) noexcept
{
    // push-pull remote-cutoff pair: odd curvature that grows as the bias moves toward cutoff,
    // plus a small even residue from the imperfect balance of the pair
    const float k3 = 0.06f * juce::jlimit (0.0f, 1.0f, vc / kVcut);
    const float h = u * (1.0f - k3 * (u * u) / (1.0f + u * u));
    return g * (h + 0.012f * h * std::abs (h));
}

float Engine::outputStage (Channel& c, float y) noexcept
{
    const float lf = c.outIronLf.lp (y);
    y += ironSat (lf, 1.4f) - lf;
    y = c.outLp.process (y);
    y = c.outDc.hp (y);
    return ironSat (y, 2.6f);
}

void Engine::runDetector (Detector& d, float level, float limit, float aA, float aR) noexcept
{
    // rectifier biased so it only conducts near the limiting level (soft conduction knee)
    const float x = level / limit;
    const float rect = std::max (0.0f, kLoopGain * kKnee * (softplus ((x - 1.0f) / kKnee) - softplus (-1.0f / kKnee)));
    d.v1 += (rect > d.v1 ? aA : aR) * (rect - d.v1);
    d.vc = d.v1;   // slow-memory share is added by processOs (needs its own coefficients)
}

void Engine::processOs (float* const* ch, int numCh, int n, double fsOs) noexcept
{
    updateFilters (fsOs);
    const float aA = coef (attackSeconds (params.attack), fsOs);
    const float aR = coef (releaseSeconds (params.release), fsOs);
    const float hC = coef (kHoldCharge, fsOs), hD = coef (kHoldDischarge, fsOs);
    const float limit = juce::Decibels::decibelsToGain (limitDb (params.threshold));
    const bool linked = params.mode != 1;
    const bool sc = params.filter > 0;

    float lastVc = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        for (int c = 0; c < numCh; ++c)
        {
            auto& d = dets[(size_t) (linked ? 0 : c)];
            const float g = cellGain (d.vc);
            float u = inputStage (chans[(size_t) c], ch[c][i], 0.0f);
            float y = outputStage (chans[(size_t) c], gainStage (u, g, d.vc));
            ch[c][i] = y;
            float s = y;
            if (sc) s = chans[(size_t) c].scHp2.process (chans[(size_t) c].scHp1.process (s));
            chans[(size_t) c].lastOut = std::abs (s);
        }
        const int groups = linked ? 1 : numCh;
        for (int gI = 0; gI < groups; ++gI)
        {
            auto& d = dets[(size_t) gI];
            const float level = linked ? (numCh > 1 ? std::max (chans[0].lastOut, chans[1].lastOut) : chans[0].lastOut)
                                       : chans[(size_t) gI].lastOut;
            runDetector (d, level, limit, aA, aR);
            d.v2 += (d.v1 > d.v2 ? hC : hD) * (d.v1 - d.v2);
            d.vc = std::max (d.v1, kHoldShare * d.v2);
            d.vc = std::min (d.vc, kVcut * 0.88f);   // the cell bottoms out around 30 dB of gain reduction
        }
        lastVc = dets[0].vc;
    }
    grDb.store (-juce::Decibels::gainToDecibels (cellGain (lastVc), -80.0f), std::memory_order_relaxed);
}

// -------------------------------------------------------------------- block
void Engine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int total = buffer.getNumSamples();
    const int numCh = std::min (numChannels, buffer.getNumChannels());
    if (numCh <= 0 || total <= 0) return;

    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = std::min (maxBlock, total - start);
        float* io[2] = { buffer.getWritePointer (0, start), numCh > 1 ? buffer.getWritePointer (1, start) : nullptr };

        // dry copy, delayed by the constant latency
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i) { dryDelay[(size_t) c].pushSample (0, io[c][i]); dry.setSample (c, i, dryDelay[(size_t) c].popSample (0)); }

        powerMix.setTargetValue (params.power ? 1.0f : 0.0f);
        if (! params.power && ! powerMix.isSmoothing())
        {
            for (int c = 0; c < numCh; ++c) juce::FloatVectorOperations::copy (io[c], dry.getReadPointer (c), n);
            grDb.store (0.0f);
            continue;
        }

        // input gain + stereo mode encode
        inGain.setTargetValue (juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 24.0f, params.inputDb)));
        const int mode = numCh > 1 ? params.mode : 0;
        const int procCh = mode == 2 ? 1 : numCh;
        for (int i = 0; i < n; ++i)
        {
            const float g = inGain.getNextValue();
            float l = io[0][i] * g, r = numCh > 1 ? io[1][i] * g : 0.0f;
            if (mode == 1) { const float m = 0.5f * (l + r), s = 0.5f * (l - r); l = m; r = s; }
            else if (mode == 2) l = 0.5f * (l + r);
            work.setSample (0, i, l);
            if (numCh > 1) work.setSample (1, i, r);
        }

        // oversampled non-linear core
        const int k = juce::jlimit (0, 2, params.oversampling);
        if (k != currentOs)
        {
            currentOs = k;
            for (int c = 0; c < 2; ++c) padDelay[(size_t) c].setDelay ((float) (latency - osLatency[(size_t) k]));
        }
        juce::dsp::AudioBlock<float> blk (work.getArrayOfWritePointers(), (size_t) procCh, (size_t) n);
        if (k == 0)
        {
            float* p[2] = { work.getWritePointer (0), procCh > 1 ? work.getWritePointer (1) : nullptr };
            processOs (p, procCh, n, fs);
        }
        else
        {
            auto up = os[(size_t) k]->processSamplesUp (blk);
            float* p[2] = { up.getChannelPointer (0), procCh > 1 ? up.getChannelPointer (1) : nullptr };
            processOs (p, procCh, (int) up.getNumSamples(), fs * (1 << k));
            os[(size_t) k]->processSamplesDown (blk);
        }
        for (int c = 0; c < procCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                padDelay[(size_t) c].pushSample (0, work.getSample (c, i));
                work.setSample (c, i, padDelay[(size_t) c].popSample (0));
            }

        // auto gain: undo the input drive and part of the average gain reduction
        const float autoDb = params.autoGain ? -params.inputDb : 0.0f;   // drives the input, gives the same back at the output
        outGain.setTargetValue (juce::Decibels::decibelsToGain (juce::jlimit (0.0f, 24.0f, params.makeupDb) + autoDb));
        wetMix.setTargetValue (juce::jlimit (0.0f, 10.0f, params.mix) / 10.0f);

        for (int i = 0; i < n; ++i)
        {
            float l = work.getSample (0, i), r = procCh > 1 ? work.getSample (1, i) : l;
            if (mode == 1) { const float m = l, s = r; l = m + s; r = m - s; }
            const float og = outGain.getNextValue(), m = wetMix.getNextValue(), pw = powerMix.getNextValue();
            const float wl = l * og, wr = r * og;
            const float dl = dry.getSample (0, i), dr = numCh > 1 ? dry.getSample (1, i) : 0.0f;
            const float ol = dl + m * (wl - dl), orr = dr + m * (wr - dr);
            io[0][i] = dl + pw * (ol - dl);
            if (numCh > 1) io[1][i] = dr + pw * (orr - dr);
        }
    }
}

} // namespace corona
