#include "Piano.h"

namespace lyra
{

void Piano::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    scratch.setSize (2, juce::jmax (1, maxBlock));
    reverb.setSampleRate (sr);
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.55f; rp.damping = 0.6f; rp.wetLevel = 0.14f; rp.dryLevel = 1.0f; rp.width = 0.9f;
    reverb.setParameters (rp);
    reset();
}

void Piano::reset()
{
    for (auto& v : voices) v = Voice();
    reverb.reset();
}

void Piano::allOff()
{
    for (auto& v : voices) if (v.note >= 0) { v.held = false; v.releasing = true; v.dampRate = 1.0f / (0.05f * (float) sr); }
}

void Piano::setPedal (bool down)
{
    pedal = down;
    if (! down)
        for (auto& v : voices)
            if (v.note >= 0 && ! v.held) { v.releasing = true; v.dampRate = 1.0f / (0.14f * (float) sr); }
}

void Piano::noteOff (int note)
{
    for (auto& v : voices)
        if (v.note == note && v.held)
        {
            v.held = false;
            if (! pedal) { v.releasing = true; v.dampRate = 1.0f / (0.14f * (float) sr); }
        }
}

void Piano::noteOn (int note, float velocity)
{
    // re-striking a key reuses its voice; otherwise a free one, else the quietest
    Voice* pick = nullptr;
    for (auto& v : voices) if (v.note == note) { pick = &v; break; }
    if (pick == nullptr) for (auto& v : voices) if (v.note < 0) { pick = &v; break; }
    if (pick == nullptr)
    {
        pick = &voices[0];
        for (auto& v : voices) if (v.level * v.damp < pick->level * pick->damp) pick = &v;
    }
    start (*pick, note, juce::jlimit (0.0f, 1.0f, velocity));
}

void Piano::start (Voice& v, int note, float vel)
{
    v = Voice();
    v.note = note;
    v.held = true;
    const double f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
    const double B = 0.00012 * std::pow (2.0, (note - 48) / 24.0);          // stiffer strings up high
    const double baseT = 7.5 * std::pow (261.6 / f0, 0.55);                   // aftersound length (s)
    const float bright = 0.25f + 0.75f * vel;                                // harder = more upper partials
    const int strings = note >= 45 ? 2 : 1;
    const float detune = strings > 1 ? 0.7f : 0.0f;                           // cents between the strings
    float norm = 0;
    v.count = 0;
    for (int s = 0; s < strings; ++s)
        for (int k = 1; k <= numPartials; ++k)
        {
            auto fk = f0 * k * std::sqrt (1.0 + B * k * k) * std::pow (2.0, (s == 0 ? -detune : detune) * 0.5 / 1200.0);
            if (fk > sr * 0.45) break;
            auto& p = v.p[(size_t) v.count++];
            auto w = juce::MathConstants<double>::twoPi * fk / sr;
            p.cosw = (float) std::cos (w); p.sinw = (float) std::sin (w);
            p.re = 1; p.im = 0;                                             // im = sin, starts at 0: no click
            // spectrum: 1/k, rounded off by the hammer (softer blows lose the top first)
            p.amp = (float) (1.0 / std::pow (k, 1.05) * std::exp (-(k - 1) * (1.15 - bright) * 0.45)) / (float) strings;
            // the hammer position (about 1/8 of the string) notches the 8th partial
            if (k % 8 == 0) p.amp *= 0.25f;
            auto tSlow = baseT / (1.0 + 0.22 * (k - 1)), tFast = juce::jmax (0.08, tSlow * 0.12);
            p.slow = (float) std::exp (-6.9 / (tSlow * sr));
            p.fast = (float) std::exp (-6.9 / (tFast * sr));
            p.ef = 0.55f; p.es = 0.45f;   // prompt sound + aftersound
            norm += p.amp;
        }
    for (int i = 0; i < v.count; ++i) v.p[(size_t) i].amp /= juce::jmax (1.0f, norm * 0.7f);
    v.level = 0.25f + 0.75f * vel * vel;
    v.noise = 0.35f * vel * vel;
    v.noiseCoef = (float) std::exp (-juce::MathConstants<double>::twoPi * (800.0 + 3500.0 * vel) / sr);
    auto pan = juce::jlimit (-0.6f, 0.6f, (note - 64) / 40.0f);              // low left, high right, like sitting at the keys
    v.panL = std::sqrt (0.5f * (1.0f - pan)); v.panR = std::sqrt (0.5f * (1.0f + pan));
}

void Piano::render (juce::AudioBuffer<float>& out, int start0, int n)
{
    if (n <= 0) return;
    if (scratch.getNumSamples() < n) scratch.setSize (2, n, false, false, true);
    scratch.clear (0, 0, n);
    scratch.clear (1, 0, n);
    auto* L = scratch.getWritePointer (0);
    auto* R = scratch.getWritePointer (1);
    bool any = false;
    for (auto& v : voices)
    {
        if (v.note < 0) continue;
        any = true;
        for (int i = 0; i < n; ++i)
        {
            float s = 0;
            for (int k = 0; k < v.count; ++k)
            {
                auto& p = v.p[(size_t) k];
                // rotate the phasor (sin = im) and decay its two envelope stages
                auto re = p.re * p.cosw - p.im * p.sinw;
                auto im = p.re * p.sinw + p.im * p.cosw;
                p.re = re; p.im = im;
                s += im * p.amp * (p.ef + p.es);
                p.ef *= p.fast;
                p.es *= p.slow;
            }
            // hammer thump: a short burst of filtered noise
            if (v.noise > 1.0e-4f)
            {
                v.noiseState = v.noiseState * v.noiseCoef + (rnd.nextFloat() * 2.0f - 1.0f) * (1.0f - v.noiseCoef);
                s += v.noiseState * v.noise;
                v.noise *= 0.9965f;
            }
            if (v.releasing) v.damp = juce::jmax (0.0f, v.damp - v.dampRate * v.damp * 6.9f);
            auto y = s * v.level * v.damp;
            L[i] += y * v.panL;
            R[i] += y * v.panR;
        }
        // keep the phasors on the unit circle (float drift)
        for (int k = 0; k < v.count; ++k)
        {
            auto& p = v.p[(size_t) k];
            auto mag = std::sqrt (p.re * p.re + p.im * p.im);
            if (mag > 0) { p.re /= mag; p.im /= mag; }
        }
        float loud = 0;
        for (int k = 0; k < v.count; ++k) loud += v.p[(size_t) k].amp * (v.p[(size_t) k].ef + v.p[(size_t) k].es);
        if ((v.releasing && v.damp < 1.0e-4f) || loud * v.level < 1.0e-5f) v.note = -1;
    }
    if (! any) return;
    reverb.processStereo (L, R, n);
    for (int i = 0; i < n; ++i)
    {
        // gentle safety: a big chord never clips
        L[i] = std::tanh (L[i] * gain * 1.4f) / 1.4f;
        R[i] = std::tanh (R[i] * gain * 1.4f) / 1.4f;
    }
    out.addFrom (0, start0, L, n);
    if (out.getNumChannels() > 1) out.addFrom (1, start0, R, n);
}

} // namespace lyra
