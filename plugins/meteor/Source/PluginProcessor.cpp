#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace meteor
{

namespace
{
    constexpr double maxSourceSeconds = 60.0;

    Buffer resample (const Buffer& in, double fromRate, double toRate)
    {
        if (std::abs (fromRate - toRate) < 0.5 || in.getNumSamples() == 0) return in;
        auto ratio = fromRate / toRate;
        auto outLen = (int) std::ceil (in.getNumSamples() / ratio);
        Buffer out (in.getNumChannels(), outLen);
        for (int c = 0; c < in.getNumChannels(); ++c)
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, in.getReadPointer (c), out.getWritePointer (c), outLen, in.getNumSamples(), 0);
        }
        return out;
    }

    Buffer toStereo (const Buffer& in)
    {
        Buffer out (2, in.getNumSamples());
        for (int c = 0; c < 2; ++c) out.copyFrom (c, 0, in, juce::jmin (c, in.getNumChannels() - 1), 0, in.getNumSamples());
        return out;
    }

    bool writeWav (const juce::File& f, const Buffer& b, double rate)
    {
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (f.createOutputStream().release());
        if (stream == nullptr) return false;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (stream.get(), rate, (unsigned int) b.getNumChannels(), 24, {}, 0));
        if (w == nullptr) return false;
        stream.release();
        return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("LT METEOR strokes"),
      state (*this, nullptr, "METEOR", createLayout())
{
    for (auto* id : IDs::live) p[id] = state.getRawParameterValue (id);
    for (auto* id : IDs::strokes) p[id] = state.getRawParameterValue (id);
    startThread (juce::Thread::Priority::low);
}

Processor::~Processor()
{
    signalThreadShouldExit();
    wake.signal();
    stopThread (4000);
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
    auto changed = std::abs (sampleRate.load() - sr) > 0.5;
    sampleRate = sr;
    engine.prepare (sr, block);
    refreshLicense();
    if (changed && hasSource()) requestRender();
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    auto in = layouts.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && (in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo()));
}

LiveSettings Processor::liveSettings() const
{
    LiveSettings s;
    s.reverseOn = v (IDs::revOn) > 0.5f;
    s.reverseSize = juce::roundToInt (v (IDs::revSize));
    s.reverseMix = v (IDs::revMix) * 0.01f;
    s.doubleOn = v (IDs::dblOn) > 0.5f;
    s.doubleVoices = juce::roundToInt (v (IDs::dblVoices));
    s.doubleDetune = v (IDs::dblDetune) * 0.01f;
    s.doubleWidth = v (IDs::dblWidth) * 0.01f;
    s.doubleMix = v (IDs::dblMix) * 0.01f;
    s.chopOn = v (IDs::chopOn) > 0.5f;
    s.chopRate = juce::roundToInt (v (IDs::chopRate));
    s.chopPattern = juce::roundToInt (v (IDs::chopPattern));
    s.chopDepth = v (IDs::chopDepth) * 0.01f;
    s.glitchOn = v (IDs::glitchOn) > 0.5f;
    s.glitchRate = juce::roundToInt (v (IDs::glitchRate));
    s.glitchChance = v (IDs::glitchChance) * 0.01f;
    s.glitchRepeats = juce::roundToInt (v (IDs::glitchRepeats));
    s.mix = v (IDs::mix) * 0.01f;
    s.outputDb = v (IDs::output);
    return s;
}

void Processor::processBlock (Buffer& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto n = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    auto s = liveSettings();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) s.bpm = *b;
            s.hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) s.ppq = *q; else s.hostPlaying = false;
        }
    bpmNow = s.bpm;

    // USE TRACK: record the incoming audio
    if (capturing.load())
    {
        auto at = captureLength.load();
        auto take = juce::jmin (n, captureBuffer.getNumSamples() - at);
        if (take > 0)
        {
            for (int c = 0; c < 2; ++c)
                captureBuffer.copyFrom (c, at, buffer, juce::jmin (c, buffer.getNumChannels() - 1), 0, take);
            captureLength = at + take;
        }
    }

    if (! usable.load()) return;
    engine.process (buffer, s);

    // stroke preview, mixed on top of the track
    if (previewIndex.load() >= 0)
    {
        juce::SpinLock::ScopedTryLockType lock (previewLock);
        if (lock.isLocked() && previewBuffer != nullptr)
        {
            auto pos = previewPos.load();
            auto take = juce::jmin (n, previewBuffer->getNumSamples() - pos);
            for (int c = 0; c < buffer.getNumChannels() && take > 0; ++c)
                buffer.addFrom (c, 0, *previewBuffer, juce::jmin (c, previewBuffer->getNumChannels() - 1), pos, take);
            previewPos = pos + juce::jmax (0, take);
            if (take < n) previewIndex = -1;
        }
    }
}

