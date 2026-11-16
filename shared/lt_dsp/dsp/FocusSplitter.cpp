namespace lt::dsp
{

juce::NormalisableRange<float> FocusParams::frequencyRange()
{
    // logarithmic 20 Hz .. 20 kHz
    return { FocusSettings::minHz, FocusSettings::maxHz,
             [] (float s, float e, float t) { return s * std::pow (e / s, t); },
             [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
             [] (float, float, float v) { return v; } };
}

void FocusParams::addTo (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    using namespace juce;
    auto hz = AudioParameterFloatAttributes().withLabel ("Hz")
                .withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { band, 1 }, "Focus Band",
                                                        StringArray { "FULL", "LOW", "MID", "HIGH", "CUSTOM" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { lowCut, 1 }, "Focus Low Cut", frequencyRange(), 30.0f, hz));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { highCut, 1 }, "Focus High Cut", frequencyRange(), 150.0f, hz));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { invert, 1 }, "Focus Invert", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { solo, 1 }, "Focus Solo", false));
}

void FocusParams::Handles::attach (juce::AudioProcessorValueTreeState& s)
{
    band = s.getRawParameterValue (FocusParams::band);
    lowCut = s.getRawParameterValue (FocusParams::lowCut);
    highCut = s.getRawParameterValue (FocusParams::highCut);
    invert = s.getRawParameterValue (FocusParams::invert);
    solo = s.getRawParameterValue (FocusParams::solo);
}

FocusSettings FocusParams::Handles::read() const noexcept
{
    FocusSettings f;
    f.band = juce::roundToInt (band->load());
    auto r = FocusSettings::rangeFor (f.band);
    f.lowHz = f.band == FocusSettings::custom ? lowCut->load() : r.getStart();
    f.highHz = f.band == FocusSettings::custom ? highCut->load() : r.getEnd();
    f.invert = invert->load() > 0.5f;
    f.solo = solo->load() > 0.5f;
    return f;
}

// ------------------------------------------------------------------

void FocusSplitter::prepare (double sr, int maxBlockSize, int numChannels)
{
    sampleRate = sr;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, maxBlockSize), (juce::uint32) juce::jmax (1, numChannels) };

    lowSplit.prepare (spec);
    highSplit.prepare (spec);
    lowAllpass.prepare (spec);
    lowAllpass.setType (juce::dsp::LinkwitzRileyFilterType::allpass);

    lowHz.reset (sr, 0.03);
    highHz.reset (sr, 0.03);
    reset();
}

void FocusSplitter::reset()
{
    lowSplit.reset();
    highSplit.reset();
    lowAllpass.reset();
    wasActive = false;
}

void FocusSplitter::updateCutoffs (const FocusSettings& f)
{
    auto nyquistSafe = (float) sampleRate * 0.45f;
    auto lo = juce::jlimit (FocusSettings::minHz, nyquistSafe / 1.25f, f.lowHz);
    auto hi = juce::jlimit (lo * 1.25f, nyquistSafe, f.highHz);

    if (! wasActive)
    {
        lowHz.setCurrentAndTargetValue (lo);
        highHz.setCurrentAndTargetValue (hi);
    }
    else
    {
        lowHz.setTargetValue (lo);
        highHz.setTargetValue (hi);
    }
}

void FocusSplitter::split (juce::AudioBuffer<float>& io, juce::AudioBuffer<float>& target, const FocusSettings& f)
{
    auto n = io.getNumSamples();
    auto chans = juce::jmin (io.getNumChannels(), target.getNumChannels());
    jassert (target.getNumSamples() >= n);

    if (! f.isActive())
    {
        for (int c = 0; c < chans; ++c)
            target.copyFrom (c, 0, io, c, 0, n);
        io.clear();
        wasActive = false;
        return;
    }

    updateCutoffs (f);
    if (! wasActive)
        reset(), wasActive = true;

    // Cut-offs move in small steps (every 32 samples) so dragging the handles stays smooth.
    constexpr int step = 32;
    for (int start = 0; start < n; start += step)
    {
        auto len = juce::jmin (step, n - start);
        auto lo = lowHz.skip (len), hi = highHz.skip (len);
        lowSplit.setCutoffFrequency (lo);
        highSplit.setCutoffFrequency (hi);
        lowAllpass.setCutoffFrequency (hi);

        for (int c = 0; c < chans; ++c)
        {
            auto* x = io.getWritePointer (c, start);
            auto* t = target.getWritePointer (c, start);
            for (int i = 0; i < len; ++i)
            {
                float lowBand, rest, midBand, highBand;
                lowSplit.processSample (c, x[i], lowBand, rest);
                lowBand = lowAllpass.processSample (c, lowBand);   // keeps the low band in phase with the others
                highSplit.processSample (c, rest, midBand, highBand);

                auto outside = lowBand + highBand;
                t[i] = f.invert ? outside : midBand;
                x[i] = f.invert ? midBand : outside;
            }
        }
    }
}

void FocusSplitter::combine (juce::AudioBuffer<float>& io, const juce::AudioBuffer<float>& target, const FocusSettings& f)
{
    auto n = io.getNumSamples();
    auto chans = juce::jmin (io.getNumChannels(), target.getNumChannels());
    for (int c = 0; c < chans; ++c)
    {
        if (f.solo && f.isActive())
            io.copyFrom (c, 0, target, c, 0, n);
        else
            io.addFrom (c, 0, target, c, 0, n);
    }
}

} // namespace lt::dsp
