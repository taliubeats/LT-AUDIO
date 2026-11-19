#include "SynthEngine.h"

namespace quasar
{

using namespace dsp;

namespace
{
    constexpr int combSize = 1 << 14;

    inline float curveOf (float x, int curve) noexcept
    {
        switch (curve)
        {
            case 1: return x * x * (x >= 0 ? 1.0f : -1.0f);                                     // EXP
            case 2: return std::sqrt (std::abs (x)) * (x >= 0 ? 1.0f : -1.0f);                  // LOG
            case 3: { auto a = std::abs (x); return a * a * (3.0f - 2.0f * a) * (x >= 0 ? 1.0f : -1.0f); }   // S-CURVE
            default: return x;
        }
    }

    /** Unison voice positions -1..1 (centre voice at 0 for odd counts), with a slightly wider outer spacing. */
    inline float unisonPos (int u, int count) noexcept
    {
        if (count <= 1) return 0.0f;
        auto x = (float) u / (float) (count - 1) * 2.0f - 1.0f;
        return x * (0.75f + 0.25f * std::abs (x));
    }

    inline float semisToRatio (float s) noexcept { return std::exp2 (s * (1.0f / 12.0f)); }
}

// ================================================================== voice

void Voice::prepare (double internalRate, int controlStep)
{
    isr = internalRate;
    stepSize = juce::jmax (4, controlStep);
    for (auto& f : flt)
        for (auto& c : f.comb)
            if ((int) c.size() != combSize) c.assign ((size_t) combSize, 0.0f);
    touched.reserve (numModSlots * 2);
}

void Voice::start (SynthEngine& e, int n, float vel, int ch, int si, int sc, bool glideIn, float glideFrom)
{
    auto wasActive = active && ! killing;
    note = n; velocity = vel; channel = ch; stackIndex = si; stackCount = sc;
    active = true; released = false; killing = false; sustained = false;
    killGain = 1.0f;
    aftertouch = 0; slide = 0; bend = 0;
    randomValue = e.random.nextFloat();

    pitchTarget = (float) n;
    pitchNow = glideIn ? glideFrom : (float) n;

    for (int i = 0; i < numEnvs; ++i) { env[(size_t) i].noteOn (! wasActive); envPrev[(size_t) i] = env[(size_t) i].level; }
    for (int l = 0; l < numLfos; ++l)
    {
        auto& s = lfo[(size_t) l];
        s.phase = e.bp.lfo[(size_t) l].phase / 360.0f; s.elapsed = 0; s.done = false; s.smoothed = s.out = 0;
        s.rnd = e.random.nextFloat(); s.rndPrev = s.rnd;
    }

    driftValue = (e.random.nextFloat() * 2 - 1) * 0.5f;
    driftTarget = e.random.nextFloat() * 2 - 1;
    noise.seed ((juce::uint32) e.random.nextInt() | 1u);

    for (int o = 0; o < numOscs; ++o)
    {
        auto& os = osc[(size_t) o];
        auto& p = e.bp.osc[(size_t) o];
        for (int u = 0; u < maxUnison; ++u)
        {
            auto& un = os.uni[(size_t) u];
            if (! wasActive || u >= os.count)
            {
                auto r = e.random.nextFloat() * p.rand;
                un.phase = std::fmod (p.phase / 360.0f + r, 1.0f);
            }
            un.drift = (e.random.nextFloat() * 2 - 1);
            un.driftTarget = un.drift;
            un.samplePos = (double) p.wtpos * (e.oscSample[(size_t) o] != nullptr ? e.oscSample[(size_t) o]->length() : 0);
        }
        if (! wasActive) { os.gainL.snap (0); os.gainR.snap (0); os.wtpos.snap (p.wtpos); os.warp.snap (p.warp); }
    }
    if (! wasActive)
    {
        subPhase = 0; subL.snap (0); subR.snap (0); noiseL.snap (0); noiseR.snap (0);
        ampGain.snap (0);
        for (auto& f : flt)
        {
            for (auto& l : f.ladder) l.reset();
            for (auto& s : f.svfA) s.reset();
            for (auto& s : f.svfB) s.reset();
            for (auto& ff : f.formant) for (auto& s : ff) s.reset();
            for (auto& c : f.comb) std::fill (c.begin(), c.end(), 0.0f);
            f.combLp = {};
            f.outL.snap (0); f.outR.snap (0);
            f.g.snap (0.1f); f.mix.snap (1);
        }
    }
    age = ++e.lastAgeCounter();
    controlStep (e, 0);
}

void Voice::release()
{
    if (! active || released) return;
    released = true;
    for (auto& en : env) en.noteOff();
}

void Voice::kill()
{
    if (! active) return;
    killing = true;
    killStep = 1.0f / (float) juce::jmax (1.0, 0.005 * isr);   // 5 ms
}

void Voice::retarget (int n, bool retrigger, SynthEngine& e)
{
    note = n;
    pitchTarget = (float) n;
    if (e.bp.glideMs <= 0.0f) pitchNow = (float) n;
    released = false;
    if (retrigger)
    {
        for (auto& en : env) en.noteOn (false);
        for (int l = 0; l < numLfos; ++l)
            if (e.bp.lfo[(size_t) l].mode != 0) { lfo[(size_t) l].phase = e.bp.lfo[(size_t) l].phase / 360.0f; lfo[(size_t) l].elapsed = 0; lfo[(size_t) l].done = false; }
    }
}

float Voice::modValue (int dest, float baseReal, SynthEngine& e) const noexcept
{
    auto off = modOff[(size_t) dest];
    if (off == 0.0f || e.destParams[(size_t) dest] == nullptr) return baseReal;
    return e.destParams[(size_t) dest]->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, e.destBase[(size_t) dest] + off));
}

