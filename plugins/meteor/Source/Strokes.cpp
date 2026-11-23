#include "Strokes.h"

namespace meteor
{

using Buffer = juce::AudioBuffer<float>;

const juce::StringArray& Strokes::names()
{
    static const juce::StringArray n { "REVERB TAIL", "DELAY ONLY", "TAPE STOP", "PITCH DOWN",
                                       "DISTORTION", "REVERSE", "REVERSE REVERB", "STUTTER" };
    return n;
}

const juce::StringArray& Strokes::fileNames()
{
    static const juce::StringArray n { "Reverb Tail", "Delay Only", "Tape Stop", "Pitch Down",
                                       "Distortion", "Reverse", "Reverse Reverb", "Stutter" };
    return n;
}

namespace
{
    Buffer stereo (const Buffer& src)
    {
        Buffer out (2, src.getNumSamples());
        for (int c = 0; c < 2; ++c)
            out.copyFrom (c, 0, src, juce::jmin (c, src.getNumChannels() - 1), 0, src.getNumSamples());
        return out;
    }

    float readLinear (const float* d, int len, double pos)
    {
        if (pos < 0 || pos >= len - 1) return pos >= 0 && (int) pos < len ? d[(int) pos] : 0.0f;
        auto i = (int) pos;
        auto f = (float) (pos - i);
        return d[i] + (d[i + 1] - d[i]) * f;
    }

    /** tone -1..1 -> low-pass cutoff 2 kHz .. 18 kHz (0 = 9 kHz) */
    float toneCutoff (float tone, double sr)
    {
        auto hz = 9000.0f * std::pow (2.0f, tone * (tone < 0 ? 2.17f : 1.0f));
        return juce::jlimit (500.0f, (float) sr * 0.45f, hz);
    }

    void filter (Buffer& b, double sr, float hpHz, float lpHz)
    {
        using C = juce::dsp::IIR::Coefficients<float>;
        auto hp = C::makeHighPass (sr, hpHz, 0.7f);
        auto lp = C::makeLowPass (sr, juce::jmin (lpHz, (float) sr * 0.45f), 0.7f);
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            juce::dsp::IIR::Filter<float> h (hp), l (lp);
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < b.getNumSamples(); ++i) d[i] = l.processSample (h.processSample (d[i]));
        }
    }

    int firstLoudSample (const Buffer& b, float threshold)
    {
        for (int i = 0; i < b.getNumSamples(); ++i)
            for (int c = 0; c < b.getNumChannels(); ++c)
                if (std::abs (b.getSample (c, i)) > threshold) return i;
        return 0;
    }

    Buffer runReverb (const Buffer& in, double sr, float room, float damping, int extraSamples)
    {
        juce::Reverb rv;
        rv.setSampleRate (sr);
        juce::Reverb::Parameters p;
        p.roomSize = room; p.damping = damping; p.wetLevel = 1.0f; p.dryLevel = 0.0f; p.width = 1.0f; p.freezeMode = 0.0f;
        rv.setParameters (p);
        Buffer out (2, in.getNumSamples() + extraSamples);
        out.clear();
        for (int c = 0; c < 2; ++c) out.copyFrom (c, 0, in, juce::jmin (c, in.getNumChannels() - 1), 0, in.getNumSamples());
        for (int start = 0; start < out.getNumSamples(); start += 1024)
        {
            auto len = juce::jmin (1024, out.getNumSamples() - start);
            rv.processStereo (out.getWritePointer (0, start), out.getWritePointer (1, start), len);
        }
        return out;
    }
}

double StrokeRenderer::delayBeats (float length)
{
    static constexpr double b[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    return b[juce::jlimit (0, 5, juce::roundToInt (length * 5.0f))];
}

void StrokeRenderer::normalise (Buffer& b, float peakDb)
{
    auto pk = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c) pk = juce::jmax (pk, b.getMagnitude (c, 0, b.getNumSamples()));
    if (pk > 1.0e-6f) b.applyGain (juce::Decibels::decibelsToGain (peakDb) / pk);
}

