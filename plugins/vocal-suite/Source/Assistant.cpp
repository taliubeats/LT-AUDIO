#include "Assistant.h"
#include <lt_shared/lt_shared.h>

namespace vocal
{

namespace
{
    juce::String valuesAsText (const std::map<juce::String, float>& values)
    {
        juce::String s;
        for (auto& p : paramInfos())
        {
            auto it = values.find (p.id);
            if (it == values.end()) continue;
            s << p.id << " = ";
            auto unit = juce::String (p.unit);
            if (unit == "choice") s << choicesFor (p.id)[juce::roundToInt (it->second)] << " (" << juce::roundToInt (it->second) << ")";
            else if (unit == "bool") s << (it->second > 0.5f ? "on (1)" : "off (0)");
            else s << juce::String (it->second, 2) << " " << unit;
            s << "\n";
        }
        return s;
    }

    float clampTo (const ParamInfo& p, float v)
    {
        v = juce::jlimit (p.min, p.max, v);
        auto unit = juce::String (p.unit);
        if (unit == "choice" || unit == "bool") v = (float) juce::roundToInt (v);
        return v;
    }

    bool containsAny (const juce::String& text, std::initializer_list<const char*> words)
    {
        for (auto* w : words)
            if (text.contains (juce::String (juce::CharPointer_UTF8 (w)))) return true;
        return false;
    }
}

Assistant::Assistant() : juce::Thread ("LT VOCAL SUITE assistant")
{
    apiKey = keyFile().loadFileAsString().trim();
}

Assistant::~Assistant()
{
    *alive = false;
    stopThread (8000);
}

juce::File Assistant::keyFile()
{
    return lt::licensing::LicenseManager::getDataDirectory().getChildFile ("Assistant").getChildFile ("claude-api-key.txt");
}

void Assistant::setApiKey (const juce::String& key)
{
    const juce::ScopedLock sl (lock);
    apiKey = key.trim();
    auto f = keyFile();
    if (apiKey.isEmpty()) { f.deleteFile(); return; }
    f.getParentDirectory().createDirectory();
    f.replaceWithText (apiKey);
}

bool Assistant::hasApiKey() const
{
    const juce::ScopedLock sl (lock);
    return apiKey.isNotEmpty();
}

void Assistant::ask (const juce::String& request, const std::map<juce::String, float>& current)
{
    if (isThreadRunning() || request.trim().isEmpty()) return;
    {
        const juce::ScopedLock sl (lock);
        pendingRequest = request.trim();
        pendingValues = current;
    }
    startThread();
}

void Assistant::run()
{
    juce::String request, key;
    std::map<juce::String, float> values;
    {
        const juce::ScopedLock sl (lock);
        request = pendingRequest; key = apiKey; values = pendingValues;
    }

    AssistantReply reply;
    if (key.isNotEmpty())
    {
        juce::String error;
        reply = askClaude (request, values, error);
        if (error.isNotEmpty())
        {
            // fall back to the offline assistant so the user still gets something useful
            reply = offlineReply (request, values);
            reply.summary = "(AI unavailable: " + error + ") " + reply.summary;
        }
    }
    else
    {
        reply = offlineReply (request, values);
    }

    juce::MessageManager::callAsync ([this, reply, token = alive] { if (*token && onReply) onReply (reply); });
}

// ------------------------------------------------------------------ Claude

juce::String Assistant::systemPrompt()
{
    juce::String s;
    s << "You are the built-in mixing assistant of LT VOCAL SUITE, a vocal chain plugin used by music producers. "
         "The user writes (often in Spanish) what they want their vocal to sound like, e.g. a style, an artist's vocal "
         "production style, or a change like 'more air' or 'less reverb'. You answer by choosing parameter values for the chain.\n\n"
         "Rules:\n"
         "- Recreate the PROCESSING (tuning style, EQ, compression, saturation, de-essing, delay, reverb, width). "
         "You are not cloning anyone's voice; when asked for an artist, use what is publicly known about how that artist's vocals are typically produced.\n"
         "- Only change the parameters that matter for the request; for small requests change one to three values.\n"
         "- Stay musical: avoid extreme settings unless the user asks for an extreme effect.\n"
         "- Write the summary in the same language the user wrote in, one short sentence, like 'Listo: tune mas duro, mas aire y delays a 1/4'.\n"
         "- Use the exact parameter ids below. Values are in the units shown; choices and on/off use their index number.\n\n"
         "Chain order: CLEANUP -> TUNE -> TONE -> DYNAMICS -> FX -> SPACE -> OUTPUT.\n\nParameters:\n";
    for (auto& p : paramInfos())
    {
        if (juce::String (p.id) == IDs::bypass) continue;
        s << "- " << p.id << " (" << p.module << "): " << p.help << ". ";
        auto unit = juce::String (p.unit);
        if (unit == "choice")
        {
            auto c = choicesFor (p.id);
            s << "Choices: ";
            for (int i = 0; i < c.size(); ++i) s << i << "=" << c[i] << (i + 1 < c.size() ? ", " : "");
        }
        else if (unit == "bool") s << "0 = off, 1 = on";
        else s << "Range " << juce::String (p.min, 2) << " to " << juce::String (p.max, 2) << " " << unit;
        s << "\n";
    }
    return s;
}

juce::var Assistant::responseSchema()
{
    auto idEnum = juce::Array<juce::var>();
    for (auto& p : paramInfos())
        if (juce::String (p.id) != IDs::bypass) idEnum.add (juce::String (p.id));

    auto change = std::make_unique<juce::DynamicObject>();
    change->setProperty ("type", "object");
    {
        auto props = std::make_unique<juce::DynamicObject>();
        auto id = std::make_unique<juce::DynamicObject>();
        id->setProperty ("type", "string");
        id->setProperty ("enum", idEnum);
        props->setProperty ("id", juce::var (id.release()));
        auto value = std::make_unique<juce::DynamicObject>();
        value->setProperty ("type", "number");
        props->setProperty ("value", juce::var (value.release()));
        change->setProperty ("properties", juce::var (props.release()));
    }
    change->setProperty ("required", juce::Array<juce::var> { "id", "value" });
    change->setProperty ("additionalProperties", false);

    auto schema = std::make_unique<juce::DynamicObject>();
    schema->setProperty ("type", "object");
    auto props = std::make_unique<juce::DynamicObject>();
    auto summary = std::make_unique<juce::DynamicObject>();
    summary->setProperty ("type", "string");
    props->setProperty ("summary", juce::var (summary.release()));
    auto changes = std::make_unique<juce::DynamicObject>();
    changes->setProperty ("type", "array");
    changes->setProperty ("items", juce::var (change.release()));
    props->setProperty ("changes", juce::var (changes.release()));
    schema->setProperty ("properties", juce::var (props.release()));
    schema->setProperty ("required", juce::Array<juce::var> { "summary", "changes" });
    schema->setProperty ("additionalProperties", false);
    return juce::var (schema.release());
}

AssistantReply Assistant::askClaude (const juce::String& request, const std::map<juce::String, float>& current, juce::String& error)
{
    AssistantReply reply;

    auto userText = "Current settings:\n" + valuesAsText (current) + "\nRequest: " + request;

    juce::Array<juce::var> messages;
    {
        const juce::ScopedLock sl (lock);
        messages = history;
    }
    auto userMsg = std::make_unique<juce::DynamicObject>();
    userMsg->setProperty ("role", "user");
    userMsg->setProperty ("content", userText);
    messages.add (juce::var (userMsg.release()));

    // stable system prompt first, cached between requests
    auto sysBlock = std::make_unique<juce::DynamicObject>();
    sysBlock->setProperty ("type", "text");
    sysBlock->setProperty ("text", systemPrompt());
    auto cache = std::make_unique<juce::DynamicObject>();
    cache->setProperty ("type", "ephemeral");
    sysBlock->setProperty ("cache_control", juce::var (cache.release()));

    auto format = std::make_unique<juce::DynamicObject>();
    format->setProperty ("type", "json_schema");
    format->setProperty ("schema", responseSchema());
    auto outputConfig = std::make_unique<juce::DynamicObject>();
    outputConfig->setProperty ("effort", "low");                  // quick answers; mixing choices don't need deep reasoning
    outputConfig->setProperty ("format", juce::var (format.release()));

    auto body = std::make_unique<juce::DynamicObject>();
    body->setProperty ("model", model);
    body->setProperty ("max_tokens", 16000);
    body->setProperty ("system", juce::Array<juce::var> { juce::var (sysBlock.release()) });
    body->setProperty ("messages", messages);
    body->setProperty ("output_config", juce::var (outputConfig.release()));
    body->setProperty ("fallbacks", "default");                   // server-side fallback if a request is declined
    auto json = juce::JSON::toString (juce::var (body.release()), true);

    juce::String key;
    { const juce::ScopedLock sl (lock); key = apiKey; }

    juce::StringPairArray headersOut;
    int status = 0;
    auto headers = "content-type: application/json\r\n"
                   "anthropic-version: 2026-10-21\r\n"
                   "anthropic-beta: server-side-fallback-2026-07-01\r\n"
                   "x-api-key: " + key + "\r\n";

    auto url = juce::URL ("https://api.anthropic.com/v1/messages").withPOSTData (json);
    auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                             .withExtraHeaders (headers)
                                             .withConnectionTimeoutMs (90000)
                                             .withResponseHeaders (&headersOut)
                                             .withStatusCode (&status)
                                             .withHttpRequestCmd ("POST"));
    if (stream == nullptr) { error = "no internet connection"; return reply; }