void Voice::controlStep (SynthEngine& e, int len)
{
    auto& bp = e.bp;
    auto& d = e.d;
    auto sr = (float) isr;

    // ---- envelopes (times can be modulated: use last step's offsets)
    for (int i = 0; i < numEnvs; ++i)
    {
        auto& p = bp.env[(size_t) i];
        env[(size_t) i].set (sr, modValue (d.att[(size_t) i], p.att, e), modValue (d.hold[(size_t) i], p.hold, e), modValue (d.dec[(size_t) i], p.dec, e),
                             modValue (d.sus[(size_t) i], p.sus, e), modValue (d.rel[(size_t) i], p.rel, e), p.ac, p.dc, p.rc);
        envPrev[(size_t) i] = env[(size_t) i].level;
        if (len > 0) env[(size_t) i].advance (len);
    }

    // ---- LFOs
    for (int l = 0; l < numLfos; ++l)
    {
        auto& p = bp.lfo[(size_t) l];
        auto& s = lfo[(size_t) l];
        double inc = p.sync ? (e.bpm / 60.0) / juce::jmax (1.0e-3, p.beats) / isr : modValue (d.lfoRate[(size_t) l], p.rateHz, e) / isr;
        auto prevPhase = s.phase;
        if (p.mode == 0)
        {
            // FREE: every voice shares the same phase, locked to the host when synced
            auto posBeats = (e.playing ? e.ppq : 0.0) + (double) e.renderPos / isr * e.bpm / 60.0;
            s.phase = p.sync ? posBeats / juce::jmax (1.0e-3, p.beats) + p.phase / 360.0
                             : e.lfoFreePhase[(size_t) l] + inc * e.renderPos;
        }
        else if (! s.done)
        {
            s.phase += inc * len;
            if (p.mode == 2 && s.phase - p.phase / 360.0 >= 1.0) { s.phase = p.phase / 360.0 + 0.9999; s.done = true; }
        }
        auto kind = e.lfoTables.kind (l);
        float v;
        if (kind == LfoShape::sampleHold || kind == LfoShape::smoothRandom)
        {
            if (std::floor (s.phase) != std::floor (prevPhase)) { s.rndPrev = s.rnd; s.rnd = e.random.nextFloat(); }
            auto fr = (float) (s.phase - std::floor (s.phase));
            v = kind == LfoShape::sampleHold ? s.rnd : s.rndPrev + (s.rnd - s.rndPrev) * (0.5f - 0.5f * std::cos (fr * pi));
        }
        else v = e.lfoTables.read (l, (float) (s.phase - std::floor (s.phase)));

        s.elapsed += (float) len / sr * 1000.0f;
        auto delay = modValue (d.lfoDelay[(size_t) l], p.delay, e), rise = modValue (d.lfoRise[(size_t) l], p.rise, e);
        float fade = 1.0f;
        if (s.elapsed < delay) fade = 0.0f;
        else if (rise > 0.0f) fade = juce::jmin (1.0f, (s.elapsed - delay) / rise);
        v *= fade;
        auto smooth = modValue (d.lfoSmooth[(size_t) l], p.smooth, e) * 0.01f;
        if (smooth > 0.001f)
        {
            auto tc = smooth * smooth * 0.25f * sr;   // up to 250 ms
            s.smoothed += (1.0f - std::exp (-(float) juce::jmax (1, len) / tc)) * (v - s.smoothed);
            v = s.smoothed;
        }
        else s.smoothed = v;
        s.out = v;
    }

    // ---- sources
    src[srcNone] = 0;
    for (int i = 0; i < numEnvs; ++i) src[(size_t) (srcEnv1 + i)] = env[(size_t) i].level;
    for (int l = 0; l < numLfos; ++l) src[(size_t) (srcLfo1 + l)] = lfo[(size_t) l].out;
    for (int m = 0; m < numMacros; ++m) src[(size_t) (srcMacro1 + m)] = bp.macro[(size_t) m];
    src[srcVelocity] = velocity;
    src[srcNote] = (float) note / 127.0f;
    src[srcModWheel] = e.modWheel;
    src[srcAftertouch] = juce::jmax (aftertouch, e.channelPressure);
    src[srcPitchBend] = bp.mpe ? bend : e.pitchBend;
    src[srcRandom] = randomValue;
    src[srcSlide] = slide;

    // ---- modulation matrix
    for (auto t : touched) modOff[(size_t) t] = 0.0f;
    touched.clear();
    for (int i = 0; i < e.numActiveSlots; ++i)
    {
        auto& m = e.slots[(size_t) i];
        if (! ModDefs::destinations()[(size_t) m.dst].perVoice) continue;
        auto v = src[(size_t) m.src];
        if (m.bip && ! ModDefs::isBipolarByNature (m.src)) v = v * 2.0f - 1.0f;
        v = curveOf (v, m.curve) * m.amt * m.out;
        if (m.aux > 0) v *= src[(size_t) m.aux];
        if (modOff[(size_t) m.dst] == 0.0f) touched.push_back (m.dst);
        modOff[(size_t) m.dst] += v;
        if (modOff[(size_t) m.dst] == 0.0f) modOff[(size_t) m.dst] = 1.0e-9f;
    }

    // ---- kill / end
    if (killing)
    {
        killGain -= killStep * (float) juce::jmax (1, len);
        if (killGain <= 0.0f) { killGain = 0.0f; active = false; return; }
    }
    if (! env[0].isActive() && len > 0) { active = false; return; }

    // ---- pitch
    auto glideMs = modValue (d.glide, bp.glideMs, e);
    if (glideMs > 0.0f && len > 0)
        pitchNow += (1.0f - std::exp (-(float) len / (glideMs * 0.001f * sr * 0.25f))) * (pitchTarget - pitchNow);
    else pitchNow = pitchTarget;
    if (std::abs (pitchNow - pitchTarget) < 0.0005f) pitchNow = pitchTarget;

    auto drift = modValue (d.drift, bp.drift * 100.0f, e) * 0.01f;
    driftPhase += (float) len / sr * 0.35f;
    if (driftPhase >= 1.0f) { driftPhase -= 1.0f; driftTarget = e.random.nextFloat() * 2 - 1; }
    driftValue += (float) len / sr * 0.6f * (driftTarget - driftValue);

    auto bendSemis = (bp.mpe ? bend : e.pitchBend);
    bendSemis = bendSemis >= 0 ? bendSemis * (float) bp.pbUp : bendSemis * (float) bp.pbDown;
    if (bp.mpe) bendSemis = bend * 48.0f;
    auto stackOff = stackCount > 1 ? ((float) stackIndex / (float) (stackCount - 1) * 2.0f - 1.0f) : 0.0f;
    auto envP = env[3].level * modValue (d.envPAmount, bp.envPAmount, e);
    auto basePitch = e.noteToPitch (pitchNow + (float) bp.transpose) + bendSemis + envP
                   + stackOff * (0.08f + 0.12f * drift) + driftValue * drift * 0.06f;
    e.lastPitch = pitchNow;

    auto velGain = 1.0f - bp.ampVel * (1.0f - std::pow (velocity, 1.4f));
    auto stackPan = stackCount > 1 ? stackOff * 0.6f : 0.0f;
    auto rampLen = juce::jmax (1, len > 0 ? len : stepSize);

    // ---- oscillators
    for (int o = 0; o < numOscs; ++o)
    {
        auto& p = bp.osc[(size_t) o];
        auto& os = osc[(size_t) o];
        os.on = p.on && e.bp.channelGain[(size_t) (chOscA + o)] > 0.0f;
        auto level = levelToGain (modValue (d.level[(size_t) o], p.level * 100.0f, e)) * e.bp.channelGain[(size_t) (chOscA + o)];
        if (! p.on) level = 0.0f;
        os.on = p.on;
        float gl, gr; panGains (modValue (d.pan[(size_t) o], p.pan * 100.0f, e) * 0.01f + stackPan, gl, gr);
        os.gainL.setLength (rampLen); os.gainR.setLength (rampLen); os.wtpos.setLength (rampLen); os.warp.setLength (rampLen);
        os.gainL.set (level * gl); os.gainR.set (level * gr);
        os.wtpos.set (juce::jlimit (0.0f, 1.0f, modValue (d.wtpos[(size_t) o], p.wtpos * 100.0f, e) * 0.01f));
        auto warp = juce::jlimit (0.0f, 1.0f, modValue (d.warp[(size_t) o], p.warp * 100.0f, e) * 0.01f);
        os.warp.set (warp);
        if (! p.on) continue;

        auto semis = modValue (d.semi[(size_t) o], p.semi, e) + (float) (p.oct * 12) + modValue (d.fine[(size_t) o], p.fine, e) * 0.01f;
        auto detune = modValue (d.detune[(size_t) o], p.detune * 100.0f, e) * 0.01f;
        auto blend = modValue (d.blend[(size_t) o], p.blend * 100.0f, e) * 0.01f;
        auto spread = modValue (d.spread[(size_t) o], p.spread * 100.0f, e) * 0.01f;
        os.count = juce::jlimit (1, maxUnison, p.unison);
        float warpSlope = 1.0f;
        switch (p.warpMode)
        {
            case 1: warpSlope = 1.0f + warp * 7.0f; break;
            case 2: case 3: warpSlope = 1.0f + 0.95f * warp; break;
            case 4: warpSlope = 0.5f / (0.5f - 0.46f * warp); break;
            case 5: warpSlope = 1.0f + warp; break;
            case 6: warpSlope = 1.0f + 2.0f * warp; break;
            case 7: warpSlope = 1.0f + 4.0f * warp; break;
            default: break;
        }
        auto detuneSemis = detune * detune * 1.2f;   // up to +-1.2 semitones at 100 %
        auto pitch = basePitch + semis;

        // gains: centre vs side voices (BLEND), normalised so more voices are not louder
        float sumSq = 0.0f;
        for (int u = 0; u < os.count; ++u)
        {
            auto& un = os.uni[(size_t) u];
            auto pos = unisonPos (u, os.count);
            auto centre = os.count == 1 || (os.count % 2 == 1 && u == os.count / 2);
            un.gain = os.count == 1 ? 1.0f : (centre ? 1.0f - 0.5f * blend : blend + (1.0f - blend) * 0.25f);
            sumSq += un.gain * un.gain;
            un.driftTarget += (e.random.nextFloat() * 2 - 1) * 0.02f;
            un.driftTarget = juce::jlimit (-1.0f, 1.0f, un.driftTarget);
            un.drift += 0.05f * (un.driftTarget - un.drift);
            auto analogDrift = p.type == 1 ? un.drift * drift * 0.08f : un.drift * drift * 0.02f;
            auto hz = 440.0f * semisToRatio (pitch + pos * detuneSemis + analogDrift - 69.0f);
            un.inc = juce::jlimit (0.0f, 0.49f, hz / sr);
            Wavetable::levelFor (un.inc * warpSlope * e.mipScale, un.level, un.w);
            float pl, pr; panGains (pos * spread, pl, pr);
            un.panL = pl; un.panR = pr;
        }
        auto norm = 1.0f / std::sqrt (juce::jmax (1.0e-6f, sumSq));
        for (int u = 0; u < os.count; ++u) os.uni[(size_t) u].gain *= norm;

        if (p.type == 2)
        {
            auto* smp = e.oscSample[(size_t) o];
            os.sampleRatio = smp != nullptr ? (float) (smp->sourceRate / isr) * semisToRatio (pitch - (float) smp->rootNote) : 1.0f;
        }
        os.level = 0; os.mipW = 0;
    }

    // ---- sub
    {
        auto lv = bp.subOn ? levelToGain (modValue (d.subLevel, bp.subLevel * 100.0f, e)) * bp.channelGain[chSub] : 0.0f;
        float gl, gr; panGains (modValue (d.subPan, bp.subPan * 100.0f, e) * 0.01f + stackPan, gl, gr);
        subL.setLength (rampLen); subR.setLength (rampLen);
        subL.set (lv * gl); subR.set (lv * gr);
        subInc = juce::jlimit (0.0f, 0.49f, 440.0f * semisToRatio (basePitch + 12.0f * (float) bp.subOct - 69.0f) / sr);
        Wavetable::levelFor (subInc * e.mipScale, subLevel, subW);
    }
    // ---- noise
    {
        auto lv = bp.noiseOn ? levelToGain (modValue (d.noiseLevel, bp.noiseLevel * 100.0f, e)) * bp.channelGain[chNoise] : 0.0f;
        float gl, gr; panGains (modValue (d.noisePan, bp.noisePan * 100.0f, e) * 0.01f + stackPan, gl, gr);
        noiseL.setLength (rampLen); noiseR.setLength (rampLen);
        noiseL.set (lv * gl); noiseR.set (lv * gr);
    }

    // ---- filters
    for (int f = 0; f < numFilters; ++f)
    {
        auto& p = bp.flt[(size_t) f];
        auto& fs = flt[(size_t) f];
        fs.on = p.on; fs.type = p.type;
        auto chGain = bp.channelGain[(size_t) (chFlt1 + f)];
        auto lv = levelToGain (modValue (d.flevel[(size_t) f], p.level * 100.0f, e)) * chGain;
        float gl, gr; panGains (modValue (d.fpan[(size_t) f], p.pan * 100.0f, e) * 0.01f, gl, gr);
        fs.outL.setLength (rampLen); fs.outR.setLength (rampLen); fs.g.setLength (rampLen); fs.mix.setLength (rampLen);
        fs.outL.set (lv * gl); fs.outR.set (lv * gr);
        fs.mix.set (p.on ? juce::jlimit (0.0f, 1.0f, modValue (d.mix[(size_t) f], p.mix * 100.0f, e) * 0.01f) : 0.0f);
        auto cutoff = modValue (d.cutoff[(size_t) f], p.cutoff, e);
        cutoff *= semisToRatio ((pitchNow - 60.0f) * p.key);
        cutoff *= 1.0f + driftValue * drift * 0.03f;
        cutoff = juce::jlimit (16.0f, 0.45f * sr, cutoff);
        auto reso = juce::jlimit (0.0f, 1.0f, modValue (d.reso[(size_t) f], p.reso * 100.0f, e) * 0.01f);
        auto drv = juce::jlimit (0.0f, 1.0f, modValue (d.drive[(size_t) f], p.drive * 100.0f, e) * 0.01f);
        fs.drive = 1.0f + drv * drv * 9.0f;
        fs.g.set (std::tan (pi * cutoff / sr));
        fs.k = reso;
        if (p.type == 10 || p.type == 11)
        {
            fs.combDelay = juce::jlimit (2.0f, (float) combSize - 4.0f, sr / juce::jmax (30.0f, cutoff));
            fs.combFb = (p.type == 10 ? 1.0f : -1.0f) * reso * 0.97f;
        }
        if (p.type == 12)
        {
            // FORMANT: the cutoff sweeps the vowels A E I O U
            auto t = juce::jlimit (0.0f, 1.0f, std::log (cutoff / 20.0f) / std::log (1000.0f));
            static constexpr float fm[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
            auto x = t * 3.999f; auto i0 = (int) x; auto fr = x - (float) i0;
            for (int k = 0; k < 3; ++k)
            {
                auto hz = juce::jmin (0.45f * sr, fm[i0][k] + fr * (fm[i0 + 1][k] - fm[i0][k]));
                fs.formantG[(size_t) k] = std::tan (pi * hz / sr);
                for (auto& ch : fs.formant) ch[(size_t) k].setCoefs (fs.formantG[(size_t) k], 0.75f + 0.24f * reso);
            }
        }
    }

    // ---- warmth and amp
    auto warmth = modValue (d.warmth, bp.warmth * 100.0f, e) * 0.01f;
    // warmth saturates, so it only runs oversampled (ECO stays clean)
    warmthGain = e.os > 1 ? 1.0f + warmth * 1.8f : 1.0f;
    ampGain.setLength (rampLen);
    ampGain.set (env[0].level * velGain * killGain);
}

void Voice::render (SynthEngine& e, int n)
{
    auto& bp = e.bp;
    float* const* out = e.streamBuf.getArrayOfWritePointers();
    const int base = e.renderPos;
    int i = 0;
    while (i < n)
    {
        if (stepLeft <= 0) { controlStep (e, stepSize); stepLeft = stepSize; if (! active) return; }
        auto len = juce::jmin (stepLeft, n - i);

        for (int k = 0; k < len; ++k)
        {
            const int idx = base + i + k;
            float srcL[5] {}, srcR[5] {};   // SUB, A, B, C, NOISE

            // ---- wavetable / analog / sample oscillators
            for (int o = 0; o < numOscs; ++o)
            {
                auto& os = osc[(size_t) o];
                auto gL = os.gainL.next(), gR = os.gainR.next();
                auto pos = os.wtpos.next(), warp = os.warp.next();
                if (! os.on) continue;
                auto& p = bp.osc[(size_t) o];
                float L = 0, R = 0;
                if (p.type == 2)
                {
                    auto* smp = e.oscSample[(size_t) o];
                    if (smp != nullptr)
                    {
                        auto lenS = (double) smp->length();
                        auto startS = (double) pos * lenS;
                        for (int u = 0; u < os.count; ++u)
                        {
                            auto& un = os.uni[(size_t) u];
                            auto ratio = os.sampleRatio * (un.inc / juce::jmax (1.0e-9f, os.uni[0].inc));
                            auto v = smp->read (un.samplePos, ratio * e.mipScale);
                            un.samplePos += ratio;
                            if (warp > 0.01f && un.samplePos >= lenS - 2)   // loop (WARP = loop on, amount = loop start)
                                un.samplePos = startS + (lenS - startS) * (double) (1.0f - warp) * 0.5;
                            L += v * un.gain * un.panL; R += v * un.gain * un.panR;
                        }
                    }
                }
                else
                {
                    auto* wt = p.type == 1 ? e.analogTable : e.oscTable[(size_t) o];
                    if (wt == nullptr) continue;
                    auto other = osc[(size_t) (o == 0 ? 1 : 0)].last;
                    for (int u = 0; u < os.count; ++u)
                    {
                        auto& un = os.uni[(size_t) u];
                        auto ph = (float) un.phase;
                        float rp = ph, v;
                        switch (p.warpMode)
                        {
                            case 1: { auto r = 1.0f + warp * 7.0f; rp = ph * r; rp -= std::floor (rp); break; }                   // SYNC
                            case 2: rp = ph + warp * 0.15f * std::sin (twoPi * ph); break;                                        // BEND +
                            case 3: rp = ph - warp * 0.15f * std::sin (twoPi * ph); break;                                        // BEND -
                            case 4: { auto pw = 0.5f - 0.46f * warp; rp = ph < pw ? ph * 0.5f / pw : 0.5f + (ph - pw) * 0.5f / (1.0f - pw); break; }  // PWM
                            case 5: { auto m = 1.0f - std::abs (2.0f * ph - 1.0f); rp = ph + warp * (m - ph); break; }            // MIRROR
                            case 6: rp = std::pow (ph, 1.0f + 2.0f * warp); break;                                                // ASYM
                            case 7: rp = ph + warp * 0.6f * other; rp -= std::floor (rp); break;                                  // FM
                            default: break;
                        }
                        v = wt->read (pos, rp, un.level, un.w);
                        if (p.warpMode == 1 && warp > 0.0f) v *= 1.0f - warp + warp * std::sin (pi * ph);   // windowed sync: no hard edge
                        un.phase += un.inc;
                        if (un.phase >= 1.0) un.phase -= 1.0;
                        L += v * un.gain * un.panL; R += v * un.gain * un.panR;
                    }
                    if (p.warpMode == 8) { auto rm = 1.0f - warp + warp * other; L *= rm; R *= rm; }                          // RING
                    else if (p.warpMode == 9 && warp > 0.0f) { auto g = 1.0f + warp * 4.0f; L = std::sin (pi * 0.5f * g * L) ; R = std::sin (pi * 0.5f * g * R); }   // FOLD
                    else if (p.warpMode == 10 && warp > 0.0f) { auto q = std::exp2 (8.0f - 7.0f * warp); L = std::round (L * q) / q; R = std::round (R * q) / q; }   // QUANTIZE
                }
                os.last = 0.5f * (L + R);
                srcL[o + 1] = L * gL; srcR[o + 1] = R * gR;
            }

            // ---- sub
            {
                auto gL = subL.next(), gR = subR.next();
                if (bp.subOn && e.analogTable != nullptr)
                {
                    static constexpr float shapePos[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
                    auto v = e.analogTable->read (shapePos[juce::jlimit (0, 4, bp.subShape)], (float) subPhase, subLevel, subW);
                    subPhase += subInc; if (subPhase >= 1.0) subPhase -= 1.0;
                    srcL[0] = v * gL; srcR[0] = v * gR;
                }
            }
            // ---- noise
            {
                auto gL = noiseL.next(), gR = noiseR.next();
                if (bp.noiseOn)
                {
                    float v;
                    auto sr = (float) isr;
                    switch (bp.noiseType)
                    {
                        case 0: v = noise.white() * 0.6f; break;
                        case 2: v = noise.brownNoise(); break;
                        case 3:   // VINYL: soft pink bed + crackles
                        {
                            v = noise.pink() * 0.25f;
                            if (noise.uni() < 30.0f / sr) crackle = (noise.white()) * (0.4f + noise.uni() * 0.6f);
                            crackle *= 0.995f;
                            v += crackleLp.lp (crackle, 0.4f);
                            break;
                        }
                        case 4: v = noiseHp.lp (noise.white(), OnePole::coeff (1500.0f, sr)); v = (noise.white() - v) * 0.5f; v = noiseLp2.lp (v, OnePole::coeff (11000.0f, sr)); break;   // TAPE HISS
                        case 5: { auto w = noise.white(); auto lp = noiseHp.lp (w, OnePole::coeff (5000.0f, sr)); v = (w - lp) * 0.7f; break; }   // AIR
                        case 6: { if (noise.uni() < 12.0f / sr) crackle = noise.white(); crackle *= 0.97f; v = crackle; break; }   // CRACKLE
                        case 7: { if (--holdCount <= 0) { holdNoise = noise.white(); holdCount = 1 + (int) (sr / (2000.0f + 18000.0f * (bp.noiseColor * 0.5f + 0.5f))); } v = holdNoise * 0.6f; break; }   // DIGITAL
                        default: v = noise.pink(); break;
                    }
                    // COLOR: tilt (negative = darker, positive = brighter)
                    auto c = bp.noiseColor;
                    if (std::abs (c) > 0.01f && bp.noiseType != 7)
                    {
                        auto lp = noiseLp.lp (v, OnePole::coeff (c < 0 ? 200.0f + 8000.0f * (1.0f + c) : 2000.0f, (float) isr));
                        v = c < 0 ? lp : v + c * (v - lp);
                    }
                    srcL[4] = v * gL; srcR[4] = v * gR;
                }
            }

            // ---- routing
            float f1L = 0, f1R = 0, f2L = 0, f2R = 0;
            float directL[5] {}, directR[5] {};
            static constexpr int srcOrder[5] = { 0, 1, 2, 3, 4 };   // SUB A B C NOISE
            for (int s : srcOrder)
            {
                int dest = s == 0 ? bp.subDest : s == 4 ? bp.noiseDest : bp.osc[(size_t) (s - 1)].dest;
                auto l = srcL[s], r = srcR[s];
                switch (dest)
                {
                    case 0: f1L += l; f1R += r; break;
                    case 1: f2L += l; f2R += r; break;
                    case 2: f1L += l; f1R += r; f2L += l; f2R += r; break;
                    default: directL[s] = l; directR[s] = r; break;
                }
            }
            if (warmthGain > 1.02f)
            {
                auto wg = warmthGain, inv = 1.0f / wg;
                f1L = softClip (f1L * wg) * inv; f1R = softClip (f1R * wg) * inv;
                f2L = softClip (f2L * wg) * inv; f2R = softClip (f2R * wg) * inv;
            }

            auto amp = ampGain.next();
            auto runFilter = [&] (int f, float& L, float& R)
            {
                auto& fs = flt[(size_t) f];
                auto g = fs.g.next(), mix = fs.mix.next();
                auto oL = fs.outL.next(), oR = fs.outR.next();
                if (fs.on && mix > 0.0f)
                {
                    float x[2] = { L, R }, y[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        auto in = x[c];
                        float out;
                        switch (fs.type)
                        {
                            case 0: case 1: case 13:
                            {
                                fs.ladder[(size_t) c].setCoefs (g, fs.k);
                                auto drv = fs.drive * (fs.type == 13 ? 1.6f : 1.0f);
                                auto inp = fs.type == 13 ? in + 0.12f * in * in : in;
                                out = fs.ladder[(size_t) c].tick (inp, fs.type == 1, drv) * std::pow (fs.drive, 0.35f);
                                break;
                            }
                            case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
                            {
                                auto pole24 = fs.type == 2 || fs.type == 4 || fs.type == 6;
                                auto v = fs.drive > 1.01f ? softClip (in * fs.drive) / std::pow (fs.drive, 0.65f) : in;
                                float lo, bd, hi;
                                fs.svfA[(size_t) c].setCoefs (g, pole24 ? fs.k * 0.5f : fs.k);
                                fs.svfA[(size_t) c].tick (v, lo, bd, hi);
                                float r1 = fs.type <= 3 ? lo : fs.type <= 5 ? hi : fs.type <= 7 ? bd : fs.type == 8 ? lo + hi : lo - hi;
                                if (pole24)
                                {
                                    fs.svfB[(size_t) c].setCoefs (g, fs.k);
                                    fs.svfB[(size_t) c].tick (r1, lo, bd, hi);
                                    r1 = fs.type == 2 ? lo : fs.type == 4 ? hi : bd;
                                }
                                if (fs.type == 6 || fs.type == 7) r1 *= 1.0f + fs.k;   // keep band-pass level up
                                out = r1;
                                break;
                            }
                            case 10: case 11:
                            {
                                auto& buf = fs.comb[(size_t) c];
                                auto rd = (float) fs.combPos - fs.combDelay;
                                if (rd < 0) rd += (float) combSize;
                                auto i0 = (int) rd; auto fr = rd - (float) i0;
                                auto a = buf[(size_t) (i0 & (combSize - 1))], b = buf[(size_t) ((i0 + 1) & (combSize - 1))];
                                auto del = a + fr * (b - a);
                                fs.combLp[(size_t) c] += 0.6f * (del - fs.combLp[(size_t) c]);
                                auto w = softClip ((in + fs.combFb * fs.combLp[(size_t) c]) * fs.drive) / fs.drive;
                                buf[(size_t) (fs.combPos & (combSize - 1))] = w;
                                out = 0.5f * (in + del);
                                break;
                            }
                            case 12: default:
                            {
                                float acc = 0, lo, bd, hi;
                                static constexpr float fg[3] = { 1.0f, 0.7f, 0.4f };
                                auto v = fs.drive > 1.01f ? softClip (in * fs.drive) / std::pow (fs.drive, 0.65f) : in;
                                for (int k3 = 0; k3 < 3; ++k3) { fs.formant[(size_t) c][(size_t) k3].tick (v, lo, bd, hi); acc += bd * fg[k3]; }
                                out = acc * 1.6f;
                                break;
                            }
                        }
                        y[c] = in + mix * (out - in);
                    }
                    if (fs.type == 10 || fs.type == 11) fs.combPos = (fs.combPos + 1) & (combSize - 1);
                    L = y[0]; R = y[1];
                }
                L *= oL; R *= oR;
            };

            runFilter (0, f1L, f1R);
            if (bp.flt[0].dest == 0) { f2L += f1L; f2R += f1R; f1L = f1R = 0; }
            runFilter (1, f2L, f2R);

            // ---- streams (post VCA)
            for (int s = 0; s < 5; ++s)
            {
                auto ch = s == 0 ? chSub : s == 4 ? chNoise : chOscA + s - 1;
                if (directL[s] != 0.0f || directR[s] != 0.0f)
                {
                    out[ch * 2][idx] += directL[s] * amp;
                    out[ch * 2 + 1][idx] += directR[s] * amp;
                }
                auto pk = (std::abs (srcL[s]) + std::abs (srcR[s])) * 0.5f * amp;
                if (pk > e.streamPeak[(size_t) ch]) e.streamPeak[(size_t) ch] = pk;
            }
            out[chFlt1 * 2][idx] += f1L * amp; out[chFlt1 * 2 + 1][idx] += f1R * amp;
            out[chFlt2 * 2][idx] += f2L * amp; out[chFlt2 * 2 + 1][idx] += f2R * amp;
            auto pf1 = (std::abs (f1L) + std::abs (f1R)) * 0.5f * amp, pf2 = (std::abs (f2L) + std::abs (f2R)) * 0.5f * amp;
            if (pf1 > e.streamPeak[chFlt1]) e.streamPeak[chFlt1] = pf1;
            if (pf2 > e.streamPeak[chFlt2]) e.streamPeak[chFlt2] = pf2;
        }
        i += len;
        stepLeft -= len;
    }
}

// ================================================================== engine

SynthEngine::SynthEngine (juce::AudioProcessorValueTreeState& s, LfoTables& t) : lfoTables (t), state (s)
{
    tables = &library.get();
    for (auto* p : s.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            raw[r->getParameterID()] = s.getRawParameterValue (r->getParameterID());

    auto& dests = ModDefs::destinations();
    jassert (dests.size() <= 512);
    for (int i = 1; i < (int) dests.size(); ++i)
        destParams[(size_t) i] = dynamic_cast<juce::RangedAudioParameter*> (s.getParameter (dests[(size_t) i].id));

    auto D = [] (const juce::String& id) { return ModDefs::destIndex (id); };
    for (int o = 0; o < numOscs; ++o)
    {
        d.semi[(size_t) o] = D (IDs::osc (o, "semi")); d.fine[(size_t) o] = D (IDs::osc (o, "fine"));
        d.wtpos[(size_t) o] = D (IDs::osc (o, "wtpos")); d.warp[(size_t) o] = D (IDs::osc (o, "warp"));
        d.detune[(size_t) o] = D (IDs::osc (o, "detune")); d.blend[(size_t) o] = D (IDs::osc (o, "blend"));
        d.spread[(size_t) o] = D (IDs::osc (o, "spread")); d.pan[(size_t) o] = D (IDs::osc (o, "pan"));
        d.level[(size_t) o] = D (IDs::osc (o, "level"));
    }
    d.subLevel = D (IDs::subLevel); d.subPan = D (IDs::subPan);
    d.noiseColor = D (IDs::noiseColor); d.noiseLevel = D (IDs::noiseLevel); d.noisePan = D (IDs::noisePan);
    for (int f = 0; f < numFilters; ++f)
    {
        d.cutoff[(size_t) f] = D (IDs::flt (f, "cutoff")); d.reso[(size_t) f] = D (IDs::flt (f, "reso"));
        d.drive[(size_t) f] = D (IDs::flt (f, "drive")); d.mix[(size_t) f] = D (IDs::flt (f, "mix"));
        d.fpan[(size_t) f] = D (IDs::flt (f, "pan")); d.flevel[(size_t) f] = D (IDs::flt (f, "level"));
    }
    for (int e = 0; e < numEnvs; ++e)
    {
        d.att[(size_t) e] = D (IDs::env (e, "att")); d.hold[(size_t) e] = D (IDs::env (e, "hold"));
        d.dec[(size_t) e] = D (IDs::env (e, "dec")); d.sus[(size_t) e] = D (IDs::env (e, "sus")); d.rel[(size_t) e] = D (IDs::env (e, "rel"));
    }
    d.envPAmount = D (IDs::envPAmount);
    for (int l = 0; l < numLfos; ++l)
    {
        d.lfoRate[(size_t) l] = D (IDs::lfo (l, "rate")); d.lfoRise[(size_t) l] = D (IDs::lfo (l, "rise"));
        d.lfoDelay[(size_t) l] = D (IDs::lfo (l, "delay")); d.lfoSmooth[(size_t) l] = D (IDs::lfo (l, "smooth"));
    }
    d.glide = D (IDs::glide); d.drift = D (IDs::drift); d.warmth = D (IDs::warmth);
    heldNotes.reserve (128);
    arpNotes.reserve (128); arpOrder.reserve (128);
    arpGateDest = ModDefs::destIndex (IDs::arpGate);

    auto build = [this] (Family& f, int count, std::function<juce::String (int)> prefix)
    {
        for (int i = 0; i < count; ++i)
        {
            auto pre = prefix (i);
            for (auto& [id, ptr] : raw)
                if (id.startsWith (pre)) f[(size_t) i][id.substring (pre.length()).toStdString()] = ptr;
        }
    };
    build (oscF, numOscs, [] (int o) { return IDs::osc (o, ""); });
    build (fltF, numFilters, [] (int f) { return IDs::flt (f, ""); });
    build (envF, numEnvs, [] (int e) { return IDs::env (e, ""); });
    build (lfoF, numLfos, [] (int l) { return IDs::lfo (l, ""); });
    build (modF, numModSlots, [] (int m) { return IDs::mod (m, ""); });
    build (mixF, numMixerChannels, [] (int c) { return IDs::mixer (c, ""); });
    for (int m = 0; m < numMacros; ++m) macroF[(size_t) m][""] = raw[IDs::macro (m)];
    for (auto& [id, ptr] : raw) globalRaw[id.toStdString()] = ptr;
}

void SynthEngine::prepare (double sampleRate, int maxBlockSize)
{
    hostRate = sampleRate;
    maxBlock = juce::jlimit (32, 1024, maxBlockSize);
    os2 = std::make_unique<juce::dsp::Oversampling<float>> (6, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    os4 = std::make_unique<juce::dsp::Oversampling<float>> (6, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    os2->initProcessing ((size_t) maxBlock);
    os4->initProcessing ((size_t) maxBlock);
    streamBuf.setSize (numMixerChannels * 2, maxBlock * 4);
    busInternal.setSize (6, maxBlock * 4);
    busHost.setSize (6, maxBlock);
    dummy.setSize (6, maxBlock);
    dummy.clear();
    arpEvents.ensureSize (4096);
    lastOs = -1;
    analogTable = tables->get (WavetableLibrary::analogIndex());
    for (auto& v : voices) v.prepare (sampleRate * 2, 32);
    reset();
}

void SynthEngine::reset()
{
    for (auto& v : voices) { v.active = false; v.stepLeft = 0; }
    heldNotes.clear(); arpNotes.clear(); arpOrder.clear();
    sustainPedal = false;
    for (auto& k : keyDown) k.store (false);
    if (os2) os2->reset();
    if (os4) os4->reset();
    arpNextBeat = -1; arpPlaying = -1;
}

float SynthEngine::noteToPitch (float n) const noexcept
{
    auto pitch = n + bp.masterTune * 0.01f;
    if (bp.scale == 0) return pitch;
    // cents away from equal temperament, for each pitch class (relative to C)
    static constexpr float just[12] = { 0, 11.7f, 3.9f, 15.6f, -13.7f, -2.0f, -17.5f, 2.0f, 13.7f, -15.6f, -3.9f, -11.7f };
    static constexpr float pyth[12] = { 0, 13.7f, 3.9f, -5.9f, 7.8f, -2.0f, 11.7f, 2.0f, 15.6f, 5.9f, -3.9f, 9.8f };
    auto pc = ((int) std::floor (n + 0.5f) % 12 + 12) % 12;
    return pitch + (bp.scale == 1 ? just[pc] : pyth[pc]) * 0.01f;
}

void SynthEngine::readParams()
{
    for (int o = 0; o < numOscs; ++o)
    {
        auto& p = bp.osc[(size_t) o];
        p.on = fo (oscF, o, "on") > 0.5f;
        p.type = (int) fo (oscF, o, "type");
        p.table = (int) fo (oscF, o, "table");
        p.oct = (int) fo (oscF, o, "oct");
        p.semi = fo (oscF, o, "semi");
        p.fine = fo (oscF, o, "fine");
        p.phase = fo (oscF, o, "phase");
        p.rand = fo (oscF, o, "rand") * 0.01f;
        p.wtpos = fo (oscF, o, "wtpos") * 0.01f;
        p.warpMode = (int) fo (oscF, o, "warpmode");
        p.warp = fo (oscF, o, "warp") * 0.01f;
        p.unison = (int) fo (oscF, o, "unison");
        p.detune = fo (oscF, o, "detune") * 0.01f;
        p.blend = fo (oscF, o, "blend") * 0.01f;
        p.spread = fo (oscF, o, "spread") * 0.01f;
        p.pan = fo (oscF, o, "pan") * 0.01f;
        p.level = fo (oscF, o, "level") * 0.01f;
        p.dest = (int) fo (oscF, o, "dest");
    }
    bp.subOn = rg (IDs::subOn) > 0.5f; bp.subShape = (int) rg (IDs::subShape); bp.subOct = (int) rg (IDs::subOct);
    bp.subLevel = rg (IDs::subLevel) * 0.01f; bp.subPan = rg (IDs::subPan) * 0.01f; bp.subDest = (int) fo (mixF, chSub, "dest");
    bp.noiseOn = rg (IDs::noiseOn) > 0.5f; bp.noiseType = (int) rg (IDs::noiseType); bp.noiseColor = rg (IDs::noiseColor) * 0.01f;
    bp.noiseLevel = rg (IDs::noiseLevel) * 0.01f; bp.noisePan = rg (IDs::noisePan) * 0.01f; bp.noiseDest = (int) fo (mixF, chNoise, "dest");
    for (int f = 0; f < numFilters; ++f)
    {
        auto& p = bp.flt[(size_t) f];
        p.on = fo (fltF, f, "on") > 0.5f; p.type = (int) fo (fltF, f, "type");
        p.cutoff = fo (fltF, f, "cutoff"); p.reso = fo (fltF, f, "reso") * 0.01f;
        p.drive = fo (fltF, f, "drive") * 0.01f; p.mix = fo (fltF, f, "mix") * 0.01f;
        p.key = fo (fltF, f, "key") * 0.01f; p.pan = fo (fltF, f, "pan") * 0.01f;
        p.level = fo (fltF, f, "level") * 0.01f; p.dest = (int) fo (fltF, f, "dest");
    }
    for (int e = 0; e < numEnvs; ++e)
    {
        auto& p = bp.env[(size_t) e];
        p.att = fo (envF, e, "att"); p.hold = fo (envF, e, "hold"); p.dec = fo (envF, e, "dec");
        p.sus = fo (envF, e, "sus"); p.rel = fo (envF, e, "rel");
        p.ac = fo (envF, e, "acurve"); p.dc = fo (envF, e, "dcurve"); p.rc = fo (envF, e, "rcurve");
    }
    bp.envPAmount = rg (IDs::envPAmount);
    bp.ampVel = rg (IDs::ampVel) * 0.01f;
    for (int l = 0; l < numLfos; ++l)
    {
        auto& p = bp.lfo[(size_t) l];
        p.rateHz = fo (lfoF, l, "rate"); p.sync = fo (lfoF, l, "sync") > 0.5f;
        p.beats = Choices::divisionBeats ((int) fo (lfoF, l, "div")); p.mode = (int) fo (lfoF, l, "mode");
        p.rise = fo (lfoF, l, "rise"); p.delay = fo (lfoF, l, "delay"); p.smooth = fo (lfoF, l, "smooth");
        p.phase = fo (lfoF, l, "phase");
    }
    for (int m = 0; m < numMacros; ++m) bp.macro[(size_t) m] = fo (macroF, m, "") * 0.01f;
    bp.voiceMode = (int) rg (IDs::voiceMode); bp.glideMs = rg (IDs::glide); bp.glideLegato = rg (IDs::glideLegato) > 0.5f;
    bp.poly = (int) rg (IDs::poly); bp.stack = (int) rg (IDs::stack); bp.steal = (int) rg (IDs::steal); bp.priority = (int) rg (IDs::priority);
    bp.mpe = rg (IDs::mpe) > 0.5f;
    bp.masterTune = rg (IDs::masterTune); bp.transpose = (int) rg (IDs::transpose);
    bp.pbUp = (int) rg (IDs::pbUp); bp.pbDown = (int) rg (IDs::pbDown); bp.scale = (int) rg (IDs::scale);
    bp.drift = rg (IDs::drift) * 0.01f; bp.warmth = rg (IDs::warmth) * 0.01f;

    bool anySolo = false;
    for (int ch = 0; ch < numMixerChannels; ++ch) anySolo |= fo (mixF, ch, "solo") > 0.5f;
    for (int ch = 0; ch < numMixerChannels; ++ch)
    {
        auto muted = fo (mixF, ch, "mute") > 0.5f;
        auto solo = fo (mixF, ch, "solo") > 0.5f;
        // a soloed source must still be heard through the filters it feeds
        auto audible = ! muted && (! anySolo || solo || ((ch == chFlt1 || ch == chFlt2) && ! muted));
        bp.channelGain[(size_t) ch] = audible ? 1.0f : 0.0f;
    }

    // base values of every destination
    auto& dests = ModDefs::destinations();
    for (int i = 1; i < (int) dests.size(); ++i)
        if (auto* p = destParams[(size_t) i])
        {
            destBase[(size_t) i] = p->getValue();
            destBaseReal[(size_t) i] = p->convertFrom0to1 (destBase[(size_t) i]);
        }

    for (int o = 0; o < numOscs; ++o)
    {
        auto t = bp.osc[(size_t) o].table;
        if (t == WavetableLibrary::userIndex()) oscTable[(size_t) o] = userTables[(size_t) o].load();
        else
        {
            oscTable[(size_t) o] = tables->tryGet (t);
            if (oscTable[(size_t) o] == nullptr) tables->request (t);
        }
        oscSample[(size_t) o] = samples[(size_t) o].load();
    }
}

void SynthEngine::readSlots()
{
    numActiveSlots = 0;
    for (int s = 0; s < numModSlots; ++s)
    {
        ModSlot m;
        m.src = (int) fo (modF, s, "src");
        m.dst = (int) fo (modF, s, "dst");
        m.on = fo (modF, s, "on") > 0.5f;
        m.amt = fo (modF, s, "amt") * 0.01f;
        if (! m.on || m.src <= 0 || m.dst <= 0 || m.dst >= (int) ModDefs::destinations().size() || m.amt == 0.0f) continue;
        m.curve = (int) fo (modF, s, "curve");
        m.bip = fo (modF, s, "bip") > 0.5f;
        m.aux = (int) fo (modF, s, "aux");
        m.out = fo (modF, s, "out") * 0.01f;
        slots[(size_t) numActiveSlots++] = m;
    }
}

void SynthEngine::updateGlobalModulation()
{
    std::array<float, numModSources> s {};
    if (newestVoice != nullptr && newestVoice->isActive()) s = newestVoice->src;
    else
    {
        for (int l = 0; l < numLfos; ++l)
            s[(size_t) (srcLfo1 + l)] = lfoTables.kind (l) == LfoShape::points ? lfoTables.read (l, (float) (lfoFreePhase[(size_t) l] - std::floor (lfoFreePhase[(size_t) l]))) : 0.5f;
    }
    for (int m = 0; m < numMacros; ++m) s[(size_t) (srcMacro1 + m)] = bp.macro[(size_t) m];
    s[srcModWheel] = modWheel;
    s[srcPitchBend] = pitchBend;
    for (auto& g : globalOff) g = 0.0f;
    for (int i = 0; i < numActiveSlots; ++i)
    {
        auto& m = slots[(size_t) i];
        if (ModDefs::destinations()[(size_t) m.dst].perVoice) continue;
        auto v = s[(size_t) m.src];
        if (m.bip && ! ModDefs::isBipolarByNature (m.src)) v = v * 2.0f - 1.0f;
        v = curveOf (v, m.curve) * m.amt * m.out;
        if (m.aux > 0) v *= s[(size_t) m.aux];
        globalOff[(size_t) m.dst] += v;
    }
    for (int i = 0; i < numModSources; ++i) uiSource[(size_t) i].store (s[(size_t) i]);
    auto& dests = ModDefs::destinations();
    for (int i = 1; i < (int) dests.size(); ++i)
        uiOffset[(size_t) i].store (dests[(size_t) i].perVoice ? (newestVoice != nullptr && newestVoice->isActive() ? newestVoice->modOff[(size_t) i] : 0.0f)
                                                               : globalOff[(size_t) i]);
}

void SynthEngine::setSample (int o, std::unique_ptr<SampleData> s)
{
    auto* raw = s.get();
    if (s) ownedSamples.push_back (std::move (s));
    auto* old = samples[(size_t) o].exchange (raw);
    juce::ignoreUnused (old);   // stays alive in ownedSamples until collectGarbage
}

void SynthEngine::setUserTable (int o, std::unique_ptr<Wavetable> t)
{
    auto* raw = t.get();
    if (t) ownedTables.push_back (std::move (t));
    userTables[(size_t) o].store (raw);
}

void SynthEngine::collectGarbage()
{
    // keep only what is in use (plus the newest, in case the audio thread has not seen it yet)
    auto inUse = [this] (const void* p) { for (auto& s : samples) if (s.load() == p) return true; for (auto& t : userTables) if (t.load() == p) return true; return false; };
    if (ownedSamples.size() > 3)
        ownedSamples.erase (std::remove_if (ownedSamples.begin(), ownedSamples.end() - 1, [&] (auto& p) { return ! inUse (p.get()); }), ownedSamples.end() - 1);
    if (ownedTables.size() > 3)
        ownedTables.erase (std::remove_if (ownedTables.begin(), ownedTables.end() - 1, [&] (auto& p) { return ! inUse (p.get()); }), ownedTables.end() - 1);
}

void SynthEngine::ensureTables()
{
    for (int o = 0; o < numOscs; ++o)
    {
        auto t = (int) fo (oscF, o, "table");
        if (t != WavetableLibrary::userIndex()) tables->get (t);
    }
}

void SynthEngine::addUiNote (int n, bool on, float vel)
{
    int s1, n1, s2, n2;
    uiFifo.prepareToWrite (1, s1, n1, s2, n2);
    auto idx = n1 > 0 ? s1 : s2;
    if (n1 + n2 > 0) uiQueue[(size_t) idx] = { on ? 1 : 0, n & 127, juce::jlimit (1, 127, (int) (vel * 127.0f)) };
    uiFifo.finishedWrite (n1 + n2);
}

void SynthEngine::addUiController (int cc, int value)
{
    int s1, n1, s2, n2;
    uiFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 + n2 > 0) uiQueue[(size_t) (n1 > 0 ? s1 : s2)] = { 2, cc & 127, juce::jlimit (0, 127, value) };
    uiFifo.finishedWrite (n1 + n2);
}

void SynthEngine::addUiPitchBend (int value14)
{
    int s1, n1, s2, n2;
    uiFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 + n2 > 0) uiQueue[(size_t) (n1 > 0 ? s1 : s2)] = { 3, juce::jlimit (0, 16383, value14), 0 };
    uiFifo.finishedWrite (n1 + n2);
}

// ------------------------------------------------------------------ voices

Voice* SynthEngine::findFreeVoice()
{
    for (auto& v : voices) if (! v.active) return &v;
    // all slots busy: reuse the quietest one
    Voice* best = nullptr;
    for (auto& v : voices) if (best == nullptr || v.loudness() < best->loudness()) best = &v;
    return best;
}

void SynthEngine::noteOn (int n, float vel, int ch)
{
    keyDown[(size_t) (n & 127)].store (true);
    noteChannel[(size_t) (n & 127)] = ch;
    auto mono = bp.voiceMode != 0;
    auto stackCount = juce::jlimit (1, 8, bp.stack);

    if (mono)
    {
        auto wasHeld = ! heldNotes.empty();
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), n), heldNotes.end());
        heldNotes.push_back (n);
        auto target = n;
        if (bp.priority == 1) target = *std::max_element (heldNotes.begin(), heldNotes.end());
        else if (bp.priority == 2) target = *std::min_element (heldNotes.begin(), heldNotes.end());

        int found = 0;
        for (auto& v : voices)
            if (v.active && ! v.killing && v.stackCount == stackCount)
            {
                auto retrigger = bp.voiceMode == 1 || ! wasHeld || v.released;
                v.retarget (target, retrigger, *this);
                if (retrigger) v.velocity = vel;
                newestVoice = &v;
                ++found;
            }
        if (found >= stackCount) return;
        for (auto& v : voices) if (v.active && ! v.killing) v.kill();
        auto glideIn = bp.glideMs > 0.0f && (! bp.glideLegato || wasHeld);
        for (int s = 0; s < stackCount; ++s)
            if (auto* v = findFreeVoice()) { v->stepLeft = 0; v->start (*this, target, vel, ch, s, stackCount, glideIn, lastPitch); newestVoice = v; }
        return;
    }

    // poly: keep the number of sounding notes under the polyphony
    int sounding = 0;
    for (auto& v : voices) if (v.active && ! v.killing) ++sounding;
    auto limit = juce::jmax (1, bp.poly) * stackCount;
    while (sounding + stackCount > limit)
    {
        if (bp.steal == 2) return;   // NONE: ignore the new note
        Voice* victim = nullptr;
        for (auto& v : voices)
            if (v.active && ! v.killing && (victim == nullptr || (v.released && ! victim->released)
                                            || (v.released == victim->released && v.age < victim->age)))
                victim = &v;
        if (victim == nullptr) break;
        // steal the whole stack of that note
        auto vn = victim->note; auto va = victim->age;
        for (auto& v : voices)
            if (v.active && ! v.killing && v.note == vn && std::abs ((int) v.age - (int) va) < stackCount)
            {
                v.kill();
                if (bp.steal == 1) v.killStep *= 0.5f;   // SMOOTH: 10 ms
                --sounding;
            }
    }
    auto glideIn = bp.glideMs > 0.0f && (! bp.glideLegato || ! heldNotes.empty());
    heldNotes.push_back (n);
    for (int s = 0; s < stackCount; ++s)
        if (auto* v = findFreeVoice())
        {
            if (v->active) v->kill();   // emergency reuse
            v->stepLeft = 0;
            v->start (*this, n, vel, ch, s, stackCount, glideIn, lastPitch);
            newestVoice = v;
        }
}

