#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace orbit
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "ORBIT", createLayout())
{
    for (auto* id : { IDs::mode, IDs::sync, IDs::note, IDs::ms, IDs::feel, IDs::feedback, IDs::mix, IDs::freeze,
                      IDs::lowCut, IDs::highCut, IDs::width, IDs::duck, IDs::wobble, IDs::drive })
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

double Processor::computeDelaySeconds (double bpm) const
{
    auto feel = juce::roundToInt (v (IDs::feel));
    if (v (IDs::sync) > 0.5f)
        return Notes::beats (juce::roundToInt (v (IDs::note)), feel) * 60.0 / juce::jlimit (20.0, 999.0, bpm);
    auto ms = (double) v (IDs::ms);
    return (feel == 1 ? ms * 1.5 : feel == 2 ? ms * 2.0 / 3.0 : ms) * 0.001;
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

    auto seconds = juce::jlimit (0.001, OrbitEngine::maxSeconds - 0.05, computeDelaySeconds (bpm));
    delaySeconds = seconds;
    if (! usable.load()) return;

    OrbitSettings s;
    s.mode = juce::roundToInt (v (IDs::mode));
    s.delaySeconds = seconds;
    s.feedback = v (IDs::feedback) * 0.01f;
    s.mix = v (IDs::mix) * 0.01f;
    s.freeze = v (IDs::freeze) > 0.5f;
    s.lowCut = v (IDs::lowCut);
    s.highCut = v (IDs::highCut);
    s.width = v (IDs::width) * 0.01f;
    s.duck = v (IDs::duck) * 0.01f;
    s.wobble = v (IDs::wobble) * 0.01f;
    s.drive = v (IDs::drive) * 0.01f;
    engine.process (buffer, s);
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("preset", presetName, nullptr);
    tree.setProperty ("light", lightTheme, nullptr);
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
    lightTheme = tree.getProperty ("light", false);
    if (onStateRestored) onStateRestored();
}

} // namespace orbit

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new orbit::Processor();
}
