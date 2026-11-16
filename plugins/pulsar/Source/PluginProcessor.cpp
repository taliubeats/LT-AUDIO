#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace pulsar
{

juce::AudioProcessorValueTreeState::ParameterLayout Processor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 75.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")
                                                           .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::rate, 1 }, "Rate", Rates::names(), Rates::quarter));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::midi, 1 }, "MIDI Trigger", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::audio, 1 }, "Audio Trigger", false));
    lt::dsp::FocusParams::addTo (layout);
    return layout;
}

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                                       .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      state (*this, nullptr, "PULSAR", createLayout())
{
    mixParam = state.getRawParameterValue (IDs::mix);
    rateParam = state.getRawParameterValue (IDs::rate);
    midiParam = state.getRawParameterValue (IDs::midi);
    audioParam = state.getRawParameterValue (IDs::audio);
    focus.attach (state);

    applyPreset (PresetLibrary::factory().front());
}

void Processor::setCurve (const Curve& c)
{
    curve = c;
    engine.setCurve (curve);
}

bool Processor::isPresetModified() const
{
    return curve != loaded.curve
        || std::abs (mixParam->load() - loaded.mix) > 0.05f
        || juce::roundToInt (rateParam->load()) != loaded.rate;
}

void Processor::applyPreset (const Preset& p)
{
    auto set = [this] (const char* id, float value)
    {
        if (auto* param = state.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (value));
            param->endChangeGesture();
        }
    };
    set (IDs::mix, p.mix);
    set (IDs::rate, (float) p.rate);
    setCurve (p.curve);
    loaded = p;
}

Preset Processor::makePreset (const juce::String& name) const
{
    Preset p;
    p.name = name;
    p.mix = mixParam->load();
    p.rate = juce::roundToInt (rateParam->load());
    p.curve = curve;
    p.factory = false;
    return p;
}

int Processor::getMatchingShape() const
{
    for (int i = 0; i < Curve::numShapes; ++i)
    {
        auto s = Curve::shape (i);
        s.setOffset (curve.getOffset());
        if (s == curve)
            return i;
    }
    return -1;
}

// ------------------------------------------------------------------ programs = presets (for hosts that list them)

int Processor::getNumPrograms()                       { return (int) PresetLibrary::factory().size(); }
int Processor::getCurrentProgram()                    { return juce::jmax (0, presets.indexOf (loaded.name)); }
const juce::String Processor::getProgramName (int i)  { auto f = PresetLibrary::factory(); return juce::isPositiveAndBelow (i, (int) f.size()) ? f[(size_t) i].name : juce::String(); }

void Processor::setCurrentProgram (int i)
{
    auto f = PresetLibrary::factory();
    if (juce::isPositiveAndBelow (i, (int) f.size()) && f[(size_t) i].name != loaded.name)
    {
        applyPreset (f[(size_t) i]);
        if (onStateRestored) onStateRestored();
    }
}

// ------------------------------------------------------------------ audio

void Processor::prepareToPlay (double sr, int)
{
    engine.prepare (sr, juce::jmax (1, getMainBusNumOutputChannels()));
    license.refresh();
    usable = license.isUsable();
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    auto mainOk = (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
                  && layouts.getMainInputChannelSet() == out;
    if (layouts.inputBuses.size() > 1)
    {
        auto sc = layouts.getChannelSet (true, 1);
        mainOk = mainOk && (sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo());
    }
    return mainOk;
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    auto main = getBusBuffer (buffer, false, 0);
    juce::AudioBuffer<float> sidechain;
    auto hasSidechain = getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled()
                        && getBus (true, 1)->getNumberOfChannels() > 0;
    if (hasSidechain)
        sidechain = getBusBuffer (buffer, true, 1);

    DuckEngine::Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            t.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) { t.ppq = *ppq; t.hasPosition = true; }
        }
    hostBpm = (float) t.bpm;
    hostPlaying = t.playing;

    if (! usable.load())
    {
        midi.clear();
        return; // trial over: audio passes untouched
    }

    DuckEngine::Settings s;
    s.mix = mixParam->load() * 0.01f;
    s.rate = juce::roundToInt (rateParam->load());
    s.midiTrigger = midiParam->load() > 0.5f;
    s.audioTrigger = audioParam->load() > 0.5f;

    engine.process (main, hasSidechain ? &sidechain : nullptr, midi, s, t, focus.read());
    midi.clear();
}

juce::AudioProcessorEditor* Processor::createEditor()
{
    return new Editor (*this);
}

// ------------------------------------------------------------------ state

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("version", 1, nullptr);
    tree.setProperty ("curve", juce::JSON::toString (curve.toVar(), true), nullptr);
    tree.setProperty ("preset", juce::JSON::toString (loaded.toVar(), true), nullptr);
    tree.setProperty ("light", lightTheme, nullptr);
    tree.setProperty ("wave", showWave, nullptr);
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

    auto c = Curve::fromVar (juce::JSON::parse (tree.getProperty ("curve").toString()));
    setCurve (c);
    auto pv = juce::JSON::parse (tree.getProperty ("preset").toString());
    loaded = pv.isObject() ? Preset::fromVar (pv) : makePreset ("Default");
    lightTheme = tree.getProperty ("light", false);
    showWave = tree.getProperty ("wave", true);

    if (onStateRestored) onStateRestored();
}

} // namespace pulsar

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new pulsar::Processor();
}
