#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace quasar
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, &undo, "QUASAR", createLayout())
{
    for (int b = 0; b < numBuses; ++b)
    {
        chains[(size_t) b] = std::make_unique<FxChain> (state, b);
        busLevelP[(size_t) b] = state.getRawParameterValue (IDs::bus (b, "level"));
        busPanP[(size_t) b] = state.getRawParameterValue (IDs::bus (b, "pan"));
        busMuteP[(size_t) b] = state.getRawParameterValue (IDs::bus (b, "mute"));
        busSoloP[(size_t) b] = state.getRawParameterValue (IDs::bus (b, "solo"));
        busLevelDest[(size_t) b] = ModDefs::destIndex (IDs::bus (b, "level"));
        busLevelParam[(size_t) b] = state.getParameter (IDs::bus (b, "level"));
        busPanDest[(size_t) b] = ModDefs::destIndex (IDs::bus (b, "pan"));
    }
    masterP = state.getRawParameterValue (IDs::master);
    masterParameter = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (IDs::master));
    limiterP = state.getRawParameterValue (IDs::limiter);
    masterDest = ModDefs::destIndex (IDs::master);
    for (int m = 0; m < numMacros; ++m) macroNames.add ("MACRO " + juce::String (m + 1));
    for (auto* p : getParameters()) p->addListener (this);
    startTimerHz (20);
}

Processor::~Processor()
{
    stopTimer();
    for (auto* p : getParameters()) p->removeListener (this);
}

void Processor::parameterGestureChanged (int, bool starting)
{
    if (starting && juce::MessageManager::existsAndIsCurrentThread()) undo.beginNewTransaction();
}

void Processor::timerCallback()
{
    engine.ensureTables();
    for (auto& c : chains) c->service();
    engine.collectGarbage();
}

void Processor::serviceNow()
{
    engine.ensureTables();
    for (auto& c : chains) c->service();
}

void Processor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;
    auto maxBlock = juce::jmax (32, block);
    engine.prepare (sr, maxBlock);
    engine.ensureTables();
    for (auto& b : buses) b.setSize (2, maxBlock);
    chunk.setSize (2, 32);
    for (auto& c : chains) c->prepare (sr, 32);
    for (int b = 0; b < numBuses; ++b) { busGainL[(size_t) b].setLength (256); busGainR[(size_t) b].setLength (256); busGainL[(size_t) b].snap (1); busGainR[(size_t) b].snap (1); }
    masterGain.setLength (256); masterGain.snap (1);
    for (auto& d : dc) { d.prepare ((float) sr); d.reset(); }
    limEnv = 0;
    refreshLicense();
}

