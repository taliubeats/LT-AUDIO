#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace corona
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CORONA", createLayout())
{
    for (auto* id : { IDs::input, IDs::threshold, IDs::attack, IDs::release, IDs::debump, IDs::filter,
                      IDs::makeup, IDs::mix, IDs::power, IDs::autoGain, IDs::mode, IDs::ovs })
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
    engine.prepare (sr, block, 2);
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

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.copyFrom (c, 0, buffer, 0, 0, buffer.getNumSamples());

    Params s;
    s.inputDb   = v (IDs::input);
    s.threshold = v (IDs::threshold);
    s.attack    = juce::roundToInt (v (IDs::attack));
    s.release   = juce::roundToInt (v (IDs::release));
    s.debump    = v (IDs::debump);
    s.filter    = juce::roundToInt (v (IDs::filter));
    s.makeupDb  = v (IDs::makeup);
    s.mix       = v (IDs::mix);
    s.power     = v (IDs::power) > 0.5f && usable.load();
    s.autoGain  = v (IDs::autoGain) > 0.5f;
    s.mode      = juce::roundToInt (v (IDs::mode));
    s.oversampling = juce::roundToInt (v (IDs::ovs));
    engine.setParams (s);
    engine.process (buffer);
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("preset", presetName, nullptr);
    tree.setProperty ("uiScale", uiScale, nullptr);
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
    uiScale = (float) tree.getProperty ("uiScale", 0.8f);
    if (onStateRestored) onStateRestored();
}

} // namespace corona

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new corona::Processor();
}
