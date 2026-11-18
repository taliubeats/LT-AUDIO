#include "NovaEngine.h"

namespace nova
{

const juce::StringArray& SatTypes::names()
{
    static const juce::StringArray n { "TAPE", "TUBE", "CONSOLE", "TRANSFORMER", "TRANSISTOR", "DIODE", "VINYL", "EXCITER" };
    return n;
}

juce::String SatTypes::description (int t)
{
    static const char* d[] = {
        "warm, soft tape compression that glues the mix",
        "valve warmth: thick, musical, rich even harmonics",
        "subtle analog console colour, expensive and clean",
        "fat, heavy low end from an iron transformer",
        "aggressive transistor bite with an edge",
        "crunchy, bright diode clipping",
        "warm saturation with an old record's softness and crackle",
        "adds brightness and presence to the highs only",
    };
    return d[juce::jlimit (0, (int) SatType::count - 1, t)];
}

namespace
{
    // maximum drive in dB at 100 % for each character
    const float maxDriveDb[] = { 24.0f, 30.0f, 14.0f, 24.0f, 36.0f, 36.0f, 20.0f, 30.0f };

    inline float softClip (float x) noexcept { return std::tanh (x); }
}

float NovaEngine::shape (float x, SatType type, float character, float sag) noexcept
{
    // CHARACTER: 0 = even harmonics (asymmetric bias), 1 = odd harmonics (symmetric, harder)
    auto bias = (1.0f - character) * 0.35f + sag;
    auto hard = 1.0f + character * 1.5f;

    switch (type)
    {
        case SatType::tape:
        {
            // soft compression that leans in gently, with a touch of asymmetry
            auto y = softClip (x * (0.9f + 0.2f * character) + bias * 0.4f) - softClip (bias * 0.4f);
            return y * 0.95f;
        }
        case SatType::tube:
            return softClip (x + bias) - softClip (bias);

        case SatType::console:
            return (2.0f / juce::MathConstants<float>::pi) * std::atan (x * (1.0f + 0.5f * character) + bias * 0.2f)
                    - (2.0f / juce::MathConstants<float>::pi) * std::atan (bias * 0.2f);

        case SatType::transformer:
        {
            auto y = softClip (x * 1.1f + bias * 0.6f) - softClip (bias * 0.6f);
            return 0.85f * y + 0.15f * x / (1.0f + std::abs (x));
        }
        case SatType::transistor:
        {
            // harder knee: x / (1 + |x|^k)^(1/k)
            auto k = 2.0f + 2.5f * hard;
            auto xb = x + bias * 0.3f;
            auto y = xb / std::pow (1.0f + std::pow (std::abs (xb), k), 1.0f / k);
            auto b = bias * 0.3f;
            return y - b / std::pow (1.0f + std::pow (std::abs (b), k), 1.0f / k);
        }
        case SatType::diode:
        {
            // asymmetric: positive half softer than the negative half -> crunchy
            auto pos = 1.0f - std::exp (-std::max (x, 0.0f) * (1.2f + bias));
            auto neg = -softClip (-std::min (x, 0.0f) * (1.8f + 1.5f * character));
            return x >= 0.0f ? pos : neg;
        }
        case SatType::vinyl:
            return softClip (x * 0.8f + bias * 0.3f) - softClip (bias * 0.3f);

        case SatType::exciter:
            return softClip (x * hard) / hard * 1.4f;

        case SatType::count: break;
    }
    return x;
}

void NovaEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    sr = sampleRate;
    channels = juce::jmax (1, numChannels);

    // 4x, linear-phase halfband FIR with integer latency so the dry path can be aligned exactly
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) channels, 2,
                        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) maxBlockSize);
    latency = (int) std::lround (oversampling->getLatencyInSamples());

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlockSize, (juce::uint32) channels };
    dryDelay.prepare (spec);
    dryDelay.setMaximumDelayInSamples (juce::jmax (1, latency + 1));
    dryDelay.setDelay ((float) latency);

    auto make = [&] (std::vector<IIR>& v) { v.clear(); v.resize ((size_t) channels); };
    for (auto* v : { &emphPre, &emphPost, &emphPre2, &emphPost2, &warmPre, &warmPost, &toneLow, &toneHigh, &airShelf, &tapeLp, &vinylLp, &vinylHp, &exciterHp })
        make (*v);
    emphType = -1;

    dcX.assign ((size_t) channels, 0.0f);
    dcY.assign ((size_t) channels, 0.0f);
    dryBuffer.setSize (channels, maxBlockSize);
    exciterBand.setSize (channels, maxBlockSize);

    for (auto* s : { &driveGain, &outGain, &mixSmooth, &autoGain })
        s->reset (sr, 0.05);
    autoGain.reset (sr, 0.3);
    driveGain.setCurrentAndTargetValue (1.0f);
    outGain.setCurrentAndTargetValue (1.0f);
    mixSmooth.setCurrentAndTargetValue (1.0f);
    autoGain.setCurrentAndTargetValue (1.0f);

    filtersReady = false;
    reset();
}