bool Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    auto t0 = juce::Time::getHighResolutionTicks();
    auto n = buffer.getNumSamples();
    buffer.clear();
    if (! usable.load() || n == 0) { midi.clear(); return; }
    if (n > buses[0].getNumSamples())
    {
        // the host sent more than it announced: render the part we can, never allocate here
        n = buses[0].getNumSamples();
    }

    juce::Optional<juce::AudioPlayHead::PositionInfo> info;
    if (auto* ph = getPlayHead()) info = ph->getPosition();
    engine.process (midi, n, info.hasValue() ? &(*info) : nullptr, buses);
    midi.clear();

    FxContext ctx { sampleRate, engine.bpm };
    auto offset = [this] (int dest) { return engine.globalOffset (dest); };

    // buses 1 and 2 through their effects, then into MAIN
    bool anySolo = busSoloP[1]->load() > 0.5f || busSoloP[2]->load() > 0.5f;
    for (int b = 1; b < numBuses; ++b)
    {
        auto& buf = buses[(size_t) b];
        for (int pos = 0; pos < n; pos += 32)
        {
            auto len = juce::jmin (32, n - pos);
            chunk.copyFrom (0, 0, buf, 0, pos, len); chunk.copyFrom (1, 0, buf, 1, pos, len);
            chains[(size_t) b]->process (chunk, len, ctx, offset);
            buf.copyFrom (0, pos, chunk, 0, 0, len); buf.copyFrom (1, pos, chunk, 1, 0, len);
        }
        auto muted = busMuteP[(size_t) b]->load() > 0.5f || (anySolo && busSoloP[(size_t) b]->load() < 0.5f);
        auto lvl = juce::jlimit (0.0f, 1.0f, busLevelParam[(size_t) b]->getValue() + offset (busLevelDest[(size_t) b]));
        auto pan = juce::jlimit (-1.0f, 1.0f, busPanP[(size_t) b]->load() * 0.01f + offset (busPanDest[(size_t) b]) * 2.0f);
        auto g = muted ? 0.0f : dsp::levelToGain (lvl * 100.0f);
        float gl, gr; dsp::panGains (pan, gl, gr);
        busGainL[(size_t) b].set (g * gl); busGainR[(size_t) b].set (g * gr);
        auto* L = buf.getWritePointer (0); auto* R = buf.getWritePointer (1);
        float pkL = 0, pkR = 0;
        for (int i = 0; i < n; ++i)
        {
            L[i] *= busGainL[(size_t) b].next(); R[i] *= busGainR[(size_t) b].next();
            pkL = juce::jmax (pkL, std::abs (L[i])); pkR = juce::jmax (pkR, std::abs (R[i]));
        }
        busLevel[(size_t) b][0].store (juce::jmax (pkL, busLevel[(size_t) b][0].load() * 0.85f));
        busLevel[(size_t) b][1].store (juce::jmax (pkR, busLevel[(size_t) b][1].load() * 0.85f));
        buses[0].addFrom (0, 0, buf, 0, 0, n);
        buses[0].addFrom (1, 0, buf, 1, 0, n);
    }

    // MAIN
    auto& main = buses[0];
    for (int pos = 0; pos < n; pos += 32)
    {
        auto len = juce::jmin (32, n - pos);
        chunk.copyFrom (0, 0, main, 0, pos, len); chunk.copyFrom (1, 0, main, 1, pos, len);
        chains[0]->process (chunk, len, ctx, offset);
        main.copyFrom (0, pos, chunk, 0, 0, len); main.copyFrom (1, pos, chunk, 1, 0, len);
    }
    {
        auto muted = busMuteP[0]->load() > 0.5f;
        auto lvl = juce::jlimit (0.0f, 1.0f, busLevelParam[0]->getValue() + offset (busLevelDest[0]));
        auto pan = juce::jlimit (-1.0f, 1.0f, busPanP[0]->load() * 0.01f + offset (busPanDest[0]) * 2.0f);
        auto g = muted ? 0.0f : dsp::levelToGain (lvl * 100.0f);
        float gl, gr; dsp::panGains (pan, gl, gr);
        busGainL[0].set (g * gl); busGainR[0].set (g * gr);
    }
    auto* masterParam = masterParameter;
    auto masterDb = masterParam->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, masterParam->getValue() + offset (masterDest)));
    masterGain.set (masterDb <= -47.9f ? 0.0f : dsp::dbToGain (masterDb));

    auto limiterOn = limiterP->load() > 0.5f;
    const float ceiling = 0.966f;                               // -0.3 dBFS
    const auto release = std::exp (-1.0f / (0.12f * (float) sampleRate));
    auto* L = main.getWritePointer (0); auto* R = main.getWritePointer (1);
    float pkL = 0, pkR = 0, pkBL = 0, pkBR = 0, maxGr = 0;
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        auto bl = L[i] * busGainL[0].next(), br = R[i] * busGainR[0].next();
        pkBL = juce::jmax (pkBL, std::abs (bl)); pkBR = juce::jmax (pkBR, std::abs (br));
        auto m = masterGain.next();
        auto l = dc[0].process (bl * m), r = dc[1].process (br * m);
        if (limiterOn)
        {
            auto peak = juce::jmax (std::abs (l), std::abs (r));
            limEnv = juce::jmax (peak, limEnv * release);
            auto g = limEnv > ceiling ? ceiling / limEnv : 1.0f;
            l *= g; r *= g;
            maxGr = juce::jmax (maxGr, 1.0f - g);
            l = juce::jlimit (-1.0f, 1.0f, l); r = juce::jlimit (-1.0f, 1.0f, r);
        }
        if (! std::isfinite (l) || ! std::isfinite (r)) { l = r = 0; engine.reset(); for (auto& c : chains) c->reset(); }
        if (outR != nullptr) { outL[i] = l; outR[i] = r; }
        else outL[i] = 0.5f * (l + r);
        pkL = juce::jmax (pkL, std::abs (l)); pkR = juce::jmax (pkR, std::abs (r));
    }
    busLevel[0][0].store (juce::jmax (pkBL, busLevel[0][0].load() * 0.85f));
    busLevel[0][1].store (juce::jmax (pkBR, busLevel[0][1].load() * 0.85f));
    outLevel[0].store (juce::jmax (pkL, outLevel[0].load() * 0.85f));
    outLevel[1].store (juce::jmax (pkR, outLevel[1].load() * 0.85f));
    limiterGr.store (maxGr);

    auto seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    auto load = (float) (seconds / ((double) juce::jmax (1, n) / sampleRate));
    cpu.store (cpu.load() * 0.9f + load * 0.1f);
}

// ------------------------------------------------------------------ extras

