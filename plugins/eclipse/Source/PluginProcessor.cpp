#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DrumSeparator.h"

namespace eclipse
{

namespace
{
}

juce::AudioProcessor::BusesProperties Processor::makeBuses()
{
    auto b = BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true);
    for (int f = 0; f < numFamilies; ++f)
        b = b.withOutput (familyLabel ((Family) f), juce::AudioChannelSet::stereo(), true);
    return b;
}

namespace
{
Family guessFamily (const juce::String& name)
{
    auto n = name.toLowerCase();
    if (n.contains ("kick") || n.contains ("bombo") || n.contains ("bd")) return Family::kick;
    if (n.contains ("snare") || n.contains ("sn ") || n.contains ("rim")) return Family::snare;
    if (n.contains ("clap") || n.contains ("palma")) return Family::clap;
    if (n.contains ("open")) return Family::openHat;
    if (n.contains ("hat") || n.contains ("hh")) return Family::hat;
    if (n.contains ("tom")) return Family::tom;
    if (n.contains ("crash") || n.contains ("ride") || n.contains ("cym")) return Family::cymbal;
    if (n.contains ("perc") || n.contains ("conga") || n.contains ("bongo") || n.contains ("shaker")) return Family::perc;
    return Family::sample;
}

juce::File tempFolder()
{
    auto d = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LT Eclipse");
    d.createDirectory();
    return d;
}

juce::String safeName (const juce::String& s) { return juce::File::createLegalFileName (s.trim().isEmpty() ? juce::String ("Untitled") : s.trim()); }
}

// ================================================================== construction
Processor::Processor()
    : AudioProcessor (makeBuses()),
      juce::Thread ("LT Eclipse clone"),
      state (*this, nullptr, "ECLIPSE", createLayout())
{
    for (int i = 0; i < numPads; ++i)
        for (int k = 0; k < numPadParams; ++k)
            padPtrs[(size_t) i].v[(size_t) k] = state.getRawParameterValue (padParamId (i, (PadParam) k));
    outputDb = state.getRawParameterValue (IDs::output);
    sensitivity = state.getRawParameterValue (IDs::sensitivity);
    previewPattern = std::make_shared<Pattern>();
    startThread (juce::Thread::Priority::low);
}

Processor::~Processor()
{
    cancelJob = true;
    signalThreadShouldExit();
    notify();
    stopThread (10000);
}

// ================================================================== audio
void Processor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;
    sampler.prepare (sr, block);
    refreshLicense();
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto& s = layouts.outputBuses.getReference (i);
        if (! s.isDisabled() && s != juce::AudioChannelSet::stereo()) return false;
    }
    return layouts.inputBuses.isEmpty() || layouts.getMainInputChannelSet().isDisabled();
}

