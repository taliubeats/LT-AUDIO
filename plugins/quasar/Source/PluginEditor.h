#pragma once
#include "PluginProcessor.h"
namespace quasar
{
class Editor : public juce::GenericAudioProcessorEditor
{
public:
    explicit Editor (Processor& p) : juce::GenericAudioProcessorEditor (p) {}
};
}
