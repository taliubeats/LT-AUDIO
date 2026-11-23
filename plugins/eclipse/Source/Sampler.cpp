#include "Sampler.h"
#include "Params.h"

namespace eclipse
{

void Sampler::prepare (double sampleRate, int block)
{
    sr = sampleRate;
    maxBlock = juce::jmax (16, block);
    scratch.setSize (numPads * 2, maxBlock);
    for (auto& b : buses) b.vibe.prepare (sr);
    reset();
}

void Sampler::reset()
{
    for (auto& v : voices) v = {};
    for (auto& b : buses) { b.hp = {}; b.lp = {}; b.vibe.reset(); b.tail = 0; }
}

void Sampler::stopAll()
{
    for (auto& v : voices)
        if (v.active) { v.releaseStep = 1.0f / (0.01f * (float) sr); }
}

float Sampler::Svf::process (int c, float v0, float g, float k, bool high)
{
    // Cytomic / Simper TPT state-variable filter
    const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    const float v3 = v0 - ic2[c];
    const float v1 = a1 * ic1[c] + a2 * v3;
    const float v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
    ic1[c] = 2 * v1 - ic1[c];
    ic2[c] = 2 * v2 - ic2[c];
    return high ? v0 - k * v1 - v2 : v2;
}

void Sampler::trigger (int pad, float velocity, const Kit::Snapshot& snap, const std::array<PadSettings, numPads>& set, int offset)
{
    if (! juce::isPositiveAndBelow (pad, numPads)) return;
    auto raw = snap.raw[(size_t) pad];
    if (raw == nullptr || raw->getNumSamples() < 4) return;
    const auto& s = set[(size_t) pad];

    // choke: this pad stops the others of its group
    if (s.choke > 0)
        for (auto& v : voices)
            if (v.active && v.pad != pad && set[(size_t) v.pad].choke == s.choke && v.releaseStep == 0)
                v.releaseStep = 1.0f / (0.006f * (float) sr);

    // a free voice, or steal the oldest
    Voice* use = nullptr;
    for (auto& v : voices) if (! v.active) { use = &v; break; }
    if (use == nullptr)
    {
        use = &voices[0];
        for (auto& v : voices) if (v.age > use->age) use = &v;
    }
    auto& v = *use;
    v = {};
    v.active = true;
    v.pad = pad;
    v.raw = raw;
    v.clean = snap.clean[(size_t) pad] != nullptr ? snap.clean[(size_t) pad] : raw;
    const double len = raw->getNumSamples();
    const float st = juce::jlimit (0.0f, 0.99f, s.start), en = juce::jlimit (st + 0.005f, 1.0f, s.end);
    v.start = st * len; v.pos = v.start; v.end = en * len;
    v.inc = snap.rate[(size_t) pad] / sr * std::pow (2.0, s.tune / 12.0);
    v.gain = std::pow (juce::jlimit (0.0f, 1.0f, velocity), 1.2f);
    v.cleanMix = juce::jlimit (0.0f, 1.0f, s.clean);
    v.fade = juce::jlimit (0.0f, 1.0f, s.fade);
    v.attack = (int) (s.attackMs * 0.001 * sr);
    v.startOffset = offset;
    hits[(size_t) pad].fetch_add (1);
}

void Sampler::renderVoice (Voice& v, float* l, float* r, int from, int to)
{
    const auto& A = *v.raw;
    const auto& B = *v.clean;
    const int n = A.getNumSamples();
    const bool stereoA = A.getNumChannels() > 1, sameLen = B.getNumSamples() == n;
    const float cm = sameLen ? v.cleanMix : 0.0f;
    const double span = juce::jmax (1.0, v.end - v.start);
    auto read = [&] (const juce::AudioBuffer<float>& buf, int ch, double p)
    {
        const int i = (int) p;
        const float t = (float) (p - i);
        auto at = [&] (int k) { return k >= 0 && k < n ? buf.getSample (ch, k) : 0.0f; };
        const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    };
    for (int i = from; i < to; ++i)
    {
        if (v.pos >= v.end) { v.active = false; break; }
        float env = v.gain;
        if (v.attack > 0 && v.age < v.attack) env *= (float) v.age / (float) v.attack;
        if (v.fade > 0)
        {   // the last "fade" part of the sound fades out with a smooth curve
            const double rel = (v.pos - v.start) / span, f0 = 1.0 - v.fade;
            if (rel > f0) { const float t = (float) ((rel - f0) / v.fade); env *= 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * juce::jmin (1.0f, t)); }
        }
        if (v.releaseStep > 0)
        {
            v.releaseGain -= v.releaseStep;
            if (v.releaseGain <= 0) { v.active = false; break; }
            env *= v.releaseGain;
        }
        float a0 = read (A, 0, v.pos), a1 = stereoA ? read (A, 1, v.pos) : a0;
        if (cm > 0)
        {
            const float b0 = read (B, 0, v.pos), b1 = B.getNumChannels() > 1 ? read (B, 1, v.pos) : b0;
            a0 += (b0 - a0) * cm; a1 += (b1 - a1) * cm;
        }
        l[i] += a0 * env;
        r[i] += a1 * env;
        v.pos += v.inc;
        ++v.age;
    }
}

