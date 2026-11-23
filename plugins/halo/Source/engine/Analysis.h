#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

/**
    LT HALO engine (no JUCE dependency, so it can be unit-tested and run on worker threads).

    Analysis of a dry lead vocal on a fixed 5 ms frame grid:
      F0 (WORLD Harvest + StoneMask), spectral envelope (CheapTrick, stored as float) and aperiodicity (D4C),
      note segmentation, global tuning, key (Krumhansl-Kessler), implied diatonic chords (Viterbi) and vibrato.
*/
namespace halo
{

constexpr double frameMs = 5.0;
constexpr double frameSec = frameMs / 1000.0;

double hzToMidi (double hz);
double midiToHz (double midi);
std::string noteName (int midi);                 // "A4"
std::string pitchClassName (int pc);             // "A#"

struct Key
{
    int tonic = 9;            // pitch class
    bool minor = true;
    float confidence = 0.0f;

    const int* steps() const;                    // 7 scale steps
    std::array<int, 7> pitchClasses() const;
    bool contains (int pc) const;
    std::string name() const;                    // "A Minor"
};

struct Note
{
    int start = 0, end = 0;   // frames [start, end)
    int midi = 60;            // quantised pitch (tuning removed)
    double mean = 60.0;       // mean sung pitch
    std::array<int, 3> chord { -1, -1, -1 };
    std::string chordName;

    int frames() const noexcept { return end - start; }
    double ms() const noexcept  { return frames() * frameMs; }
    bool chordHas (int pc) const noexcept { return chord[0] == pc || chord[1] == pc || chord[2] == pc; }
};

/** Spectral data for one frame range, stored as float to keep memory reasonable (3 min of voice ~ 150 MB). */
struct Spectra
{
    int bins = 0;                         // fft/2+1
    int fftSize = 0;
    std::vector<float> sp, ap;            // frames * bins, row-major
    const float* spRow (int f) const noexcept { return sp.data() + (size_t) f * (size_t) bins; }
    const float* apRow (int f) const noexcept { return ap.data() + (size_t) f * (size_t) bins; }
};

struct Analysis
{
    int sampleRate = 44100;
    std::vector<double> audio;            // mono lead
    std::vector<double> f0;               // Hz per frame, 0 = unvoiced
    std::vector<double> midi;             // fractional midi per frame, 0 = unvoiced
    Spectra spectra;
    double tuning = 0.0;                  // semitones, -0.5..0.5
    std::vector<Note> notes;
    Key key;
    double vibratoDepth = 0.15, vibratoRate = 5.5;

    int numFrames() const noexcept { return (int) f0.size(); }
    double seconds() const noexcept { return (double) audio.size() / sampleRate; }
};

struct AnalysisOptions
{
    double f0Floor = 65.0, f0Ceil = 1100.0;
    const Key* forcedKey = nullptr;
};

/** progress: 0..1 callback (may be empty); shouldStop: polled, returns nullptr when cancelled. */
std::shared_ptr<Analysis> analyse (std::vector<double> mono, int sampleRate, const AnalysisOptions&,
                                   const std::function<void (float)>& progress = {},
                                   const std::function<bool()>& shouldStop = {});

// building blocks (exposed for tests and for re-running after a key change)
std::vector<Note> segmentNotes (const std::vector<double>& midi, double tuning, double minNoteMs = 60.0);
double estimateTuning (const std::vector<double>& midi);
Key estimateKey (const std::vector<Note>&);
void estimateChords (std::vector<Note>&, const Key&, double changePenalty = 2.0);
void estimateVibrato (const std::vector<double>& midi, const std::vector<Note>&, double& depth, double& rate);

} // namespace halo
