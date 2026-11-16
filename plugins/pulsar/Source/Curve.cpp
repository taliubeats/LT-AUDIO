#include "Curve.h"

namespace pulsar
{

bool CurvePoint::operator== (const CurvePoint& o) const noexcept
{
    return std::abs (x - o.x) < 1.0e-4f && std::abs (y - o.y) < 1.0e-4f && std::abs (tension - o.tension) < 1.0e-4f;
}

Curve::Curve() : Curve (shape (0)) {}

Curve::Curve (std::vector<CurvePoint> pts, float o) : points (std::move (pts))
{
    sanitise();
    setOffset (o);
}

void Curve::sanitise()
{
    for (auto& p : points)
    {
        p.x = juce::jlimit (0.0f, 1.0f, p.x);
        p.y = juce::jlimit (0.0f, 1.0f, p.y);
        p.tension = juce::jlimit (-1.0f, 1.0f, p.tension);
    }
    std::stable_sort (points.begin(), points.end(), [] (const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });

    if (points.empty())
        points = { { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };
    if (points.size() == 1)
        points.push_back (points.front());

    points.front().x = 0.0f;
    points.back().x = 1.0f;
}

void Curve::setOffset (float o) noexcept
{
    offset = o - std::floor (o);
    if (offset >= 0.9999f) offset = 0.0f;
}

float Curve::bend (float u, float tension) noexcept
{
    // exponential bend: positive tension = slow start / fast end, negative = fast start / slow end
    auto c = tension * 9.0f;
    if (std::abs (c) < 1.0e-3f)
        return u;
    return (std::exp (c * u) - 1.0f) / (std::exp (c) - 1.0f);
}

int Curve::segmentAt (float x) const noexcept
{
    for (int i = 0; i < (int) points.size() - 1; ++i)
        if (x <= points[(size_t) i + 1].x)
            return i;
    return (int) points.size() - 2;
}

float Curve::evaluateShape (float x) const noexcept
{
    x = juce::jlimit (0.0f, 1.0f, x);
    auto i = segmentAt (x);
    auto& a = points[(size_t) i];
    auto& b = points[(size_t) i + 1];
    auto w = b.x - a.x;
    if (w <= 1.0e-6f)
        return b.y;
    auto u = (x - a.x) / w;
    return a.y + (b.y - a.y) * bend (u, a.tension);
}

float Curve::evaluate (float phase) const noexcept
{
    auto x = phase - offset;
    x -= std::floor (x);
    if (phase >= 1.0f && offset <= 0.0f)
        x = 1.0f;
    return evaluateShape (x);
}

void Curve::fillTable (float* table, int size) const noexcept
{
    for (int i = 0; i <= size; ++i)
    {
        auto phase = (float) i / (float) size;
        if (i == size)
            table[i] = evaluate (offset > 0.0f ? 1.0f - 1.0e-6f : 1.0f);
        else
            table[i] = evaluate (phase);
    }
}

int Curve::addPoint (float x, float y)
{
    x = juce::jlimit (0.001f, 0.999f, x);
    y = juce::jlimit (0.0f, 1.0f, y);
    auto seg = segmentAt (x);
    auto tension = points[(size_t) seg].tension;
    points.insert (points.begin() + seg + 1, { x, y, tension });
    return seg + 1;
}

void Curve::removePoint (int index)
{
    if (index <= 0 || index >= (int) points.size() - 1)
        return;
    points.erase (points.begin() + index);
}

void Curve::movePoint (int index, float x, float y)
{
    if (index < 0 || index >= (int) points.size())
        return;

    auto& p = points[(size_t) index];
    p.y = juce::jlimit (0.0f, 1.0f, y);

    if (index == 0 || index == (int) points.size() - 1)
        return; // the ends stay at the start / end of the cycle

    auto lo = points[(size_t) index - 1].x, hi = points[(size_t) index + 1].x;
    p.x = juce::jlimit (lo, hi, x);
}

void Curve::setTension (int index, float tension)
{
    if (index >= 0 && index < (int) points.size() - 1)
        points[(size_t) index].tension = juce::jlimit (-1.0f, 1.0f, tension);
}

bool Curve::operator== (const Curve& o) const noexcept
{
    return points == o.points && std::abs (offset - o.offset) < 1.0e-4f;
}

juce::var Curve::toVar() const
{
    juce::Array<juce::var> pts;
    for (auto& p : points)
        pts.add (juce::Array<juce::var> { p.x, p.y, p.tension });

    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("points", pts);
    obj->setProperty ("offset", offset);
    return juce::var (obj.release());
}

Curve Curve::fromVar (const juce::var& v)
{
    std::vector<CurvePoint> pts;
    if (auto* arr = v["points"].getArray())
        for (auto& p : *arr)
            if (p.isArray() && p.size() >= 2)
                pts.push_back ({ (float) p[0], (float) p[1], p.size() > 2 ? (float) p[2] : 0.0f });

    if (pts.size() < 2)
        return {};
    return Curve (std::move (pts), (float) v["offset"]);
}

// ------------------------------------------------------------------ the 8 shape buttons

juce::String Curve::shapeName (int index)
{
    static const char* names[] = { "Pump", "S-Fall", "Linear", "Fade", "Ramp", "Arc", "Drop", "Cut" };
    return names[juce::jlimit (0, numShapes - 1, index)];
}

Curve Curve::shape (int index)
{
    switch (index)
    {
        case 0:  return Curve ({ { 0.0f, 0.0f, -0.55f }, { 1.0f, 1.0f, 0.0f } });                                   // fast recovery
        case 1:  return Curve ({ { 0.0f, 1.0f, 0.45f }, { 0.5f, 0.5f, -0.45f }, { 1.0f, 0.0f, 0.0f } });            // S fall
        case 2:  return Curve ({ { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } });                                     // linear
        case 3:  return Curve ({ { 0.0f, 1.0f, -0.3f }, { 1.0f, 0.0f, 0.0f } });                                    // fade out
        case 4:  return Curve ({ { 0.0f, 0.0f, 0.0f }, { 0.25f, 0.0f, 0.0f }, { 0.6f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }); // hold, ramp
        case 5:  return Curve ({ { 0.0f, 1.0f, 0.55f }, { 1.0f, 0.0f, 0.0f } });                                    // quarter arc
        case 6:  return Curve ({ { 0.0f, 1.0f, 0.0f }, { 0.25f, 1.0f, 0.45f }, { 0.5f, 0.5f, -0.45f }, { 0.75f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }); // drop
        default: return Curve ({ { 0.0f, 1.0f, 0.0f }, { 0.2f, 1.0f, 0.0f }, { 0.24f, 0.3f, -0.4f }, { 1.0f, 0.0f, 0.0f } }); // cut
    }
}

} // namespace pulsar