    auto responseText = stream->readEntireStreamAsString();
    auto resp = juce::JSON::parse (responseText);

    if (status != 200)
    {
        auto msg = resp["error"]["message"].toString();
        error = status == 401 ? "invalid API key"
              : status == 429 ? "rate limited, try again in a moment"
              : status >= 500 ? "Claude is busy, try again"
              : (msg.isNotEmpty() ? msg : "HTTP " + juce::String (status));
        return reply;
    }

    if (resp["stop_reason"].toString() == "refusal")
    {
        error = "the request was declined";
        return reply;
    }

    juce::String text;
    if (auto* content = resp["content"].getArray())
        for (auto& block : *content)
            if (block["type"].toString() == "text") { text = block["text"].toString(); break; }

    auto out = juce::JSON::parse (text);
    if (! out.isObject()) { error = "unexpected answer"; return reply; }

    reply.usedAI = true;
    reply.summary = out["summary"].toString();
    if (auto* changes = out["changes"].getArray())
        for (auto& c : *changes)
            if (auto* p = findParam (c["id"].toString()))
                reply.changes[p->id] = clampTo (*p, (float) (double) c["value"]);

    // keep a short memory of the conversation (text only)
    {
        const juce::ScopedLock sl (lock);
        auto u = std::make_unique<juce::DynamicObject>();
        u->setProperty ("role", "user"); u->setProperty ("content", "Request: " + request);
        auto a = std::make_unique<juce::DynamicObject>();
        a->setProperty ("role", "assistant"); a->setProperty ("content", text);
        history.add (juce::var (u.release()));
        history.add (juce::var (a.release()));
        while (history.size() > 12) history.remove (0);
    }
    return reply;
}