void Processor::readPadSettings()
{
    for (int i = 0; i < numPads; ++i)
    {
        const auto& p = padPtrs[(size_t) i];
        auto& s = padSettings[(size_t) i];
        s.gainDb = p.get (PadParam::gain);   s.pan = p.get (PadParam::pan);     s.tune = p.get (PadParam::tune);
        s.attackMs = p.get (PadParam::attack); s.fade = p.get (PadParam::fade); s.hp = p.get (PadParam::hp);
        s.lp = p.get (PadParam::lp);         s.clean = p.get (PadParam::clean); s.vibe = p.get (PadParam::vibe);
        s.choke = juce::roundToInt (p.get (PadParam::choke)); s.out = juce::roundToInt (p.get (PadParam::out));
        s.mute = p.get (PadParam::mute) > 0.5f; s.solo = p.get (PadParam::solo) > 0.5f;
        s.start = p.get (PadParam::start);   s.end = p.get (PadParam::end);
    }
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    readPadSettings();
    auto snap = kit.snapshot();
    for (int i = 0; i < numPads; ++i) padSettings[(size_t) i].family = familyOfPad[(size_t) i].load();

    // pad clicks from the UI
    {
        const juce::SpinLock::ScopedTryLockType tl (uiMidiLock);
        if (tl.isLocked() && ! uiMidi.isEmpty()) { midi.addEvents (uiMidi, 0, n, 0); uiMidi.clear(); }
    }
    // the pattern preview (free running at the BPM)
    if (previewOn.load())
    {
        auto pat = std::atomic_load (&previewPattern);
        const double ticksPerSample = bpmValue.load() / 60.0 * 16.0 / sampleRate;
        double pos = previewPos.load();
        const double len = juce::jmax (1, pat->ticks());
        const double end = pos + ticksPerSample * n;
        for (auto& note : pat->notes)
            for (double base = std::floor (pos / len) * len; base <= end; base += len)
            {
                const double t = base + note.tick;
                if (t >= pos && t < end)
                    midi.addEvent (juce::MidiMessage::noteOn (10, noteForPad (note.pad), juce::jlimit (0.01f, 1.0f, note.velocity)),
                                   juce::jlimit (0, n - 1, (int) ((t - pos) / ticksPerSample)));
            }
        previewPos = std::fmod (end, len);
    }

    std::array<std::array<float*, 2>, 1 + numFamilies> outs {};
    const int numOuts = juce::jmin (getBusCount (false), 1 + numFamilies);
    for (int b = 0; b < numOuts; ++b)
    {
        auto bb = getBusBuffer (buffer, false, b);
        outs[(size_t) b] = { bb.getNumChannels() > 0 ? bb.getWritePointer (0) : nullptr, bb.getNumChannels() > 1 ? bb.getWritePointer (1) : nullptr };
    }
    if (! usable.load())
    {
        buffer.clear();
        return;
    }
    sampler.process (outs.data(), numOuts, n, midi, *snap, padSettings, (Vibe) vibeType.load());

    if (auto pv = std::atomic_load (&preview); pv != nullptr && outs[0][0] != nullptr)
    {   // FIND SIMILAR audition on the main out
        const int at = previewPosSamples.load(), num = juce::jmin (n, pv->getNumSamples() - at);
        for (int c = 0; c < 2 && num > 0; ++c) if (outs[0][(size_t) c] != nullptr) juce::FloatVectorOperations::add (outs[0][(size_t) c], pv->getReadPointer (c, at), num);
        previewPosSamples = at + juce::jmax (0, num);
        if (at + num >= pv->getNumSamples()) std::atomic_store (&preview, std::shared_ptr<const juce::AudioBuffer<float>>());
    }
    const float g = juce::Decibels::decibelsToGain (outputDb->load());
    for (int b = 0; b < numOuts; ++b)
        for (auto* p : outs[(size_t) b]) if (p != nullptr) juce::FloatVectorOperations::multiply (p, g, n);
    for (int c = 0; c < 2; ++c)
    {
        const float pk = outs[0][(size_t) c] != nullptr ? juce::FloatVectorOperations::findMaximum (outs[0][(size_t) c], n) : 0.0f;
        const float mn = outs[0][(size_t) c] != nullptr ? -juce::FloatVectorOperations::findMinimum (outs[0][(size_t) c], n) : 0.0f;
        const float v = juce::jmax (pk, mn), old = meter[(size_t) c].load();
        meter[(size_t) c] = juce::jmax (v, old * 0.92f);
    }
    midi.clear();
}

// ================================================================== clone thread
void Processor::cloneFile (const juce::File& f, bool ai)
{
    auto j = std::make_unique<Job>();
    j->file = f; j->ai = ai; j->sens = sensitivity->load(); j->name = f.getFileNameWithoutExtension();
    {
        const juce::ScopedLock sl (jobLock);
        cancelJob = cloning.load();
        job = std::move (j);
    }
    lastError.clear();
    cloning = true; progress = 0;
    notify();
}

void Processor::recloneSource (bool ai)
{
    auto src = kit.sourceAudio;
    if (src == nullptr)
    {
        if (kit.sourceFile.existsAsFile()) cloneFile (kit.sourceFile, ai);
        return;
    }
    auto j = std::make_unique<Job>();
    j->audio = std::const_pointer_cast<juce::AudioBuffer<float>> (src);
    j->sr = sourceRate; j->ai = ai; j->sens = sensitivity->load(); j->name = kit.sourceName; j->file = kit.sourceFile;
    j->stem = drumStem;
    j->parts = drumParts;
    {
        const juce::ScopedLock sl (jobLock);
        cancelJob = cloning.load();
        job = std::move (j);
    }
    cloning = true; progress = 0;
    notify();
}