void Sampler::process (std::array<float*, 2>* outs, int numOuts, int n, const juce::MidiBuffer& midi,
                       const Kit::Snapshot& snap, const std::array<PadSettings, numPads>& set, Vibe vibeType)
{
    for (int o = 0; o < numOuts; ++o)
        for (auto* p : outs[o]) if (p != nullptr) juce::FloatVectorOperations::clear (p, n);
    if (n > maxBlock) prepare (sr, n);

    bool anySolo = false;
    for (auto& s : set) anySolo |= s.solo;

    // render in slices between MIDI events
    std::fill (busActive.begin(), busActive.end(), false);
    for (auto& v : voices) if (v.active) busActive[(size_t) v.pad] = true;
    for (int p = 0; p < numPads; ++p) if (buses[(size_t) p].tail > 0) busActive[(size_t) p] = true;
    for (int p = 0; p < numPads; ++p)
    {
        juce::FloatVectorOperations::clear (scratch.getWritePointer (p * 2), n);
        juce::FloatVectorOperations::clear (scratch.getWritePointer (p * 2 + 1), n);
    }

    int pos = 0;
    auto renderTo = [&] (int to)
    {
        for (auto& v : voices)
            if (v.active && to > pos)
            {
                busActive[(size_t) v.pad] = true;
                renderVoice (v, scratch.getWritePointer (v.pad * 2), scratch.getWritePointer (v.pad * 2 + 1), pos, to);
            }
        pos = to;
    };
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        const int at = juce::jlimit (0, n, m.samplePosition);
        renderTo (at);
        if (msg.isNoteOn())
        {
            const int pad = msg.getNoteNumber() - firstNote;
            trigger (pad, msg.getFloatVelocity(), snap, set, at);
            if (juce::isPositiveAndBelow (pad, numPads)) busActive[(size_t) pad] = true;
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) stopAll();
    }
    renderTo (n);

    // pad buses
    const int tailLen = (int) (0.25 * sr);
    for (int p = 0; p < numPads; ++p)
    {
        if (! busActive[(size_t) p]) continue;
        auto& bus = buses[(size_t) p];
        const auto& s = set[(size_t) p];
        float* ch[2] = { scratch.getWritePointer (p * 2), scratch.getWritePointer (p * 2 + 1) };

        bool voicesLeft = false;
        for (auto& v : voices) voicesLeft |= v.active && v.pad == p;
        bus.tail = voicesLeft ? tailLen : juce::jmax (0, bus.tail - n);

        if (s.hp > 20.5f || s.lp < 19900.0f)
        {
            const float k = 1.41421356f;   // Butterworth Q
            const float gh = std::tan (juce::MathConstants<float>::pi * juce::jmin (s.hp, (float) sr * 0.45f) / (float) sr);
            const float gl = std::tan (juce::MathConstants<float>::pi * juce::jmin (s.lp, (float) sr * 0.45f) / (float) sr);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    float x = ch[c][i];
                    if (s.hp > 20.5f) x = bus.hp.process (c, x, gh, k, true);
                    if (s.lp < 19900.0f) x = bus.lp.process (c, x, gl, k, false);
                    ch[c][i] = x;
                }
        }
        bus.vibe.process (ch, 2, n, vibeType, s.vibe);

        const bool silent = s.mute || (anySolo && ! s.solo);
        if (silent) continue;
        const float g = juce::Decibels::decibelsToGain (s.gainDb);
        const float pan = juce::jlimit (-1.0f, 1.0f, s.pan);
        const float gl = g * std::cos ((pan + 1) * juce::MathConstants<float>::pi * 0.25f) * 1.41421356f;
        const float gr = g * std::sin ((pan + 1) * juce::MathConstants<float>::pi * 0.25f) * 1.41421356f;
        auto add = [&] (int o)
        {
            if (o >= numOuts) return;
            if (outs[o][0] != nullptr) juce::FloatVectorOperations::addWithMultiply (outs[o][0], ch[0], gl, n);
            if (outs[o][1] != nullptr) juce::FloatVectorOperations::addWithMultiply (outs[o][1], ch[1], gr, n);
        };
        if (s.out != Outs::familyOnly) add (0);
        if (s.out != 0 && s.family != Family::sample) add (1 + (int) s.family);
        else if (s.out == Outs::familyOnly && s.family == Family::sample) add (0);
    }
}

} // namespace eclipse