void SynthEngine::noteOff (int n, int ch)
{
    keyDown[(size_t) (n & 127)].store (false);
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), n), heldNotes.end());
    if (bp.voiceMode != 0)
    {
        if (! heldNotes.empty())
        {
            auto target = heldNotes.back();
            if (bp.priority == 1) target = *std::max_element (heldNotes.begin(), heldNotes.end());
            else if (bp.priority == 2) target = *std::min_element (heldNotes.begin(), heldNotes.end());
            for (auto& v : voices) if (v.active && ! v.killing && ! v.released) v.retarget (target, false, *this);
            return;
        }
        for (auto& v : voices)
            if (v.active && ! v.killing) { if (sustainPedal) v.sustained = true; else v.release(); }
        return;
    }
    for (auto& v : voices)
        if (v.active && ! v.killing && ! v.released && v.note == n && (! bp.mpe || v.channel == ch))
        {
            if (sustainPedal) v.sustained = true;
            else v.release();
        }
}

void SynthEngine::handleMidi (const juce::MidiMessage& m)
{
    auto ch = m.getChannel();
    if (m.isNoteOn())
    {
        noteOn (m.getNoteNumber(), m.getFloatVelocity(), ch);
        if (bp.mpe)
            for (auto& v : voices)
                if (v.active && v.channel == ch && v.note == m.getNoteNumber())
                { v.bend = channelBend[(size_t) ((ch - 1) & 15)]; v.slide = channelSlide[(size_t) ((ch - 1) & 15)]; }
    }
    else if (m.isNoteOff()) noteOff (m.getNoteNumber(), ch);
    else if (m.isSustainPedalOn()) sustainPedal = true;
    else if (m.isSustainPedalOff())
    {
        sustainPedal = false;
        for (auto& v : voices) if (v.active && v.sustained) { v.sustained = false; v.release(); }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices) if (v.active) { if (m.isAllSoundOff()) v.kill(); else v.release(); }
        heldNotes.clear(); arpNotes.clear();
    }
    else if (m.isPitchWheel())
    {
        auto b = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        if (bp.mpe && ch > 1)
        {
            channelBend[(size_t) ((ch - 1) & 15)] = b;
            for (auto& v : voices) if (v.active && v.channel == ch) v.bend = b;
        }
        else pitchBend = b;
    }
    else if (m.isController())
    {
        auto v = (float) m.getControllerValue() / 127.0f;
        if (m.getControllerNumber() == 1) modWheel = v;
        else if (m.getControllerNumber() == 74)
        {
            channelSlide[(size_t) ((ch - 1) & 15)] = v;
            for (auto& vo : voices) if (vo.active && (! bp.mpe || vo.channel == ch)) vo.slide = v;
        }
    }
    else if (m.isChannelPressure())
    {
        auto v = (float) m.getChannelPressureValue() / 127.0f;
        if (bp.mpe && ch > 1) { for (auto& vo : voices) if (vo.active && vo.channel == ch) vo.aftertouch = v; }
        else channelPressure = v;
    }
    else if (m.isAftertouch())
    {
        auto v = (float) m.getAfterTouchValue() / 127.0f;
        for (auto& vo : voices) if (vo.active && vo.note == m.getNoteNumber()) vo.aftertouch = v;
    }
}

