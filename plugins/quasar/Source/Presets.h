#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace quasar
{

/**
    A QUASAR preset (.ltq, JSON): parameter values plus what parameters cannot hold
    (LFO shapes, macro names, sample files). Only values that differ from the defaults are stored.
*/
struct Preset
{
    juce::String name, category, style, author, bank, description;
    std::map<juce::String, float> values;           // parameter ID -> real value
    juce::var lfoShapes;                            // array of 4 LfoShape vars
    juce::var mods, fx;                             // factory presets: routes and effects by name (see Processor::applyPreset)
    juce::StringArray macroNames;
    std::array<juce::String, 3> samplePaths;        // relative to the bank folder when inside a bank
    bool factory = false;
    juce::File file;

    juce::var toVar() const;
    static Preset fromVar (const juce::var&);
    juce::String key() const { return bank + "/" + name; }
};

struct Bank
{
    juce::String name, author, description;
    juce::File folder;                               // user / imported banks
    bool factory = false;
    juce::Colour colour;                             // cover tint (procedural cover)
    juce::Image cover;                               // optional cover.png
    std::vector<Preset> presets;
};

/**
    All banks: factory (compiled in), "USER" (single presets saved by the user) and imported banks.
    Folder: <user app data>/LT Audio/Presets/LT QUASAR/{User, Banks/<name>}.
    A bank file (.ltqbank) is a zip with bank.json, presets/*.ltq, optional cover.png and samples/.
*/
class PresetLibrary
{
public:
    PresetLibrary();

    static juce::File rootFolder();
    static juce::File userFolder();
    static juce::File banksFolder();
    static const juce::StringArray& categories();   // BASS REESE 808 SUB PLUCK KEYS LEAD PAD BELL ARP CHORD FX VOX
    static const juce::StringArray& styles();       // TRAP REGGAETON R&B LO-FI DRILL AFRO

    void rescan();
    const std::vector<Bank>& banks() const noexcept { return all; }
    const Preset* find (const juce::String& bank, const juce::String& name) const;
    std::vector<const Preset*> allPresets() const;

    bool saveUserPreset (Preset p, const juce::String& bankName = "USER");
    bool deletePreset (const Preset&);
    bool createBank (const juce::String& name, const juce::String& author, const juce::String& description);
    bool exportBank (const juce::String& bankName, const juce::File& target, juce::String& error) const;
    bool importBank (const juce::File& ltqbank, juce::String& error, juce::String* importedName = nullptr);
    bool exportPreset (const Preset&, const juce::File& target) const;
    bool importPreset (const juce::File& ltq, juce::String& error);

    // favourites / recent
    bool isFavourite (const Preset& p) const { return favourites.contains (p.key()); }
    void toggleFavourite (const Preset&);
    void addRecent (const Preset&);
    const juce::StringArray& getRecent() const noexcept { return recent; }

private:
    void loadFactory();
    void loadFolderBank (const juce::File& folder, bool isUser);
    void saveLists() const;

    std::vector<Bank> all;
    juce::StringArray favourites, recent;
};

} // namespace quasar