void NovaEngine::reset()
{
    if (oversampling) oversampling->reset();
    dryDelay.reset();
    for (auto* v : { &emphPre, &emphPost, &emphPre2, &emphPost2, &warmPre, &warmPost, &toneLow, &toneHigh, &airShelf, &tapeLp, &vinylLp, &vinylHp, &exciterHp })
        for (auto& f : *v) f.reset();
    sagEnv = 0.0f;
    std::fill (dcX.begin(), dcX.end(), 0.0f);
    std::fill (dcY.begin(), dcY.end(), 0.0f);
    inPow = outPow = 0.0f;
    firstBlock = true;
}

void NovaEngine::updateFilters (const NovaSettings& s)
{
    using C = juce::dsp::IIR::Coefficients<float>;
    auto warmDb = s.warmth * 7.0f;
    auto preWarm  = C::makePeakFilter (sr, 260.0, 0.7f, juce::Decibels::decibelsToGain (warmDb));
    auto postWarm = C::makePeakFilter (sr, 260.0, 0.7f, juce::Decibels::decibelsToGain (-warmDb * 0.55f));
    auto tiltDb = s.tone * 6.0f;
    auto low  = C::makeLowShelf (sr, 600.0, 0.6f, juce::Decibels::decibelsToGain (-tiltDb));
    auto high = C::makeHighShelf (sr, 1800.0, 0.6f, juce::Decibels::decibelsToGain (tiltDb));
    auto air  = C::makeHighShelf (sr, 10000.0, 0.7f, juce::Decibels::decibelsToGain (s.air * 8.0f));
    auto tape = C::makeLowPass (sr, juce::jmin (16000.0, sr * 0.45), 0.6f);
    auto vlp  = C::makeLowPass (sr, juce::jmin (11000.0, sr * 0.45), 0.7f);
    auto vhp  = C::makeHighPass (sr, 45.0, 0.7f);
    auto exc  = C::makeHighPass (sr, 3200.0, 0.7f);

    for (int c = 0; c < channels; ++c)
    {
        warmPre[(size_t) c].coefficients = preWarm;
        warmPost[(size_t) c].coefficients = postWarm;
        toneLow[(size_t) c].coefficients = low;
        toneHigh[(size_t) c].coefficients = high;
        airShelf[(size_t) c].coefficients = air;
        tapeLp[(size_t) c].coefficients = tape;
        vinylLp[(size_t) c].coefficients = vlp;
        vinylHp[(size_t) c].coefficients = vhp;
        exciterHp[(size_t) c].coefficients = exc;
    }
    last = s;
    filtersReady = true;
}