// ------------------------------------------------------------------ source

void Processor::setSource (Buffer original, double rate, const juce::String& name, const juce::File& file)
{
    auto maxLen = (int) (rate * maxSourceSeconds);
    if (original.getNumSamples() > maxLen) original.setSize (original.getNumChannels(), maxLen, true, true, false);
    auto ptr = std::make_shared<const Buffer> (toStereo (original));
    {
        const juce::ScopedLock sl (sourceLock);
        originalSource = ptr;
        displaySource = ptr;
        originalRate = rate;
        sourceName = name;
        sourceFile = file;
    }
    {
        const juce::ScopedLock rl (resultLock);
        for (auto& r : results) r.reset();
    }
    preview (-1);
    requestRender();
}

bool Processor::loadSourceFile (const juce::File& f, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr) { error = "Can't read " + f.getFileName(); return false; }
    auto len = (int) juce::jmin ((juce::int64) (reader->sampleRate * maxSourceSeconds), reader->lengthInSamples);
    if (len <= 0) { error = f.getFileName() + " is empty"; return false; }
    Buffer b ((int) juce::jmin (2u, reader->numChannels), len);
    reader->read (&b, 0, len, 0, true, b.getNumChannels() > 1);
    setSource (std::move (b), reader->sampleRate, f.getFileNameWithoutExtension(), f);
    return true;
}

void Processor::startCapture()
{
    capturing = false;
    captureRate = sampleRate.load();
    captureBuffer.setSize (2, (int) (captureRate * maxSourceSeconds), false, true, false);
    captureBuffer.clear();
    captureLength = 0;
    capturing = true;
}

void Processor::stopCapture()
{
    if (! capturing.exchange (false)) return;
    auto n = captureLength.load();
    auto loud = [&] (int i) { return std::abs (captureBuffer.getSample (0, i)) + std::abs (captureBuffer.getSample (1, i)) > 0.002f; };
    int first = 0, last = n - 1;
    while (first < n && ! loud (first)) ++first;
    while (last > first && ! loud (last)) --last;
    if (last - first < (int) (captureRate * 0.05)) { captureBuffer.setSize (0, 0); return; }
    first = juce::jmax (0, first - (int) (captureRate * 0.01));
    last = juce::jmin (n - 1, last + (int) (captureRate * 0.05));
    Buffer take (2, last - first + 1);
    for (int c = 0; c < 2; ++c) take.copyFrom (c, 0, captureBuffer, c, first, take.getNumSamples());
    captureBuffer.setSize (0, 0);

    auto name = "Capture " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
    auto file = outputFolder().getChildFile ("Captures").getChildFile (name + ".wav");
    if (! writeWav (file, take, captureRate)) file = juce::File();
    setSource (std::move (take), captureRate, name, file);
}

double Processor::getCaptureSeconds() const { return captureLength.load() / captureRate; }

bool Processor::hasSource() const
{
    const juce::ScopedLock sl (sourceLock);
    return originalSource != nullptr;
}

juce::String Processor::getSourceName() const
{
    const juce::ScopedLock sl (sourceLock);
    return sourceName;
}

BufferPtr Processor::getSourceForDisplay() const
{
    const juce::ScopedLock sl (sourceLock);
    return displaySource;
}

// ------------------------------------------------------------------ rendering

void Processor::requestRender()
{
    ++requested;
    wake.signal();
}

