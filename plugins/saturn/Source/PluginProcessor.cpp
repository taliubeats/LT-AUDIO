#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace saturn
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "SATURN", createLayout())
{
    for (auto* id : IDs::all)
        p[id] = state.getRawParameterValue (id);
}

bool Processor::isPresetModified()
{
    auto i = presets.indexOf (presetName);
    return i < 0 || ! presets.all()[(size_t) i].matches (state);
}

void Processor::setCurrentProgram (int i)
{
    auto f = factoryPresets();
    if (juce::isPositiveAndBelow (i, (int) f.size()))
    {
        applyPreset (f[(size_t) i]);
        if (onStateRestored) onStateRestored();
    }
}

const juce::String Processor::getProgramName (int i)
{
    auto f = factoryPresets();
    return juce::isPositiveAndBelow (i, (int) f.size()) ? f[(size_t) i].name : juce::String();
}

void Processor::prepareToPlay (double sr, int block)
{
    engine.prepare (sr, block);
    setLatencySamples (engine.getLatencySamples());
    refreshLicense();
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    auto in = layouts.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && (in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo()));
}

SaturnSettings Processor::currentSettings() const
{
    SaturnSettings s;
    s.tape = juce::roundToInt (v (IDs::tape));
    s.age = v (IDs::age) * 0.01f;
    s.wow = v (IDs::wow) * 0.01f;
    s.flutter = v (IDs::flutter) * 0.01f;
    s.drift = v (IDs::drift) * 0.01f;
    s.saturation = v (IDs::saturation) * 0.01f;
    s.tone = v (IDs::tone) * 0.01f;
    s.bandwidth = v (IDs::bandwidth) * 0.01f;
    s.hiss = v (IDs::hiss) * 0.01f;
    s.ensemble = v (IDs::ensemble) * 0.01f;
    s.mix = v (IDs::mix) * 0.01f;
    s.outputDb = v (IDs::output);
    return s;
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
    if (! usable.load()) return;
    engine.process (buffer, currentSettings());
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("preset", presetName, nullptr);
    tree.setProperty ("night", night, nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, dest);
}

void Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    state.replaceState (tree);
    presetName = tree.getProperty ("preset", "Default").toString();
    night = tree.getProperty ("night", false);
    if (onStateRestored) onStateRestored();
}

} // namespace saturn

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new saturn::Processor();
}