void NovaEngine::updateEmphasis (SatType type, float drive01)
{
    // Frequency-dependent saturation, the way real hardware behaves. Each pair is exactly inverse
    // (pre: +X dB, post: -X dB), so the tone stays neutral at low drive and only the
    // distortion becomes frequency dependent.
    using C = juce::dsp::IIR::Coefficients<float>;
    // identity (an all-pass would shift the phase and comb against the dry path at MIX < 100 %)
    typename C::Ptr flat (new C (1.0f, 0.0f, 1.0f, 0.0f));
    auto pre = flat, post = flat, pre2 = flat, post2 = flat;

    switch (type)
    {
        case SatType::tape:
            // NAB-style: highs hit the tape harder (soft HF compression), plus the low "head bump"
            pre   = C::makeHighShelf (sr, 3000.0, 0.6f, juce::Decibels::decibelsToGain (6.0f));
            post  = C::makeHighShelf (sr, 3000.0, 0.6f, juce::Decibels::decibelsToGain (-6.0f));
            pre2  = C::makePeakFilter (sr, 75.0, 0.9f, juce::Decibels::decibelsToGain (1.0f + 1.5f * drive01));
            post2 = C::makePeakFilter (sr, 180.0, 1.2f, juce::Decibels::decibelsToGain (-0.6f * drive01));
            break;
        case SatType::transformer:
            // iron core saturates on low frequencies first
            pre   = C::makeLowShelf (sr, 150.0, 0.6f, juce::Decibels::decibelsToGain (8.0f));
            post  = C::makeLowShelf (sr, 150.0, 0.6f, juce::Decibels::decibelsToGain (-8.0f));
            pre2  = flat;
            post2 = C::makeHighShelf (sr, 12000.0, 0.6f, juce::Decibels::decibelsToGain (-1.5f * drive01));
            break;
        case SatType::console:
            pre   = C::makeHighShelf (sr, 5000.0, 0.6f, juce::Decibels::decibelsToGain (2.5f));
            post  = C::makeHighShelf (sr, 5000.0, 0.6f, juce::Decibels::decibelsToGain (-2.0f));
            break;
        case SatType::tube:
            // valves: a little more drive in the mids, the classic "forward" sound
            pre   = C::makePeakFilter (sr, 1200.0, 0.5f, juce::Decibels::decibelsToGain (3.0f));
            post  = C::makePeakFilter (sr, 1200.0, 0.5f, juce::Decibels::decibelsToGain (-3.0f));
            break;
        default: break;
    }

    for (int c = 0; c < channels; ++c)
    {
        emphPre[(size_t) c].coefficients = pre;   emphPost[(size_t) c].coefficients = post;
        emphPre2[(size_t) c].coefficients = pre2; emphPost2[(size_t) c].coefficients = post2;
    }
    emphType = (int) type;
    emphDrive = drive01;
}

