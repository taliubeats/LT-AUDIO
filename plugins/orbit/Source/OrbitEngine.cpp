#include "OrbitEngine.h"

namespace orbit
{

const juce::StringArray& Modes::names()
{
    static const juce::StringArray n { "DIGITAL", "TAPE", "ANALOG", "PING-PONG", "REVERSE", "SHIMMER", "LO-FI", "DUB" };
    return n;
}

void OrbitEngine::Line::prepare (int n)
{
    int p = 1;
    while (p < n + 8) p <<= 1;
    buf.assign ((size_t) p, 0.0f);
    mask = p - 1;
    pos = 0;
}

float OrbitEngine::Line::read (float d) const noexcept
{
    d = juce::jmax (1.0f, d);
    auto i = (int) d;
    auto f = d - (float) i;
    auto xm1 = tap (i - 1), x0 = tap (i), x1 = tap (i + 1), x2 = tap (i + 2);
    auto c1 = 0.5f * (x1 - xm1);
    auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

void OrbitEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    auto n = (int) (maxSeconds * sr) + 64;
    lineL.prepare (n); lineR.prepare (n);
    revL.prepare (n); revR.prepare (n);
    shimL.prepare ((int) (0.1 * sr)); shimR.prepare ((int) (0.1 * sr));

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, maxBlockSize), 2 };
    hp.prepare (spec); hp.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    lp.prepare (spec); lp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    colourLp.prepare (spec); colourLp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    mixS.reset (sr, 0.05);
    fbS.reset (sr, 0.05);
    widthS.reset (sr, 0.05);
    reset();
}

void OrbitEngine::reset()
{
    lineL.clear(); lineR.clear(); revL.clear(); revR.clear(); shimL.clear(); shimR.clear();
    hp.reset(); lp.reset(); colourLp.reset();
    delayNow = 0; duckEnv = inEnv = 0; freezeAmt = 0;
}

float OrbitEngine::saturate (float x, float drive) const noexcept
{
    auto k = 1.0f + drive * 5.0f;
    return std::tanh (x * k) / std::tanh (k) ;
}