void StrokeRenderer::trimSilentEnd (Buffer& b, float thresholdDb, double sr)
{
    auto th = juce::Decibels::decibelsToGain (thresholdDb);
    auto last = 0;
    for (int i = b.getNumSamples() - 1; i >= 0 && last == 0; --i)
        for (int c = 0; c < b.getNumChannels(); ++c)
            if (std::abs (b.getSample (c, i)) > th) { last = i; break; }
    auto keep = juce::jmin (b.getNumSamples(), last + (int) (sr * 0.05) + 1);
    b.setSize (b.getNumChannels(), juce::jmax (1, keep), true, true, false);
}

void StrokeRenderer::fadeEdges (Buffer& b, double sr, double inMs, double outMs)
{
    auto n = b.getNumSamples();
    auto fi = juce::jmin (n / 2, (int) (sr * inMs * 0.001));
    auto fo = juce::jmin (n / 2, (int) (sr * outMs * 0.001));
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        if (fi > 0) b.applyGainRamp (c, 0, fi, 0.0f, 1.0f);
        if (fo > 0) b.applyGainRamp (c, n - fo, fo, 1.0f, 0.0f);
    }
}

// ------------------------------------------------------------------ effects

Buffer StrokeRenderer::reverbWet (const Buffer& src, const StrokeSettings& s)
{
    auto sr = s.sampleRate;
    auto room = juce::jlimit (0.5f, 0.98f, 0.72f + 0.26f * s.length);
    auto damping = juce::jlimit (0.05f, 0.95f, 0.5f - 0.42f * s.tone);
    auto tail = (int) (sr * (2.0 + 9.0 * s.length));
    auto wet = runReverb (src, sr, room, damping, tail);
    if (s.intensity > 0.01f)
    {
        // a second pass smears it into a denser, longer wash
        auto dense = runReverb (wet, sr, room, damping, 0);
        auto k = 0.75f * s.intensity;
        for (int c = 0; c < 2; ++c)
        {
            wet.applyGain (c, 0, wet.getNumSamples(), 1.0f - k);
            wet.addFrom (c, 0, dense, c, 0, wet.getNumSamples(), k * 0.6f);
        }
    }
    filter (wet, sr, 120.0f, toneCutoff (s.tone, sr));
    return wet;
}

Buffer StrokeRenderer::delayWet (const Buffer& src, const StrokeSettings& s)
{
    auto sr = s.sampleRate;
    auto delay = juce::jmax (16, (int) (delayBeats (s.length) * 60.0 / juce::jlimit (30.0, 300.0, s.bpm) * sr));
    auto fb = juce::jlimit (0.0f, 0.9f, 0.3f + 0.6f * s.intensity);
    auto echoes = std::log (0.001) / std::log ((double) juce::jmax (0.05f, fb));
    auto total = juce::jmin ((int) (sr * maxSeconds), src.getNumSamples() + (int) (delay * (echoes + 1.0)));
    Buffer out (2, total);
    out.clear();

    using C = juce::dsp::IIR::Coefficients<float>;
    juce::dsp::IIR::Filter<float> lp[2] { { C::makeLowPass (sr, toneCutoff (s.tone, sr) * 0.7f, 0.7f) },
                                          { C::makeLowPass (sr, toneCutoff (s.tone, sr) * 0.7f, 0.7f) } };
    juce::dsp::IIR::Filter<float> hp[2] { { C::makeHighPass (sr, 150.0f, 0.7f) }, { C::makeHighPass (sr, 150.0f, 0.7f) } };
    std::vector<float> lineL ((size_t) delay, 0.0f), lineR ((size_t) delay, 0.0f);
    auto in = stereo (src);
    int pos = 0;
    for (int i = 0; i < total; ++i)
    {
        auto m = i < in.getNumSamples() ? 0.5f * (in.getSample (0, i) + in.getSample (1, i)) : 0.0f;
        auto outL = lineL[(size_t) pos], outR = lineR[(size_t) pos];
        // ping-pong: input -> L, L -> R, R -> L
        lineL[(size_t) pos] = hp[0].processSample (lp[0].processSample (m + fb * outR));
        lineR[(size_t) pos] = hp[1].processSample (lp[1].processSample (fb * outL));
        out.setSample (0, i, outL);
        out.setSample (1, i, outR);
        if (++pos >= delay) pos = 0;
    }
    return out;
}