void NovaEngine::process (juce::AudioBuffer<float>& buffer, const NovaSettings& s)
{
    if (oversampling == nullptr) return;
    auto n = buffer.getNumSamples();
    auto nc = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || nc == 0) return;

    if (! filtersReady || s.warmth != last.warmth || s.tone != last.tone || s.air != last.air)
        updateFilters (s);
    last = s;

    auto type = (SatType) juce::jlimit (0, (int) SatType::count - 1, s.type);
    auto drive01 = juce::jlimit (0.0f, 1.0f, s.drive);
    driveGain.setTargetValue (juce::Decibels::decibelsToGain (drive01 * maxDriveDb[(int) type] + s.inputDb));
    outGain.setTargetValue (juce::Decibels::decibelsToGain (s.outputDb));
    mixSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, s.mix));
    if (firstBlock)
    {
        // start exactly at the saved settings (no ramp from the defaults when a project loads)
        driveGain.setCurrentAndTargetValue (driveGain.getTargetValue());
        outGain.setCurrentAndTargetValue (outGain.getTargetValue());
        mixSmooth.setCurrentAndTargetValue (mixSmooth.getTargetValue());
        firstBlock = false;
    }

    // keep a latency-aligned dry copy for MIX and auto gain
    for (int c = 0; c < nc; ++c)
    {
        auto* in = buffer.getReadPointer (c);
        auto* d = dryBuffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            dryDelay.pushSample (c, in[i]);
            d[i] = dryDelay.popSample (c);
        }
    }

    // exciter works in parallel on the high band only
    if (type == SatType::exciter)
        for (int c = 0; c < nc; ++c)
        {
            exciterBand.copyFrom (c, 0, buffer, c, 0, n);
            auto* e = exciterBand.getWritePointer (c);
            for (int i = 0; i < n; ++i) e[i] = exciterHp[(size_t) c].processSample (e[i]);
        }

    auto& source = type == SatType::exciter ? exciterBand : buffer;

    if (emphType != (int) type || std::abs (emphDrive - drive01) > 0.02f)
        updateEmphasis (type, drive01);

    // pre-emphasis + drive
    juce::AudioBuffer<float> work (source.getArrayOfWritePointers(), nc, n);
    float peak = 0.0f;
    for (int c = 0; c < nc; ++c)
    {
        auto* x = work.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            x[i] = emphPre2[(size_t) c].processSample (emphPre[(size_t) c].processSample (warmPre[(size_t) c].processSample (x[i])));
            peak = juce::jmax (peak, std::abs (x[i]));
        }
    }

    // valve "sag": the bias shifts with the programme level (only for TUBE)
    auto sagTarget = type == SatType::tube ? juce::jmin (0.25f, peak * driveGain.getTargetValue() * 0.08f) * (1.0f - s.character) : 0.0f;
    sagEnv += (sagTarget - sagEnv) * (sagTarget > sagEnv ? 0.3f : 0.05f);
    {
        auto g0 = driveGain.getCurrentValue();
        driveGain.skip (n);
        auto g1 = driveGain.getCurrentValue();
        for (int c = 0; c < nc; ++c) work.applyGainRamp (c, 0, n, g0, g1);
    }

    // oversampled waveshaper
    {
        juce::dsp::AudioBlock<float> block (work);
        auto up = oversampling->processSamplesUp (block);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* x = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                x[i] = shape (x[i], type, s.character, sagEnv);
        }
        oversampling->processSamplesDown (block);
    }

    // drive compensation so 100 % drive is loud but not absurd (auto gain does the precise match)
    auto comp = 1.0f / std::sqrt (juce::jmax (1.0f, juce::Decibels::decibelsToGain (drive01 * maxDriveDb[(int) type] * 0.5f)));

    for (int c = 0; c < nc; ++c)
    {
        auto* x = work.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            auto v = x[i] * comp;

            // DC blocker (asymmetric curves create DC)
            auto y = v - dcX[(size_t) c] + 0.9995f * dcY[(size_t) c];
            dcX[(size_t) c] = v; dcY[(size_t) c] = y;
            v = y;

            v = emphPost2[(size_t) c].processSample (emphPost[(size_t) c].processSample (v));
            v = warmPost[(size_t) c].processSample (v);
            if (type == SatType::tape)  v = tapeLp[(size_t) c].processSample (v);
            if (type == SatType::vinyl)
            {
                v = vinylHp[(size_t) c].processSample (vinylLp[(size_t) c].processSample (v));
                if (noise.nextFloat() < 0.00012f * (0.3f + drive01))   // sparse crackle
                    v += (noise.nextFloat() * 2.0f - 1.0f) * 0.05f * drive01;
                v += (noise.nextFloat() * 2.0f - 1.0f) * 0.0006f * drive01; // faint hiss
            }
            x[i] = v;
        }
    }

    // exciter: add the saturated highs back on top of the (aligned) dry signal
    if (type == SatType::exciter)
        for (int c = 0; c < nc; ++c)
        {
            auto* out = buffer.getWritePointer (c);
            auto* hi = exciterBand.getReadPointer (c);
            auto* d = dryBuffer.getReadPointer (c);
            for (int i = 0; i < n; ++i) out[i] = d[i] + hi[i] * (0.6f * drive01 + 0.1f);
        }

    // post EQ: tone tilt + air
    for (int c = 0; c < nc; ++c)
    {
        auto* x = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
            x[i] = airShelf[(size_t) c].processSample (toneHigh[(size_t) c].processSample (toneLow[(size_t) c].processSample (x[i])));
    }

    // auto gain: match the wet loudness to the dry loudness (slow RMS)
    double ip = 0, op = 0;
    for (int c = 0; c < nc; ++c)
    {
        auto* d = dryBuffer.getReadPointer (c);
        auto* x = buffer.getReadPointer (c);
        for (int i = 0; i < n; ++i) { ip += (double) d[i] * d[i]; op += (double) x[i] * x[i]; }
    }
    auto coeff = 1.0f - std::exp (-(float) n / (float) (sr * 0.4));
    inPow += coeff * ((float) (ip / (n * nc)) - inPow);
    outPow += coeff * ((float) (op / (n * nc)) - outPow);

    auto target = 1.0f;
    if (s.autoGain && outPow > 1.0e-9f && inPow > 1.0e-9f)
        target = juce::jlimit (juce::Decibels::decibelsToGain (-24.0f), juce::Decibels::decibelsToGain (12.0f),
                               std::sqrt (inPow / outPow));
    autoGain.setTargetValue (target);
    autoGainDb.store (juce::Decibels::gainToDecibels (autoGain.getTargetValue()));

    // dry / wet + output
    for (int i = 0; i < n; ++i)
    {
        auto ag = autoGain.getNextValue();
        auto m = mixSmooth.getNextValue();
        auto og = outGain.getNextValue();
        for (int c = 0; c < nc; ++c)
        {
            auto* x = buffer.getWritePointer (c);
            auto d = dryBuffer.getSample (c, i);
            x[i] = (d * (1.0f - m) + x[i] * ag * m) * og;
        }
    }

    // energy for the visual (drive x level)
    auto rms = std::sqrt (outPow);
    energy.store (juce::jlimit (0.0f, 1.0f, drive01 * 0.6f + juce::jmin (1.0f, rms * 3.0f) * 0.4f));
}

} // namespace nova
