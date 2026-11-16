#include "NebulaEngine.h"

namespace nebula
{

namespace { constexpr float pi = juce::MathConstants<float>::pi; }

void NebulaEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    recordLen = (int) (8.0 * sr);
    record.setSize (2, recordLen);
    tapeBuf.setSize (2, (int) (8.0 * sr));
    for (int c = 0; c < 2; ++c)
    {
        gdBuf[c].assign ((size_t) (2.2 * sr), 0.0f);
        shimBuf[c].assign ((size_t) (0.12 * sr), 0.0f);
    }
    auto block = juce::jmax (1, maxBlockSize);
    wet.setSize (2, block);
    verb.setSize (2, block);

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) block, 2 };
    gdHp.prepare (spec); gdHp.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    gdLp.prepare (spec); gdLp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    dirtLp.prepare (spec); dirtLp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    reverb.setSampleRate (sr);
    mixS.reset (sr, 0.05);
    reset();
}

void NebulaEngine::reset()
{
    record.clear(); tapeBuf.clear();
    writePos = tapePos = 0;
    for (auto& g : grains) g.active = false;
    for (int c = 0; c < 2; ++c) { std::fill (gdBuf[c].begin(), gdBuf[c].end(), 0.0f); std::fill (shimBuf[c].begin(), shimBuf[c].end(), 0.0f); }
    gdHp.reset(); gdLp.reset(); dirtLp.reset(); reverb.reset();
    grainClock = sparkleClock = 0; lastStep = -1; semisNow = semisTarget = 0;
    tapeLag = 0; tapePhase = -1; tapeRelease = 0; lastTapeCycle = -1; gdTimeNow = 0;
}

float NebulaEngine::window (float x, float shape) const noexcept
{
    auto h = 0.5f - 0.5f * std::cos (2.0f * pi * x);
    if (shape < 0.5f)
        return h * h + (h - h * h) * shape * 2.0f;           // soft (smooth clouds) -> Hann
    auto trap = juce::jmin (1.0f, x / 0.08f, (1.0f - x) / 0.08f);
    return h + (trap - h) * (shape - 0.5f) * 2.0f;          // Hann -> punchy (almost square)
}

float NebulaEngine::readRecord (int ch, double pos) const noexcept
{
    while (pos < 0) pos += recordLen;
    auto i0 = (int) pos % recordLen;
    auto i1 = (i0 + 1) % recordLen;
    auto f = (float) (pos - std::floor (pos));
    auto* d = record.getReadPointer (ch);
    return d[i0] + (d[i1] - d[i0]) * f;
}

float NebulaEngine::quantise (float semis, int scale, int) const noexcept
{
    if (scale <= 0) return semis;
    static const std::vector<int> scales[] = { {}, { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 3, 5, 7, 10 } };
    auto& sc = scales[juce::jlimit (1, 3, scale)];
    auto best = semis; auto bestD = 1.0e9f;
    for (int oct = -3; oct <= 3; ++oct)
        for (auto d : sc)
        {
            auto v = (float) (oct * 12 + d);
            if (std::abs (v - semis) < bestD) { bestD = std::abs (v - semis); best = v; }
        }
    return best;
}

void NebulaEngine::startGrain (const NebulaSettings& s, float semis, bool sparkle)
{
    Grain* g = nullptr;
    for (auto& gr : grains) if (! gr.active) { g = &gr; break; }
    if (g == nullptr) return;

    auto texture = s.texture, motion = s.motion;
    auto sizeMs = s.sizeMs * (1.4f - 0.8f * texture) * (s.grainCloud ? 1.8f : 1.0f);
    if (sparkle) sizeMs = 20.0f + rnd.nextFloat() * 50.0f;
    sizeMs *= 1.0f + (rnd.nextFloat() - 0.5f) * 0.35f * motion;
    auto len = juce::jlimit (64, (int) (1.2 * sr), (int) (sizeMs * 0.001f * (float) sr));

    auto st = s.grainPitch + semis + s.pitchCents * 0.01f + (rnd.nextFloat() - 0.5f) * 0.15f * motion;
    if (sparkle) st += rnd.nextBool() ? 12.0f : 24.0f;
    if (s.shimmer && rnd.nextFloat() < 0.3f) st += 12.0f;
    auto ratio = std::pow (2.0, (double) st / 12.0);

    auto rev = rnd.nextFloat() < juce::jmax (s.reverse, s.reverseFx ? 0.6f : 0.0f);
    auto spray = juce::jlimit (0.0f, 1.0f, s.spray * (0.3f + 1.4f * motion) + (s.grainCloud ? 0.3f : 0.0f) + (sparkle ? 0.4f : 0.0f));

    auto back = (double) s.position * 4.0 * sr + (double) spray * rnd.nextFloat() * 1.0 * sr;
    auto margin = 64.0 + (! rev && ratio > 1.0 ? len * (ratio - 1.0) : 0.0) + (rev ? len * ratio : 0.0);
    back = juce::jlimit (margin, (double) recordLen - len * ratio - 64.0, back + margin);

    g->pos = (double) writePos - back;
    g->inc = rev ? -ratio : ratio;
    g->length = len;
    g->age = 0;
    auto width = juce::jlimit (0.0f, 1.0f, s.stereo * (sparkle ? 1.4f : 1.0f));
    auto pan = (rnd.nextFloat() * 2.0f - 1.0f) * width;
    g->panL = std::cos ((pan + 1.0f) * pi * 0.25f) * 1.4142f;
    g->panR = std::sin ((pan + 1.0f) * pi * 0.25f) * 1.4142f;

    auto dens = s.density * (0.3f + 1.7f * texture) * (s.grainCloud ? 3.0f : 1.0f);
    auto overlap = juce::jmax (1.0f, dens * (float) len / (float) sr);
    g->gain = 0.9f / std::sqrt (overlap) * (sparkle ? 0.5f : 1.0f);
    g->shape = sparkle ? 0.3f : s.shape;
    g->active = true;
    ++grainsStarted;
}

