// LT COSMOS presets: save a look, and a new COSMOS opens with it. Leaves the user's presets as they were.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <iostream>

static int failures = 0;
static void check (bool ok, const juce::String& what) { std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl; if (! ok) ++failures; }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto startupBefore = cosmos::Processor::getStartupPreset();
    const juce::String name = "zz test preset 7f3";
    {
        cosmos::Processor p;
        p.layout = "16:1.000,17:0.500";
        p.skin = 5; p.target = -9; p.width = 1500; p.height = 230;
        check (p.savePreset (name, true), "save a preset");
        check (cosmos::Processor::getStartupPreset() == name, "saving makes it the startup preset");
        check (p.listPresets().contains (name), "it shows in the list");
    }
    {
        cosmos::Processor fresh;
        check (fresh.layout == "16:1.000,17:0.500" && fresh.skin == 5 && fresh.target == -9 && fresh.width == 1500 && fresh.presetName == name,
               "a new COSMOS opens with it: " + fresh.layout + " skin " + juce::String (fresh.skin));
        juce::MemoryBlock mb;
        fresh.getStateInformation (mb);
        cosmos::Processor restored;
        restored.skin = 0;
        restored.setStateInformation (mb.getData(), (int) mb.getSize());
        check (restored.presetName == name && restored.skin == 5, "the project keeps the preset name");
        check (fresh.deletePreset (name) && ! fresh.listPresets().contains (name) && cosmos::Processor::getStartupPreset().isEmpty(), "delete removes it and the startup choice");
    }
    if (startupBefore.isNotEmpty()) cosmos::Processor::setStartupPreset (startupBefore);
    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