void Processor::run()
{
    while (! threadShouldExit())
    {
        std::unique_ptr<Job> j;
        {
            const juce::ScopedLock sl (jobLock);
            j = std::move (job);
        }
        if (j == nullptr) { wait (-1); continue; }
        cancelJob = false;
        auto setStatus = [this] (float p, const juce::String& s) { progress = p; const juce::ScopedLock sl (statusLock); status = s; };
        setStatus (0.01f, "Loading");

        double sr = j->sr;
        auto audio = j->audio;
        if (audio == nullptr)
        {
            juce::String err;
            audio = CloneEngine::load (j->file, sr, err);
            if (audio == nullptr)
            {
                lastError = err; cloning = false;
                triggerAsyncUpdate();
                continue;
            }
        }
        CloneSettings cs; cs.ai = j->ai; cs.sensitivity = j->sens; cs.splitKits = splitKits;
        float from = 0.0f;
        if (j->ai && DrumSeparator::available())
        {   // v2: neural drum separation first (each song once: it is cached)
            if (j->stem != nullptr && j->stem->getNumSamples() == audio->getNumSamples()) cs.drumStem = j->stem;
            else
            {
                juce::String err;
                cs.drumStem = DrumSeparator::separate (*audio, sr, &cancelJob, [&] (float p, const juce::String& t) { setStatus (0.02f + 0.43f * p, "1/2 " + t); }, err);
                if (cancelJob.load() || threadShouldExit()) continue;
                if (cs.drumStem == nullptr) lastError = err + " Using the DSP separation.";
            }
            from = 0.45f;
            // v3: the drum stem split into kick / snare / cymbals / toms (each song once: cached by the song)
            if (cs.drumStem != nullptr && DrumSeparator::partsAvailable())
            {
                if (j->parts[0] != nullptr && j->parts[0]->getNumSamples() == audio->getNumSamples()) cs.parts = j->parts;
                else
                {
                    juce::String err;
                    cs.parts = DrumSeparator::separateParts (*cs.drumStem, sr, &cancelJob, [&] (float p, const juce::String& t) { setStatus (0.45f + 0.43f * p, "2/2 " + t); },
                                                             err, true, DrumSeparator::keyOf (*audio, sr));
                    if (cancelJob.load() || threadShouldExit()) continue;
                    if (cs.parts[0] == nullptr) lastError = err;
                }
                from = 0.88f;
            }
        }
        auto res = CloneEngine::clone (*audio, sr, cs, &cancelJob, [&] (float p, const juce::String& t) { setStatus (from + (1.0f - from) * p, t); });
        if (cancelJob.load() || threadShouldExit()) continue;
        {
            const juce::ScopedLock sl (jobLock);
            if (job != nullptr) continue;                 // a newer job is waiting
            pending = std::make_unique<Pending>();
            pending->res = std::move (res);
            pending->name = j->name; pending->file = j->file; pending->source = audio; pending->sr = sr; pending->ai = j->ai;
            pending->restoreOnly = j->restoreOnly;
            pending->stem = cs.drumStem;
            pending->parts = cs.parts;
            pending->cutParts = res.parts;
        }
        triggerAsyncUpdate();
    }
}

void Processor::handleAsyncUpdate()
{
    std::unique_ptr<Pending> p;
    {
        const juce::ScopedLock sl (jobLock);
        p = std::move (pending);
    }
    if (p != nullptr)
    {
        if (p->restoreOnly)
        {   // a project was reopened: the pads stay as saved, only HIT browsing comes back
            kit.analysis = p->res.analysis;
            kit.sourceAudio = p->source;
            sourceRate = p->sr;
            extractSource = p->res.extractFrom;
            padOfGroup = p->res.padOfGroup;
            drumStem = p->stem; drumParts = p->parts; cutParts = p->cutParts;
        }
        else
        {
            drumStem = p->stem; drumParts = p->parts; cutParts = p->cutParts;
            applyClone (std::move (p->res), p->name, p->file, p->source, p->sr, p->ai);
        }
    }
    if (job == nullptr) cloning = false;
    kitChanged();
}

