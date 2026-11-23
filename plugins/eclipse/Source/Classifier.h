#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Skins.h"

namespace eclipse
{

/**
    v2: what drums are in a hit. A small neural network (MLP) trained on thousands of labelled
    one-shots (kick, snare, clap, hat, open hat, tom, perc, cymbal), also on mixes of two or three of them,
    so a kick with a hat on top says "kick + hat". Multi-label: one probability per family.

    Features: log energies in 36 bands (40 Hz - 16 kHz) over 9 time slices (0-300 ms, fine at the start
    where a clap and a snare differ) + burst count + decay + balance.
*/
namespace classifier
{
constexpr int numBands = 36, numSlices = 9;
constexpr int numFeatures = numBands * numSlices + 4;
constexpr int numClasses = numFamilies;           // kick, snare, clap, hat, openHat, tom, perc, cymbal

using Features = std::array<float, numFeatures>;
using Probs = std::array<float, numClasses>;

/** Features of the sound that starts at `onset` (mono mix of the channels). */
Features features (const juce::AudioBuffer<float>&, juce::int64 onset, double sampleRate);

/** Probability of each family being in the hit. */
Probs predict (const Features&);

/** True when trained weights are compiled in. */
bool hasWeights();

// ---------------------------------------------------------------- training (used by the training tool)
struct Layer { int in = 0, out = 0; std::vector<float> w, b; };   // w = [out x in]
struct Net
{
    std::vector<float> mean, std;                 // feature standardisation
    std::vector<Layer> layers;                    // leaky ReLU between, sigmoid at the end
    bool ready() const { return ! layers.empty(); }
    /** acts[0] = standardised input, acts[i] = output of layer i (before the sigmoid for the last). */
    Probs forward (const Features&, std::vector<std::vector<float>>* acts = nullptr) const;
};
const Net& net();
} // namespace classifier

} // namespace eclipse
