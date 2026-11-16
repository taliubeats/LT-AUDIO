#pragma once

namespace lt::dsp
{

/**
    Real-time spectrum for the Frequency Focus window.
    The audio thread pushes samples (lock-free); the UI thread calls update() and reads levels.
*/
class SpectrumAnalyser
{
public:
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, numBins = fftSize / 2;

    SpectrumAnalyser();

    void setSampleRate (double sr) noexcept       { sampleRate = sr; }

    /** Audio thread: adds the mono sum of the buffer. */
    void push (const juce::AudioBuffer<float>&) noexcept;

    /** UI thread: consumes new audio. Returns true when the spectrum changed. */
    bool update();

    /** UI thread: smoothed level 0..1 (about -90 dB .. 0 dB) at a frequency. */
    float getLevel (float hz) const noexcept;

private:
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };

    juce::AbstractFifo fifo { fftSize * 4 };
    std::vector<float> ring = std::vector<float> ((size_t) fftSize * 4);
    std::vector<float> frame = std::vector<float> ((size_t) fftSize);
    std::vector<float> fftData = std::vector<float> ((size_t) fftSize * 2);
    std::vector<float> levels = std::vector<float> ((size_t) numBins);
    int frameFill = 0;
    std::atomic<double> sampleRate { 44100.0 };
};

} // namespace lt::dsp
