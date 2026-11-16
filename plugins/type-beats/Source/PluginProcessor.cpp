#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace typebeats
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("LT TYPE BEATS analysis")
{
    formats.registerBasicFormats();
}

Processor::~Processor()
{
    *alive = false;
    stopThread (4000);
}

juce::File Processor::getDownloadFolder() const
{
    if (downloadFolder != juce::File())
        return downloadFolder;
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("LT Type Beats");
}

void Processor::setStatus (const juce::String& s)
{
    {
        const juce::ScopedLock sl (statusLock);
        status = s;
    }
    juce::MessageManager::callAsync ([this, token = alive] { if (*token && onResultChanged) onResultChanged(); });
}

void Processor::loadFile (const juce::File& f)
{
    if (! license.isUsable())
    {
        setStatus ("Your trial has ended. Activate your license to analyse songs.");
        return;
    }

    stopThread (4000);
    previewing = false;
    pendingFile = f;
    analysing = true;
    progress = 0.0f;
    setStatus ("Analysing " + f.getFileName() + "...");
    startThread (juce::Thread::Priority::low);
}

void Processor::run()
{
    auto file = pendingFile;
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        analysing = false;
        setStatus ("Could not open " + file.getFileName());
        return;
    }

    // keep up to 10 minutes
    auto maxSamples = (juce::int64) (reader->sampleRate * 600.0);
    auto n = (int) juce::jmin (reader->lengthInSamples, maxSamples);
    juce::AudioBuffer<float> audio ((int) juce::jlimit (1u, 2u, reader->numChannels), n);
    reader->read (&audio, 0, n, 0, true, audio.getNumChannels() > 1);

    auto r = Analyzer::analyse (audio, reader->sampleRate,
                                [this] { return threadShouldExit(); },
                                [this] (float p) { progress = p; });

    if (threadShouldExit())
    {
        analysing = false;
        return;
    }

    {
        const juce::ScopedLock sl (previewLock);
        fileAudio = std::move (audio);
        fileRate = reader->sampleRate;
    }

    juce::MessageManager::callAsync ([this, token = alive, file, r]
    {
        if (! *token) return;
        loadedFile = file;
        result = r;
        preparePreview();
        analysing = false;
        setStatus (r.isValid() ? juce::String() : "Could not analyse this file.");
    });
}

void Processor::preparePreview()
{
    const juce::ScopedLock sl (previewLock);
    if (fileAudio.getNumSamples() == 0)
    {
        previewAudio.setSize (0, 0);
        return;
    }

    auto ratio = fileRate / hostRate;
    auto outN = (int) (fileAudio.getNumSamples() / ratio);
    previewAudio.setSize (2, outN, false, false, true);
    for (int c = 0; c < 2; ++c)
    {
        juce::LagrangeInterpolator interp;
        interp.process (ratio, fileAudio.getReadPointer (juce::jmin (c, fileAudio.getNumChannels() - 1)),
                        previewAudio.getWritePointer (c), outN);
    }
    previewPos = 0;
}

void Processor::togglePreview (double fromSeconds)
{
    if (previewing)
    {
        previewing = false;
        return;
    }
    previewPos = (juce::int64) (juce::jmax (0.0, fromSeconds) * hostRate);
    previewing = previewAudio.getNumSamples() > 0;
}

double Processor::getPreviewPosition() const noexcept
{
    return (double) previewPos.load() / hostRate;
}

void Processor::prepareToPlay (double sr, int)
{
    if (std::abs (sr - hostRate) > 0.5)
    {
        auto seconds = getPreviewPosition();
        hostRate = sr;
        preparePreview();
        previewPos = (juce::int64) (seconds * hostRate);
    }
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

    if (! previewing.load())
        return;

    const juce::ScopedTryLock sl (previewLock);
    if (! sl.isLocked())
        return;

    auto pos = previewPos.load();
    auto available = (juce::int64) previewAudio.getNumSamples() - pos;
    if (available <= 0)
    {
        previewing = false;
        return;
    }

    auto n = (int) juce::jmin ((juce::int64) buffer.getNumSamples(), available);
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        buffer.addFrom (c, 0, previewAudio, juce::jmin (c, previewAudio.getNumChannels() - 1), (int) pos, n, 0.8f);

    previewPos = pos + n;
}

juce::AudioProcessorEditor* Processor::createEditor()
{
    return new Editor (*this);
}

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("version", 1);
    obj->setProperty ("light", lightTheme);
    obj->setProperty ("file", loadedFile.getFullPathName());
    obj->setProperty ("downloads", downloadFolder.getFullPathName());
    obj->setProperty ("result", result.toVar());
    auto json = juce::JSON::toString (juce::var (obj.release()), true);
    dest.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void Processor::setStateInformation (const void* data, int size)
{
    auto v = juce::JSON::parse (juce::String::fromUTF8 ((const char*) data, size));
    if (! v.isObject())
        return;

    lightTheme = v["light"];
    auto dl = v["downloads"].toString();
    downloadFolder = dl.isNotEmpty() ? juce::File (dl) : juce::File();
    loadedFile = juce::File (v["file"].toString());
    result = AnalysisResult::fromVar (v["result"]);

    // reload audio for preview / dragging, without re-analysing
    if (loadedFile.existsAsFile())
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (loadedFile));
        if (reader != nullptr)
        {
            const juce::ScopedLock sl (previewLock);
            auto n = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 600.0));
            fileAudio.setSize ((int) juce::jlimit (1u, 2u, reader->numChannels), n);
            reader->read (&fileAudio, 0, n, 0, true, fileAudio.getNumChannels() > 1);
            fileRate = reader->sampleRate;
        }
        preparePreview();
    }

    if (onResultChanged) onResultChanged();
}

} // namespace typebeats

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new typebeats::Processor();
}
