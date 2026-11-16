#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace pulsar
{

/**
    The ducking shape over one cycle (one 1/4 note, one bar...).
    x = position in the cycle (0..1), y = volume (1 = untouched, 0 = silent).
    Each point's `tension` bends the segment that starts at it (-1..1, 0 = straight).
    `offset` shifts the whole shape left/right in time (SHIFT + drag).
*/
struct CurvePoint
{
    float x = 0, y = 1, tension = 0;
    bool operator== (const CurvePoint& o) const noexcept;
};

class Curve
{
public:
    Curve();
    explicit Curve (std::vector<CurvePoint> pts, float offset = 0.0f);

    static constexpr int numShapes = 8;
    static Curve shape (int index);
    static juce::String shapeName (int index);

    /** Value of the shape itself (no offset), x in 0..1. */
    float evaluateShape (float x) const noexcept;

    /** Value at a position in the cycle, with the offset applied. */
    float evaluate (float phase) const noexcept;

    /** Fills a lookup table of `size + 1` values (the last equals the end of the cycle). */
    void fillTable (float* table, int size) const noexcept;

    const std::vector<CurvePoint>& getPoints() const noexcept { return points; }
    float getOffset() const noexcept                         { return offset; }
    void setOffset (float o) noexcept;

    /** Editing. Returns the index of the point (it may change when x is moved past nothing - x is clamped). */
    int addPoint (float x, float y);
    void removePoint (int index);
    void movePoint (int index, float x, float y);
    void setTension (int index, float tension);
    int segmentAt (float shapeX) const noexcept;

    bool operator== (const Curve& o) const noexcept;
    bool operator!= (const Curve& o) const noexcept { return ! operator== (o); }

    juce::var toVar() const;
    static Curve fromVar (const juce::var&);

    static float bend (float u, float tension) noexcept;

private:
    void sanitise();

    std::vector<CurvePoint> points;
    float offset = 0;
};

} // namespace pulsar
