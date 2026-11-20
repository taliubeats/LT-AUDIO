#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace lyra
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

static void setBit (std::array<std::atomic<juce::uint64>, 2>& bits, int note, bool on)
{
    auto& w = bits[(size_t) (note >> 6)];
    auto mask = (juce::uint64) 1 << (note & 63);
    if (on) w.fetch_or (mask); else w.fetch_and (~mask);
}

void Processor::handle (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        auto n = m.getNoteNumber();
        setBit (held, n, true);
        setBit (sustained, n, true);
        velocity[(size_t) n] = m.getVelocity();
    }
    else if (m.isNoteOff())
    {
        auto n = m.getNoteNumber();
        setBit (held, n, false);
        if (! pedal.load()) setBit (sustained, n, false);
    }
    else if (m.isSustainPedalOn())
    {
        pedal = true;
    }
    else if (m.isSustainPedalOff())
    {
        pedal = false;
        for (size_t i = 0; i < 2; ++i) sustained[i] = held[i].load();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (size_t i = 0; i < 2; ++i) { held[i] = 0; sustained[i] = 0; }
    }
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
        handle (meta.getMessage());
    buffer.clear();   // never makes sound
}

std::array<bool, 128> Processor::getSounding() const
{
    std::array<bool, 128> out {};
    auto& src = usePedal ? sustained : held;
    for (int n = 0; n < 128; ++n)
        out[(size_t) n] = ((src[(size_t) (n >> 6)].load() >> (n & 63)) & 1) != 0;
    return out;
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree t ("LYRA");
    t.setProperty ("key", key, nullptr);
    t.setProperty ("spelling", spelling, nullptr);
    t.setProperty ("theme", theme, nullptr);
    t.setProperty ("colour", colour, nullptr);
    t.setProperty ("language", language, nullptr);
    t.setProperty ("pedal", usePedal, nullptr);
    t.setProperty ("width", width, nullptr);
    if (auto xml = t.createXml()) copyXmlToBinary (*xml, dest);
}

void Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("LYRA")) return;
    auto t = juce::ValueTree::fromXml (*xml);
    key = juce::jlimit (-1, 23, (int) t.getProperty ("key", -1));
    spelling = juce::jlimit (0, 2, (int) t.getProperty ("spelling", 0));
    theme = juce::jlimit (0, 1, (int) t.getProperty ("theme", 0));
    colour = juce::jlimit (0, 7, (int) t.getProperty ("colour", 0));
    language = juce::jlimit (0, 3, (int) t.getProperty ("language", 0));
    usePedal = t.getProperty ("pedal", true);
    width = t.getProperty ("width", 0);
    if (onStateRestored) onStateRestored();
}

} // namespace lyra

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new lyra::Processor();
}
