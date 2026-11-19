#include "SampleData.h"

namespace quasar
{

int SampleData::rootFromName (const juce::String& n)
{
    // last token like "C", "F#", "Bb2": most one-shot packs name the note without octave, meaning C3 (MIDI 48..60).
    auto base = juce::File::createFileWithoutCheckingPath (n).getFileNameWithoutExtension();
    juce::StringArray tokens;
    tokens.addTokens (base.replace ("-", "_").replace (" ", "_").replace (".", "_"), "_", "");
    static const int semis[] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
    for (int t = tokens.size(); --t >= 0;)
    {
        auto tok = tokens[t].trim();
        if (tok.isEmpty() || tok.length() > 4) continue;
        auto letter = juce::CharacterFunctions::toUpperCase (tok[0]);
        if (letter < 'A' || letter > 'G') continue;
        auto semi = semis[letter - 'A'];
        int i = 1;
        if (i < tok.length() && tok[i] == '#') { ++semi; ++i; }
        else if (i < tok.length() && tok[i] == 'b') { --semi; ++i; }
        auto rest = tok.substring (i);
        if (rest.isNotEmpty() && ! rest.containsOnly ("-0123456789")) continue;
        auto oct = rest.isEmpty() ? 4 : rest.getIntValue();   // "C" alone: middle C (60)
        return juce::jlimit (0, 127, (oct + 1) * 12 + semi);
    }
    return 60;
}

std::unique_ptr<SampleData> SampleData::load (const juce::File& f, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr) { error = "Unsupported audio file"; return {}; }
    auto len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (maxSeconds * reader->sampleRate));
    if (len < 16) { error = "The file is empty"; return {}; }
    juce::AudioBuffer<float> b ((int) juce::jmin (2u, reader->numChannels), len);
    reader->read (&b, 0, len, 0, true, true);
    auto s = fromBuffer (b, reader->sampleRate, f.getFileNameWithoutExtension(), rootFromName (f.getFileName()));
    if (s != nullptr) s->file = f;
    return s;
}

std::unique_ptr<SampleData> SampleData::fromBuffer (const juce::AudioBuffer<float>& b, double sr, const juce::String& n, int root)
{
    auto s = std::make_unique<SampleData>();
    s->name = n;
    s->rootNote = root;
    s->sourceRate = sr;
    auto len = b.getNumSamples();
    auto& l0 = s->levels[0];
    l0.assign ((size_t) len + 4, 0.0f);
    float peak = 0;
    for (int i = 0; i < len; ++i)
    {
        float v = 0;
        for (int c = 0; c < b.getNumChannels(); ++c) v += b.getSample (c, i);
        v /= (float) juce::jmax (1, b.getNumChannels());
        l0[(size_t) i] = v;
        peak = juce::jmax (peak, std::abs (v));
    }
    if (peak > 1.0e-6f) for (auto& v : l0) v /= peak;

    // octave-down copies: 31-tap windowed-sinc half-band, then drop every other sample
    std::array<float, 31> h {};
    for (int k = 0; k < 31; ++k)
    {
        auto m = k - 15;
        auto sinc = m == 0 ? 0.5f : std::sin (juce::MathConstants<float>::pi * 0.5f * (float) m) / (juce::MathConstants<float>::pi * (float) m);
        auto w = 0.42f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) k / 30.0f) + 0.08f * std::cos (2.0f * juce::MathConstants<float>::twoPi * (float) k / 30.0f);
        h[(size_t) k] = sinc * w;
    }
    for (int L = 1; L < numLevels; ++L)
    {
        auto& src = s->levels[(size_t) L - 1];
        auto& dst = s->levels[(size_t) L];
        auto n2 = (int) src.size() / 2;
        dst.assign ((size_t) n2 + 4, 0.0f);
        for (int i = 0; i < n2; ++i)
        {
            float acc = 0;
            for (int k = 0; k < 31; ++k)
            {
                auto j = 2 * i + k - 15;
                if (j >= 0 && j < (int) src.size()) acc += h[(size_t) k] * src[(size_t) j];
            }
            dst[(size_t) i] = acc;
        }
    }
    return s;
}

std::vector<float> SampleData::overview (int points) const
{
    std::vector<float> o ((size_t) juce::jmax (1, points), 0.0f);
    auto& d = levels[0];
    auto per = juce::jmax (1, (int) d.size() / juce::jmax (1, points));
    for (int p = 0; p < points; ++p)
    {
        float m = 0;
        for (int i = p * per; i < juce::jmin ((int) d.size(), (p + 1) * per); ++i) m = juce::jmax (m, std::abs (d[(size_t) i]));
        o[(size_t) p] = m;
    }
    return o;
}

} // namespace quasar
