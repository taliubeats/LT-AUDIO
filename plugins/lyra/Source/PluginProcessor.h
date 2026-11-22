#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <lt_shared/lt_shared.h>
#include "Piano.h"

namespace lyra
{

/**
    LT LYRA: a mirror of what you play. Reads MIDI (keyboard or clip), never makes sound.
    Loads as an instrument so it sits on a MIDI track in any host; the audio output is silent.
*/
class Processor : public juce::AudioProcessor
{
public:
    Processor();

    lt::licensing::LicenseManager license { lt::licensing::Products::lyra, "LT LYRA" };

    // ------------------------------------------------------------------ saved with the project
    int key = -1;            // -1 auto, 0..11 major, 12..23 minor
    int spelling = 0;        // 0 by key, 1 sharps, 2 flats
    int theme = 0;           // 0 dark, 1 light
    int colour = 0;
    int language = 0;        // 0 EN, 1 ES, 2 JA, 3 DE
    bool usePedal = true;
    int width = 0;
    /** Built-in piano: 0 = off, else the level in dB (-18, -12, -6, 0). The instrument plays it by
        default so LYRA can be used on its own; the audio-effect version starts with it off. */
    int pianoDb = defaultPianoDb();
    static int defaultPianoDb()
    {
       #if LYRA_AUDIO_FX || LYRA_MIDI_FX
        return 0;
       #else
        return -6;
       #endif
    }
    static bool hasAudio()
    {
       #if LYRA_MIDI_FX
        return false;
       #else
        return true;
       #endif
    }

    // ------------------------------------------------------------------ live notes
    /** Notes sounding now (held keys, plus pedal-sustained ones when usePedal). */
    std::array<bool, 128> getSounding() const;
    bool isPedalDown() const noexcept { return pedal.load(); }
    int getVelocity (int note) const noexcept { return velocity[(size_t) (note & 127)].load(); }
    void refreshLicense() { license.refresh(); }

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }
    const juce::String getName() const override
    {
       #if LYRA_AUDIO_FX
        return "LT LYRA FX";
       #elif LYRA_MIDI_FX
        return "LT LYRA MIDI";
       #else
        return "LT LYRA";
       #endif
    }
    bool acceptsMidi() const override                    { return true; }
    bool producesMidi() const override                   { return true; }    // notes pass straight through
#if LYRA_MIDI_FX
    bool isMidiEffect() const override                   { return true; }
#endif
#if ! LYRA_MIDI_FX
    bool isMidiEffect() const override                   { return false; }
#endif
    double getTailLengthSeconds() const override         { return 0.0; }
    int getNumPrograms() override                        { return 1; }
    int getCurrentProgram() override                     { return 0; }
    void setCurrentProgram (int) override                {}
    const juce::String getProgramName (int) override     { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    std::function<void()> onStateRestored;

private:
    void handle (const juce::MidiMessage&);
    Piano piano;

    std::array<std::atomic<juce::uint64>, 2> held {}, sustained {};
    std::array<std::atomic<int>, 128> velocity {};
    std::atomic<bool> pedal { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace lyra