void Processor::setLfoShape (int lfo, const LfoShape& s) { lfoTables.publish (lfo, s); }

bool Processor::loadSample (int osc, const juce::File& f, juce::String& error)
{
    auto s = SampleData::load (f, error);
    if (s == nullptr) return false;
    samplePaths[(size_t) osc] = f.getFullPathName();
    engine.setSample (osc, std::move (s));
    return true;
}

void Processor::clearSample (int osc)
{
    samplePaths[(size_t) osc] = {};
    engine.setSample (osc, nullptr);
}

bool Processor::loadUserTable (int osc, const juce::File& f, juce::String& error)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) { error = "Unsupported audio file"; return false; }
    auto len = (int) juce::jmin<juce::int64> (r->lengthInSamples, 2048 * 256);
    juce::AudioBuffer<float> b (1, len);
    r->read (&b, 0, len, 0, true, false);
    // Serum-style tables use 2048-sample frames; short files are one single cycle
    auto frame = len >= 2048 ? 2048 : len;
    auto t = Wavetable::fromAudio (f.getFileNameWithoutExtension().toUpperCase(), b.getReadPointer (0), len, frame);
    if (t == nullptr) { error = "Could not read the wavetable"; return false; }
    userTablePaths[(size_t) osc] = f.getFullPathName();
    engine.setUserTable (osc, std::move (t));
    if (auto* p = state.getParameter (IDs::osc (osc, "table"))) p->setValueNotifyingHost (p->convertTo0to1 ((float) WavetableLibrary::userIndex()));
    return true;
}

int Processor::findModulation (int source, int dest) const
{
    for (int s = 0; s < numModSlots; ++s)
        if ((int) state.getRawParameterValue (IDs::mod (s, "src"))->load() == source
            && (int) state.getRawParameterValue (IDs::mod (s, "dst"))->load() == dest) return s;
    return -1;
}

int Processor::addModulation (int source, int dest, float amountPct)
{
    if (source <= 0 || dest <= 0) return -1;
    auto set = [this] (const juce::String& id, float v)
    { if (auto* p = state.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); } };
    auto existing = findModulation (source, dest);
    if (existing >= 0) return existing;
    for (int s = 0; s < numModSlots; ++s)
        if ((int) state.getRawParameterValue (IDs::mod (s, "src"))->load() == 0)
        {
            undo.beginNewTransaction();
            set (IDs::mod (s, "src"), (float) source);
            set (IDs::mod (s, "dst"), (float) dest);
            set (IDs::mod (s, "amt"), amountPct);
            set (IDs::mod (s, "curve"), 0); set (IDs::mod (s, "aux"), 0);
            set (IDs::mod (s, "bip"), 0); set (IDs::mod (s, "out"), 100); set (IDs::mod (s, "on"), 1);
            return s;
        }
    return -1;
}

void Processor::removeModulation (int s)
{
    auto set = [this] (const juce::String& id, float v)
    { if (auto* p = state.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); } };
    undo.beginNewTransaction();
    set (IDs::mod (s, "src"), 0); set (IDs::mod (s, "dst"), 0); set (IDs::mod (s, "amt"), 0);
}

// ------------------------------------------------------------------ presets

Preset Processor::capturePreset (const juce::String& name) const
{
    Preset p;
    p.name = name;
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (std::abs (r->getValue() - r->getDefaultValue()) > 1.0e-5f)
                p.values[r->getParameterID()] = r->convertFrom0to1 (r->getValue());
    juce::Array<juce::var> lfos;
    for (int l = 0; l < numLfos; ++l) lfos.add (lfoTables.shape (l).toVar());
    p.lfoShapes = lfos;
    p.macroNames = macroNames;
    for (int o = 0; o < numOscs; ++o) p.samplePaths[(size_t) o] = samplePaths[(size_t) o];
    return p;
}

void Processor::applyPreset (const Preset& pr)
{
    undo.beginNewTransaction ("Preset");
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            auto it = pr.values.find (r->getParameterID());
            auto norm = it != pr.values.end() ? r->convertTo0to1 (it->second) : r->getDefaultValue();
            if (std::abs (norm - r->getValue()) > 1.0e-7f) r->setValueNotifyingHost (norm);
        }
    for (int l = 0; l < numLfos; ++l)
    {
        auto v = pr.lfoShapes.isArray() && l < pr.lfoShapes.size() ? pr.lfoShapes[l] : juce::var();
        setLfoShape (l, v.isObject() ? LfoShape::fromVar (v) : LfoShape::preset (0));
    }
    macroNames.clear();
    for (int m = 0; m < numMacros; ++m) macroNames.add (m < pr.macroNames.size() && pr.macroNames[m].isNotEmpty() ? pr.macroNames[m] : "MACRO " + juce::String (m + 1));
    for (int o = 0; o < numOscs; ++o)
    {
        juce::String err;
        auto path = pr.samplePaths[(size_t) o];
        if (path.isNotEmpty() && juce::File (path).existsAsFile()) loadSample (o, juce::File (path), err);
        else clearSample (o);
    }
    presetName = pr.name;
    presetBank = pr.bank;
    engine.ensureTables();
    loadedPreset = capturePreset (pr.name);
    for (auto& c : chains) c->service();
    if (pr.bank.isNotEmpty()) presets.addRecent (pr);
    if (onStateRestored) onStateRestored();
}

