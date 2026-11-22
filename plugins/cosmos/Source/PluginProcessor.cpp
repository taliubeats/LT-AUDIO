#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace cosmos
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    meter.prepare (48000.0, 2);
}

void Processor::prepareToPlay (double sr, int)
{
    currentRate = sr;
    meter.prepare (sr, juce::jmax (1, getTotalNumInputChannels()));
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
    const auto n = buffer.getNumSamples();
    const auto inCh = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());

    // host timing (bar position of the first sample of this block)
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            Timing t;
            auto bpm = pos->getBpm();
            auto ppq = pos->getPpqPosition();
            t.valid = bpm.hasValue() && ppq.hasValue();
            t.playing = pos->getIsPlaying();
            t.bpm = bpm.orFallback (120.0);
            t.ppq = ppq.orFallback (0.0);
            if (auto sig = pos->getTimeSignature()) { t.num = sig->numerator; t.den = sig->denominator; }
            auto barLen = t.num * 4.0 / juce::jmax (1, t.den);
            t.barStart = pos->getPpqPositionOfLastBarStart().orFallback (std::floor (t.ppq / barLen) * barLen);
            t.atWritten = samplesWritten.load();
            const juce::SpinLock::ScopedTryLockType lock (timingLock);
            if (lock.isLocked()) timing = t;
        }
    }

    if (inCh > 0 && usable.load() && ! externalSource.load())
        feed (buffer.getArrayOfReadPointers(), inCh, n);

    for (int c = inCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    // the app listens to an input: never play it back out (no feedback, no doubled audio)
    if (isStandalone()) buffer.clear();
}

void Processor::feed (const float* const* ch, int numCh, int n)
{
    meter.process (ch, numCh, n);
    int s1, n1, s2, n2;
    fifo.prepareToWrite (n, s1, n1, s2, n2);
    auto* l = ch[0];
    auto* r = ch[numCh > 1 ? 1 : 0];
    if (n1 > 0) { fifoBuffer.copyFrom (0, s1, l, n1); fifoBuffer.copyFrom (1, s1, r, n1); }
    if (n2 > 0) { fifoBuffer.copyFrom (0, s2, l + n1, n2); fifoBuffer.copyFrom (1, s2, r + n1, n2); }
    fifo.finishedWrite (n1 + n2);
    samplesWritten += n1 + n2;
}

juce::String Processor::startSystemAudio()
{
    externalSource = true;   // the input device stops feeding the meter from now on
    auto err = system.start ([this] (const float* l, const float* r, int n, double rate)
    {
        if (std::abs (rate - currentRate.load()) > 1.0)
        {
            currentRate = rate;          // the editor sees the new rate and restarts its analysis
            meter.prepare (rate, 2);     // once, on the first block at a new rate
        }
        const float* ch[2] = { l, r };
        if (usable.load()) feed (ch, 2, n);
    });
    if (! err.empty()) externalSource = false;
    return juce::String (err);
}

void Processor::stopSystemAudio()
{
    system.stop();
    externalSource = false;
    currentRate = getSampleRate() > 0 ? getSampleRate() : 48000.0;
    meter.prepare (currentRate.load(), juce::jmax (1, getTotalNumInputChannels()));
}

int Processor::readAudio (float* l, float* r, int maxSamples)
{
    int s1, n1, s2, n2;
    fifo.prepareToRead (maxSamples, s1, n1, s2, n2);
    if (n1 > 0) { std::copy_n (fifoBuffer.getReadPointer (0, s1), n1, l); std::copy_n (fifoBuffer.getReadPointer (1, s1), n1, r); }
    if (n2 > 0) { std::copy_n (fifoBuffer.getReadPointer (0, s2), n2, l + n1); std::copy_n (fifoBuffer.getReadPointer (1, s2), n2, r + n1); }
    fifo.finishedRead (n1 + n2);
    samplesRead += n1 + n2;
    return n1 + n2;
}

void Processor::flushAudio()
{
    auto ready = fifo.getNumReady();
    int s1, n1, s2, n2;
    fifo.prepareToRead (ready, s1, n1, s2, n2);
    fifo.finishedRead (n1 + n2);
    samplesRead += n1 + n2;
}

Processor::Timing Processor::getTiming() const
{
    const juce::SpinLock::ScopedLockType lock (timingLock);
    return timing;
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree tree ("COSMOS");
    tree.setProperty ("layout", layout, nullptr);
    tree.setProperty ("skin", skin, nullptr);
    tree.setProperty ("target", target, nullptr);
    tree.setProperty ("width", width, nullptr);
    tree.setProperty ("height", height, nullptr);
    tree.setProperty ("pinned", pinned, nullptr);
    tree.setProperty ("system", wantSystemAudio, nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, dest);
}

void Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("COSMOS")) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    layout = tree.getProperty ("layout", layout).toString();
    skin = tree.getProperty ("skin", 0);
    target = tree.getProperty ("target", -14);
    width = tree.getProperty ("width", 0);
    height = tree.getProperty ("height", 0);
    pinned = tree.getProperty ("pinned", false);
    wantSystemAudio = tree.getProperty ("system", true);
    if (onStateRestored) onStateRestored();
}

} // namespace cosmos

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cosmos::Processor();
}
