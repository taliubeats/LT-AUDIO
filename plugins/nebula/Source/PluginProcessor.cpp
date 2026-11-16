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
        {
            p[r->getParameterID()] = state.getRawParameterValue (r->getParameterID());
            params[r->getParameterID()] = r;
        }
    applyPreset (factoryPresets().front());
}

float Processor::ev (const juce::String& id)
{
    auto t = Modulation::targetIndex (id);
    if (t < 0) return v (id);
    auto* prm = params.at (id);
    return prm->convertFrom0to1 (mod.modulated (t, prm->convertTo0to1 (v (id))));
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

    // modulation sources for this block
    Modulation::Inputs mi;
    for (int i = 0; i < 8; ++i) mi.steps[(size_t) i] = v (IDs::mod1Step (i)) * 0.01f;
    mi.mod1Beats = Choices::stepRateBeats (juce::roundToInt (v (IDs::mod1Rate)));
    mi.mod2Beats = Choices::stepRateBeats (juce::roundToInt (v (IDs::mod2Rate)));
    mi.rndBeats = Choices::stepRateBeats (juce::roundToInt (v (IDs::rndRate)));
    mi.mod1Swing = v (IDs::mod1Swing) * 0.01f;
    mi.mod2Swing = v (IDs::mod2Swing) * 0.01f;
    mi.mod2Shape = juce::roundToInt (v (IDs::mod2Shape));
    mi.ppq = t.ppq; mi.bpm = t.bpm; mi.playing = t.playing && t.hasPosition;
    mi.inputLevel = juce::jmax (engine.getInputLevel (0), engine.getInputLevel (1));
    mi.numSamples = buffer.getNumSamples();
    mi.sampleRate = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    mod.advance (mi);

    NebulaSettings s;
    s.motion = ev (IDs::motion) * 0.01f; s.texture = ev (IDs::texture) * 0.01f; s.pitchCents = ev (IDs::pitch);
    s.space = ev (IDs::space) * 0.01f; s.dirt = ev (IDs::dirt) * 0.01f; s.mix = ev (IDs::mix) * 0.01f;
    s.sizeMs = ev (IDs::size); s.density = ev (IDs::density); s.position = ev (IDs::position) * 0.01f; s.spray = ev (IDs::spray) * 0.01f;
    s.grainPitch = ev (IDs::grainPitch); s.stereo = ev (IDs::stereo) * 0.01f; s.shape = ev (IDs::shape) * 0.01f;
    s.reverse = ev (IDs::reverse) * 0.01f; s.freeze = v (IDs::freeze) > 0.5f;
    s.stepMode = juce::roundToInt (v (IDs::stepMode));
    for (int i = 0; i < 8; ++i) s.steps[(size_t) i] = juce::roundToInt (v (IDs::step (i)));
    s.stepBeats = Choices::stepRateBeats (juce::roundToInt (v (IDs::stepRate)));
    s.glide = v (IDs::glide) * 0.01f; s.chance = v (IDs::chance) * 0.01f; s.stepAmount = v (IDs::stepAmount) * 0.01f;
    s.scaleLock = juce::roundToInt (v (IDs::scaleLock)); s.scaleKey = juce::roundToInt (v (IDs::scaleKey));
    s.tapeEveryBeats = Choices::tapeEveryBeats (juce::roundToInt (v (IDs::tapeEvery)));
    s.tapeLengthBeats = v (IDs::tapeLength); s.tapeCurve = v (IDs::tapeCurve) * 0.01f; s.tapeStart = v (IDs::tapeStart) * 0.01f;
    s.tapeChance = v (IDs::tapeChance) * 0.01f; s.tapePosition = v (IDs::tapePosition) * 0.01f;
    s.tapeManual = trigger != 0;
    s.gdTimeMs = ev (IDs::gdTime); s.gdFeedback = ev (IDs::gdFeedback) * 0.01f; s.gdHighPass = ev (IDs::gdHighPass);
    s.gdLowPass = ev (IDs::gdLowPass); s.gdMix = ev (IDs::gdMix) * 0.01f;
    s.particles = v (IDs::particles) > 0.5f; s.grainCloud = v (IDs::cloud) > 0.5f;
    s.reverseFx = v (IDs::reverseFx) > 0.5f; s.shimmer = v (IDs::shimmer) > 0.5f;
    s.input = juce::Decibels::decibelsToGain (v (IDs::input)); s.output = juce::Decibels::decibelsToGain (v (IDs::output));
    for (int i = 0; i < 8; ++i)
    {
        auto type = juce::roundToInt (v (IDs::fxSlot (i)));
        s.fxTypes[(size_t) i] = type;
        s.fxAmounts[(size_t) i] = ev (IDs::fxAmt (i)) * 0.01f;
        if (type == FxTypes::particles && s.fxAmounts[(size_t) i] > 0.01f) s.particles = true;
        if (type == FxTypes::grainCloud && s.fxAmounts[(size_t) i] > 0.01f) s.grainCloud = true;
    }
    engine.process (buffer, s, t);
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("preset", presetName, nullptr);
    tree.setProperty ("light", lightTheme, nullptr);
    tree.setProperty ("advanced", advanced, nullptr);
    tree.setProperty ("mods", juce::JSON::toString (mod.toVar(), true), nullptr);
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
    mod.fromVar (juce::JSON::parse (tree.getProperty ("mods").toString()));
    if (onStateRestored) onStateRestored();
}

} // namespace nebula

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new nebula::Processor();
}