void NebulaEngine::process (juce::AudioBuffer<float>& buffer, const NebulaSettings& s, const Transport& t)
{
    auto n = buffer.getNumSamples();
    auto chans = juce::jmin (2, buffer.getNumChannels());
    if (n == 0 || chans == 0) return;
    if (wet.getNumSamples() < n) { wet.setSize (2, n, false, false, true); verb.setSize (2, n, false, false, true); }

    buffer.applyGain (s.input);
    float inPk[2] {}, outPk[2] {}, wetPk = 0;

    auto bpm = juce::jlimit (20.0, 999.0, t.bpm);
    auto spb = sr * 60.0 / bpm;
    auto dens = s.density * (0.3f + 1.7f * s.texture) * (s.grainCloud ? 3.0f : 1.0f);
    auto sparkleRate = s.particles ? 14.0f * (0.5f + s.texture) : 0.0f;
    auto glideCoeff = s.glide <= 0.001f ? 1.0f : 1.0f - std::exp (-1.0f / (s.glide * 0.25f * (float) sr));

    // grain delay settings
    auto gdMix = juce::jlimit (0.0f, 1.0f, s.gdMix * (0.3f + 1.4f * s.space));
    gdHp.setCutoffFrequency (juce::jlimit (20.0f, 5000.0f, s.gdHighPass));
    gdLp.setCutoffFrequency (juce::jlimit (500.0f, (float) sr * 0.45f, s.gdLowPass));
    auto gdTarget = juce::jlimit (10.0f, 2000.0f, s.gdTimeMs) * 0.001f * (float) sr;
    if (gdTimeNow <= 0) gdTimeNow = gdTarget;

    // dirt
    auto d = s.dirt;
    dirtLp.setCutoffFrequency (20000.0f * std::pow (0.3f, d));
    auto crushLevels = std::pow (2.0f, 14.0f - d * 9.0f);
    auto crushFactor = 1 + (int) (d * d * 6.0f);
    mixS.setTargetValue (s.mix);

    for (int i = 0; i < n; ++i)
    {
        float in[2] = { buffer.getSample (0, i), buffer.getSample (chans > 1 ? 1 : 0, i) };
        inPk[0] = juce::jmax (inPk[0], std::abs (in[0])); inPk[1] = juce::jmax (inPk[1], std::abs (in[1]));

        // 1) remember the input (FREEZE keeps what is there)
        if (! s.freeze)
        {
            record.setSample (0, writePos, in[0]);
            record.setSample (1, writePos, in[1]);
            writePos = (writePos + 1) % recordLen;
        }

        // 2) pitch step sequencer
        double beat;
        if (t.playing && t.hasPosition) beat = t.ppq + (double) i / spb;
        else { beat = freeBeats; freeBeats += 1.0 / spb; }

        if (s.stepMode > 0)
        {
            auto idx = std::floor (beat / juce::jmax (0.0625, s.stepBeats));
            if (std::abs (idx - lastStep) > 0.5)
            {
                lastStep = idx;
                auto k = (int) ((juce::int64) idx % 8);
                if (k < 0) k += 8;
                static constexpr int simpleIntervals[] = { 0, 5, 7, 12, -12 };
                float v = s.stepMode == 5 ? (float) s.steps[(size_t) k] : ((k & 1) ? (float) simpleIntervals[juce::jlimit (0, 4, s.stepMode)] : 0.0f);
                if (rnd.nextFloat() > s.chance) v = 0.0f;
                semisTarget = quantise (v * s.stepAmount, s.scaleLock, s.scaleKey);
                stepIndexNow = s.stepMode == 5 ? k : (k & 1);
                ++stepFlash;
            }
        }
        else semisTarget = 0.0f;
        semisNow += (semisTarget - semisNow) * glideCoeff;

        // 3) schedule grains
        grainClock += dens / sr;
        while (grainClock >= 1.0)
        {
            startGrain (s, semisNow, false);
            grainClock -= 1.0 + (rnd.nextFloat() - 0.5f) * 0.8f * s.motion;
        }
        if (sparkleRate > 0)
        {
            sparkleClock += sparkleRate / sr;
            while (sparkleClock >= 1.0) { startGrain (s, semisNow, true); sparkleClock -= 1.0 + (rnd.nextFloat() - 0.5f); }
        }

        // 4) play the grains
        float w[2] {};
        int active = 0;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            ++active;
            auto x = (float) g.age / (float) g.length;
            auto env = window (x, g.shape) * g.gain;
            w[0] += readRecord (0, g.pos) * env * g.panL;
            w[1] += readRecord (1, g.pos) * env * g.panR;
            g.pos += g.inc;
            if (++g.age >= g.length) g.active = false;
        }
        activeGrains = active;

        // 5) grain delay
        if (gdMix > 0.001f)
        {
            gdTimeNow += (gdTarget - gdTimeNow) * 0.0005f;
            auto size = (int) gdBuf[0].size();
            auto rp = (float) gdPos - gdTimeNow;
            while (rp < 0) rp += (float) size;
            auto i0 = (int) rp, i1 = (i0 + 1) % size;
            auto fr = rp - (float) i0;
            for (int c = 0; c < 2; ++c)
            {
                auto y = gdBuf[c][(size_t) i0] * (1.0f - fr) + gdBuf[c][(size_t) i1] * fr;
                auto fb = gdLp.processSample (c, gdHp.processSample (c, y)) * juce::jlimit (0.0f, 0.95f, s.gdFeedback);
                gdBuf[c][(size_t) gdPos] = juce::jlimit (-4.0f, 4.0f, w[c] + fb);
                w[c] += y * gdMix;
            }
            gdPos = (gdPos + 1) % size;
        }

        // 6) DIRT
        if (d > 0.001f)
        {
            if (++crushCount >= crushFactor)
            {
                crushCount = 0;
                for (int c = 0; c < 2; ++c) crushHold[c] = std::round (w[c] * crushLevels) / crushLevels;
            }
            for (int c = 0; c < 2; ++c)
            {
                auto v = w[c] + (crushHold[c] - w[c]) * juce::jmin (1.0f, d * 1.5f);
                v = std::tanh (v * (1.0f + d * 4.0f)) / (1.0f + d * 1.5f);
                w[c] = dirtLp.processSample (c, v);
            }
        }

        wet.setSample (0, i, w[0]);
        wet.setSample (1, i, w[1]);
    }

    // 7) SPACE: reverb (with octave-up shimmer feeding it)
    if (s.space > 0.01f)
    {
        verb.copyFrom (0, 0, wet, 0, 0, n);
        verb.copyFrom (1, 0, wet, 1, 0, n);
        if (s.shimmer)
        {
            auto len = 0.05f * (float) sr;
            auto size = (int) shimBuf[0].size();
            for (int i = 0; i < n; ++i)
            {
                shimPhase += 1.0f / len; if (shimPhase >= 1.0f) shimPhase -= 1.0f;
                for (int c = 0; c < 2; ++c)
                {
                    shimBuf[c][(size_t) shimPos] = verb.getSample (c, i);
                    float o = 0;
                    for (auto off : { 0.0f, 0.5f })
                    {
                        auto p = shimPhase + off; if (p >= 1.0f) p -= 1.0f;
                        auto age = 2.0f + (1.0f - p) * len;
                        auto idx = ((shimPos - (int) age) % size + size) % size;
                        auto ww = std::sin (pi * p);
                        o += shimBuf[c][(size_t) idx] * ww * ww;
                    }
                    verb.addSample (c, i, o * 0.7f);
                }
                shimPos = (shimPos + 1) % size;
            }
        }
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.6f + 0.38f * s.space; rp.damping = s.shimmer ? 0.2f : 0.4f;
        rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f; rp.freezeMode = s.freeze ? 0.0f : 0.0f;
        reverb.setParameters (rp);
        reverb.processStereo (verb.getWritePointer (0), verb.getWritePointer (1), n);
        wet.addFrom (0, 0, verb, 0, 0, n, s.space * 0.8f);
        wet.addFrom (1, 0, verb, 1, 0, n, s.space * 0.8f);
    }

    // 8) mix, 9) tape stop on the final sound
    auto tapeLen = juce::jmax (64.0, s.tapeLengthBeats * spb);
    auto tapeSize = tapeBuf.getNumSamples();
    for (int i = 0; i < n; ++i)
    {
        auto m = mixS.getNextValue();
        auto dryG = std::cos (m * juce::MathConstants<float>::halfPi), wetG = std::sin (m * juce::MathConstants<float>::halfPi);
        float o[2];
        for (int c = 0; c < 2; ++c)
        {
            o[c] = buffer.getSample (juce::jmin (c, chans - 1), i) * dryG + wet.getSample (c, i) * wetG;
            wetPk = juce::jmax (wetPk, std::abs (wet.getSample (c, i)));
            tapeBuf.setSample (c, tapePos, o[c]);
        }

        // when does the tape stop?
        double beat = (t.playing && t.hasPosition) ? t.ppq + (double) i / spb : freeBeats - (double) (n - i) / spb;
        if (tapePhase < 0)
        {
            bool start = false;
            if (s.tapeManual) start = manualTrigger.exchange (false);
            else if (s.tapeEveryBeats > 0)
            {
                auto cycle = std::floor ((beat - s.tapePosition * s.tapeEveryBeats) / s.tapeEveryBeats);
                if (std::abs (cycle - lastTapeCycle) > 0.5)
                {
                    if (lastTapeCycle >= 0 || cycle >= 0) start = lastTapeCycle > -1 && rnd.nextFloat() < s.tapeChance;
                    lastTapeCycle = cycle;
                }
            }
            if (start) { tapePhase = 0; tapeLag = 0; }
        }

        float speed = 1.0f;
        if (tapePhase >= 0)
        {
            tapePhase += (float) (1.0 / tapeLen);
            if (tapePhase < 1.0f)
                speed = std::pow (1.0f - tapePhase, 0.4f + s.tapeCurve * 2.6f);          // slowing down
            else if (s.tapeStart > 0.01f && tapePhase < 1.0f + s.tapeStart)
            {
                if (tapePhase - (float) (1.0 / tapeLen) < 1.0f) tapeLag = 0;            // spin up from "now"
                speed = (tapePhase - 1.0f) / s.tapeStart;
            }
            else
            {
                tapePhase = -1; tapeRelease = 1.0f;
            }
            tapeLag += 1.0 - speed;
            tapeLag = juce::jmin (tapeLag, (double) tapeSize - 8);
        }
        tapeSpeedNow = speed;

        if (tapePhase >= 0 || tapeRelease > 0)
        {
            auto rp = (double) tapePos - tapeLag;
            while (rp < 0) rp += tapeSize;
            auto i0 = (int) rp % tapeSize, i1 = (i0 + 1) % tapeSize;
            auto fr = (float) (rp - std::floor (rp));
            for (int c = 0; c < 2; ++c)
            {
                auto slow = tapeBuf.getSample (c, i0) * (1.0f - fr) + tapeBuf.getSample (c, i1) * fr;
                if (tapePhase >= 1.0f && s.tapeStart <= 0.01f) slow = 0.0f;
                o[c] = tapePhase >= 0 ? slow : o[c] + (slow - o[c]) * tapeRelease;
            }
            if (tapePhase < 0) tapeRelease = juce::jmax (0.0f, tapeRelease - 1.0f / (0.015f * (float) sr));
        }
        tapePos = (tapePos + 1) % tapeSize;

        for (int c = 0; c < chans; ++c)
        {
            auto v = o[c] * s.output;
            buffer.setSample (c, i, v);
            outPk[c] = juce::jmax (outPk[c], std::abs (v));
        }
    }
    if (chans == 1) { inPk[1] = inPk[0]; outPk[1] = outPk[0]; }

    for (int c = 0; c < 2; ++c)
    {
        inLevel[(size_t) c] = juce::jmax (inPk[c], inLevel[(size_t) c].load() * 0.85f);
        outLevel[(size_t) c] = juce::jmax (outPk[c], outLevel[(size_t) c].load() * 0.85f);
    }
    wetLevel = juce::jmax (wetPk, wetLevel.load() * 0.85f);
    currentSemis = semisNow;
}

} // namespace nebula