Buffer StrokeRenderer::tapeStop (const Buffer& src, const StrokeSettings& s)
{
    auto sr = s.sampleRate;
    auto in = stereo (src);
    auto len = in.getNumSamples();
    auto stopLen = juce::jmax (64, (int) (sr * (0.25 + 1.75 * s.length)));
    auto curve = 0.6f + 1.8f * s.intensity;
    auto stopStart = juce::jmax (0, len - stopLen);
    Buffer out (2, stopStart + stopLen);
    for (int c = 0; c < 2; ++c) out.copyFrom (c, 0, in, c, 0, stopStart);
    double pos = stopStart;
    for (int i = 0; i < stopLen; ++i)
    {
        auto u = (float) i / (float) stopLen;
        auto speed = std::pow (1.0f - u, curve);
        for (int c = 0; c < 2; ++c)
            out.setSample (c, stopStart + i, readLinear (in.getReadPointer (c), len, pos));
        pos += speed;
    }
    // the tape loses its highs as it slows down
    filter (out, sr, 30.0f, toneCutoff (s.tone, sr) * 1.5f);
    return out;
}

Buffer StrokeRenderer::pitchDown (const Buffer& src, const StrokeSettings& s)
{
    auto sr = s.sampleRate;
    auto in = stereo (src);
    auto len = in.getNumSamples();
    auto semis = 12.0f + 12.0f * s.intensity;
    auto glide = juce::jmax (1.0, (0.15 + 0.85 * s.length) * len);
    auto maxOut = (int) juce::jmin (sr * maxSeconds, len * std::pow (2.0, semis / 12.0) + 16.0);
    Buffer out (2, maxOut);
    out.clear();
    double pos = 0;
    int i = 0;
    for (; i < maxOut && pos < len - 1; ++i)
    {
        auto t = juce::jmin (1.0, i / glide);
        auto ratio = std::pow (2.0, -(double) semis / 12.0 * std::pow (t, 0.8));
        for (int c = 0; c < 2; ++c)
            out.setSample (c, i, readLinear (in.getReadPointer (c), len, pos));
        pos += ratio;
    }
    out.setSize (2, juce::jmax (1, i), true, true, false);
    filter (out, sr, 25.0f, toneCutoff (s.tone, sr) * 2.0f);
    return out;
}

Buffer StrokeRenderer::distort (const Buffer& src, const StrokeSettings& s)
{
    auto sr = s.sampleRate;
    auto out = stereo (src);
    filter (out, sr, 80.0f, (float) sr * 0.45f);
    auto drive = 1.0f + 30.0f * std::pow (s.intensity, 1.5f);
    auto fold = 0.5f * s.length;
    for (int c = 0; c < 2; ++c)
    {
        auto* d = out.getWritePointer (c);
        for (int i = 0; i < out.getNumSamples(); ++i)
        {
            auto x = d[i] * drive;
            auto sat = 0.7f * std::tanh (x) + 0.3f * juce::jlimit (-1.0f, 1.0f, x * 0.5f);
            d[i] = (1.0f - fold) * sat + fold * std::sin (2.5f * sat);
        }
    }
    filter (out, sr, 60.0f, toneCutoff (s.tone, sr));
    return out;
}

Buffer StrokeRenderer::reversed (const Buffer& src)
{
    auto out = stereo (src);
    for (int c = 0; c < 2; ++c)
    {
        auto* d = out.getWritePointer (c);
        std::reverse (d, d + out.getNumSamples());
    }
    return out;
}

