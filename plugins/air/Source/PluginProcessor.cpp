#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace air
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, &undo, "AIR", createLayout())
{
    for (auto* id : IDs::presetParams)
        p[id] = state.getRawParameterValue (id);
    p[IDs::bypass] = state.getRawParameterValue (IDs::bypass);
    p[IDs::autoGain] = state.getRawParameterValue (IDs::autoGain);
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

void Processor::prepareToPlay (double sr, int block)
{
    engine.prepare (sr, block, juce::jmax (1, juce::jmin (2, getTotalNumOutputChannels())));
    spectrum.setSampleRate (sr);
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

AirSettings Processor::currentSettings() const
{
    AirSettings s;
    for (int i = 0; i < 3; ++i)
    {
        s.amount[i] = v (IDs::amount[(size_t) i]) * 0.01f;
        s.model[i] = juce::roundToInt (v (IDs::model[(size_t) i]));
    }
    s.trimDb = v (IDs::trim);
    s.mix = v (IDs::mix) * 0.01f;
    s.outputDb = v (IDs::output);
    s.autoGain = v (IDs::autoGain) > 0.5f;
    s.bypass = v (IDs::bypass) > 0.5f || ! usable.load();
    return s;
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
    engine.process (buffer, currentSettings());
    spectrum.push (buffer);
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

// ------------------------------------------------------------------ A | B

Processor::Snapshot Processor::capture() const
{
    Snapshot s;
    for (auto* id : IDs::presetParams) s[id] = v (id);
    return s;
}

void Processor::restore (const Snapshot& s)
{
    for (auto& [id, value] : s) setParam (state, id, value);
}

void Processor::switchAB (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == abActive) return;
    ab[(size_t) abActive] = capture();
    abPreset[(size_t) abActive] = presetName;
    if (ab[(size_t) slot].empty())          // first time: B starts as a copy of A
    {
        ab[(size_t) slot] = ab[(size_t) abActive];
        abPreset[(size_t) slot] = presetName;
    }
    undo.beginNewTransaction();
    restore (ab[(size_t) slot]);
    presetName = abPreset[(size_t) slot];
    abActive = slot;
    undo.beginNewTransaction();
}

juce::String Processor::toString (const Snapshot& s)
{
    juce::StringArray parts;
    for (auto& [id, value] : s) parts.add (id + "=" + juce::String (value, 3));
    return parts.joinIntoString (";");
}

Processor::Snapshot Processor::fromString (const juce::String& text)
{
    Snapshot s;
    for (auto& part : juce::StringArray::fromTokens (text, ";", ""))
    {
        auto id = part.upToFirstOccurrenceOf ("=", false, false).trim();
        for (auto* known : IDs::presetParams)
            if (id == known)
                s[id] = part.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
    }
    return s;
}

// ------------------------------------------------------------------ state

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("preset", presetName, nullptr);
    tree.setProperty ("ab", abActive, nullptr);
    auto other = 1 - abActive;
    tree.setProperty ("abOther", toString (ab[(size_t) other]), nullptr);
    tree.setProperty ("abOtherPreset", abPreset[(size_t) other], nullptr);
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
    presetName = tree.getProperty ("preset", "Silky Pop Vocal").toString();
    abActive = juce::jlimit (0, 1, (int) tree.getProperty ("ab", 0));
    auto other = 1 - abActive;
    ab[(size_t) other] = fromString (tree.getProperty ("abOther", "").toString());
    abPreset[(size_t) other] = tree.getProperty ("abOtherPreset", "").toString();
    ab[(size_t) abActive].clear();
    if (onStateRestored) onStateRestored();
}

} // namespace air

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new air::Processor();
}
