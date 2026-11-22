#include <juce_audio_formats/juce_audio_formats.h>
#include "Piano.h"
#include <iostream>
int run();
int main()
{
    return run();
}
int run()
{
    lyra::Piano p; const double sr = 48000; p.prepare (sr, 512); p.setGain (0.5f);
    const int total = (int) (sr * 9);
    juce::AudioBuffer<float> out (2, total); out.clear();
    struct Ev { double t; int note; float vel; bool on; };
    std::vector<Ev> ev;
    int chords[4][4] = { { 45, 60, 64, 69 }, { 41, 60, 65, 69 }, { 48, 60, 64, 67 }, { 43, 59, 62, 67 } };   // Am F C G
    for (int c = 0; c < 4; ++c) for (int k = 0; k < 4; ++k) { ev.push_back ({ c * 2.0 + k * 0.012, chords[c][k], 0.55f + 0.1f * k, true }); ev.push_back ({ c * 2.0 + 1.8, chords[c][k], 0, false }); }
    ev.push_back ({ 8.0, 84, 1.0f, true });
    std::sort (ev.begin(), ev.end(), [] (auto& a, auto& b) { return a.t < b.t; });
    int pos = 0; size_t e = 0;
    while (pos < total)
    {
        int next = std::min (total, pos + 512);
        while (e < ev.size() && (int) (ev[e].t * sr) < next)
        {
            int at = std::max (pos, (int) (ev[e].t * sr));
            p.render (out, pos, at - pos); pos = at;
            if (ev[e].on) p.noteOn (ev[e].note, ev[e].vel); else p.noteOff (ev[e].note);
            ++e;
        }
        p.render (out, pos, next - pos); pos = next;
    }
    float peak = out.getMagnitude (0, total); bool finite = true;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < total; ++i) if (! std::isfinite (out.getSample (c, i))) finite = false;
    auto rmsAt = [&] (double a, double b) { return out.getRMSLevel (0, (int) (a * sr), (int) ((b - a) * sr)); };
    // biggest sample-to-sample jump right at a note start (clicks show as jumps)
    float jump = 0; for (int i = 1; i < (int) (0.05 * sr); ++i) jump = std::max (jump, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
    std::cout << "peak " << peak << " finite " << finite << "\n";
    std::cout << "rms chord1 0.05-0.5s " << rmsAt (0.05, 0.5) << "  1.0-1.7s " << rmsAt (1.0, 1.7) << "  after release 1.95-2.0s " << out.getRMSLevel (0, (int) (1.95 * sr), (int) (0.04 * sr)) << "\n";
    std::cout << "max jump at the first attack " << jump << "\n";
    auto outPath = juce::SystemStats::getEnvironmentVariable ("LT_PIANO_WAV", {});
    if (outPath.isEmpty()) return finite && peak < 1.0f ? 0 : 1;
    juce::File f (outPath); f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (out, 0, total);
    return finite && peak < 1.0f ? 0 : 1;
}
