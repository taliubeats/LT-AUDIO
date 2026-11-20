#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

namespace cosmos
{

/** Musical names and helpers. */
namespace music
{
    juce::String noteName (int pitchClass, bool flats);
    juce::String noteNameWithOctave (int midi, bool flats);
    /** 0..11 = C..B major, 12..23 = C..B minor. */
    juce::String keyName (int key);
    bool keyUsesFlats (int key);
    struct ChordType { const char* suffix; std::array<int, 4> intervals; int size; };
    const std::vector<ChordType>& chordTypes();
    juce::String chordName (int root, int type, int bassPc, bool flats);
    /** Roman numeral of a chord root in a key ("ii", "V", "bVII"...). */
    juce::String romanNumeral (int key, int root, int type);
}

/** Recent drum hit. time is in samples (analysis timeline); barTime = bar index + fraction, or -1 when unknown. */
struct Hit
{
    enum Kind { kick = 0, snare = 1, hat = 2 };
    int kind = 0;
    float strength = 0;
    juce::int64 time = 0;
    double barTime = -1.0;
};

/**
    Everything the visuals show, computed on the message thread from the audio
    the processor hands over through its FIFO. Nothing here touches the audio thread.
*/
class Analyzer
{
public:
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder, numBins = fftSize / 2 + 1;
    static constexpr int historySize = 1 << 15;
    static constexpr int numLogBands = 160;      // 25 Hz .. 20 kHz
    static constexpr int numThirds = 30;         // 1/3 octave, 25 Hz .. 20 kHz
    static constexpr int numWaveCols = 2048;
    static constexpr int numOctaves = 9;
    static constexpr int gridSteps = 16;
    static constexpr int lowBins = 28;           // FFT bins used for kick / bass (0 .. ~330 Hz at 48k)
    static constexpr int envLength = 256;        // low-end envelopes (2 bars)
    static constexpr int loudHistory = 600;      // 60 s of 100 ms readings

    Analyzer();

    void prepare (double sampleRate);
    void clear();

    /** Feeds new stereo audio (in time order). */
    void push (const float* left, const float* right, int numSamples);

    /** Host timing in quarter notes. ppqAtSample is the analysis sample the ppq value belongs to. */
    void setHostTiming (bool valid, bool playing, double bpm, double ppq, double barStartPpq, int num, int den,
                        juce::int64 ppqAtSample);

    /** Per display frame: spectra, stereo, tuner. */
    void frame();

    /** Records loudness readings for the history graph (call at ~10 Hz). */
    void pushLoudness (float momentary, float shortTerm);

    double getSampleRate() const noexcept { return sr; }
    juce::int64 getTime() const noexcept  { return totalSamples; }

    float binFrequency (int bin) const noexcept { return (float) (bin * sr / fftSize); }
    static float logBandFrequency (float band) noexcept; // band index (fractional) -> Hz
    static float thirdFrequency (int i) noexcept;

    /** Copies the newest n history samples (oldest first). */
    void copyHistory (int channel, float* dest, int n) const;

    // ------------------------------------------------------------------ spectra
    std::vector<float> spectrumDb, peakDb, leftDb, rightDb;          // per FFT bin, dBFS (smoothed)
    std::array<float, numLogBands> logDb {}, logRaw {};              // smoothed / instant
    std::array<float, numThirds> thirdDb {}, thirdWidth {};          // level and width (0 mono .. 1 wide)
    float correlation = 1.0f, balance = 0.0f;                        // -1..1, L(-1)..R(+1)

    // ------------------------------------------------------------------ levels
    std::array<float, 2> peak {}, rms {}, vu {};                     // linear / dB
    struct WaveCol { float minL = 0, maxL = 0, minR = 0, maxR = 0, low = 0, high = 0; };
    std::array<WaveCol, numWaveCols> wave {};
    int waveWrite = 0, waveColLength = 256;

    // ------------------------------------------------------------------ loudness history
    std::array<float, loudHistory> momentaryHistory {}, shortHistory {};
    int loudWrite = 0;

    // ------------------------------------------------------------------ tuner
    float tunerHz = 0, tunerCents = 0, tunerClarity = 0;
    int tunerMidi = -1;

