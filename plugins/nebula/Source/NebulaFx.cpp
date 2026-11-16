#include "NebulaFx.h"

namespace nebula
{

namespace { constexpr float pi = juce::MathConstants<float>::pi, twoPi = juce::MathConstants<float>::twoPi; }

const juce::StringArray& FxTypes::names()
{
    static const juce::StringArray n {
        "-", "PARTICLES", "GRAIN CLOUD", "REVERB", "SHIMMER", "DELAY", "PING-PONG", "REVERSE DELAY", "CHORUS", "FLANGER",
        "PHASER", "TREMOLO", "AUTO-PAN", "WIDENER", "PITCH SHIFTER", "FREQ SHIFTER", "HARMONIZER", "FORMANT", "SATURATION",
        "DISTORTION", "TAPE", "FILTER LP", "FILTER HP", "FILTER BP", "COMB", "EQ", "CRUSH", "TAPE WOBBLE", "STUTTER",
        "SPECTRAL FREEZE", "RING MOD", "VINYL" };
    return n;
}

void FxUnit::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, maxBlock), 2 };
    f1.prepare (spec); f2.prepare (spec); f3.prepare (spec);
    reverb.setSampleRate (sr);
    for (int ch = 0; ch < 2; ++ch) { a[ch].prepare ((int) (4.0 * sr)); c[ch].prepare ((int) (0.2 * sr)); }
    reset();
}

void FxUnit::reset()
{
    for (int ch = 0; ch < 2; ++ch) { a[ch].clear(); c[ch].clear(); ph[ch] = ph2[ch] = fbState[ch] = hold[ch] = 0; for (auto& z : apZ[ch]) z = 0; }
    for (auto& x : hilbZ) for (auto& y : x) for (auto& z : y) z[0] = z[1] = z[2] = z[3] = 0;
    hilbDelay[0] = hilbDelay[1] = 0;
    f1.reset(); f2.reset(); f3.reset(); reverb.reset();
    lfo = lfo2 = oscPhase = env = 0; count = loopLen = loopPos = 0; lastSlice = -1; repeating = captured = false;
}

void FxUnit::setType (int t)
{
    if (t == type) return;
    type = t;
    reset();
    using T = juce::dsp::StateVariableTPTFilterType;
    switch (type)
    {
        case FxTypes::filterHP: f1.setType (T::highpass); break;
        case FxTypes::filterBP: f1.setType (T::bandpass); break;
        case FxTypes::formant:  f1.setType (T::bandpass); f2.setType (T::bandpass); f3.setType (T::bandpass); break;
        default:                f1.setType (T::lowpass); f2.setType (T::lowpass); break;
    }
}

float FxUnit::pitchShift (Line& l, float& phase, float ratio, float window) const noexcept
{
    // two crossfaded read heads moving at (1 - ratio) through a short window
    phase += (1.0f - ratio) / window;
    while (phase < 0) phase += 1.0f;
    while (phase >= 1.0f) phase -= 1.0f;
    auto p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
    auto w1 = std::sin (pi * phase), w2 = std::sin (pi * p2);
    return l.read (2.0f + phase * window) * w1 * w1 + l.read (2.0f + p2 * window) * w2 * w2;
}

