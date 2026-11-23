#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace halo
{

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, &undo, "HALO", createLayout())
{
    for (int i = 0; i < numStrips; ++i) seeds[(size_t) i] = (unsigned long long) (i + 1) * 7919ull;
    applyPreset (factoryPresets().front());   // a fresh instance starts on "Pop Thirds"
    undo.clearUndoHistory();
}

Processor::~Processor() = default;

float Processor::v (const juce::String& id) const
{
    auto* raw = state.getRawParameterValue (id);
    return raw != nullptr ? raw->load() : 0.0f;
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

juce::AudioParameterBool* Processor::getBypassParameter() const
{
    return dynamic_cast<juce::AudioParameterBool*> (state.getParameter (IDs::bypass));
}

// ------------------------------------------------------------------ specs

StripSpec Processor::stripSpec (int i) const
{
    StripSpec s;
    s.part = (Part) juce::jlimit (0, (int) Part::count - 1, juce::roundToInt (v (IDs::strip (i, IDs::part))));
    s.smart = v (IDs::strip (i, IDs::smart)) > 0.5f;
    s.style = (Style) juce::roundToInt (v (IDs::strip (i, IDs::style)));
    s.voice = (VoiceType) juce::roundToInt (v (IDs::strip (i, IDs::voice)));
    s.humanize = v (IDs::strip (i, IDs::humanize)) * 0.01;
    s.delayMs = v (IDs::strip (i, IDs::delay));
    s.seed = seeds[(size_t) i];
    s.stripIndex = i;
    return s;
}

bool Processor::stripEnabled (int i) const { return v (IDs::strip (i, IDs::on)) > 0.5f; }

ChoirSpec Processor::choirSpec() const
{
    ChoirSpec c;
    c.voices = juce::roundToInt (v (IDs::cVoices));
    c.type = (ChoirType) juce::roundToInt (v (IDs::cType));
    c.harmony = (ChoirHarmony) juce::roundToInt (v (IDs::cHarmony));
    c.sections[0] = v (IDs::cSop); c.sections[1] = v (IDs::cAlto); c.sections[2] = v (IDs::cTenor); c.sections[3] = v (IDs::cBass);
    c.spread = v (IDs::cSpread) * 0.01; c.loose = v (IDs::cLoose) * 0.01; c.width = v (IDs::cWidth) * 0.01;
    c.room = v (IDs::cRoom) * 0.01; c.breath = v (IDs::cBreath) * 0.01;
    c.seed = choirSeed;
    return c;
}

GlobalSpec Processor::globalSpec() const
{
    GlobalSpec g;
    g.tightness = v (IDs::tightness) * 0.01;
    auto host = hostBpm.load();
    g.bpm = host > 0 ? host : session.getBpm();
    g.refuerzoWords = juce::roundToInt (v (IDs::refuerzoWords));
    return g;
}

Key Processor::currentKey() const
{
    if (v (IDs::keyAuto) > 0.5f) return session.getDetectedKey();
    Key k;
    k.tonic = juce::roundToInt (v (IDs::key));
    k.minor = v (IDs::scale) > 0.5f;
    k.confidence = 1.0f;
    return k;
}

Session::StripGains Processor::stripGains (int i, bool asHeard) const
{
    Session::StripGains g;
    bool anySolo = false;
    for (int k = 0; k < numStrips; ++k) anySolo = anySolo || v (IDs::strip (k, IDs::solo)) > 0.5f;
    g.audible = stripEnabled (i) && v (IDs::strip (i, IDs::mute)) < 0.5f && (! anySolo || v (IDs::strip (i, IDs::solo)) > 0.5f);
    auto level = juce::Decibels::decibelsToGain (v (IDs::strip (i, IDs::level)), -36.0f);
    auto pan = juce::jlimit (-1.0f, 1.0f, v (IDs::strip (i, IDs::pan)) * 0.01f * v (IDs::width) * 0.01f);
    auto a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    // constant-power balance that keeps a centred source at unity
    g.gl = level * std::cos (a) * juce::MathConstants<float>::sqrt2;
    g.gr = level * std::sin (a) * juce::MathConstants<float>::sqrt2;
    if (! asHeard) g.audible = true;
    if (! g.audible) g.gl = g.gr = 0.0f;
    return g;
}

void Processor::generateHarmony (int only, bool regenerate)
{
    Session::Request r;
    for (int i = 0; i < numStrips; ++i)
    {
        if (regenerate && (only < 0 || only == i)) seeds[(size_t) i] = seeds[(size_t) i] * 6364136223846793005ull + 1442695040888963407ull;
        r.specs[(size_t) i] = stripSpec (i);
        r.enabled[(size_t) i] = stripEnabled (i) && (only < 0 || only == i);
        r.force[(size_t) i] = regenerate && (only < 0 || only == i);
    }
    r.global = globalSpec();
    r.best = v (IDs::quality) > 0.5f;
    session.setKey (currentKey());
    session.generate (r);
}

void Processor::generateChoir (bool regenerate)
{
    if (regenerate) choirSeed = choirSeed * 6364136223846793005ull + 1442695040888963407ull;
    session.setKey (currentKey());
    session.generateChoir (choirSpec());
}

// ------------------------------------------------------------------ transport

void Processor::play (bool fromStart)
{
    auto pd = session.getPlayback();
    if (pd == nullptr) return;
    if (fromStart || playPos.load() >= (juce::int64) pd->length) playPos = 0;
    playing = true;
}

void Processor::stop() { playing = false; }

double Processor::getPlayPositionSeconds() const
{
    auto pd = session.getPlayback();
    return pd != nullptr ? (double) playPos.load() / pd->rate : 0.0;
}

void Processor::setPlayPositionSeconds (double s)
{
    auto pd = session.getPlayback();
    if (pd != nullptr) playPos = (juce::int64) juce::jlimit (0.0, (double) pd->length, s * pd->rate);
}

void Processor::startCapture()
{
    stop();
    captureRate = sampleRate;
    captureBuffer.setSize (2, (int) (sampleRate * 6 * 60), false, true, false);
    captureLength = 0;
    capturing = true;
}

void Processor::stopCapture()
{
    if (! capturing.exchange (false)) return;
    auto n = captureLength.load();
    if (n < (int) (captureRate * 0.5)) return;
    // trim leading/trailing silence so the take starts at the first sound
    int first = 0, last = n - 1;
    auto loud = [&] (int i) { return std::abs (captureBuffer.getSample (0, i)) + std::abs (captureBuffer.getSample (1, i)) > 0.002f; };
    while (first < n && ! loud (first)) ++first;
    while (last > first && ! loud (last)) --last;
    if (last - first < (int) (captureRate * 0.5)) return;
    juce::AudioBuffer<float> take (2, last - first + 1);
    for (int c = 0; c < 2; ++c) take.copyFrom (c, 0, captureBuffer, c, first, take.getNumSamples());
    captureBuffer.setSize (0, 0);
    session.loadBuffer (take, captureRate, "Track take");
}

double Processor::getCaptureSeconds() const { return captureLength.load() / captureRate; }

// ------------------------------------------------------------------ audio

void Processor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    for (auto* s : { &leadGain, &choirGain }) s->reset (sr, 0.03);
    for (int i = 0; i < numStrips; ++i) { gainL[(size_t) i].reset (sr, 0.03); gainR[(size_t) i].reset (sr, 0.03); }
    session.setPlaybackRate (sr);
    refreshLicense();
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    auto in = layouts.getMainInputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) hostBpm = *bpm;

    inLevel = juce::jmax (buffer.getMagnitude (0, 0, n), buffer.getNumChannels() > 1 ? buffer.getMagnitude (1, 0, n) : 0.0f);

    // USE TRACK: record what comes in
    if (capturing.load())
    {
        auto at = captureLength.load();
        auto count = juce::jmin (n, captureBuffer.getNumSamples() - at);
        if (count > 0)
        {
            for (int c = 0; c < 2; ++c)
                captureBuffer.copyFrom (c, at, buffer, juce::jmin (c, buffer.getNumChannels() - 1), 0, count);
            captureLength = at + count;
        }
    }

    // preview: the plugin's own voices are added on top of the track
    auto pd = session.getPlayback();
    const bool bypassed = v (IDs::bypass) > 0.5f || ! usable.load();
    if (playing.load() && pd != nullptr && ! bypassed && std::abs (pd->rate - sampleRate) < 0.5)
    {
        const auto wet = v (IDs::wet) * 0.01f;
        const bool withLead = v (IDs::withLead) > 0.5f;
        const int solo = soloStrip.load();
        leadGain.setTargetValue (withLead && solo < 0 ? v (IDs::dry) * 0.01f : 0.0f);
        choirGain.setTargetValue (tab == 1 && solo < 0 ? juce::Decibels::decibelsToGain (v (IDs::cLevel), -36.0f) * wet : 0.0f);
        for (int i = 0; i < numStrips; ++i)
        {
            auto g = stripGains (i);
            bool on = tab == 0 && pd->stripValid[(size_t) i] && (solo < 0 ? g.audible : solo == i);
            if (solo == i) { g.gl = juce::jmax (g.gl, 0.7f); g.gr = juce::jmax (g.gr, 0.7f); }
            gainL[(size_t) i].setTargetValue (on ? g.gl * wet : 0.0f);
            gainR[(size_t) i].setTargetValue (on ? g.gr * wet : 0.0f);
        }

        auto pos = playPos.load();
        auto* outL = buffer.getWritePointer (0);
        auto* outR = buffer.getWritePointer (buffer.getNumChannels() > 1 ? 1 : 0);
        for (int s = 0; s < n; ++s, ++pos)
        {
            if (pos >= (juce::int64) pd->length) { playing = false; break; }
            auto idx = (size_t) pos;
            float l = 0, r = 0;
            auto lg = leadGain.getNextValue();
            l += pd->lead[idx] * lg; r += pd->lead[idx] * lg;
            for (int i = 0; i < numStrips; ++i)
            {
                auto a = gainL[(size_t) i].getNextValue(), b = gainR[(size_t) i].getNextValue();
                if (a == 0.0f && b == 0.0f) continue;
                auto& st = pd->strips[(size_t) i];
                if (idx >= st.size()) continue;
                l += st.l[idx] * a;
                r += st.r[idx] * b;
            }
            auto cg = choirGain.getNextValue();
            if (cg > 0.0f && pd->choirValid)
                for (auto& sec : pd->choirSections)
                    if (idx < sec.size()) { l += sec.l[idx] * cg; r += sec.r[idx] * cg; }
            outL[s] += l;
            if (outR != outL) outR[s] += r;
        }
        playPos = pos;
    }
    outLevel = juce::jmax (buffer.getMagnitude (0, 0, n), buffer.getNumChannels() > 1 ? buffer.getMagnitude (1, 0, n) : 0.0f);
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