void Processor::applyClone (CloneResult&& r, const juce::String& name, const juce::File& file,
                            std::shared_ptr<juce::AudioBuffer<float>> source, double sr, bool ai)
{
    // fresh pad settings
    for (int i = 0; i < numPads; ++i)
        for (int k = 0; k < numPadParams; ++k)
            if (auto* prm = state.getParameter (padParamId (i, (PadParam) k)))
                prm->setValueNotifyingHost (prm->getDefaultValue());
    kit.clearAll();
    for (int i = 0; i < numPads; ++i)
        if (! r.pads[(size_t) i].empty()) kit.setPad (i, std::move (r.pads[(size_t) i]));
    kit.analysis = r.analysis;
    kit.sourceName = name;
    kit.sourceFile = file;
    kit.sourceAudio = source;
    kit.usedAI = ai;
    sourceRate = sr;
    extractSource = r.extractFrom;
    padOfGroup = r.padOfGroup;
    kit.patterns = r.analysis->patterns;
    for (size_t i = 0; i < kit.patterns.size(); ++i) kit.patterns[i].name = "Original";
    if (kit.patterns.empty()) { Pattern p; p.name = "Original"; kit.patterns.push_back (p); }
    kit.currentPattern = 0;
    history.clear();
    setBpm (r.analysis->bpm);
    kitName = name;
    selectedPad = 0;
    for (int i = 0; i < numPads; ++i) if (! kit.pad (i).empty()) { selectedPad = i; break; }
    patternEdited();
}

void Processor::kitChanged()
{
    for (int i = 0; i < numPads; ++i) familyOfPad[(size_t) i] = kit.pad (i).type;
    if (onKitChanged) onKitChanged();
}

// ================================================================== pads
int Processor::hitCount (int pad) const
{
    auto p = kit.pad (pad);
    auto an = kit.analysis;
    if (an == nullptr || ! juce::isPositiveAndBelow (p.group, (int) an->groups.size())) return 0;
    return (int) an->groups[(size_t) p.group].hits.size();
}

void Processor::selectHit (int pad, int hitIndex)
{
    auto p = kit.pad (pad);
    auto an = kit.analysis;
    auto from = extractSource;
    if (an == nullptr || from == nullptr || ! juce::isPositiveAndBelow (p.group, (int) an->groups.size())) return;
    const int n = (int) an->groups[(size_t) p.group].hits.size();
    hitIndex = ((hitIndex % n) + n) % n;
    const int src = an->groups[(size_t) p.group].source;      // v3: cut from its own part
    auto np = CloneEngine::makePad (src >= 0 && cutParts[(size_t) src] != nullptr ? *cutParts[(size_t) src] : *from, *an, p.group, hitIndex);
    np.name = p.name; np.type = p.type; np.tag = p.tag;
    kit.setPad (pad, std::move (np));
    kitChanged();
}

void Processor::loadSampleToPad (int pad, const juce::File& f)
{
    double sr = 44100;
    juce::String err;
    auto audio = CloneEngine::load (f, sr, err);
    if (audio == nullptr) { lastError = err; return; }
    const int maxLen = (int) (sr * 10.0);
    if (audio->getNumSamples() > maxLen) audio->setSize (audio->getNumChannels(), maxLen, true);
    auto old = kit.pad (pad);
    PadSound p;
    p.raw = audio; p.clean = audio; p.sampleRate = sr;
    p.name = f.getFileNameWithoutExtension().toUpperCase().substring (0, 18);
    const auto g = guessFamily (f.getFileName());
    p.type = g != Family::sample ? g : (old.empty() ? Family::sample : old.type);
    kit.setPad (pad, std::move (p));
    if (auto* prm = state.getParameter (padParamId (pad, PadParam::clean))) prm->setValueNotifyingHost (0.0f);
    kitChanged();
}