Buffer StrokeRenderer::stutterRoll (const Buffer& src, const StrokeSettings& s)
{
    auto sr = s.sampleRate;
    auto in = stereo (src);
    auto onset = firstLoudSample (in, 0.01f);
    auto beat = 60.0 / juce::jlimit (30.0, 300.0, s.bpm) * sr;
    auto base = beat * 0.5 * (0.5 + s.length);     // first slices: an 1/8 at LENGTH 50%
    struct Stage { double frac; int repeats; };
    std::vector<Stage> stages { { 1.0, 2 } };
    if (s.intensity > 0.2f) stages.push_back ({ 0.5, 2 });
    if (s.intensity > 0.45f) stages.push_back ({ 0.25, 4 });
    if (s.intensity > 0.7f) stages.push_back ({ 0.125, 8 });

    auto rollLen = 0;
    for (auto& st : stages) rollLen += (int) (base * st.frac) * st.repeats;
    auto restLen = juce::jmax (0, in.getNumSamples() - onset);
    Buffer out (2, rollLen + restLen);
    out.clear();
    auto fade = juce::jmax (8, (int) (sr * 0.002));
    auto at = 0;
    for (auto& st : stages)
    {
        auto slice = juce::jmax (fade * 2 + 1, (int) (base * st.frac));
        for (int r = 0; r < st.repeats; ++r)
        {
            for (int c = 0; c < 2; ++c)
            {
                auto avail = juce::jmax (0, juce::jmin (slice, in.getNumSamples() - onset));
                if (avail > 0) out.copyFrom (c, at, in, c, onset, juce::jmin (avail, out.getNumSamples() - at));
                auto n = juce::jmin (slice, out.getNumSamples() - at);
                if (n > fade * 2)
                {
                    out.applyGainRamp (c, at, fade, 0.0f, 1.0f);
                    out.applyGainRamp (c, at + n - fade, fade, 1.0f, 0.0f);
                }
            }
            at += slice;
            if (at >= out.getNumSamples()) break;
        }
    }
    at = juce::jmin (at, out.getNumSamples());
    for (int c = 0; c < 2; ++c)
        if (out.getNumSamples() - at > 0)
            out.copyFrom (c, at, in, c, onset, juce::jmin (restLen, out.getNumSamples() - at));
    return out;
}

// ------------------------------------------------------------------

Buffer StrokeRenderer::render (int stroke, const Buffer& source, const StrokeSettings& s)
{
    if (source.getNumSamples() == 0 || source.getNumChannels() == 0) return {};
    Buffer out;
    switch (stroke)
    {
        case Strokes::reverbTail:    out = reverbWet (source, s); trimSilentEnd (out, -70.0f, s.sampleRate); break;
        case Strokes::delayOnly:     out = delayWet (source, s); trimSilentEnd (out, -66.0f, s.sampleRate); break;
        case Strokes::tapeStop:      out = tapeStop (source, s); break;
        case Strokes::pitchDown:     out = pitchDown (source, s); break;
        case Strokes::distortion:    out = distort (source, s); break;
        case Strokes::reverse:       out = reversed (source); break;
        case Strokes::reverseReverb: out = reverbWet (source, s); trimSilentEnd (out, -60.0f, s.sampleRate); out = reversed (out); break;
        case Strokes::stutter:       out = stutterRoll (source, s); break;
        default: return {};
    }
    auto maxLen = (int) (s.sampleRate * maxSeconds);
    if (out.getNumSamples() > maxLen) out.setSize (2, maxLen, true, true, false);
    normalise (out, -1.0f);
    fadeEdges (out, s.sampleRate, stroke == Strokes::reverseReverb ? 30.0 : 1.0, stroke == Strokes::reverseReverb ? 4.0 : 20.0);
    return out;
}

} // namespace meteor