// ------------------------------------------------------------------ state

void Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("preset", presetName, nullptr);
    tree.setProperty ("tab", tab.load(), nullptr);
    tree.setProperty ("vocal", session.getVocalFile().getFullPathName(), nullptr);
    juce::StringArray s;
    for (auto seed : seeds) s.add (juce::String ((juce::int64) seed));
    tree.setProperty ("seeds", s.joinIntoString (","), nullptr);
    tree.setProperty ("choirSeed", juce::String ((juce::int64) choirSeed), nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, dest);
}

void Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    state.replaceState (tree);
    undo.clearUndoHistory();
    presetName = tree.getProperty ("preset", "Pop Thirds").toString();
    tab = (int) tree.getProperty ("tab", 0);
    auto s = juce::StringArray::fromTokens (tree.getProperty ("seeds").toString(), ",", {});
    for (int i = 0; i < numStrips && i < s.size(); ++i) seeds[(size_t) i] = (unsigned long long) s[i].getLargeIntValue();
    choirSeed = (unsigned long long) tree.getProperty ("choirSeed", "1").toString().getLargeIntValue();
    auto vocal = juce::File (tree.getProperty ("vocal").toString());
    if (vocal.existsAsFile() && vocal != session.getVocalFile())
        session.restore (vocal);
    if (onStateRestored) onStateRestored();
}

} // namespace halo

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new halo::Processor();
}
