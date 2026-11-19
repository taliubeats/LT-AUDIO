#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "ModDefs.h"
#include "Dsp.h"
#include "Wavetables.h"
#include "SampleData.h"
#include "LfoShape.h"

namespace quasar
{

class SynthEngine;

/** Values the voices need for one control step, read once per block from the parameters. */
struct BlockParams
{
    struct Osc
    {
        bool on = false; int type = 0, table = 0, oct = 0, warpMode = 0, unison = 1, dest = 0;
        float semi = 0, fine = 0, phase = 0, rand = 1, wtpos = 0, warp = 0, detune = 0.25f, blend = 0.75f, spread = 1, pan = 0, level = 1;
    };
    std::array<Osc, numOscs> osc;
    bool subOn = false; int subShape = 0, subOct = -1, subDest = 0; float subLevel = 1, subPan = 0;
    bool noiseOn = false; int noiseType = 1, noiseDest = 0; float noiseColor = 0, noiseLevel = 0.5f, noisePan = 0;
    struct Flt { bool on = false; int type = 0, dest = 1; float cutoff = 2500, reso = 0.15f, drive = 0, mix = 1, key = 0, pan = 0, level = 1; };
    std::array<Flt, numFilters> flt;
    struct Env { float att = 1, hold = 0, dec = 600, sus = 0, rel = 300, ac = -30, dc = 60, rc = 60; };
    std::array<Env, numEnvs> env;
    float envPAmount = 0, ampVel = 0.4f;
    struct Lfo { float rateHz = 2; bool sync = true; double beats = 1; int mode = 1; float rise = 0, delay = 0, smooth = 0, phase = 0; };
    std::array<Lfo, numLfos> lfo;
    std::array<float, numMacros> macro {};
    int voiceMode = 0, stack = 1, steal = 0, priority = 0, poly = 16, scale = 0;
    bool glideLegato = false, mpe = false;
    float glideMs = 0, masterTune = 0, drift = 0.2f, warmth = 0.2f;
    int transpose = 0, pbUp = 2, pbDown = 2;
    std::array<float, numMixerChannels> channelGain {};   // mute / solo (0 or 1)
};

struct ModSlot { int src = 0, dst = 0, aux = 0, curve = 0; float amt = 0, out = 1; bool bip = false, on = false; };

/** One note (or one voice of a stack). Renders into the engine streams at the internal rate. */
class Voice
{
public:
    void prepare (double internalRate, int controlStep);
    void start (SynthEngine&, int note, float velocity, int channel, int stackIndex, int stackCount, bool legatoGlide, float glideFrom);
    void release();
    void kill();                       // fast fade (voice stealing)
    void retarget (int note, bool retrigger, SynthEngine&);   // mono / legato
    bool isActive() const noexcept     { return active; }
    bool isReleased() const noexcept   { return released; }
    float loudness() const noexcept    { return env[0].level * killGain; }

    void render (SynthEngine&, int numSamples);   // adds into the engine streams (offset by engine.renderPos)

    int note = 60, channel = 1, stackIndex = 0, stackCount = 1;
    float velocity = 1, aftertouch = 0, slide = 0, bend = 0, randomValue = 0;
    juce::uint32 age = 0;
    bool sustained = false;
    std::array<float, numModSources> src {};       // source values at the last control step (for the UI and mono mod)

private:
    friend class SynthEngine;
    void controlStep (SynthEngine&, int stepLen);
    float modValue (int dest, float baseReal, SynthEngine&) const noexcept;

    double isr = 88200.0; int stepSize = 32, stepLeft = 0;
    bool active = false, released = false, killing = false;
    float killGain = 1, killStep = 0;

    std::array<dsp::Envelope, numEnvs> env;
    std::array<float, numEnvs> envPrev {};
    struct LfoState { double phase = 0; float out = 0, smoothed = 0, rnd = 0, rndPrev = 0, elapsed = 0; bool done = false; };
    std::array<LfoState, numLfos> lfo;

    // pitch
    float pitchNow = 60, pitchTarget = 60, glideCoef = 1;

    // oscillators
    struct Unison { double phase = 0; int level = 0; float w = 0; float inc = 0, panL = 1, panR = 1, gain = 1, drift = 0, driftTarget = 0; double samplePos = 0; };
    struct OscState
    {
        std::array<Unison, maxUnison> uni;
        int count = 1, level = 0; float mipW = 0;
        dsp::Ramp gainL, gainR, wtpos, warp;
        float last = 0, sampleRatio = 1;
        bool on = false;
    };
    std::array<OscState, numOscs> osc;
    double subPhase = 0; float subInc = 0, subW = 0; int subLevel = 0; dsp::Ramp subL, subR;
    dsp::Noise noise; dsp::OnePole noiseLp, noiseHp, noiseLp2, crackleLp; dsp::Ramp noiseL, noiseR; float crackle = 0, holdNoise = 0; int holdCount = 0;
    float driftPhase = 0, driftValue = 0, driftTarget = 0;