// ------------------------------------------------------------------ offline assistant

juce::StringArray Assistant::describeChanges (const std::map<juce::String, float>& changes, const std::map<juce::String, float>& current)
{
    auto T = [] (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); };
    auto before = [&] (const char* id) { auto it = current.find (id); return it != current.end() ? it->second : findParam (id)->def; };
    auto after = [&] (const char* id) { auto it = changes.find (id); return it != changes.end() ? it->second : before (id); };
    auto delta = [&] (const char* id) { return after (id) - before (id); };
    juce::StringArray out;

    if (after (IDs::tuneOn) > 0.5f)
    {
        if (after (IDs::retune) <= 5.0f && delta (IDs::retune) < -3.0f) out.add (T ("tune duro"));
        else if (delta (IDs::retune) < -8.0f) out.add (T ("tune más rápido"));
        else if (delta (IDs::retune) > 8.0f) out.add (T ("tune más natural"));
    }
    if (delta (IDs::air) > 1.0f || delta (IDs::presence) > 1.5f) out.add (T ("más brillo"));
    else if (delta (IDs::air) < -1.0f) out.add (T ("más oscura"));
    if (delta (IDs::body) > 1.0f) out.add (T ("más cuerpo"));
    if (delta (IDs::comp) > 8.0f) out.add (T ("más compresión"));
    else if (delta (IDs::comp) < -8.0f) out.add (T ("menos compresión"));
    if (delta (IDs::satDrive) > 8.0f) out.add (T ("más saturación"));
    if (delta (IDs::doubler) > 10.0f || delta (IDs::width) > 10.0f) out.add (T ("más ancha"));
    if (after (IDs::phone) > 30.0f && delta (IDs::phone) > 10.0f) out.add (T ("filtro teléfono"));
    if (after (IDs::delayOn) > 0.5f && after (IDs::delayMix) > 6.0f
        && (delta (IDs::delayMix) > 3.0f || delta (IDs::delayNote) != 0.0f || before (IDs::delayOn) < 0.5f))
        out.add (T ("delays a ") + choicesFor (IDs::delayNote)[juce::roundToInt (after (IDs::delayNote))]);
    if (after (IDs::reverbOn) > 0.5f && (delta (IDs::reverbMix) > 3.0f || delta (IDs::reverbType) != 0.0f || before (IDs::reverbOn) < 0.5f))
        out.add (T ("reverb ") + choicesFor (IDs::reverbType)[juce::roundToInt (after (IDs::reverbType))].toLowerCase());
    else if (delta (IDs::reverbMix) < -3.0f) out.add (T ("más seca"));
    while (out.size() > 4) out.remove (out.size() - 1);
    return out;
}

