#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "FxTypes.h"
#include "Params.h"
#include "Dsp.h"

namespace quasar
{

struct FxContext { double sampleRate = 48000.0, bpm = 120.0; };

/** One effect. process() works in place and applies its own dry / wet (`mix` 0..1). */
class Effect
{
public:
    virtual ~Effect() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void reset() = 0;
    virtual void process (float* L, float* R, int n, const std::array<float, 8>& p, float mix, const FxContext&) = 0;
    FxType type = FxType::empty;

    static std::unique_ptr<Effect> create (FxType);
};

/**
    The 8 slots of one bus. The message thread creates and prepares effects (service());
    the audio thread picks them up and crossfades from the old one, so changing or moving
    an effect never allocates on the audio thread and never clicks.
*/
class FxChain
{
public:
    FxChain (juce::AudioProcessorValueTreeState&, int busIndex);
    ~FxChain();

    void prepare (double sampleRate, int maxBlock);
    void service (bool immediate = false);   // message thread
    void process (juce::AudioBuffer<float>&, int n, const FxContext&, const std::function<float (int dest)>& globalOffset);
    void reset();

    int getType (int slot) const;
    /** Moves the effect in slot `from` to `to` (message thread): parameters are moved, instances recreated. */
    void move (int from, int to);
    void setType (int slot, int type);       // also writes the default values
    void remove (int slot);
    float getActivity (int slot) const noexcept { return activity[(size_t) slot].load(); }

private:
    struct Slot
    {
        std::atomic<Effect*> pending { nullptr };
        Effect* current = nullptr;
        Effect* fading = nullptr;
        int fadePos = 0;
        int requested = 0;                    // message thread: type of the newest instance
        dsp::Ramp on, mixR;
        std::atomic<float>* type = nullptr, *enabled = nullptr, *mix = nullptr;
        std::array<std::atomic<float>*, numFxParams> p {};
        std::array<int, numFxParams> pDest {};
        int mixDest = 0;
    };

    juce::AudioProcessorValueTreeState& state;
    int bus;
    double sr = 48000.0; int maxBlock = 512;
    std::array<Slot, numFxSlots> slots;
    std::vector<std::unique_ptr<Effect>> alive;     // owned by the message thread
    juce::AbstractFifo retireFifo { 64 };
    std::array<Effect*, 64> retireQueue {};
    juce::AudioBuffer<float> scratch;
    std::array<std::atomic<float>, numFxSlots> activity {};
    juce::CriticalSection serviceLock;
    static constexpr int fadeLength = 512;

    void retire (Effect*) noexcept;
};

} // namespace quasar
