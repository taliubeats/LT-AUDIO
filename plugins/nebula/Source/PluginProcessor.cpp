#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace nebula
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "NEBULA", createLayout())
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            p[r->getParameterID()] = state.getRawParameterValue (r->getParameterID());
    applyPreset (factoryPresets().front());
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

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            t.playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) t.bpm = *b;
            if (auto q = pos->getPpqPosition()) { t.ppq = *q; t.hasPosition = true; }
        }

    auto trigger = juce::roundToInt (v (IDs::tapeTrigger));
    if (trigger == 2)
        for (auto m : midi)
            if (m.getMessage().isNoteOn()) engine.triggerTapeStop();
    midi.clear();
    if (! usable.load()) return;

    NebulaSettings s;
    s.motion = v (IDs::motion) * 0.01f; s.texture = v (IDs::texture) * 0.01f; s.pitchCents = v (IDs::pitch);
    s.space = v (IDs::space) * 0.01f; s.dirt = v (IDs::dirt) * 0.01f; s.mix = v (IDs::mix) * 0.01f;
    s.sizeMs = v (IDs::size); s.density = v (IDs::density); s.position = v (IDs::position) * 0.01f; s.spray = v (IDs::spray) * 0.01f;
    s.grainPitch = v (IDs::grainPitch); s.stereo = v (IDs::stereo) * 0.01f; s.shape = v (IDs::shape) * 0.01f;
    s.reverse = v (IDs::reverse) * 0.01f; s.freeze = v (IDs::freeze) > 0.5f;
    s.stepMode = juce::roundToInt (v (IDs::stepMode));
    for (int i = 0; i < 8; ++i) s.steps[(size_t) i] = juce::roundToInt (v (IDs::step (i)));
    s.stepBeats = Choices::stepRateBeats (juce::roundToInt (v (IDs::stepRate)));
    s.glide = v (IDs::glide) * 0.01f; s.chance = v (IDs::chance) * 0.01f; s.stepAmount = v (IDs::stepAmount) * 0.01f;
    s.scaleLock = juce::roundToInt (v (IDs::scaleLock)); s.scaleKey = juce::roundToInt (v (IDs::scaleKey));
    s.tapeEveryBeats = Choices::tapeEveryBeats (juce::roundToInt (v (IDs::tapeEvery)));
    s.tapeLengthBeats = v (IDs::tapeLength); s.tapeCurve = v (IDs::tapeCurve) * 0.01f; s.tapeStart = v (IDs::tapeStart) * 0.01f;
    s.tapeChance = v (IDs::tapeChance) * 0.01f; s.tapePosition = v (IDs::tapePosition) * 0.01f;
    s.tapeManual = trigger != 0;
    s.gdTimeMs = v (IDs::gdTime); s.gdFeedback = v (IDs::gdFeedback) * 0.01f; s.gdHighPass = v (IDs::gdHighPass);
    s.gdLowPass = v (IDs::gdLowPass); s.gdMix = v (IDs::gdMix) * 0.01f;
    s.particles = v (IDs::particles) > 0.5f; s.grainCloud = v (IDs::cloud) > 0.5f;
    s.reverseFx = v (IDs::reverseFx) > 0.5f; s.shimmer = v (IDs::shimmer) > 0.5f;
    s.input = juce::Decibels::decibelsToGain (v (IDs::input)); s.output = juce::Decibels::decibelsToGain (v (IDs::output));
    engine.process (buffer, s, t);
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("preset", presetName, nullptr);
    tree.setProperty ("light", lightTheme, nullptr);
    tree.setProperty ("advanced", advanced, nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, dest);
}

void Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    state.replaceState (tree);
    presetName = tree.getProperty ("preset", "Stardust Keys").toString();
    lightTheme = tree.getProperty ("light", false);
    advanced = tree.getProperty ("advanced", false);
    if (onStateRestored) onStateRestored();
}

} // namespace nebula

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new nebula::Processor();
}
