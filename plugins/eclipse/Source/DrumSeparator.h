#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace eclipse
{

/**
    AI drum separation (v2): Demucs v4 "htdemucs_ft" drums model (Meta, MIT licence) running with
    demucs.cpp (MIT) on the CPU, on several threads. Offline, nothing leaves the computer.

    The model file (ggml-model-htdemucs_ft_drums-4s-f16.bin, 84 MB) is installed in
    /Library/Application Support/LT Audio/LT Eclipse/Models (or the same folder in ~/Library).
    Each song is separated once: the drum stem is cached in ~/Library/Caches/LT Eclipse/Stems.
*/
class DrumSeparator
{
public:
    using Progress = std::function<void (float, const juce::String&)>;

    static juce::File modelFile();                     // empty if not installed
    static bool available() { return modelFile().existsAsFile(); }

    /** v3: the drum parts model (drumsep, inagoy, MIT: Hybrid Demucs v3 trained to split a drum kit). */
    static juce::File partsModelFile();
    static bool partsAvailable() { return partsModelFile().existsAsFile(); }

    /** The drum stem split into kick, snare, cymbals (hats) and toms. Empty pointers if cancelled / no model. */
    static std::array<std::shared_ptr<juce::AudioBuffer<float>>, 4> separateParts (const juce::AudioBuffer<float>& drums, double sampleRate,
                                                                                   const std::atomic<bool>* cancel, Progress progress,
                                                                                   juce::String& error, bool useCache = true,
                                                                                   const juce::String& songKey = {});

    /** The cache key of a song (its content, not its name). */
    static juce::String keyOf (const juce::AudioBuffer<float>&, double sampleRate);

    /** The drum stem of the audio, at the same sample rate and channel count. nullptr if cancelled / no model. */
    static std::shared_ptr<juce::AudioBuffer<float>> separate (const juce::AudioBuffer<float>&, double sampleRate,
                                                               const std::atomic<bool>* cancel, Progress progress,
                                                               juce::String& error, bool useCache = true);

    static juce::File cacheFolder();
};

} // namespace eclipse
