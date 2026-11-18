#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace nova
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "NOVA", createLayout())
{
    for (auto* id : { IDs::type, IDs::drive, IDs::tone, IDs::warmth, IDs::air, IDs::character, IDs::input, IDs::mix,
                      IDs::output, IDs::autoGain })
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
    auto ch = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels(), 1);
    engine.prepare (sr, block, ch);
    splitter.prepare (sr, block, ch);
    restDelay.prepare (sr, block, ch);
    target.setSize (ch, block);
    spectrum.setSampleRate (sr);
    setLatencySamples (engine.getLatencySamples());
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
    auto n = buffer.getNumSamples();
    for (int c = 0; c < 2; ++c)
        inLevel[(size_t) c].store (buffer.getMagnitude (juce::jmin (c, buffer.getNumChannels() - 1), 0, n));
    spectrum.push (buffer);

    if (! usable.load())
    {
        for (int c = 0; c < 2; ++c) outLevel[(size_t) c].store (inLevel[(size_t) c].load());
        return;
    }

    NovaSettings s;
    s.type = juce::roundToInt (v (IDs::type));
    s.drive = v (IDs::drive) * 0.01f;
    s.tone = v (IDs::tone) * 0.01f;
    s.warmth = v (IDs::warmth) * 0.01f;
    s.air = v (IDs::air) * 0.01f;
    s.character = v (IDs::character) * 0.01f;
    s.inputDb = v (IDs::input);
    s.outputDb = v (IDs::output);
    s.mix = v (IDs::mix) * 0.01f;
    s.autoGain = v (IDs::autoGain) > 0.5f;

    auto f = focus.read();
    if (f.isActive())
    {
        // the engine has latency: delay the untouched band by the same amount so both stay aligned
        target.setSize (buffer.getNumChannels(), n, false, false, true);
        splitter.split (buffer, target, f);
        engine.process (target, s);
        restDelay.process (buffer, engine.getLatencySamples());
        lt::dsp::FocusSplitter::combine (buffer, target, f);
    }
    else
    {
        engine.process (buffer, s);
    }

    for (int c = 0; c < 2; ++c)
        outLevel[(size_t) c].store (buffer.getMagnitude (juce::jmin (c, buffer.getNumChannels() - 1), 0, n));
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

} // namespace nova

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new nova::Processor();
}