void FxUnit::process (float* L, float* R, int n, float amt, double bpm, double ppq, bool playing)
{
    if (type == FxTypes::none || FxTypes::isEngineFx (type) || amt <= 0.001f) return;
    auto spb = sr * 60.0 / juce::jlimit (20.0, 999.0, bpm);
    float* io[2] = { L, R };

    auto wetMix = [&] (int i, float wl, float wr, float mix)
    {
        L[i] += (wl - L[i]) * mix;
        R[i] += (wr - R[i]) * mix;
    };

    switch (type)
    {
        case FxTypes::reverb:
        case FxTypes::shimmer:
        {
            juce::Reverb::Parameters p;
            p.roomSize = 0.55f + 0.43f * amt; p.damping = type == FxTypes::shimmer ? 0.15f : 0.4f;
            p.wetLevel = 0.9f * amt; p.dryLevel = 1.0f - 0.4f * amt; p.width = 1.0f;
            reverb.setParameters (p);
            if (type == FxTypes::shimmer)
                for (int i = 0; i < n; ++i)
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        c[ch].push (io[ch][i] + fbState[ch] * 0.5f);
                        auto up = pitchShift (c[ch], ph[ch], 2.0f, (float) sr * 0.06f);
                        fbState[ch] = up * amt;
                        io[ch][i] += up * 0.5f * amt;
                    }
            reverb.processStereo (L, R, n);
            break;
        }

        case FxTypes::delay:
        case FxTypes::pingPong:
        case FxTypes::reverseDelay:
        {
            auto d = (float) (spb * (type == FxTypes::pingPong ? 0.5 : type == FxTypes::reverseDelay ? 1.0 : 0.75));
            d = juce::jmin (d, (float) a[0].b.size() * 0.45f);
            for (int i = 0; i < n; ++i)
            {
                float y[2];
                if (type == FxTypes::reverseDelay)
                {
                    lfo += 1.0f / d; if (lfo >= 1.0f) lfo -= 1.0f;
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        y[ch] = 0;
                        for (auto off : { 0.0f, 0.5f })
                        {
                            auto p = lfo + off; if (p >= 1.0f) p -= 1.0f;
                            auto w = std::sin (pi * p);
                            y[ch] += a[ch].read (2.0f * p * d) * w * w;
                        }
                    }
                }
                else { y[0] = a[0].read (d); y[1] = a[1].read (d); }
                auto fb = 0.3f + 0.35f * amt;
                if (type == FxTypes::pingPong)
                {
                    a[0].push (0.5f * (L[i] + R[i]) + y[1] * fb);
                    a[1].push (y[0] * fb);
                }
                else { a[0].push (L[i] + y[0] * fb); a[1].push (R[i] + y[1] * fb); }
                L[i] += y[0] * 0.7f * amt; R[i] += y[1] * 0.7f * amt;
            }
            break;
        }

        case FxTypes::chorus:
        case FxTypes::flanger:
        {
            auto flanger = type == FxTypes::flanger;
            auto rate = flanger ? 0.25f : 0.8f;
            for (int i = 0; i < n; ++i)
            {
                lfo += rate / (float) sr; if (lfo >= 1.0f) lfo -= 1.0f;
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto m = 0.5f + 0.5f * std::sin (twoPi * (lfo + (ch ? 0.25f : 0.0f)));
                    auto d = flanger ? (0.0008f + 0.004f * m) * (float) sr : (0.008f + 0.012f * m) * (float) sr;
                    auto y = c[ch].read (d);
                    c[ch].push (io[ch][i] + (flanger ? y * 0.7f * amt : 0.0f));
                    io[ch][i] = io[ch][i] * (1.0f - 0.5f * amt) + y * 0.7f * amt;
                }
            }
            break;
        }

        case FxTypes::phaser:
            for (int i = 0; i < n; ++i)
            {
                lfo += 0.3f / (float) sr; if (lfo >= 1.0f) lfo -= 1.0f;
                auto hz = 250.0f * std::pow (12.0f, 0.5f + 0.5f * std::sin (twoPi * lfo));
                auto t = std::tan (pi * hz / (float) sr);
                auto coef = (t - 1.0f) / (t + 1.0f);
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto x = io[ch][i] + fbState[ch] * 0.5f * amt;
                    for (auto& z : apZ[ch]) { auto y = coef * x + z; z = x - coef * y; x = y; }
                    fbState[ch] = x;
                    io[ch][i] = io[ch][i] * (1.0f - 0.5f * amt) + x * 0.5f * amt * 1.4f;
                }
            }
            break;

        case FxTypes::tremolo:
        case FxTypes::autoPan:
            for (int i = 0; i < n; ++i)
            {
                auto beat = playing ? ppq + (double) i / spb : (lfo2 += (float) (1.0 / spb), (double) lfo2);
                auto ph0 = (float) (beat / (type == FxTypes::tremolo ? 0.5 : 1.0));
                auto s = std::sin (twoPi * (ph0 - std::floor (ph0)));
                if (type == FxTypes::tremolo)
                {
                    auto g = 1.0f - amt * (0.5f + 0.5f * s);
                    L[i] *= g; R[i] *= g;
                }
                else
                {
                    auto pan = s * amt;
                    L[i] *= std::sqrt (0.5f * (1.0f - pan)) * 1.4142f;
                    R[i] *= std::sqrt (0.5f * (1.0f + pan)) * 1.4142f;
                }
            }
            break;

        case FxTypes::widener:
            for (int i = 0; i < n; ++i)
            {
                c[0].push (R[i]);
                auto m = 0.5f * (L[i] + R[i]);
                auto s = 0.5f * (L[i] - R[i]) * (1.0f + amt * 1.5f) + (c[0].read (0.012f * (float) sr) - R[i]) * 0.15f * amt;
                L[i] = m + s; R[i] = m - s;
            }
            break;

        case FxTypes::pitchShifter:
        case FxTypes::harmonizer:
            for (int i = 0; i < n; ++i)
                for (int ch = 0; ch < 2; ++ch)
                {
                    c[ch].push (io[ch][i]);
                    auto w = (float) sr * 0.05f;
                    if (type == FxTypes::pitchShifter)
                        io[ch][i] += (pitchShift (c[ch], ph[ch], 2.0f, w) - io[ch][i]) * amt;
                    else
                        io[ch][i] += (pitchShift (c[ch], ph[ch], 1.4983f, w) * 0.6f + pitchShift (c[ch], ph2[ch], 2.0f, w) * 0.4f) * amt * 0.8f;
                }
            break;

        case FxTypes::freqShifter:
        case FxTypes::ringMod:
        {
            auto hz = type == FxTypes::ringMod ? 50.0f * std::pow (20.0f, amt) : 5.0f + amt * 300.0f;
            // Hilbert pair (two 4-stage allpass chains) for single-sideband shifting
            static constexpr float k1[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
            static constexpr float k2[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
            for (int i = 0; i < n; ++i)
            {
                oscPhase += hz / (float) sr; if (oscPhase >= 1.0f) oscPhase -= 1.0f;
                auto co = std::cos (twoPi * oscPhase), si = std::sin (twoPi * oscPhase);
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto x = io[ch][i];
                    if (type == FxTypes::ringMod) { io[ch][i] += (x * si - x) * juce::jmin (1.0f, amt * 1.5f); continue; }
                    // Olli Niemitalo's 90-degree phase-difference network: y = a^2 (x + y[n-2]) - x[n-2]
                    auto chain = [&] (const float* k, int which)
                    {
                        auto v = x;
                        for (int st = 0; st < 4; ++st)
                        {
                            auto& z = hilbZ[ch][which][st];   // x1, x2, y1, y2
                            auto kk = k[st] * k[st];
                            auto y = kk * (v + z[3]) - z[1];
                            z[1] = z[0]; z[0] = v; z[3] = z[2]; z[2] = y;
                            v = y;
                        }
                        return v;
                    };
                    auto re = hilbDelay[ch];
                    hilbDelay[ch] = chain (k1, 0);
                    auto im = chain (k2, 1);
                    auto shifted = re * co - im * si;
                    io[ch][i] += (shifted - x) * 0.8f;
                }
            }
            break;
        }

        case FxTypes::formant:
            for (int i = 0; i < n; ++i)
            {
                lfo += 0.2f / (float) sr; if (lfo >= 1.0f) lfo -= 1.0f;
                auto m = 0.5f + 0.5f * std::sin (twoPi * lfo);   // vowel A -> O
                if ((count++ & 31) == 0)
                {
                    f1.setCutoffFrequency (800.0f - 350.0f * m); f1.setResonance (6.0f);
                    f2.setCutoffFrequency (1200.0f - 400.0f * m); f2.setResonance (6.0f);
                    f3.setCutoffFrequency (2600.0f - 200.0f * m); f3.setResonance (6.0f);
                }
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto x = io[ch][i];
                    auto y = f1.processSample (ch, x) + 0.7f * f2.processSample (ch, x) + 0.4f * f3.processSample (ch, x);
                    io[ch][i] = x + (y * 1.5f - x) * amt;
                }
            }
            break;

        case FxTypes::saturation:
        case FxTypes::distortion:
        case FxTypes::tape:
        {
            if (type == FxTypes::tape) f1.setCutoffFrequency (14000.0f - 8000.0f * amt);
            for (int i = 0; i < n; ++i)
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto x = io[ch][i];
                    float y;
                    if (type == FxTypes::saturation) y = std::tanh (x * (1.0f + amt * 5.0f)) / (1.0f + amt * 1.2f);
                    else if (type == FxTypes::distortion) y = juce::jlimit (-1.0f, 1.0f, x * (1.0f + amt * 30.0f)) * 0.5f;
                    else y = f1.processSample (ch, std::tanh (x * (1.0f + amt * 2.5f)) * 0.85f);
                    io[ch][i] = x + (y - x) * juce::jmin (1.0f, amt * 2.0f);
                }
            break;
        }

        case FxTypes::filterLP:
        case FxTypes::filterHP:
        case FxTypes::filterBP:
        {
            auto hz = type == FxTypes::filterLP ? 20000.0f * std::pow (0.01f, amt)
                    : type == FxTypes::filterHP ? 20.0f * std::pow (250.0f, amt) : 300.0f * std::pow (10.0f, amt);
            f1.setCutoffFrequency (hz);
            f1.setResonance (type == FxTypes::filterBP ? 2.0f : 0.9f);
            for (int i = 0; i < n; ++i)
                for (int ch = 0; ch < 2; ++ch) io[ch][i] = f1.processSample (ch, io[ch][i]) * (type == FxTypes::filterBP ? 1.6f : 1.0f);
            break;
        }

        case FxTypes::comb:
        {
            auto d = (0.002f + 0.008f * (1.0f - amt)) * (float) sr;
            for (int i = 0; i < n; ++i)
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto y = c[ch].read (d * (ch ? 1.07f : 1.0f));
                    c[ch].push (io[ch][i] + y * 0.85f * amt);
                    io[ch][i] = io[ch][i] * (1.0f - 0.5f * amt) + y * 0.6f * amt;
                }
            break;
        }

        case FxTypes::eq:
        {
            // "air" tilt: more highs, a little less mud
            f1.setCutoffFrequency (3000.0f); f2.setCutoffFrequency (250.0f);
            for (int i = 0; i < n; ++i)
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto x = io[ch][i];
                    auto low = f2.processSample (ch, x);
                    auto high = x - f1.processSample (ch, x);
                    io[ch][i] = x + high * amt * 1.5f - low * amt * 0.4f;
                }
            break;
        }

        case FxTypes::bitcrusher:
        {
            auto levels = std::pow (2.0f, 14.0f - amt * 11.0f);
            auto factor = 1 + (int) (amt * amt * 20.0f);
            for (int i = 0; i < n; ++i)
            {
                if (++count >= factor) { count = 0; for (int ch = 0; ch < 2; ++ch) hold[ch] = std::round (io[ch][i] * levels) / levels; }
                for (int ch = 0; ch < 2; ++ch) io[ch][i] = hold[ch];
            }
            break;
        }

        case FxTypes::tapeWobble:
        case FxTypes::vinyl:
        {
            if (type == FxTypes::vinyl) f1.setCutoffFrequency (6000.0f - 2500.0f * amt);
            for (int i = 0; i < n; ++i)
            {
                lfo += 0.6f / (float) sr; if (lfo >= 1.0f) lfo -= 1.0f;
                lfo2 += 7.0f / (float) sr; if (lfo2 >= 1.0f) lfo2 -= 1.0f;
                auto depth = (type == FxTypes::tapeWobble ? 0.004f : 0.0015f) * amt * (float) sr;
                auto d = 0.006f * (float) sr + depth * (0.5f + 0.5f * std::sin (twoPi * lfo)) + depth * 0.1f * std::sin (twoPi * lfo2);
                auto pop = type == FxTypes::vinyl && rnd.nextFloat() < amt * 5.0f / (float) sr ? (rnd.nextFloat() - 0.5f) * 0.3f * amt : 0.0f;
                for (int ch = 0; ch < 2; ++ch)
                {
                    c[ch].push (io[ch][i]);
                    auto y = c[ch].read (d);
                    if (type == FxTypes::vinyl) y = f1.processSample (ch, y) + (rnd.nextFloat() - 0.5f) * 0.004f * amt + pop;
                    io[ch][i] = y;
                }
            }
            break;
        }

        case FxTypes::stutter:
        {
            auto slice = 0.25;   // 1/16
            auto len = (int) juce::jmin ((double) a[0].b.size() - 4, slice * spb);
            for (int i = 0; i < n; ++i)
            {
                auto beat = playing ? ppq + (double) i / spb : (double) (lfo2 += (float) (1.0 / spb));
                auto s = std::floor (beat / slice);
                if (std::abs (s - lastSlice) > 0.5)
                {
                    lastSlice = s;
                    repeating = captured && rnd.nextFloat() < amt * 0.85f;
                    loopPos = 0;
                    if (! repeating) { loopLen = 0; captured = false; }
                }
                for (int ch = 0; ch < 2; ++ch)
                {
                    if (repeating)
                    {
                        auto fade = juce::jlimit (0.0f, 1.0f, juce::jmin ((float) loopPos, (float) (loopLen - loopPos)) / 48.0f);
                        io[ch][i] = a[ch].b[(size_t) juce::jmin (loopPos, loopLen - 1)] * fade + io[ch][i] * (1.0f - fade);
                    }
                    else if (loopLen < len) a[ch].b[(size_t) loopLen] = io[ch][i];
                }
                if (repeating) { if (++loopPos >= loopLen) loopPos = 0; }
                else if (loopLen < len) { ++loopLen; captured = loopLen >= len - 2; }
            }
            break;
        }

        case FxTypes::spectralFreeze:
        {
            // holds a 300 ms cloud of the sound and loops it with crossfades (an "infinite" pad)
            auto len = (int) (0.3 * sr);
            for (int i = 0; i < n; ++i)
            {
                if (! captured)
                {
                    for (int ch = 0; ch < 2; ++ch) a[ch].b[(size_t) loopLen] = io[ch][i];
                    if (++loopLen >= len) { captured = true; loopPos = 0; }
                    continue;
                }
                auto p = (float) loopPos / (float) len;
                auto p2 = p + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
                auto w1 = std::sin (pi * p), w2 = std::sin (pi * p2);
                auto j2 = (int) (p2 * (float) len);
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto frozen = a[ch].b[(size_t) loopPos] * w1 * w1 + a[ch].b[(size_t) j2] * w2 * w2;
                    io[ch][i] += (frozen - io[ch][i]) * amt;
                }
                if (++loopPos >= len) loopPos = 0;
            }
            break;
        }

        default: break;
    }
    juce::ignoreUnused (wetMix);
}

} // namespace nebula
