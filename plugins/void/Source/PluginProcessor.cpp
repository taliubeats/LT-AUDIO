#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace lt_void
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "VOID", createLayout())
{
    for (auto* id : { IDs::type, IDs::mix, IDs::preDelay, IDs::preSync, IDs::preNote, IDs::size, IDs::decay, IDs::highFreq,
                      IDs::lowFreq, IDs::attack, IDs::diffusion, IDs::modRate, IDs::modDepth, IDs::lowCut, IDs::highCut,
                      IDs::width, IDs::duck, IDs::freeze })
        p[id] = state.getRawParameterValue (id);
}

void Processor::applyPreset (const lt::presets::ParamPreset& preset)
{
    preset.apply (state);
    presetName = preset.name;
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
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;
    hostBpm = (float) bpm;

    if (! usable.load())
        return;

    VoidSettings s;
    s.type = juce::roundToInt (v (IDs::type));
    s.mix = v (IDs::mix) * 0.01f;
    s.preDelayMs = v (IDs::preSync) > 0.5f ? (float) (Notes::beats (juce::roundToInt (v (IDs::preNote))) * 60000.0 / bpm) : v (IDs::preDelay);
    s.size = v (IDs::size) * 0.01f;
    s.decay = v (IDs::decay);
    s.highFreq = v (IDs::highFreq);
    s.lowFreq = v (IDs::lowFreq);
    s.attack = v (IDs::attack) * 0.01f;
    s.diffusion = v (IDs::diffusion) * 0.01f;
    s.modRate = v (IDs::modRate);
    s.modDepth = v (IDs::modDepth) * 0.01f;
    s.lowCut = v (IDs::lowCut);
    s.highCut = v (IDs::highCut);
    s.width = v (IDs::width) * 0.01f;
    s.duck = v (IDs::duck) * 0.01f;
    s.freeze = v (IDs::freeze) > 0.5f;
    engine.process (buffer, s);
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("preset", presetName, nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, dest);
}

void Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    state.replaceState (tree);
    presetName = tree.getProperty ("preset", "Infinite Cloud").toString();
    if (onStateRestored) onStateRestored();
}

} // namespace lt_void

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new lt_void::Processor();
}
