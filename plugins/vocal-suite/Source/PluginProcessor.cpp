#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace vocal
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "VOCALSUITE", createLayout())
{
    for (auto& info : paramInfos())
        p[info.id] = state.getRawParameterValue (info.id);
    state.addParameterListener (IDs::voice, this);
}

Processor::~Processor()
{
    state.removeParameterListener (IDs::voice, this);
    cancelPendingUpdate();
}

void Processor::parameterChanged (const juce::String&, float)
{
    // the singer range changes the tuner's latency: re-prepare off the audio thread
    triggerAsyncUpdate();
}

void Processor::reconfigure()
{
    suspendProcessing (true);
    chain.prepare (sampleRate, blockSize, juce::roundToInt (v (IDs::voice)), false);
    setLatencySamples (chain.getLatency());
    suspendProcessing (false);
}

bool Processor::isPresetModified()
{
    auto i = presets.indexOf (presetName);
    return i < 0 || ! presets.all()[(size_t) i].matches (state);
}

std::map<juce::String, float> Processor::currentValues() const
{
    std::map<juce::String, float> m;
    for (auto& info : paramInfos()) m[info.id] = p.at (info.id)->load();
    return m;
}

void Processor::pushUndo()
{
    undoStack.push_back (currentValues());
    if (undoStack.size() > 50) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void Processor::applyValues (const std::map<juce::String, float>& values, bool addUndo)
{
    if (addUndo) pushUndo();
    for (auto& [id, value] : values)
        if (auto* prm = state.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
            prm->endChangeGesture();
        }
}

void Processor::applyPreset (const lt::presets::ParamPreset& preset)
{
    pushUndo();
    preset.apply (state, false);
    presetName = preset.name;
}

void Processor::undo()
{
    if (undoStack.empty()) return;
    redoStack.push_back (currentValues());
    auto s = undoStack.back(); undoStack.pop_back();
    applyValues (s, false);
}

void Processor::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back (currentValues());
    auto s = redoStack.back(); redoStack.pop_back();
    applyValues (s, false);
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
    sampleRate = sr;
    blockSize = block;
    chain.prepare (sr, block, juce::roundToInt (v (IDs::voice)), false);
    setLatencySamples (chain.getLatency());
    refreshLicense();
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && layouts.getMainInputChannelSet() == out;
}

ChainSettings Processor::readSettings (double bpm) const
{
    ChainSettings s;
    s.lowCutHz = v (IDs::lowCut);
    s.gateDb = v (IDs::gate);
    s.deEss = v (IDs::deEss) * 0.01f;
    s.deEssHz = v (IDs::deEssFreq);

    s.tuneOn = v (IDs::tuneOn) > 0.5f;
    s.tune.mask = tune::Scales::mask (juce::roundToInt (v (IDs::key)), juce::roundToInt (v (IDs::scale)));
    s.tune.voice = juce::roundToInt (v (IDs::voice));
    s.tune.retuneMs = v (IDs::retune);
    s.tune.humanize = v (IDs::humanize) * 0.01f;
    s.tune.flex = v (IDs::flex) * 0.01f;
    s.tune.mix = v (IDs::tuneMix) * 0.01f;
    s.tune.preserve = true;

    s.body = v (IDs::body);
    s.mudCut = v (IDs::mud);
    s.presence = v (IDs::presence);
    s.air = v (IDs::air);

    s.comp = v (IDs::comp) * 0.01f;
    s.compAttackMs = v (IDs::attack);
    s.compReleaseMs = v (IDs::release);
    s.sat.type = juce::roundToInt (v (IDs::satType));
    s.sat.drive = v (IDs::satDrive) * 0.01f;
    s.sat.mix = v (IDs::satMix) * 0.01f;
    s.sat.character = 0.4f;
    s.sat.autoGain = true;

    s.doubler = v (IDs::doubler) * 0.01f;
    s.phone = v (IDs::phone) * 0.01f;
    s.width = v (IDs::width) * 0.01f;

    s.delayOn = v (IDs::delayOn) > 0.5f;
    s.delay.mode = juce::roundToInt (v (IDs::delayMode));
    s.delay.delaySeconds = DelayNotes::beats (juce::roundToInt (v (IDs::delayNote))) * 60.0 / juce::jlimit (40.0, 300.0, bpm);
    s.delay.feedback = v (IDs::delayFeedback) * 0.01f;
    s.delay.mix = v (IDs::delayMix) * 0.01f;
    s.delay.lowCut = 280.0f; s.delay.highCut = 7500.0f;      // vocal-friendly echoes
    s.delay.duck = 0.35f;  s.delay.wobble = 0.15f; s.delay.drive = 0.1f; s.delay.width = 1.1f;

    s.reverbOn = v (IDs::reverbOn) > 0.5f;
    s.reverb.type = juce::roundToInt (v (IDs::reverbType));
    s.reverb.decay = v (IDs::reverbDecay);
    s.reverb.preDelayMs = v (IDs::reverbPre);
    s.reverb.mix = v (IDs::reverbMix) * 0.01f;
    s.reverb.size = juce::jlimit (0.3f, 1.0f, 0.45f + 0.06f * s.reverb.decay);
    s.reverb.lowCut = 220.0f; s.reverb.highCut = 10000.0f;
    s.reverb.duck = 0.25f;

    s.outputDb = v (IDs::output);
    s.bypass = v (IDs::bypass) > 0.5f;
    return s;
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (! usable.load()) return;

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;

    auto s = readSettings (bpm);
    if (chain.needsReconfigure (s)) return;      // waiting for reconfigure() on the message thread
    chain.process (buffer, s);
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("preset", presetName, nullptr);
    tree.setProperty ("light", lightTheme, nullptr);
    tree.setProperty ("lastMessage", lastAssistantMessage, nullptr);
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
    lastAssistantMessage = tree.getProperty ("lastMessage", "").toString();
    if (onStateRestored) onStateRestored();
}

} // namespace vocal

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new vocal::Processor();
}