void Processor::initPatch()
{
    Preset p;
    p.name = "Init";
    applyPreset (p);
    presetBank = {};
}

bool Processor::isPresetModified() const
{
    auto now = capturePreset (presetName);
    if (now.values.size() != loadedPreset.values.size()) return true;
    for (auto& [id, v] : now.values)
    {
        auto it = loadedPreset.values.find (id);
        if (it == loadedPreset.values.end() || std::abs (it->second - v) > 1.0e-4f * juce::jmax (1.0f, std::abs (v))) return true;
    }
    return false;
}

// ------------------------------------------------------------------ state

juce::ValueTree Processor::extraState() const
{
    juce::ValueTree t ("QUASAR_EXTRA");
    t.setProperty ("preset", presetName, nullptr);
    t.setProperty ("bank", presetBank, nullptr);
    t.setProperty ("page", lastPage, nullptr);
    t.setProperty ("simple", simpleView, nullptr);
    juce::Array<juce::var> lfos;
    for (int l = 0; l < numLfos; ++l) lfos.add (lfoTables.shape (l).toVar());
    t.setProperty ("lfo", juce::JSON::toString (juce::var (lfos), true), nullptr);
    t.setProperty ("macros", macroNames.joinIntoString ("\n"), nullptr);
    for (int o = 0; o < numOscs; ++o)
    {
        t.setProperty ("sample" + juce::String (o), samplePaths[(size_t) o], nullptr);
        t.setProperty ("table" + juce::String (o), userTablePaths[(size_t) o], nullptr);
    }
    t.setProperty ("loaded", juce::JSON::toString (loadedPreset.toVar(), true), nullptr);
    return t;
}

void Processor::restoreExtra (const juce::ValueTree& t)
{
    presetName = t.getProperty ("preset", "Init").toString();
    presetBank = t.getProperty ("bank", "").toString();
    lastPage = t.getProperty ("page", "OSC").toString();
    simpleView = t.getProperty ("simple", false);
    auto lfos = juce::JSON::parse (t.getProperty ("lfo", "").toString());
    for (int l = 0; l < numLfos; ++l)
        setLfoShape (l, lfos.isArray() && l < lfos.size() ? LfoShape::fromVar (lfos[l]) : LfoShape::preset (0));
    macroNames = juce::StringArray::fromLines (t.getProperty ("macros", "").toString());
    while (macroNames.size() < numMacros) macroNames.add ("MACRO " + juce::String (macroNames.size() + 1));
    for (int o = 0; o < numOscs; ++o)
    {
        juce::String err;
        juce::File s (t.getProperty ("sample" + juce::String (o), "").toString());
        if (s.existsAsFile()) loadSample (o, s, err); else clearSample (o);
        juce::File w (t.getProperty ("table" + juce::String (o), "").toString());
        if (w.existsAsFile()) { auto keep = state.getRawParameterValue (IDs::osc (o, "table"))->load(); loadUserTable (o, w, err);
                                if (auto* p = state.getParameter (IDs::osc (o, "table"))) p->setValueNotifyingHost (p->convertTo0to1 (keep)); }
    }
    loadedPreset = Preset::fromVar (juce::JSON::parse (t.getProperty ("loaded", "").toString()));
}

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.removeChild (tree.getChildWithName ("QUASAR_EXTRA"), nullptr);
    tree.appendChild (extraState(), nullptr);
    if (auto xml = tree.createXml()) copyXmlToBinary (*xml, dest);
}

void Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    auto extra = tree.getChildWithName ("QUASAR_EXTRA");
    if (extra.isValid()) tree.removeChild (extra, nullptr);
    state.replaceState (tree);
    if (extra.isValid()) restoreExtra (extra);
    serviceNow();
    undo.clearUndoHistory();
    if (onStateRestored) juce::MessageManager::callAsync ([safe = juce::WeakReference<Processor> (this)] { if (safe != nullptr && safe->onStateRestored) safe->onStateRestored(); });
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

} // namespace quasar

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new quasar::Processor();
}