    // ------------------------------------------------------------------ rhythm
    std::array<float, 3> hitFlash {};                                // decays each frame
    std::vector<Hit> hits;                                           // last ~8 s
    float bpm = 0;
    bool hostTiming = false, playing = false;
    int beatsPerBar = 4;
    double barTime = -1.0;                                           // bar index + fraction, or -1
    float beatPulse = 0;                                             // 1 on the beat, decays
    float onsetFlux = 0;                                             // overall novelty (0..1)

    struct BarGrid
    {
        std::array<std::array<float, numOctaves>, gridSteps> energy {};
        std::array<int, gridSteps> hits {};                          // bit 0 kick, 1 snare, 2 hat
        juce::int64 index = -1;
    };
    BarGrid currentBar, previousBar;
    std::array<float, numOctaves> octaveDb {};

    // ------------------------------------------------------------------ harmony
    std::array<float, 12> chroma {}, chromaKey {};                  // normalised 0..1
    int key = -1;                                                    // see music::keyName
    float keyConfidence = 0;
    int chordRoot = -1, chordType = 0, bassPc = -1;
    juce::String chordText = "-";
    int bassMidi = -1;
    struct BarChord { juce::int64 bar = -1; int root = -1, type = 0; };
    std::array<BarChord, 8> chordStrip {};                          // last 8 bars, oldest first

    // ------------------------------------------------------------------ low end
    std::array<float, lowBins> kickSpectrum {}, bassSpectrum {};     // dB
    std::array<float, envLength> kickEnv {}, bassEnv {};             // 0..1, newest at envWrite-1
    int envWrite = 0;
    float duckDb = 0, kickDb = -90, bassDb = -90;

    // ------------------------------------------------------------------ frame motion (for generative visuals)
    float energy = 0, energyLow = 0, energyMid = 0, energyHigh = 0;  // 0..1 smoothed
    double phase = 0;                                                // seconds of audio seen

private:
    void hop();
    void detectOnsets (const float* mag);
    void updateChroma();
    void updateTempo();
    void updateBarClock();
    void runTuner();
    void addHit (int kind, float strength);
    void computeSpectrum (const float* x, float* outMag, std::vector<float>* complexOut);

    double sr = 48000.0;
    juce::dsp::FFT fft { fftOrder }, bigFft { fftOrder + 1 };
    std::vector<float> bigWindow, bigWork, bigMag;
    int hopCount = 0;
    std::vector<float> window, work, mag, magL, magR, cplxL, cplxR;
    std::array<std::vector<float>, 2> hist;
    int histWrite = 0;
    juce::int64 totalSamples = 0;

    int hopLength = 1024, hopFill = 0;
    double hopRate = 46.875;

    // onsets
    std::array<float, 4> bandPrev {}, bandPrev2 {}, fluxMean {}, fluxDev {};
    std::array<juce::int64, 3> lastHit { -100000, -100000, -100000 };
    std::vector<float> noveltyRing;
    int noveltyWrite = 0;
    int tempoCountdown = 0;
    float freeBpm = 0, beatPhaseHops = 0;
    std::array<float, 8> downbeatScore {};
    int downbeatOffset = 0, previousBarPending = 0;

    // host timing
    bool hostValid = false;
    double hostBpm = 120, hostPpq = 0, hostBarStart = 0;
    int hostNum = 4, hostDen = 4;
    juce::int64 hostAt = 0;
    juce::int64 lastBarIndex = -1, lastBeatIndex = -1;

    // harmony
    std::array<float, 12> chromaFast {}, chromaSlow {};
    std::array<int, 12 * 16> barChordVotes {};
    int stableRoot = -1, stableType = 0, candidateRoot = -1, candidateType = 0, candidateCount = 0;
    double freeBeats = 0; float freeBpmCandidate = 0; int beatKick = 0; std::array<float, 4> beatKickAcc {};
    std::array<float, 12> chordVote {};

    // low end
    juce::int64 lastKickTime = -100000;
    float duckPre = 0, duckPost = 0;
    int pendingKickCapture = 0;

    int frameCount = 0;
    float lowState = 0;
    std::array<float, 2> blockPeak {};
    juce::int64 waveFill = 0;
    WaveCol waveAcc;
    std::vector<float> yinBuf, yinDiff;
};

} // namespace cosmos