void SynthEngine::renderVoices (int from, int to)
{
    if (to <= from) return;
    renderPos = from * os;
    auto n = (to - from) * os;
    for (auto& v : voices)
        if (v.active) v.render (*this, n);
}

// ------------------------------------------------------------------ arpeggiator

namespace
{
    /** 16-step patterns: velocity (0 = rest), octave offset, ratchet (1 or 2). */
    struct ArpStep { float vel; int oct; int ratchet; };
    const std::array<ArpStep, 16>& arpPattern (int p)
    {
        static const std::array<std::array<ArpStep, 16>, 12> pats = { {
            { { {1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1} } },   // STRAIGHT
            { { {1,0,1},{.7f,0,1},{.8f,0,2},{.7f,0,1},{1,0,1},{.6f,0,2},{.8f,1,1},{.6f,0,1},{1,0,1},{.7f,0,1},{.8f,0,2},{.7f,0,1},{1,0,2},{.6f,0,2},{.9f,1,2},{.7f,0,2} } },   // TRAP ROLL
            { { {1,0,1},{0,0,1},{.8f,0,1},{.7f,0,1},{0,0,1},{.8f,1,1},{1,0,1},{0,0,1},{.8f,0,1},{.7f,0,1},{0,0,1},{.8f,1,1},{1,0,1},{.6f,0,1},{.8f,0,1},{.7f,1,1} } },   // TRIPLET FLOW
            { { {1,0,1},{0,0,1},{.7f,1,1},{0,0,1},{.9f,0,1},{.6f,0,1},{.8f,1,1},{0,0,1},{1,0,1},{0,0,1},{.7f,1,1},{0,0,1},{.9f,0,1},{.6f,1,1},{.8f,0,1},{.5f,1,1} } },   // BOUNCE
            { { {1,0,1},{0,0,1},{0,0,1},{.8f,0,1},{0,0,1},{0,0,1},{.9f,0,1},{0,0,1},{1,0,1},{0,0,1},{0,0,1},{.8f,0,1},{0,0,1},{0,0,1},{.9f,0,1},{0,0,1} } },   // DEMBOW (3-3-2)
            { { {1,0,1},{.7f,0,1},{0,0,1},{.8f,0,1},{1,0,1},{0,0,1},{.7f,0,1},{.8f,0,1},{1,0,1},{.7f,0,1},{0,0,1},{.8f,1,1},{1,0,1},{0,0,1},{.7f,0,1},{.6f,1,1} } },   // SKIP
            { { {1,0,1},{.8f,0,1},{.8f,0,1},{.8f,0,1},{1,1,1},{.8f,1,1},{.8f,1,1},{.8f,1,1},{1,0,1},{.8f,0,1},{.8f,0,1},{.8f,0,1},{1,1,1},{.8f,1,1},{.8f,1,1},{.8f,1,1} } },   // STAIRS
            { { {1,0,2},{.7f,0,1},{.8f,0,1},{.7f,0,2},{1,0,1},{.7f,0,2},{.8f,0,1},{.7f,0,1},{1,0,2},{.7f,0,1},{.8f,0,2},{.7f,0,1},{1,0,2},{.7f,0,2},{.8f,0,2},{.7f,0,2} } },   // RATCHET
            { { {1,0,1},{.8f,1,1},{.9f,0,1},{.8f,1,1},{1,0,1},{.8f,1,1},{.9f,0,1},{.8f,1,1},{1,0,1},{.8f,1,1},{.9f,0,1},{.8f,1,1},{1,0,1},{.8f,1,1},{.9f,0,1},{.8f,1,1} } },   // OCT JUMP
            { { {1,0,1},{0,0,1},{0,0,1},{.8f,0,1},{0,0,1},{0,0,1},{.9f,1,1},{0,0,1},{0,0,1},{.8f,0,1},{0,0,1},{.7f,0,2},{1,0,1},{0,0,1},{.8f,1,1},{0,0,1} } },   // DRILL
            { { {1,0,1},{0,0,1},{.7f,0,1},{0,0,1},{.8f,1,1},{0,0,1},{.7f,0,1},{.6f,0,1},{1,0,1},{0,0,1},{.7f,0,1},{0,0,1},{.8f,1,1},{.6f,0,1},{0,0,1},{.7f,0,1} } },   // PLUGG
            { { {1,0,1},{0,0,1},{.7f,0,1},{.5f,0,1},{.9f,0,1},{0,0,1},{.6f,1,1},{0,0,1},{1,0,1},{0,0,1},{.7f,0,1},{.5f,0,1},{.8f,0,1},{0,0,1},{.6f,1,1},{.4f,0,1} } },   // LO-FI SWING
        } };
        return pats[(size_t) juce::jlimit (0, 11, p)];
    }
}