void Processor::movePad (int from, int to, bool copy)
{
    if (from == to || ! juce::isPositiveAndBelow (from, numPads) || ! juce::isPositiveAndBelow (to, numPads)) return;
    auto a = kit.pad (from), b = kit.pad (to);
    std::array<float, numPadParams> va {}, vb {};
    for (int k = 0; k < numPadParams; ++k)
    {
        va[(size_t) k] = state.getParameter (padParamId (from, (PadParam) k))->getValue();
        vb[(size_t) k] = state.getParameter (padParamId (to, (PadParam) k))->getValue();
    }
    kit.setPad (to, a);
    for (int k = 0; k < numPadParams; ++k) state.getParameter (padParamId (to, (PadParam) k))->setValueNotifyingHost (va[(size_t) k]);
    if (! copy)
    {
        kit.setPad (from, b);
        for (int k = 0; k < numPadParams; ++k) state.getParameter (padParamId (from, (PadParam) k))->setValueNotifyingHost (vb[(size_t) k]);
        for (auto& pat : kit.patterns)
            for (auto& n : pat.notes) n.pad = n.pad == from ? to : n.pad == to ? from : n.pad;
        patternEdited();
    }
    selectedPad = to;
    kitChanged();
}

void Processor::setPadType (int pad, Family f)
{
    auto p = kit.pad (pad);
    if (p.empty()) return;
    p.type = f;
    kit.setPad (pad, std::move (p));
    kitChanged();
}

void Processor::clearPad (int pad)
{
    kit.setPad (pad, {});
    for (auto& pat : kit.patterns)
        pat.notes.erase (std::remove_if (pat.notes.begin(), pat.notes.end(), [pad] (const Note& n) { return n.pad == pad; }), pat.notes.end());
    patternEdited();
    kitChanged();
}

void Processor::triggerPad (int pad, float velocity)
{
    const juce::SpinLock::ScopedLockType sl (uiMidiLock);
    uiMidi.addEvent (juce::MidiMessage::noteOn (10, noteForPad (pad), juce::jlimit (0.01f, 1.0f, velocity)), 0);
}

juce::File Processor::exportPad (int pad)
{
    auto p = kit.pad (pad);
    if (p.empty()) return {};
    const float c = state.getRawParameterValue (padParamId (pad, PadParam::clean))->load();
    juce::AudioBuffer<float> b (*p.raw);
    if (p.clean != nullptr && p.clean->getNumSamples() == b.getNumSamples() && c > 0)
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            b.applyGain (ch, 0, b.getNumSamples(), 1.0f - c);
            b.addFrom (ch, 0, *p.clean, juce::jmin (ch, p.clean->getNumChannels() - 1), 0, b.getNumSamples(), c);
        }
    auto f = tempFolder().getChildFile (safeName (kitName + " " + (p.name.isEmpty() ? juce::String (familyName (p.type)) : p.name)) + ".wav");
    return writeWav (f, b, p.sampleRate) ? f : juce::File();
}

std::array<Family, numPads> Processor::padFamilies() const
{
    std::array<Family, numPads> f;
    for (int i = 0; i < numPads; ++i) f[(size_t) i] = kit.pad (i).type;
    return f;
}

void Processor::setPreviewPlaying (bool on)
{
    if (on && ! previewOn.load()) previewPos = 0;
    previewOn = on;
}

void Processor::patternEdited()
{
    std::shared_ptr<const Pattern> p;
    if (juce::isPositiveAndBelow (kit.currentPattern, (int) kit.patterns.size()))
        p = std::make_shared<Pattern> (kit.patterns[(size_t) kit.currentPattern]);
    else
        p = std::make_shared<Pattern>();
    std::atomic_store (&previewPattern, p);
}

juce::File Processor::exportPattern (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) kit.patterns.size())) return {};
    auto mf = toMidiFile (kit.patterns[(size_t) index], bpm());
    auto f = tempFolder().getChildFile (safeName (kitName + " Pattern " + juce::String (index + 1)) + ".mid");
    f.deleteFile();
    juce::FileOutputStream os (f);
    if (! os.openedOk() || ! mf.writeTo (os)) return {};
    return f;
}

