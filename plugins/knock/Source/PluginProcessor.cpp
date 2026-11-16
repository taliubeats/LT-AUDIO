#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace knock
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "KNOCK", createLayout())
{
    for (auto* id : { IDs::knock, IDs::mode, IDs::input, IDs::output, IDs::punch, IDs::saturation, IDs::distortion, IDs::distType,
                      IDs::bright, IDs::low, IDs::glue, IDs::width, IDs::crush, IDs::lofi, IDs::stutter, IDs::filter, IDs::ring,
                      IDs::reverse, IDs::reverb, IDs::delay, IDs::delayTime })
        p[id] = state.getRawParameterValue (id);
    focus.attach (state);
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
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && layouts.getMainInputChannelSet() == out;
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            t.playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) t.bpm = *b;
            if (auto q = pos->getPpqPosition()) { t.ppq = *q; t.hasPosition = true; }
        }
    if (! usable.load()) return;

    KnockSettings s;
    s.melody = v (IDs::mode) > 0.5f;
    s.input = juce::Decibels::decibelsToGain (v (IDs::input));
    s.output = juce::Decibels::decibelsToGain (v (IDs::output));
    s.knock = v (IDs::knock) * 0.01f;
    s.punch = v (IDs::punch) * 0.01f;
    s.saturation = v (IDs::saturation) * 0.01f;
    s.distortion = v (IDs::distortion) * 0.01f;
    s.distType = juce::roundToInt (v (IDs::distType));
    s.bright = v (IDs::bright) * 0.01f;
    s.low = v (IDs::low) * 0.01f;
    s.glue = v (IDs::glue) * 0.01f;
    s.width = v (IDs::width) * 0.01f;
    s.crush = v (IDs::crush) * 0.01f;
    s.lofi = v (IDs::lofi) * 0.01f;
    s.stutter = v (IDs::stutter) * 0.01f;
    s.filter = v (IDs::filter) * 0.01f;
    s.ring = v (IDs::ring) * 0.01f;
    s.reverse = v (IDs::reverse) * 0.01f;
    s.reverb = v (IDs::reverb) * 0.01f;
    s.delay = v (IDs::delay) * 0.01f;
    s.delayTime = juce::roundToInt (v (IDs::delayTime));
    engine.process (buffer, s, t, focus.read());
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("preset", presetName, nullptr);
    tree.setProperty ("light", lightTheme, nullptr);
    tree.setProperty ("lockTone", lockTone, nullptr);
    tree.setProperty ("lockFx", lockFx, nullptr);
    tree.setProperty ("lockSpace", lockSpace, nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, dest);
}

void Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    state.replaceState (tree);
    presetName = tree.getProperty ("preset", "Default Preset").toString();
    lightTheme = tree.getProperty ("light", false);
    lockTone = tree.getProperty ("lockTone", false);
    lockFx = tree.getProperty ("lockFx", false);
    lockSpace = tree.getProperty ("lockSpace", false);
    if (onStateRestored) onStateRestored();
}

} // namespace knock

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new knock::Processor();
}