BufferPtr Processor::getStroke (int i) const
{
    const juce::ScopedLock rl (resultLock);
    return juce::isPositiveAndBelow (i, Strokes::count) ? results[(size_t) i] : nullptr;
}

void Processor::run()
{
    int done = 0;
    while (! threadShouldExit())
    {
        wake.wait (500);
        auto want = requested.load();
        if (want == done) continue;

        BufferPtr src;
        double rate;
        {
            const juce::ScopedLock sl (sourceLock);
            src = originalSource;
            rate = originalRate;
        }
        if (src == nullptr) { done = want; continue; }

        // small debounce: knobs being turned send many requests
        juce::Thread::sleep (120);
        if (requested.load() != want) continue;

        rendering = true;
        StrokeSettings s;
        s.sampleRate = sampleRate.load();
        s.bpm = bpmNow.load();
        s.length = v (IDs::strokeLength) * 0.01f;
        s.intensity = v (IDs::strokeIntensity) * 0.01f;
        s.tone = v (IDs::strokeTone) * 0.01f;
        auto input = resample (*src, rate, s.sampleRate);

        bool interrupted = false;
        for (int k = 0; k < Strokes::count; ++k)
        {
            if (threadShouldExit() || requested.load() != want) { interrupted = true; break; }
            auto out = std::make_shared<const Buffer> (StrokeRenderer::render (k, input, s));
            {
                const juce::ScopedLock rl (resultLock);
                results[(size_t) k] = out;
            }
            ++generation;
        }
        rendering = false;
        if (! interrupted) done = want;
        else wake.signal();
    }
}

// ------------------------------------------------------------------ preview / export

void Processor::preview (int index)
{
    auto buf = index >= 0 ? getStroke (index) : nullptr;
    BufferPtr old;
    {
        juce::SpinLock::ScopedLockType lock (previewLock);
        old = std::move (previewBuffer);
        previewBuffer = buf;
        previewPos = 0;
        previewIndex = buf != nullptr ? index : -1;
    }
    // `old` is released here, on the message thread
}

float Processor::getPreviewProgress() const
{
    auto i = previewIndex.load();
    if (i < 0) return 0.0f;
    auto b = getStroke (i);
    return b != nullptr && b->getNumSamples() > 0 ? juce::jlimit (0.0f, 1.0f, (float) previewPos.load() / (float) b->getNumSamples()) : 0.0f;
}

juce::File Processor::outputFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("LT METEOR");
}

juce::File Processor::exportStroke (int index)
{
    auto b = getStroke (index);
    if (b == nullptr || b->getNumSamples() == 0) return {};
    auto name = juce::File::createLegalFileName (getSourceName().isEmpty() ? juce::String ("Meteor") : getSourceName());
    // the settings go into the name, so a file already dragged into a project is never overwritten with other audio
    auto tag = juce::String::toHexString ((juce::int64) juce::DefaultHashFunctions::generateHash (
                   juce::String (v (IDs::strokeLength), 1) + "|" + juce::String (v (IDs::strokeIntensity), 1) + "|"
                   + juce::String (v (IDs::strokeTone), 1) + "|" + juce::String (juce::roundToInt (bpmNow.load())) + "|"
                   + juce::String (b->getNumSamples()), 1 << 20));
    auto f = outputFolder().getChildFile (name + " - " + Strokes::fileNames()[index] + " " + tag + ".wav");
    if (f.existsAsFile()) return f;
    return writeWav (f, *b, sampleRate.load()) ? f : juce::File();
}

// ------------------------------------------------------------------ state

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("preset", presetName, nullptr);
    tree.setProperty ("light", lightTheme, nullptr);
    tree.setProperty ("page", page, nullptr);
    {
        const juce::ScopedLock sl (sourceLock);
        tree.setProperty ("source", sourceFile.getFullPathName(), nullptr);
    }
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
    page = tree.getProperty ("page", 0);
    auto path = tree.getProperty ("source", {}).toString();
    if (path.isNotEmpty() && juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile())
    {
        juce::String err;
        loadSourceFile (juce::File (path), err);
    }
    if (onStateRestored) onStateRestored();
}

} // namespace meteor

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new meteor::Processor();
}
