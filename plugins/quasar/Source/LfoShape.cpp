#include "LfoShape.h"

namespace quasar
{

const juce::StringArray& LfoShape::presetNames()
{
    static const juce::StringArray n { "SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "S&H", "SMOOTH RANDOM", "PUMP", "STEPS" };
    return n;
}

LfoShape LfoShape::preset (int i)
{
    LfoShape s;
    switch (i)
    {
        case 0:  s.pts = { { 0, 0.5f, -0.45f }, { 0.25f, 1, 0.45f }, { 0.5f, 0.5f, -0.45f }, { 0.75f, 0, 0.45f } }; break;
        case 1:  s.pts = { { 0, 0, 0 }, { 0.5f, 1, 0 } }; break;
        case 2:  s.pts = { { 0, 0, 0 }, { 0.999f, 1, 0 } }; break;
        case 3:  s.pts = { { 0, 1, 0 }, { 0.999f, 0, 0 } }; break;
        case 4:  s.pts = { { 0, 1, 0 }, { 0.499f, 1, 0 }, { 0.5f, 0, 0 }, { 0.999f, 0, 0 } }; break;
        case 5:  s.kind = sampleHold; s.pts = { { 0, 0.5f, 0 } }; break;
        case 6:  s.kind = smoothRandom; s.pts = { { 0, 0.5f, 0 } }; break;
        case 7:  s.pts = { { 0, 0, 0.6f }, { 0.4f, 1, 0 }, { 0.999f, 1, 0 } }; break;   // sidechain-like pump
        case 8:  s.pts = { { 0, 0, 0 }, { 0.249f, 0, 0 }, { 0.25f, 0.33f, 0 }, { 0.499f, 0.33f, 0 }, { 0.5f, 0.66f, 0 }, { 0.749f, 0.66f, 0 }, { 0.75f, 1, 0 }, { 0.999f, 1, 0 } }; break;
        default: s.pts = { { 0, 0, 0 }, { 0.5f, 1, 0 } }; break;
    }
    return s;
}

float LfoShape::evaluate (float x) const
{
    if (pts.empty()) return 0.0f;
    x = x - std::floor (x);
    // segments wrap from the last point back to the first one (at x + 1)
    int n = (int) pts.size();
    for (int i = 0; i < n; ++i)
    {
        auto& a = pts[(size_t) i];
        auto& b = pts[(size_t) ((i + 1) % n)];
        auto ax = a.x, bx = i + 1 < n ? b.x : b.x + 1.0f;
        auto xx = x < pts.front().x ? x + 1.0f : x;
        if (xx >= ax && xx <= bx)
        {
            auto t = bx > ax ? (xx - ax) / (bx - ax) : 0.0f;
            auto c = a.curve;
            if (std::abs (c) > 0.01f)
            {
                auto k = std::exp2 (std::abs (c) * 3.0f);
                t = c > 0 ? std::pow (t, k) : 1.0f - std::pow (1.0f - t, k);
            }
            return a.y + t * (b.y - a.y);
        }
    }
    return pts.front().y;
}

juce::var LfoShape::toVar() const
{
    auto o = std::make_unique<juce::DynamicObject>();
    o->setProperty ("kind", (int) kind);
    juce::Array<juce::var> a;
    for (auto& p : pts) a.add (juce::Array<juce::var> { p.x, p.y, p.curve });
    o->setProperty ("points", a);
    return juce::var (o.release());
}

LfoShape LfoShape::fromVar (const juce::var& v)
{
    LfoShape s;
    s.kind = (Kind) juce::jlimit (0, 2, (int) v.getProperty ("kind", 0));
    if (auto* a = v.getProperty ("points", {}).getArray())
        for (auto& p : *a)
            if (p.isArray() && p.size() >= 3)
                s.pts.push_back ({ juce::jlimit (0.0f, 1.0f, (float) p[0]), juce::jlimit (0.0f, 1.0f, (float) p[1]), juce::jlimit (-1.0f, 1.0f, (float) p[2]) });
    std::sort (s.pts.begin(), s.pts.end(), [] (const Point& a, const Point& b) { return a.x < b.x; });
    if (s.pts.empty()) s = preset (0);
    return s;
}

bool LfoShape::operator== (const LfoShape& o) const
{
    if (kind != o.kind || pts.size() != o.pts.size()) return false;
    for (size_t i = 0; i < pts.size(); ++i)
        if (pts[i].x != o.pts[i].x || pts[i].y != o.pts[i].y || pts[i].curve != o.pts[i].curve) return false;
    return true;
}

LfoTables::LfoTables()
{
    for (int i = 0; i < 4; ++i) publish (i, LfoShape::preset (0));
}

void LfoTables::publish (int lfo, const LfoShape& s)
{
    lfo = juce::jlimit (0, 3, lfo);
    shapes[(size_t) lfo] = s;
    auto next = 1 - current[(size_t) lfo].load();
    auto& t = tables[(size_t) lfo][(size_t) next];
    for (int i = 0; i < size; ++i) t[(size_t) i] = s.evaluate ((float) i / (float) size);
    kinds[(size_t) lfo].store ((int) s.kind);
    current[(size_t) lfo].store (next, std::memory_order_release);
}

} // namespace quasar
