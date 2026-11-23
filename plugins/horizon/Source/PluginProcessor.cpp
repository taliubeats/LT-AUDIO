#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace horizon
{

// ------------------------------------------------------------------ global settings

juce::PropertiesFile& GlobalSettings::get()
{
    static juce::PropertiesFile file ([]
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "LT HORIZON";
        o.filenameSuffix = ".settings";
        o.folderName = "LT Audio";
        o.osxLibrarySubFolder = "Application Support";
        o.millisecondsBeforeSaving = 0;
        return o;
    }());
    return file;
}

juce::File GlobalSettings::exportFolder()
{
    auto fallback = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("LT Horizon");
    auto path = get().getValue ("exportFolder", fallback.getFullPathName());
    return juce::File::isAbsolutePath (path) ? juce::File (path) : fallback;
}

void GlobalSettings::setExportFolder (const juce::File& f) { get().setValue ("exportFolder", f.getFullPathName()); }
bool GlobalSettings::bpmInName()                           { return get().getBoolValue ("bpmInName", true); }
void GlobalSettings::setBpmInName (bool b)                 { get().setValue ("bpmInName", b); }
double GlobalSettings::defaultLength()                     { return get().getDoubleValue ("defaultLength", Recorder::defaultSeconds); }
void GlobalSettings::setDefaultLength (double s)           { get().setValue ("defaultLength", s); }

// ------------------------------------------------------------------ processor

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    wantedLength = juce::jlimit (Recorder::minSeconds, Recorder::maxSeconds, GlobalSettings::defaultLength());
}

void Processor::prepareToPlay (double sr, int)
{
    recorder.prepare (sr, wantedLength.load());
    stopPreview();
    refreshLicense();
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    auto in = layouts.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && (in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo()));
}

void Processor::setLength (double seconds)
{
    seconds = juce::jlimit (Recorder::minSeconds, Recorder::maxSeconds, seconds);
    wantedLength = seconds;
    recorder.setLength (seconds);
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto numIn = getTotalNumInputChannels();
    auto n = buffer.getNumSamples();
    if (numIn == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm.store (*bpm);

    // 1. record what came in (before the preview is added, so the preview is never re-recorded)
    if (usable.load())
        recorder.push (buffer.getArrayOfReadPointers(), juce::jmin (2, buffer.getNumChannels()), n);

    // 2. preview
    auto pp = previewPos.load();
    if (pp >= 0)
    {
        auto end = previewEnd.load();
        auto want = (int) juce::jmin ((juce::int64) n, end - pp);
        auto got = want > 0 ? recorder.readForPreview (pp, buffer.getArrayOfWritePointers(), buffer.getNumChannels(), want) : 0;
        if (got <= 0 || pp + got >= end) previewPos.compare_exchange_strong (pp, -1);
        else                             previewPos.compare_exchange_strong (pp, pp + got);
    }
}

void Processor::startPreview (juce::Range<juce::int64> r)
{
    r = recorder.clampToAvailable (r);
    if (r.isEmpty()) { stopPreview(); return; }
    previewPos.store (-1);
    previewEnd.store (r.getEnd());
    previewPos.store (r.getStart());
}

juce::File Processor::exportRange (juce::Range<juce::int64> r, juce::String& error)
{
    auto audio = recorder.read (r);
    if (audio.getNumSamples() == 0) { error = "That audio is no longer in the buffer."; return {}; }

    auto folder = GlobalSettings::exportFolder();
    if (! folder.createDirectory()) { error = "Can't create " + folder.getFullPathName(); return {}; }
    auto file = folder.getChildFile (Recorder::makeFileName (hostBpm.load(), GlobalSettings::bpmInName(), juce::Time::getCurrentTime()))
                      .getNonexistentSibling (false);
    if (! Recorder::writeWav (file, audio, recorder.getSampleRate())) { error = "Can't write " + file.getFullPathName(); return {}; }
    return file;
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree tree ("HORIZON");
    tree.setProperty ("length", wantedLength.load(), nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, dest);
}

void Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("HORIZON")) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    auto len = (double) tree.getProperty ("length", Recorder::defaultSeconds);
    wantedLength = juce::jlimit (Recorder::minSeconds, Recorder::maxSeconds, len);
    if (juce::MessageManager::existsAndIsCurrentThread()) setLength (len);
    else triggerAsyncUpdate();                          // some hosts restore state off the message thread
}

} // namespace horizon

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new horizon::Processor();
}
