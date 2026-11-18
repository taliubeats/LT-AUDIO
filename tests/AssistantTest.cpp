// Checks the built-in (offline) assistant of LT VOCAL SUITE: common requests in Spanish/English
// must change the right parameters, stay inside the ranges, and give a readable summary.
#include "Assistant.h"
#include <iostream>

using namespace vocal;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl;
    if (! ok) ++failures;
}

static std::map<juce::String, float> defaults()
{
    std::map<juce::String, float> m;
    for (auto& p : paramInfos()) m[p.id] = p.def;
    return m;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto cur = defaults();
    auto u = [] (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); };

    struct Case { const char* text; std::vector<const char*> mustChange; };
    const Case cases[] = {
        { "quiero un vocal chain estilo trap melódico", { IDs::retune, IDs::delayMix } },
        { "más brillo", { IDs::air } },
        { "menos reverb", { IDs::reverbMix } },
        { "autotune duro tipo robot", { IDs::retune } },
        { "voz de teléfono para la intro", { IDs::phone } },
        { "make it wider", { IDs::width } },
        { "hay mucha sibilancia en las eses", { IDs::deEss } },
        { "más compresión y más cuerpo", { IDs::comp, IDs::body } },
        { "reggaeton", { IDs::retune } },
        { "quita el ruido de fondo", { IDs::gate } },
    };

    for (auto& c : cases)
    {
        auto r = Assistant::offlineReply (u (c.text), cur);
        std::cout << c.text << "  ->  " << r.summary << std::endl;
        check (! r.failed && r.summary.isNotEmpty(), "understood");
        for (auto* id : c.mustChange)
        {
            auto it = r.changes.find (id);
            check (it != r.changes.end() && std::abs (it->second - cur[id]) > 1.0e-4f, juce::String ("changes ") + id);
        }
        for (auto& [id, v] : r.changes)
        {
            auto* p = findParam (id);
            check (p != nullptr && v >= p->min - 1.0e-4f && v <= p->max + 1.0e-4f, "in range " + id);
        }
    }

    auto nonsense = Assistant::offlineReply ("xyzzy", cur);
    check (nonsense.failed && nonsense.changes.empty(), "unknown request changes nothing");

    std::cout << (failures == 0 ? "ALL ASSISTANT TESTS PASSED" : "ASSISTANT TESTS FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