void SynthEngine::runArp (int numSamples, juce::MidiBuffer& out)
{
    auto rate = Choices::divisionBeats ((int) rg (IDs::arpRate));
    auto octs = juce::jlimit (1, 4, (int) rg (IDs::arpOct));
    auto gate = juce::jlimit (0.05f, 1.0f, (rg (IDs::arpGate) + globalOff[(size_t) arpGateDest] * 95.0f) * 0.01f);
    auto swing = rg (IDs::arpSwing) * 0.01f;
    auto mode = (int) rg (IDs::arpMode);
    auto pat = arpPattern ((int) rg (IDs::arpPattern));
    auto beatsPerSample = bpm / 60.0 / hostRate;
    auto startBeat = playing ? ppq : freeBeats;
    auto endBeat = startBeat + beatsPerSample * numSamples;

    if (arpNotes.empty())
    {
        if (arpPlaying >= 0) { out.addEvent (juce::MidiMessage::noteOff (1, arpPlaying), 0); arpPlaying = -1; }
        arpNextBeat = -1;
        return;
    }
    if (arpNextBeat < 0 || arpNextBeat > endBeat + 4 * rate || arpNextBeat < startBeat - 4 * rate)
    {
        arpNextBeat = std::ceil (startBeat / rate - 1.0e-6) * rate;   // stay on the grid
        arpStep = 0;
    }

    // the ordered list of notes to walk through
    arpOrder = arpNotes;
    if (mode != 5) std::sort (arpOrder.begin(), arpOrder.end());
    auto count = (int) arpOrder.size() * octs;

    auto beatToSample = [&] (double b) { return juce::jlimit (0, numSamples - 1, (int) std::floor ((b - startBeat) / beatsPerSample)); };
    if (arpPlaying >= 0 && arpOffAt >= startBeat && arpOffAt < endBeat)
    {
        out.addEvent (juce::MidiMessage::noteOff (1, arpPlaying), beatToSample (arpOffAt));
        arpPlaying = -1;
    }

    int guard = 0;
    while (arpNextBeat < endBeat && guard++ < 64)
    {
        auto stepInPattern = arpStep % 16;
        auto& st = pat[(size_t) stepInPattern];
        auto swingOffset = (arpStep % 2 == 1) ? swing * 0.5 * rate : 0.0;
        auto at = arpNextBeat + swingOffset;
        if (at >= endBeat) break;
        if (st.vel > 0.0f)
        {
            int noteIdx;
            switch (mode)
            {
                case 1: noteIdx = count - 1 - (arpStep % count); break;
                case 2: { auto period = juce::jmax (1, 2 * count - 2); auto k = arpStep % period; noteIdx = k < count ? k : period - k; break; }
                case 3: { auto period = juce::jmax (1, 2 * count - 2); auto k = arpStep % period; noteIdx = count - 1 - (k < count ? k : period - k); break; }
                case 4: noteIdx = random.nextInt (count); break;
                default: noteIdx = arpStep % count; break;
            }
            auto ratchets = st.ratchet;
            for (int r = 0; r < ratchets; ++r)
            {
                auto t0 = at + r * rate / ratchets;
                if (t0 >= endBeat) break;
                auto smp = beatToSample (t0);
                if (arpPlaying >= 0) out.addEvent (juce::MidiMessage::noteOff (1, arpPlaying), smp);
                if (mode == 6)
                {
                    for (auto nn : arpOrder) out.addEvent (juce::MidiMessage::noteOn (1, juce::jlimit (0, 127, nn + 12 * st.oct), st.vel), smp);
                    arpPlaying = -1;
                }
                else
                {
                    auto nn = juce::jlimit (0, 127, arpOrder[(size_t) (noteIdx % (int) arpOrder.size())] + 12 * (noteIdx / (int) arpOrder.size()) + 12 * st.oct);
                    out.addEvent (juce::MidiMessage::noteOn (1, nn, st.vel), smp);
                    arpPlaying = nn;
                }
                arpOffAt = t0 + gate * rate / ratchets;
                if (arpOffAt < endBeat && arpPlaying >= 0)
                {
                    out.addEvent (juce::MidiMessage::noteOff (1, arpPlaying), juce::jmax (smp, beatToSample (arpOffAt)));
                    arpPlaying = -1;
                }
                if (mode == 6)
                    for (auto nn : arpOrder) out.addEvent (juce::MidiMessage::noteOff (1, juce::jlimit (0, 127, nn + 12 * st.oct)), juce::jmin (numSamples - 1, juce::jmax (smp + 1, beatToSample (t0 + gate * rate / ratchets))));
            }
        }
        arpNextBeat += rate;
        ++arpStep;
    }
}