AssistantReply Assistant::offlineReply (const juce::String& requestIn, const std::map<juce::String, float>& current)
{
    AssistantReply r;
    auto T = [] (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); };
    auto text = requestIn.toLowerCase();
    juce::StringArray done;

    auto get = [&] (const char* id) { auto it = current.find (id); return it != current.end() ? it->second : findParam (id)->def; };
    auto set = [&] (const char* id, float v) { if (auto* p = findParam (id)) r.changes[id] = clampTo (*p, v); };
    auto add = [&] (const char* id, float d) { set (id, (r.changes.count (id) ? r.changes[id] : get (id)) + d); };
    auto less = containsAny (text, { "menos", "less", "quita", "baja", "reduce", "remove", "lower", "sin " });
    juce::String styleLabel;

    // ---- whole styles first (they set many values at once)
    struct Style { std::initializer_list<const char*> words; const char* preset; const char* label; };
    const Style styles[] = {
        { { "rage" },                                     "Rage Vocal",         "estilo rage" },
        { { "robot", "t-pain", "tpain", "hard tune", "autotune duro", "tune duro", "muy afinad" }, "Hard Tune Robot", "hard tune" },
        { { "drill" },                                    "Drill Vocal",        "estilo drill" },
        { { "boom bap", "boombap", "old school" },        "Boom Bap Rap",       "boom bap" },
        { { "melodic", "melódic", "trap" },               "Melodic Trap Lead",  "trap melódico" },
        { { "r&b", "rnb", "r and b", "soul" },            "R&B Silk",           "R&B" },
        { { "reggaeton", "regueton", "perreo", "dembow" },"Reggaeton Lead",     "reggaetón" },
        { { "afro" },                                     "Afro Smooth",        "afro" },
        { { "balada", "ballad", "acústic", "acoustic" },  "Ballad Natural",     "balada natural" },
        { { "pop" },                                      "Pop Lead Polished",  "pop" },
        { { "rock", "punk", "grunge" },                   "Rock Grit",          "rock" },
        { { "lofi", "lo-fi", "lo fi" },                   "Lo-Fi Bedroom",      "lo-fi" },
        { { "rap", "hip hop", "hiphop" },                 "Modern Rap Upfront", "rap moderno" },
        { { "teléfono", "telefono", "phone", "radio" },   "Phone Intro",        "efecto teléfono" },
        { { "ad-lib", "adlib", "ad lib" },                "Ad-Lib Wide",        "ad-libs abiertos" },
        { { "etére", "ethereal", "angel", "celestial", "shimmer" }, "Ethereal Shimmer", "voz etérea" },
    };
    for (auto& st : styles)
    {
        if (! containsAny (text, st.words)) continue;
        for (auto& pr : factoryPresets())
            if (pr.name == st.preset)
                for (auto& [k, v] : pr.values) r.changes[k] = v;
        done.add (juce::String (juce::CharPointer_UTF8 (st.label)));
        styleLabel = done[0];
        break;
    }

    // ---- tweaks
    if (containsAny (text, { "brill", "bright", "aire", "air", "agudo", "crisp" }))
    { add (IDs::air, less ? -3.0f : 3.0f); add (IDs::presence, less ? -1.0f : 1.0f); done.add (less ? T ("menos brillo") : T ("más brillo")); }
    if (containsAny (text, { "oscur", "dark", "opac" }))
    { add (IDs::air, -3.0f); done.add (T ("más oscura")); }
    if (containsAny (text, { "cuerpo", "body", "cálid", "calid", "warm", "grave", "gorda", "fat" }))
    { add (IDs::body, less ? -2.0f : 2.0f); add (IDs::satDrive, less ? 0.0f : 6.0f); done.add (less ? T ("menos cuerpo") : T ("más cuerpo y calidez")); }
    if (containsAny (text, { "barro", "mud", "boxy", "caja", "nasal" }))
    { add (IDs::mud, 3.0f); done.add (T ("menos barro")); }
    if (containsAny (text, { "presencia", "presence", "al frente", "adelante", "upfront", "clar", "clear" }))
    { add (IDs::presence, less ? -2.0f : 2.0f); add (IDs::comp, less ? 0.0f : 10.0f); done.add (T ("voz más al frente")); }
    if (containsAny (text, { "reverb", "reverberación", "espacio", "space", "ambiente" }))
    { add (IDs::reverbMix, less ? -8.0f : 8.0f); if (! less) set (IDs::reverbOn, 1); done.add (less ? T ("menos reverb") : T ("más reverb")); }
    if (containsAny (text, { "seco", "seca", "dry" }))
    { set (IDs::reverbMix, juce::jmin (6.0f, get (IDs::reverbMix))); set (IDs::delayMix, juce::jmin (5.0f, get (IDs::delayMix))); done.add (T ("más seca")); }
    if (containsAny (text, { "delay", "eco", "echo", "throw" }))
    { add (IDs::delayMix, less ? -8.0f : 8.0f); if (! less) set (IDs::delayOn, 1); done.add (less ? T ("menos delay") : T ("más delay")); }
    if (containsAny (text, { "compres", "comp ", "punch", "control", "pareja", "even" }))
    { add (IDs::comp, less ? -12.0f : 12.0f); done.add (less ? T ("menos compresión") : T ("más compresión")); }
    if (containsAny (text, { "sibil", " s ", "eses", "ess", "silb", "harsh", "chill" }))
    { add (IDs::deEss, 20.0f); done.add (T ("menos sibilancia")); }
    if (containsAny (text, { "distors", "sucia", "grit", "agresiv", "aggressive", "satur", "crunch" }))
    { add (IDs::satDrive, less ? -12.0f : 15.0f); done.add (less ? T ("menos saturación") : T ("más saturación")); }
    if (containsAny (text, { "ancha", "ancho", "wide", "abierta", "stereo", "estéreo", "doble", "double" }))
    { add (IDs::doubler, less ? -20.0f : 20.0f); add (IDs::width, less ? -15.0f : 15.0f); done.add (less ? T ("más centrada") : T ("más ancha")); }
    if (containsAny (text, { "natural", "suave", "smooth", "humano" }) && ! containsAny (text, { "trap", "robot" }))
    { set (IDs::retune, juce::jmax (55.0f, get (IDs::retune))); add (IDs::humanize, 20.0f); done.add (T ("afinación más natural")); }
    if (containsAny (text, { "afinaci", "autotune", "tune" }) && ! containsAny (text, { "natural", "duro", "hard", "robot" }))
    { add (IDs::retune, less ? 20.0f : -10.0f); set (IDs::tuneOn, 1); done.add (less ? T ("afinación más suave") : T ("afinación más marcada")); }
    if (containsAny (text, { "ruido", "noise", "hiss", "limpia", "clean" }))
    { set (IDs::gate, juce::jmax (-55.0f, get (IDs::gate))); done.add (T ("puerta de ruido activada")); }

    if (done.isEmpty())
    {
        r.summary = T ("No entendí del todo. Prueba con: \"estilo trap melódico\", \"más brillo\", \"menos reverb\", "
                       "\"autotune duro\" o \"voz de teléfono\". Con una clave de Claude puedo entender cualquier pedido.");
        r.failed = true;
        return r;
    }

    // for whole styles, also say what the style did, in plain words
    if (styleLabel.isNotEmpty())
    {
        auto traits = describeChanges (r.changes, current);
        done.set (0, styleLabel + (traits.isEmpty() ? juce::String() : " (" + traits.joinIntoString (", ") + ")"));
    }
    r.summary = juce::String (juce::CharPointer_UTF8 ("Listo: ")) + done.joinIntoString (", ") + ".";
    return r;
}

} // namespace vocal
