namespace lt::dsp
{

SpectrumAnalyser::SpectrumAnalyser()
{
    std::fill (levels.begin(), levels.end(), 0.0f);
}

void SpectrumAnalyser::push (const juce::AudioBuffer<float>& buffer) noexcept
{
    auto n = buffer.getNumSamples();
    auto chans = buffer.getNumChannels();
    if (n == 0 || chans == 0)
        return;

    auto gain = 1.0f / (float) chans;
    int s1, n1, s2, n2;
    fifo.prepareToWrite (n, s1, n1, s2, n2);

    auto write = [&] (int dest, int count, int srcOffset)
    {
        for (int i = 0; i < count; ++i)
        {
            float sum = 0.0f;
            for (int c = 0; c < chans; ++c)
                sum += buffer.getSample (c, srcOffset + i);
            ring[(size_t) (dest + i)] = sum * gain;
        }
    };
    write (s1, n1, 0);
    write (s2, n2, n1);
    fifo.finishedWrite (n1 + n2);
}

bool SpectrumAnalyser::update()
{
    bool changed = false;

    while (fifo.getNumReady() > 0)
    {
        int s1, n1, s2, n2;
        auto want = juce::jmin (fifo.getNumReady(), fftSize - frameFill);
        fifo.prepareToRead (want, s1, n1, s2, n2);
        std::copy (ring.begin() + s1, ring.begin() + s1 + n1, frame.begin() + frameFill);
        std::copy (ring.begin() + s2, ring.begin() + s2 + n2, frame.begin() + frameFill + n1);
        fifo.finishedRead (n1 + n2);
        frameFill += n1 + n2;

        if (frameFill < fftSize)
            break;

        // analyse with 50% overlap
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        std::copy (frame.begin(), frame.end(), fftData.begin());
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        for (int i = 0; i < numBins; ++i)
        {
            auto mag = fftData[(size_t) i] * 4.0f / (float) fftSize;
            auto db = juce::Decibels::gainToDecibels (mag, -90.0f);
            auto level = juce::jmap (db, -90.0f, 0.0f, 0.0f, 1.0f);
            auto& l = levels[(size_t) i];
            l = level > l ? l + (level - l) * 0.6f : l + (level - l) * 0.15f;   // fast rise, slow fall
        }

        std::copy (frame.begin() + fftSize / 2, frame.end(), frame.begin());
        frameFill = fftSize / 2;
        changed = true;
    }

    return changed;
}

float SpectrumAnalyser::getLevel (float hz) const noexcept
{
    auto binF = hz / (float) sampleRate.load() * (float) fftSize;
    auto b = juce::jlimit (0, numBins - 2, (int) binF);
    auto frac = juce::jlimit (0.0f, 1.0f, binF - (float) b);

    // at high frequencies one pixel covers many bins: take the max of the neighbourhood
    auto width = juce::jmax (1, (int) (binF * 0.03f));
    auto level = 0.0f;
    for (int i = juce::jmax (0, b - width / 2); i <= juce::jmin (numBins - 1, b + width / 2); ++i)
        level = juce::jmax (level, levels[(size_t) i]);

    if (width <= 1)
        level = juce::jmap (frac, levels[(size_t) b], levels[(size_t) b + 1]);
    return level;
}

} // namespace lt::dsp
