#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace lyra
{

/**
    A small modelled piano so LYRA can be played on its own (no samples).
    Each voice is a bank of slightly inharmonic string partials with a two-stage decay
    (the fast "prompt" sound and the long "aftersound"), two detuned strings for the mid
    and high range, a velocity-shaped hammer thump, and dampers that obey the sustain pedal.
*/
class Piano
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void noteOn (int note, float velocity);   // velocity 0..1
    void noteOff (int note);
    void setPedal (bool down);
    void allOff();
    /** Adds the piano into the buffer (stereo), from start for n samples. */
    void render (juce::AudioBuffer<float>&, int start, int n);
    void setGain (float linear) { gain = linear; }

private:
    static constexpr int numVoices = 24, numPartials = 14;
    struct Partial { float re = 0, im = 0, cosw = 1, sinw = 0, amp = 0, fast = 0, slow = 0, ef = 0, es = 0; };   // ef / es: fast and slow decay stages
    struct Voice
    {
        int note = -1;
        bool held = false, releasing = false;
        float age = 0, level = 0, damp = 1, dampRate = 0;
        float noise = 0, noiseState = 0, noiseCoef = 0;
        float panL = 0.7f, panR = 0.7f;
        std::array<Partial, numPartials * 2> p;
        int count = 0;
    };

    void start (Voice&, int note, float velocity);
    double sr = 48000.0;
    std::array<Voice, numVoices> voices;
    bool pedal = false;
    float gain = 0.5f;
    juce::Random rnd;
    juce::Reverb reverb;
    juce::AudioBuffer<float> scratch;
};

} // namespace lyra