// ================================================================== FIND SIMILAR audition
void Processor::previewFile (const juce::File& f)
{
    double sr = 44100;
    auto b = SimilarSearch::loadOneShot (f, sr, 4.0);
    if (b.getNumSamples() == 0) return;
    auto out = std::make_shared<juce::AudioBuffer<float>> (2, (int) std::ceil (b.getNumSamples() * sampleRate / sr));
    for (int c = 0; c < 2; ++c)
    {
        juce::LagrangeInterpolator li;
        li.process (sr / sampleRate, b.getReadPointer (juce::jmin (c, b.getNumChannels() - 1)), out->getWritePointer (c), out->getNumSamples());
    }
    const float pk = out->getMagnitude (0, out->getNumSamples());
    if (pk > 0) out->applyGain (0.8f / pk);
    previewPosSamples = 0;
    std::atomic_store (&preview, std::shared_ptr<const juce::AudioBuffer<float>> (out));
}

// ================================================================== kits on disk
juce::File Processor::kitsFolder() const
{
    auto d = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("LT Eclipse").getChildFile ("Kits");
    d.createDirectory();
    return d;
}

bool Processor::saveKit (const juce::String& name)
{
    kitName = name;
    juce::MemoryBlock mb;
    getStateInformation (mb);
    auto f = kitsFolder().getChildFile (safeName (name) + ".ltkit");
    return f.replaceWithData (mb.getData(), mb.getSize());
}

bool Processor::loadKit (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) return false;
    const auto keepSkin = skinId;
    const auto keepScale = uiScale;
    setStateInformation (mb.getData(), (int) mb.getSize());
    skinId = keepSkin; uiScale = keepScale;               // the look is yours, not the kit's
    setSkin (skinId);
    return true;
}

void Processor::setSkin (const juce::String& id)
{
    skinId = skinById (id).id;
    vibeType = (int) skin().vibe;
}

// ================================================================== state
void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("skin", skinId, nullptr);
    tree.setProperty ("uiScale", uiScale, nullptr);
    tree.setProperty ("pad", selectedPad, nullptr);
    tree.setProperty ("kitName", kitName, nullptr);
    tree.setProperty ("bpm", bpm(), nullptr);
    tree.setProperty ("splitKits", splitKits, nullptr);
    tree.appendChild (kit.toValueTree(), nullptr);
    juce::MemoryOutputStream os (dest, false);
    tree.writeToStream (os);
}

void Processor::setStateInformation (const void* data, int size)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) size);
    if (! tree.isValid() || ! tree.hasType (state.state.getType())) return;
    auto kt = tree.getChildWithName ("KIT").createCopy();
    tree.removeChild (tree.getChildWithName ("KIT"), nullptr);
    state.replaceState (tree);
    setSkin (tree.getProperty ("skin", "negra").toString());
    uiScale = (float) tree.getProperty ("uiScale", 0.8f);
    selectedPad = juce::jlimit (0, numPads - 1, (int) tree.getProperty ("pad", 0));
    kitName = tree.getProperty ("kitName", "New kit").toString();
    splitKits = tree.getProperty ("splitKits", true);
    kit.fromValueTree (kt);
    kit.sourceAudio = nullptr;
    extractSource = nullptr;
    padOfGroup.clear();
    if (kit.patterns.empty()) { Pattern p; p.name = "Original"; kit.patterns.push_back (p); }
    setBpm ((double) tree.getProperty ("bpm", 120.0));
    history.clear();
    patternEdited();
    for (int i = 0; i < numPads; ++i) familyOfPad[(size_t) i] = kit.pad (i).type;

    // bring HIT browsing back in the background if the source is still on disk
    if (kit.sourceFile.existsAsFile())
    {
        auto j = std::make_unique<Job>();
        j->file = kit.sourceFile; j->ai = kit.usedAI; j->sens = sensitivity->load(); j->name = kit.sourceName; j->restoreOnly = true;
        { const juce::ScopedLock sl (jobLock); job = std::move (j); }
        notify();
    }
    juce::MessageManager::callAsync ([sp = juce::WeakReference<Processor> (this)]
    {
        if (auto* p = sp.get()) { p->kitChanged(); if (p->onStateRestored) p->onStateRestored(); }
    });
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

} // namespace eclipse

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new eclipse::Processor();
}
