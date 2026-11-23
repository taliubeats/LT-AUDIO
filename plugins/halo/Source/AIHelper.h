#pragma once

#include <juce_core/juce_core.h>
#include "engine/Layers.h"

namespace halo
{

/**
    BEST quality: the FAST take is sung again by an AI voice-conversion model (Seed-VC) with the timbre of the lead.
    It runs as a separate helper program (so a crash or lack of memory never takes the DAW down):
        ~/Library/Application Support/LT Audio/HALO/ai/halo-ai  <job.json>
    installed by tools/halo-prototype/install_ai_helper.sh. Multi-take layers (stack, gang) and pitched character
    voices stay FAST on purpose.
*/
struct AIHelper
{
    static juce::File launcher();
    static bool available();

    struct Item
    {
        Stereo* stereo = nullptr;      // replaced in place
        bool eligible = true;
    };

    static bool render (const Analysis&, std::vector<Item>&, const std::function<void (float)>& progress,
                        const std::function<bool()>& shouldStop);
};

} // namespace halo
