#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace tune
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, &undo, "TUNE", createLayout())
{
    for (auto* id : { IDs::key, IDs::scale, IDs::edited, IDs::voice, IDs::retune, IDs::humanize, IDs::flexTune, IDs::vibrato,
                      IDs::formant, IDs::preserve, IDs::transpose, IDs::detune, IDs::mix, IDs::mode, IDs::bypass })
        p[id] = state.getRawParameterValue (id);
    for (int i = 0; i < 12; ++i)
        p[IDs::note (i)] = state.getRawParameterValue (IDs::note (i));
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

juce::AudioParameterBool* Processor::getBypassParameter() const
{
    return dynamic_cast<juce::AudioParameterBool*> (state.getParameter (IDs::bypass));
}

void Processor::configureEngine (int voice, bool hq)
{
    engine.prepare (sampleRate, blockSize, voice, hq);
    setLatencySamples (engine.getLatency());
}

void Processor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;
    blockSize = block;
    configureEngine (juce::roundToInt (v (IDs::voice)), v (IDs::mode) > 0.5f);
    refreshLicense();
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    auto in = layouts.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && (in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo()));
}

TuneSettings Processor::readSettings()
{
    TuneSettings s;
    auto key = juce::roundToInt (v (IDs::key));
    auto scale = juce::roundToInt (v (IDs::scale));
    if (scale != Scales::custom && v (IDs::edited) < 0.5f)
        s.mask = Scales::mask (key, scale);
    else
        for (int i = 0; i < 12; ++i) s.mask[(size_t) i] = p.at (IDs::note (i))->load() > 0.5f;
    s.voice = juce::roundToInt (v (IDs::voice));
    s.hq = v (IDs::mode) > 0.5f;
    s.retuneMs = v (IDs::retune);
    s.humanize = v (IDs::humanize) * 0.01f;
    s.flex = v (IDs::flexTune) * 0.01f;
    s.vibrato = v (IDs::vibrato) * 0.01f;
    s.formant = v (IDs::formant);
    s.preserve = v (IDs::preserve) > 0.5f;
    s.transpose = v (IDs::transpose);
    s.refHz = v (IDs::detune);
    s.mix = v (IDs::mix) * 0.01f;
    s.bypass = v (IDs::bypass) > 0.5f || ! usable.load();
    return s;
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    auto s = readSettings();
    if (engine.needsReconfigure (s))
        configureEngine (s.voice, s.hq);   // VOICE / MODE change the latency
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
    undo.clearUndoHistory();
    presetName = tree.getProperty ("preset", "Default").toString();
    lightTheme = tree.getProperty ("light", false);
    if (onStateRestored) onStateRestored();
}

} // namespace tune

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new tune::Processor();
}