    // filters (stereo)
    struct FilterState
    {
        std::array<dsp::Ladder, 2> ladder;
        std::array<dsp::Svf, 2> svfA, svfB;
        std::array<std::array<dsp::Svf, 3>, 2> formant;
        std::array<std::vector<float>, 2> comb; int combPos = 0; std::array<float, 2> combLp {};
        dsp::Ramp g, outL, outR, mix;
        float k = 0, drive = 1, combDelay = 100, combFb = 0;
        std::array<float, 3> formantG {};
        int type = 0; bool on = false;
    };
    std::array<FilterState, numFilters> flt;
    dsp::Ramp ampGain;
    float warmthGain = 1;

    std::array<float, 512> modOff {};      // per destination (normalised offsets)
    std::vector<int> touched;
};

/**
    The whole synth: MIDI, voice allocation, modulation, mixer and buses.
    Effects and the master section live in the processor (they need the effect chains).
*/
class SynthEngine
{
public:
    SynthEngine (juce::AudioProcessorValueTreeState&, LfoTables&);

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /** Renders the three bus outputs (MAIN, BUS 1, BUS 2) at the host rate, stereo each. */
    void process (juce::MidiBuffer&, int numSamples, const juce::AudioPlayHead::PositionInfo* position,
                  std::array<juce::AudioBuffer<float>, numBuses>& buses);

    // sample slots (message thread hands over; audio thread reads)
    void setSample (int osc, std::unique_ptr<SampleData>);
    const SampleData* getSample (int osc) const noexcept { return samples[(size_t) osc].load(); }
    void setUserTable (int osc, std::unique_ptr<Wavetable>);
    const Wavetable* getUserTable (int osc) const noexcept { return userTables[(size_t) osc].load(); }
    void collectGarbage();   // message thread: frees replaced samples / tables
    void ensureTables();     // message thread: builds the wavetables the oscillators use now

    // global modulation (for effects and other per-block destinations): offsets in normalised units
    float globalOffset (int dest) const noexcept { return juce::isPositiveAndBelow (dest, 512) ? globalOff[(size_t) dest] : 0.0f; }
    void updateGlobalModulation();

    // --- UI / meters (any thread)
    float getChannelLevel (int ch) const noexcept   { return channelLevel[(size_t) ch].load(); }
    float getSourceValue (int src) const noexcept   { return uiSource[(size_t) juce::jlimit (0, numModSources - 1, src)].load(); }
    float getModOffset (int dest) const noexcept    { return juce::isPositiveAndBelow (dest, 512) ? uiOffset[(size_t) dest].load() : 0.0f; }
    float getLfoPhase (int l) const noexcept        { return uiLfoPhase[(size_t) l].load(); }
    float getEnvPhase (int e) const noexcept        { return uiEnvPos[(size_t) e].load(); }
    int getActiveVoices() const noexcept            { return activeVoices.load(); }
    bool isNoteOn (int note) const noexcept         { return juce::isPositiveAndBelow (note, 128) && keyDown[(size_t) note].load(); }
    float getOscPhaseView (int osc) const noexcept  { return uiOscLevel[(size_t) osc].load(); }

    // keyboard on the UI
    void addUiNote (int note, bool on, float velocity = 0.8f);
    void addUiController (int cc, int value);      // 0..127
    void addUiPitchBend (int value14);             // 0..16383

    // ---- used by the voices
    BlockParams bp;
    std::array<ModSlot, numModSlots> slots;
    int numActiveSlots = 0;
    std::array<juce::RangedAudioParameter*, 512> destParams {};
    std::array<float, 512> destBase {};          // normalised base value
    std::array<float, 512> destBaseReal {};
    LfoTables& lfoTables;
    WavetableLibrary* tables = nullptr;
    std::array<const Wavetable*, numOscs> oscTable {};
    const Wavetable* analogTable = nullptr;
    std::array<const SampleData*, numOscs> oscSample {};
    double internalRate = 88200.0, hostRate = 44100.0;
    int os = 2, renderPos = 0;
    float mipScale = 2.0f;   // oscillators stay band-limited to the host Nyquist even when oversampled
    double bpm = 120.0, ppq = 0.0; bool playing = false;
    float modWheel = 0, pitchBend = 0, channelPressure = 0;
    juce::AudioBuffer<float> streamBuf;          // 14 channels: 7 stereo streams at the internal rate
    std::array<float, numMixerChannels> streamPeak {};
    juce::Random random;

