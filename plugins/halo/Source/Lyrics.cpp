#include "Lyrics.h"
#include <juce_audio_basics/juce_audio_basics.h>

#if LT_HALO_WHISPER
 #include <whisper.h>
#endif

namespace halo
{

juce::File Lyrics::modelFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("LT Audio/HALO/models/ggml-small-q5_1.bin");
}

juce::URL Lyrics::modelUrl()
{
    return juce::URL ("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small-q5_1.bin");
}

bool Lyrics::available()
{
   #if LT_HALO_WHISPER
    return modelFile().getSize() > 50 * 1024 * 1024;
   #else
    return false;
   #endif
}

std::vector<Word> Lyrics::transcribe (const std::vector<double>& audio, int sr,
                                      const std::function<void (float)>& progress,
                                      const std::function<bool()>& shouldStop, const char* language)
{
    std::vector<Word> words;
   #if LT_HALO_WHISPER
    if (! available() || audio.empty()) return words;

    // whisper wants 16 kHz mono float
    const double ratio = (double) sr / 16000.0;
    std::vector<float> in (audio.begin(), audio.end()), pcm ((size_t) ((double) audio.size() / ratio) + 1);
    in.resize (in.size() + 16, 0.0f);
    juce::LagrangeInterpolator interp;
    interp.process (ratio, in.data(), pcm.data(), (int) pcm.size());

    auto cparams = whisper_context_default_params();
    cparams.use_gpu = true;
    auto* ctx = whisper_init_from_file_with_params (modelFile().getFullPathName().toRawUTF8(), cparams);
    if (ctx == nullptr) return words;

    auto p = whisper_full_default_params (WHISPER_SAMPLING_GREEDY);
    p.language = language;
    p.translate = false;
    p.print_progress = false;
    p.print_realtime = false;
    p.print_timestamps = false;
    p.print_special = false;
    p.token_timestamps = true;
    p.max_len = 1;              // one word per segment
    p.split_on_word = true;
    p.n_threads = juce::jlimit (1, 8, juce::SystemStats::getNumCpus() - 1);

    struct Ctx { const std::function<void (float)>* progress; const std::function<bool()>* stop; };
    Ctx c { &progress, &shouldStop };
    p.progress_callback = [] (whisper_context*, whisper_state*, int pct, void* user)
    {
        auto* c = static_cast<Ctx*> (user);
        if (c->progress && *c->progress) (*c->progress) ((float) pct / 100.0f);
    };
    p.progress_callback_user_data = &c;
    p.abort_callback = [] (void* user) -> bool
    {
        auto* c = static_cast<Ctx*> (user);
        return c->stop && *c->stop && (*c->stop)();
    };
    p.abort_callback_user_data = &c;

    if (whisper_full (ctx, p, pcm.data(), (int) pcm.size()) == 0)
    {
        const int n = whisper_full_n_segments (ctx);
        for (int i = 0; i < n; ++i)
        {
            auto text = juce::String (juce::CharPointer_UTF8 (whisper_full_get_segment_text (ctx, i))).trim();
            if (text.isEmpty() || text.startsWithChar ('[') || text.startsWithChar ('(')) continue;
            Word w;
            w.text = text.toStdString();
            w.start = (double) whisper_full_get_segment_t0 (ctx, i) / 100.0;
            w.end = (double) whisper_full_get_segment_t1 (ctx, i) / 100.0;
            // punctuation-only tokens belong to the previous word
            if (! words.empty() && text.containsOnly (".,;:!?¡¿-'\"…"))
            {
                words.back().end = std::max (words.back().end, w.end);
                continue;
            }
            words.push_back (w);
        }
    }
    whisper_free (ctx);
   #else
    juce::ignoreUnused (audio, sr, progress, shouldStop, language);
   #endif
    return words;
}

} // namespace halo
