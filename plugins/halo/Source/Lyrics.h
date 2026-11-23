#pragma once

#include <juce_core/juce_core.h>
#include "engine/Fx.h"

namespace halo
{

/**
    Lyrics with word timings (for the refuerzo voices), using whisper.cpp locally.
    The model (ggml-small-q5_1.bin, ~190 MB, multilingual) is downloaded once from Settings into
    ~/Library/Application Support/LT Audio/HALO/models. Without it, refuerzo falls back to phrase endings.
*/
struct Lyrics
{
    static juce::File modelFile();
    static bool available();
    static juce::URL modelUrl();

    /** Blocking. `language` = "auto", "es", "en"... */
    static std::vector<Word> transcribe (const std::vector<double>& audio, int sampleRate,
                                         const std::function<void (float)>& progress,
                                         const std::function<bool()>& shouldStop,
                                         const char* language = "auto");
};

} // namespace halo
