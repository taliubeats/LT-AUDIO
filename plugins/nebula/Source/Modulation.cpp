#include "Modulation.h"

namespace nebula
{

const juce::StringArray& Modulation::sourceNames() { static const juce::StringArray n { "MOD 1", "MOD 2", "ENV", "RND" }; return n; }

juce::Colour Modulation::sourceColour (int s)
{
    static const juce::Colour c[] = { juce::Colour (0xffffd35a), juce::Colour (0xffff7aa8), juce::Colour (0xff5ee6ff), juce::Colour (0xff7dff9b) };
    return c[juce::jlimit (0, 3, s)];
}

const juce::StringArray& Modulation::mod2Shapes()
{
    static const juce::StringArray n { "SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "SMOOTH RND" };
    return n;
}

const juce::StringArray& Modulation::targets()
{
    static const juce::StringArray t { "motion", "texture", "pitch", "space", "dirt", "mix",
                                       "size", "density", "position", "spray", "grainPitch", "stereo", "shape", "reverse",
                                       "gdTime", "gdFeedback", "gdHP", "gdLP", "gdMix",
                                       "fxAmt1", "fxAmt2", "fxAmt3", "fxAmt4", "fxAmt5", "fxAmt6", "fxAmt7", "fxAmt8" };
    return t;
}

void Modulation::addRoute (int source, int target, float depth)
{
    if (target < 0 || source < 0) return;
    for (auto& r : routes)
        if (r.source.load() == source && r.target.load() == target) { r.depth = depth; return; }
    for (auto& r : routes)
        if (r.source.load() < 0)
        {
            r.depth = depth;
            r.target = target;
            r.source = source;   // last: the route becomes active
            return;
        }
}

void Modulation::removeRoute (int source, int target)
{
    for (auto& r : routes)
        if (r.source.load() == source && r.target.load() == target) { r.source = -1; r.target = -1; }
}

void Modulation::setDepth (int source, int target, float depth)
{
    for (auto& r : routes)
        if (r.source.load() == source && r.target.load() == target) r.depth = depth;
}

float Modulation::getDepth (int source, int target) const
{
    for (auto& r : routes)
        if (r.source.load() == source && r.target.load() == target) return r.depth.load();
    return 0.0f;
}

juce::var Modulation::toVar() const
{
    juce::Array<juce::var> list;
    for (auto& r : routes)
        if (r.source.load() >= 0)
            list.add (juce::Array<juce::var> { r.source.load(), targets()[r.target.load()], r.depth.load() });
    return list;
}

void Modulation::fromVar (const juce::var& v)
{
    for (auto& r : routes) { r.source = -1; r.target = -1; }
    if (auto* arr = v.getArray())
        for (auto& item : *arr)
            if (item.isArray() && item.size() >= 3)
                addRoute ((int) item[0], targetIndex (item[1].toString()), (float) item[2]);
}

void Modulation::advance (const Inputs& in)
{
    auto spb = in.sampleRate * 60.0 / juce::jlimit (20.0, 999.0, in.bpm);
    double beat;
    if (in.playing) beat = in.ppq;
    else { beat = freeBeats; freeBeats += in.numSamples / spb; }

    // MOD 1: 8 steps, SWING delays every other step
    auto stepLen = juce::jmax (0.0625, in.mod1Beats);
    auto pos = beat / stepLen;
    auto idx = (juce::int64) std::floor (pos);
    auto frac = pos - (double) idx;
    if ((idx & 1) && frac < in.mod1Swing * 0.5) idx -= 1;
    auto step = (int) (((idx % 8) + 8) % 8);
    mod1Step = step;
    values[mod1] = in.steps[(size_t) step];

    // MOD 2: LFO, SWING skews the shape
    auto p = beat / juce::jmax (0.0625, in.mod2Beats);
    auto ph = (float) (p - std::floor (p));
    auto skew = 0.5f + 0.45f * in.mod2Swing;
    auto sp = ph < skew ? ph / skew * 0.5f : 0.5f + (ph - skew) / (1.0f - skew) * 0.5f;
    mod2Phase = ph;
    float v;
    switch (in.mod2Shape)
    {
        case 1:  v = sp < 0.5f ? sp * 2.0f : 2.0f - sp * 2.0f; break;
        case 2:  v = sp; break;
        case 3:  v = 1.0f - sp; break;
        case 4:  v = sp < 0.5f ? 1.0f : 0.0f; break;
        case 5:  smoothRnd += (rndNow - smoothRnd) * 0.08f; v = 0.5f + 0.5f * smoothRnd; break;
        default: v = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * sp); break;
    }
    values[mod2] = v;

    // ENV follows the input level
    auto target = juce::jlimit (0.0f, 1.0f, in.inputLevel * 1.8f);
    env += (target - env) * (target > env ? 0.5f : 0.08f);
    values[envelope] = env;

    // RND: sample & hold at the rate
    auto r = std::floor (beat / juce::jmax (0.0625, in.rndBeats));
    if (std::abs (r - lastRnd) > 0.5) { lastRnd = r; rndPrev = rndNow; rndNow = rnd.nextFloat() * 2.0f - 1.0f; }
    values[random] = 0.5f + 0.5f * rndNow;

    for (auto& o : offsets) o = 0.0f;
}

float Modulation::modulated (int target, float base) noexcept
{
    if (target < 0) return base;
    float sum = 0;
    for (auto& r : routes)
    {
        auto s = r.source.load();
        if (s < 0 || r.target.load() != target) continue;
        auto v = values[(size_t) s].load();
        // MOD 2 and RND swing both ways around the knob; MOD 1 and ENV push in one direction
        auto bipolar = s == mod2 || s == random;
        sum += r.depth.load() * (bipolar ? (v * 2.0f - 1.0f) * 0.5f : v);
    }
    if (juce::isPositiveAndBelow (target, 64)) offsets[(size_t) target] = sum;
    return juce::jlimit (0.0f, 1.0f, base + sum);
}

} // namespace nebula
