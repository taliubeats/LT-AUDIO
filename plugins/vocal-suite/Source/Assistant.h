#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "Params.h"

namespace vocal
{

/** What the assistant wants to do to the chain. Values are in real units (as in ParamInfo). */
struct AssistantReply
{
    juce::String summary;
    std::map<juce::String, float> changes;
    bool usedAI = false, failed = false;
};

/**
    The chat brain of LT VOCAL SUITE.
    - With a Claude API key: sends the request + the current settings to Claude, which answers
      with structured JSON (only valid parameter ids and values can come back).
    - Without a key (or offline): a built-in assistant that understands common requests in
      Spanish and English and knows style presets.
    Replies arrive on the message thread.
*/
class Assistant : private juce::Thread
{
public:
    Assistant();
    ~Assistant() override;

    void ask (const juce::String& request, const std::map<juce::String, float>& current);
    bool isBusy() const                      { return isThreadRunning(); }
    void clearHistory()                      { const juce::ScopedLock sl (lock); history.clear(); }

    // API key (stored in the user's LT Audio folder, never inside projects or presets)
    void setApiKey (const juce::String&);
    bool hasApiKey() const;
    static juce::File keyFile();

    std::function<void (const AssistantReply&)> onReply;

    /** The offline assistant (also used by tests). */
    static AssistantReply offlineReply (const juce::String& request, const std::map<juce::String, float>& current);
    static juce::StringArray describeChanges (const std::map<juce::String, float>& changes, const std::map<juce::String, float>& current);

    static constexpr const char* model = "claude-opus-5-5";

private:
    void run() override;
    AssistantReply askClaude (const juce::String& request, const std::map<juce::String, float>& current, juce::String& error);
    static juce::String systemPrompt();
    static juce::var responseSchema();

    juce::CriticalSection lock;
    juce::String pendingRequest, apiKey;
    std::map<juce::String, float> pendingValues;
    juce::Array<juce::var> history;      // previous {role, content} turns
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};

} // namespace vocal