    // destination indices used by the DSP
    struct DestIds
    {
        std::array<int, numOscs> semi {}, fine {}, wtpos {}, warp {}, detune {}, blend {}, spread {}, pan {}, level {};
        int subLevel = 0, subPan = 0, noiseColor = 0, noiseLevel = 0, noisePan = 0;
        std::array<int, numFilters> cutoff {}, reso {}, drive {}, mix {}, fpan {}, flevel {};
        std::array<int, numEnvs> att {}, hold {}, dec {}, sus {}, rel {};
        int envPAmount = 0;
        std::array<int, numLfos> lfoRate {}, lfoRise {}, lfoDelay {}, lfoSmooth {};
        int glide = 0, drift = 0, warmth = 0;
    } d;

    float noteToPitch (float note) const noexcept;   // MIDI note (fractional) -> tuned note, applies scale + master tune
    float lastPitch = 60;
    juce::uint32& lastAgeCounter() noexcept { return ageCounter; }
    std::array<double, numLfos> lfoFreePhase {};

private:
    void readParams();
    void readSlots();
    void handleMidi (const juce::MidiMessage&);
    void noteOn (int note, float velocity, int channel);
    void noteOff (int note, int channel);
    Voice* findFreeVoice();
    void renderVoices (int from, int to);
    void runArp (int blockSamples, juce::MidiBuffer& out);
    void mixToBuses (int numInternal, std::array<juce::AudioBuffer<float>, numBuses>& buses, int numHost);

    juce::AudioProcessorValueTreeState& state;
    juce::SharedResourcePointer<WavetableLibrary> library;
    std::map<juce::String, std::atomic<float>*> raw;
    // allocation-free parameter access for the audio thread
    using Family = std::array<std::map<std::string, std::atomic<float>*, std::less<>>, 32>;
    Family oscF, fltF, envF, lfoF, modF, mixF, macroF;
    std::map<std::string, std::atomic<float>*, std::less<>> globalRaw;
    float fo (const Family& f, int i, const char* field) const noexcept
    {
        auto& m = f[(size_t) i];
        auto it = m.find (field);
        jassert (it != m.end());
        return it != m.end() ? it->second->load() : 0.0f;
    }
    float rg (const char* id) const noexcept
    {
        auto it = globalRaw.find (id);
        jassert (it != globalRaw.end());
        return it != globalRaw.end() ? it->second->load() : 0.0f;
    }
    int arpGateDest = 0;

    std::array<Voice, maxVoices + 8> voices;
    juce::uint32 ageCounter = 0;
    std::vector<int> heldNotes;            // for mono / legato priority and the arp
    std::array<int, 128> noteChannel {};
    bool sustainPedal = false;
    std::array<float, 16> channelBend {}, channelPressureMpe {}, channelSlide {};
    std::array<float, 512> globalOff {};

    // arp
    std::vector<int> arpNotes, arpOrder;
    double arpNextBeat = -1; int arpStep = 0, arpPlaying = -1, arpDir = 1; double arpOffAt = -1; double freeBeats = 0;
    bool arpWasOn = false;

    // oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> os2, os4;
    juce::AudioBuffer<float> busInternal;  // 6 channels at the internal rate
    juce::AudioBuffer<float> dummy, busHost;
    juce::MidiBuffer arpEvents;
    int maxBlock = 512, lastOs = -1;

    // hand-over of samples / user tables
    std::array<std::atomic<const SampleData*>, numOscs> samples {};
    std::array<std::atomic<const Wavetable*>, numOscs> userTables {};
    std::vector<std::unique_ptr<SampleData>> ownedSamples, retiredSamples;
    std::vector<std::unique_ptr<Wavetable>> ownedTables;

    // UI
    juce::AbstractFifo uiFifo { 256 };
    std::array<std::array<int, 3>, 256> uiQueue {};
    std::array<std::atomic<float>, numMixerChannels> channelLevel {};
    std::array<std::atomic<float>, numModSources> uiSource {};
    std::array<std::atomic<float>, 512> uiOffset {};
    std::array<std::atomic<float>, numLfos> uiLfoPhase {};
    std::array<std::atomic<float>, numEnvs> uiEnvPos {};
    std::array<std::atomic<float>, numOscs> uiOscLevel {};
    std::array<std::atomic<bool>, 128> keyDown {};
    std::atomic<int> activeVoices { 0 };
    Voice* newestVoice = nullptr;
};

} // namespace quasar