void OrbitEngine::process (juce::AudioBuffer<float>& buffer, const OrbitSettings& s)
{
    auto n = buffer.getNumSamples();
    if (n == 0) return;
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    auto mode = juce::jlimit (0, Modes::count - 1, s.mode);
    if (mode != lastMode) { lastMode = mode; hp.reset(); lp.reset(); colourLp.reset(); }

    // loop filters: the user's LOW / HIGH CUT plus the colour of the mode
    auto lc = s.lowCut, hc = s.highCut;
    float colourHz = 20000.0f;
    switch (mode)
    {
        case Modes::tape:   colourHz = 7000.0f; break;
        case Modes::analog: colourHz = 3800.0f; break;
        case Modes::lofi:   lc = juce::jmax (lc, 300.0f); hc = juce::jmin (hc, 3200.0f); break;
        case Modes::dub:    lc = juce::jmax (lc, 250.0f); colourHz = 2600.0f; break;
        default: break;
    }
    hp.setCutoffFrequency (juce::jlimit (10.0f, (float) sr * 0.45f, lc));
    lp.setCutoffFrequency (juce::jlimit (200.0f, (float) sr * 0.45f, hc));
    colourLp.setCutoffFrequency (juce::jlimit (200.0f, (float) sr * 0.45f, colourHz));

    auto target = (float) juce::jlimit (0.001, maxSeconds - 0.05, s.delaySeconds) * (float) sr;
    if (delayNow <= 0.0f) delayNow = target;
    // TAPE / ANALOG / DUB glide like real tape when TIME changes; the others move quickly
    auto glide = (mode == Modes::tape || mode == Modes::analog || mode == Modes::dub) ? 0.00012f : 0.0012f;

    auto wobbleAmt = s.wobble * (mode == Modes::tape ? 1.0f : mode == Modes::lofi ? 1.4f : mode == Modes::analog ? 0.6f : 0.35f);
    auto wowDepth = wobbleAmt * 0.004f * (float) sr;     // up to 4 ms
    auto flutterDepth = wobbleAmt * 0.0004f * (float) sr;
    auto drive = mode == Modes::digital ? s.drive * 0.5f : mode == Modes::dub ? 0.3f + s.drive * 0.7f : s.drive;

    mixS.setTargetValue (juce::jlimit (0.0f, 1.0f, s.mix));
    fbS.setTargetValue (juce::jlimit (0.0f, mode == Modes::dub ? 1.05f : 0.98f, s.feedback * (mode == Modes::dub ? 1.05f : 0.98f)));
    widthS.setTargetValue (s.width);

    const auto release = std::exp (-1.0f / (0.2f * (float) sr));
    const auto follow = std::exp (-1.0f / (0.08f * (float) sr));
    const auto freezeStep = 1.0f / (0.05f * (float) sr);
    const auto shimLen = 0.05f * (float) sr;
    float inPk[2] = {}, outPk[2] = {}, wetPk = 0;

    for (int i = 0; i < n; ++i)
    {
        auto dl = L[i], dr = R != nullptr ? R[i] : dl;
        auto absIn = juce::jmax (std::abs (dl), std::abs (dr));
        inPk[0] = juce::jmax (inPk[0], std::abs (dl)); inPk[1] = juce::jmax (inPk[1], std::abs (dr));
        auto prev = inEnv;
        inEnv = juce::jmax (absIn, inEnv * follow);
        if (inEnv > 0.05f && inEnv > prev * 1.6f + 0.02f) ++hits;

        freezeAmt = juce::jlimit (0.0f, 1.0f, freezeAmt + (s.freeze ? freezeStep : -freezeStep));

        delayNow += (target - delayNow) * glide;
        wowPhase += 0.6f / (float) sr; if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flutterPhase += 7.3f / (float) sr; if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
        auto mod = wowDepth * (0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * wowPhase))
                 + flutterDepth * std::sin (juce::MathConstants<float>::twoPi * flutterPhase);
        auto d = delayNow + mod;

        float yl, yr;
        if (mode == Modes::reverse)
        {
            // the delayed signal, played backwards in chunks one delay long
            auto chunk = juce::jlimit (64.0f, (float) (maxSeconds * sr * 0.48), delayNow);
            revPhase += 1.0f / chunk;
            if (revPhase >= 1.0f) revPhase -= 1.0f;
            yl = yr = 0;
            for (auto off : { 0.0f, 0.5f })
            {
                auto p = revPhase + off; if (p >= 1.0f) p -= 1.0f;
                auto age = 2.0f * p * chunk + 1.0f;
                auto w = std::sin (juce::MathConstants<float>::pi * p);
                yl += lineL.read (age) * w * w;
                yr += lineR.read (age) * w * w;
            }
        }
        else
        {
            yl = lineL.read (d);
            yr = lineR.read (mode == Modes::pingPong ? d : d * 1.0f);
        }

        // feedback path
        auto fbl = yl, fbr = yr;
        if (mode == Modes::shimmer)
        {
            shimL.push (fbl); shimR.push (fbr);
            shimPhase += 1.0f / shimLen; if (shimPhase >= 1.0f) shimPhase -= 1.0f;
            auto p2 = shimPhase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
            auto wa = std::sin (juce::MathConstants<float>::pi * shimPhase), wb = std::sin (juce::MathConstants<float>::pi * p2);
            auto oct = [&] (Line& l) { return l.read (2.0f + (1.0f - shimPhase) * shimLen) * wa * wa + l.read (2.0f + (1.0f - p2) * shimLen) * wb * wb; };
            fbl = 0.5f * fbl + 0.6f * oct (shimL);
            fbr = 0.5f * fbr + 0.6f * oct (shimR);
        }

        auto open = freezeAmt;   // frozen: no filtering, no loss, no new input
        auto filt = [&] (int c, float x)
        {
            auto f = lp.processSample (c, hp.processSample (c, x));
            if (colourHz < 19000.0f) f = colourLp.processSample (c, f);
            return x + (f - x) * (1.0f - open);
        };
        fbl = filt (0, fbl);
        fbr = filt (1, fbr);
        if (drive > 0.01f && open < 1.0f)
        {
            fbl = saturate (fbl, drive);
            fbr = saturate (fbr, drive);
        }
        if (mode == Modes::analog || mode == Modes::lofi)
        {
            auto hiss = (mode == Modes::lofi ? 0.0015f : 0.0004f) * (1.0f - open);
            fbl += (noise.nextFloat() - 0.5f) * hiss; fbr += (noise.nextFloat() - 0.5f) * hiss;
        }
        if (mode == Modes::lofi)
        {
            if (++crushCount >= 3) { crushCount = 0; crushL = std::round (fbl * 96.0f) / 96.0f; crushR = std::round (fbr * 96.0f) / 96.0f; }
            fbl = crushL; fbr = crushR;
        }

        auto fb = fbS.getNextValue();
        fb = fb + (1.0f - fb) * open;
        auto feedIn = 1.0f - open;

        float wl, wr;
        if (mode == Modes::pingPong)
        {
            wl = 0.5f * (dl + dr) * feedIn + fbr * fb;
            wr = fbl * fb;
        }
        else
        {
            wl = dl * feedIn + fbl * fb;
            wr = dr * feedIn + fbr * fb;
        }
        lineL.push (juce::jlimit (-4.0f, 4.0f, wl));
        lineR.push (juce::jlimit (-4.0f, 4.0f, wr));

        // output
        auto width = widthS.getNextValue();
        auto mid = 0.5f * (yl + yr), side = 0.5f * (yl - yr) * width;
        auto ol = mid + side, orr = mid - side;

        duckEnv = juce::jmax (absIn, duckEnv * release);
        auto duckDb = juce::Decibels::gainToDecibels (duckEnv, -80.0f);
        auto duckGain = 1.0f - s.duck * 0.9f * juce::jlimit (0.0f, 1.0f, (duckDb + 45.0f) / 35.0f);

        auto mix = mixS.getNextValue();
        auto dryG = std::cos (mix * juce::MathConstants<float>::halfPi);
        auto wetG = std::sin (mix * juce::MathConstants<float>::halfPi) * duckGain;
        wetPk = juce::jmax (wetPk, std::abs (ol), std::abs (orr));

        L[i] = dl * dryG + ol * wetG;
        if (R != nullptr) R[i] = dr * dryG + orr * wetG;
        outPk[0] = juce::jmax (outPk[0], std::abs (L[i]));
        outPk[1] = juce::jmax (outPk[1], std::abs (R != nullptr ? R[i] : L[i]));
    }

    for (int c = 0; c < 2; ++c)
    {
        inLevel[(size_t) c] = juce::jmax (inPk[c], inLevel[(size_t) c].load() * 0.85f);
        outLevel[(size_t) c] = juce::jmax (outPk[c], outLevel[(size_t) c].load() * 0.85f);
    }
    wetLevel = juce::jmax (wetPk, wetLevel.load() * 0.85f);
}

} // namespace orbit
