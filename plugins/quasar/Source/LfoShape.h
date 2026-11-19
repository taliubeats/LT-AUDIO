#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

namespace quasar
{

/**
    A drawable LFO shape: points (x 0..1, y 0..1) with a curve per segment, or a random kind.
    The message thread edits it and renders a 1024-point table; the audio thread reads the table.
*/
struct LfoShape
{
    enum Kind { points = 0, sampleHold, smoothRandom };
    struct Point { float x, y, curve; };   // curve -1..1 for the segment that starts at this point

    Kind kind = points;
    std::vector<Point> pts;

    static LfoShape preset (int index);         // 0 sine 1 triangle 2 saw up 3 saw down 4 square 5 S&H 6 smooth rnd 7 pump
    static const juce::StringArray& presetNames();

    float evaluate (float x) const;
    juce::var toVar() const;
    static LfoShape fromVar (const juce::var&);
    bool operator== (const LfoShape& o) const;
};

/** Lock-free hand-over of rendered LFO tables to the audio thread. */
class LfoTables
{
public:
    static constexpr int size = 1024;
    LfoTables();
    void publish (int lfo, const LfoShape&);    // message thread
    const LfoShape& shape (int lfo) const       { return shapes[(size_t) lfo]; }

    // audio thread
    inline float read (int lfo, float phase) const noexcept
    {
        auto& t = tables[(size_t) lfo][(size_t) current[(size_t) lfo].load (std::memory_order_acquire)];
        auto x = (phase - std::floor (phase)) * (float) size;
        auto i = (int) x; auto f = x - (float) i;
        return t[(size_t) (i & (size - 1))] + f * (t[(size_t) ((i + 1) & (size - 1))] - t[(size_t) (i & (size - 1))]);
    }
    int kind (int lfo) const noexcept { return kinds[(size_t) lfo].load(); }

private:
    std::array<LfoShape, 4> shapes;
    std::array<std::array<std::array<float, size>, 2>, 4> tables {};
    std::array<std::atomic<int>, 4> current {}, kinds {};
};

} // namespace quasar