// ------------------------------------------------------------------ process

void SynthEngine::process (juce::MidiBuffer& midi, int numSamples, const juce::AudioPlayHead::PositionInfo* pos,
                           std::array<juce::AudioBuffer<float>, numBuses>& buses)
{
    // transport
    playing = false;
    if (pos != nullptr)
    {
        if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 400.0, *b);
        if (auto p = pos->getPpqPosition()) ppq = *p;
        playing = pos->getIsPlaying();
    }

    readParams();
    readSlots();

    // oversampling factor
    auto quality = (int) rg (IDs::quality);
    auto newOs = quality == 0 ? 1 : ((int) rg (IDs::oversampling) == 1 ? 4 : 2);
    if (hostRate >= 88200.0) newOs = juce::jmax (1, newOs / 2);
    if (newOs != lastOs)
    {
        os = newOs; lastOs = newOs;
        mipScale = (float) os;
        internalRate = hostRate * os;
        auto step = (quality == 2 ? 8 : 16) * os;
        for (auto& v : voices) { v.isr = internalRate; v.stepSize = step; }
        if (os2) os2->reset();
        if (os4) os4->reset();
    }

    // UI keyboard
    {
        int s1, n1, s2, n2;
        uiFifo.prepareToRead (uiFifo.getNumReady(), s1, n1, s2, n2);
        auto add = [&] (int i)
        {
            auto& q = uiQueue[(size_t) i];
            switch (q[0])
            {
                case 1:  midi.addEvent (juce::MidiMessage::noteOn (1, q[1], (juce::uint8) q[2]), 0); break;
                case 2:  midi.addEvent (juce::MidiMessage::controllerEvent (1, q[1], q[2]), 0); break;
                case 3:  midi.addEvent (juce::MidiMessage::pitchWheel (1, q[1]), 0); break;
                default: midi.addEvent (juce::MidiMessage::noteOff (1, q[1]), 0); break;
            }
        };
        for (int i = 0; i < n1; ++i) add (s1 + i);
        for (int i = 0; i < n2; ++i) add (s2 + i);
        uiFifo.finishedRead (n1 + n2);
    }

    for (auto& b : buses) b.clear (0, numSamples);

    int done = 0;
    auto midiIt = midi.cbegin();
    while (done < numSamples)
    {
        auto n = juce::jmin (maxBlock, numSamples - done);
        streamBuf.clear (0, n * os);
        streamPeak.fill (0.0f);

        // events of this chunk (arp transforms note events)
        arpEvents.clear();
        auto arpOn = rg (IDs::arpOn) > 0.5f;
        if (arpWasOn && ! arpOn)
        {
            if (arpPlaying >= 0) arpEvents.addEvent (juce::MidiMessage::noteOff (1, arpPlaying), 0);
            arpPlaying = -1;
            for (auto& v : voices) if (v.active) v.release();
            arpNotes.clear();
        }
        arpWasOn = arpOn;
        auto latch = rg (IDs::arpLatch) > 0.5f;
        for (; midiIt != midi.cend(); ++midiIt)
        {
            auto meta = *midiIt;
            if (meta.samplePosition >= done + n) break;
            auto m = meta.getMessage();
            auto at = meta.samplePosition - done;
            if (arpOn && (m.isNoteOn() || m.isNoteOff()))
            {
                auto nn = m.getNoteNumber();
                if (m.isNoteOn())
                {
                    if (latch && std::none_of (keyDown.begin(), keyDown.end(), [] (auto& k) { return k.load(); })) arpNotes.clear();
                    if (std::find (arpNotes.begin(), arpNotes.end(), nn) == arpNotes.end()) arpNotes.push_back (nn);
                    keyDown[(size_t) nn].store (true);
                }
                else
                {
                    keyDown[(size_t) nn].store (false);
                    if (! latch) arpNotes.erase (std::remove (arpNotes.begin(), arpNotes.end(), nn), arpNotes.end());
                }
                continue;
            }
            arpEvents.addEvent (m, juce::jmax (0, at));
        }
        if (arpOn) runArp (n, arpEvents);

        int pos0 = 0;
        for (auto meta : arpEvents)
        {
            auto at = juce::jlimit (0, n, meta.samplePosition);
            renderVoices (pos0, at);
            pos0 = at;
            handleMidi (meta.getMessage());
        }
        renderVoices (pos0, n);

        // advance the free-running clocks
        for (int l = 0; l < numLfos; ++l)
        {
            auto& p = bp.lfo[(size_t) l];
            auto inc = p.sync ? (bpm / 60.0) / juce::jmax (1.0e-3, p.beats) / internalRate : p.rateHz / internalRate;
            lfoFreePhase[(size_t) l] = std::fmod (lfoFreePhase[(size_t) l] + inc * n * os, 1024.0);
        }
        if (! playing) freeBeats += bpm / 60.0 / hostRate * n;
        else { freeBeats = ppq; ppq += bpm / 60.0 / hostRate * n; }

        // ---- mixer: streams -> buses at the internal rate
        busInternal.clear (0, n * os);
        auto ni = n * os;
        for (int ch = 0; ch < numMixerChannels; ++ch)
        {
            int dest;
            if (ch == chFlt1)      { auto dd = (int) fo (mixF, ch, "dest"); dest = dd == 0 ? -1 : dd - 1; }
            else if (ch == chFlt2) dest = (int) fo (mixF, ch, "dest");
            else                   { auto dd = (int) fo (mixF, ch, "dest"); dest = dd >= 3 ? dd - 3 : -1; }
            if (dest < 0) continue;
            auto s1 = fo (mixF, ch, "send1") * 0.01f, s2 = fo (mixF, ch, "send2") * 0.01f;
            for (int c = 0; c < 2; ++c)
            {
                auto* src = streamBuf.getReadPointer (ch * 2 + c);
                busInternal.addFrom (dest * 2 + c, 0, src, ni);
                if (s1 > 0.0f && dest != 1) busInternal.addFrom (2 + c, 0, src, ni, s1 * s1);
                if (s2 > 0.0f && dest != 2) busInternal.addFrom (4 + c, 0, src, ni, s2 * s2);
            }
        }
        for (int ch = 0; ch < numMixerChannels; ++ch)
        {
            auto prev = channelLevel[(size_t) ch].load();
            channelLevel[(size_t) ch].store (juce::jmax (streamPeak[(size_t) ch], prev * 0.9f));
        }

        // ---- back to the host rate
        if (os == 1)
        {
            for (int c = 0; c < 6; ++c) buses[(size_t) (c / 2)].copyFrom (c % 2, done, busInternal, c, 0, n);
        }
        else
        {
            auto& over = os == 2 ? *os2 : *os4;
            juce::dsp::AudioBlock<float> in (dummy.getArrayOfWritePointers(), 6, (size_t) n);
            auto up = over.processSamplesUp (in);
            for (int c = 0; c < 6; ++c)
                juce::FloatVectorOperations::copy (up.getChannelPointer ((size_t) c), busInternal.getReadPointer (c), ni);
            juce::dsp::AudioBlock<float> outBlock (busHost.getArrayOfWritePointers(), 6, (size_t) n);
            over.processSamplesDown (outBlock);
            for (int c = 0; c < 6; ++c) buses[(size_t) (c / 2)].copyFrom (c % 2, done, busHost, c, 0, n);
        }
        done += n;
    }

    // UI state
    int count = 0;
    for (auto& v : voices) if (v.active) ++count;
    activeVoices.store (count);
    if (newestVoice != nullptr && ! newestVoice->active) newestVoice = nullptr;
    if (newestVoice == nullptr)
        for (auto& v : voices) if (v.active && (newestVoice == nullptr || v.age > newestVoice->age)) newestVoice = &v;
    updateGlobalModulation();
    if (newestVoice != nullptr)
    {
        for (int l = 0; l < numLfos; ++l)
            uiLfoPhase[(size_t) l].store ((float) (newestVoice->lfo[(size_t) l].phase - std::floor (newestVoice->lfo[(size_t) l].phase)));
        for (int e = 0; e < numEnvs; ++e)
        {
            auto& en = newestVoice->env[(size_t) e];
            uiEnvPos[(size_t) e].store ((float) en.stage + en.t);
        }
    }
    else
    {
        for (int l = 0; l < numLfos; ++l) uiLfoPhase[(size_t) l].store ((float) (lfoFreePhase[(size_t) l] - std::floor (lfoFreePhase[(size_t) l])));
        for (auto& e : uiEnvPos) e.store (0.0f);
    }
}

} // namespace quasar
