#pragma once

#include <lt_shared/lt_shared.h>
#include <lt_dsp/lt_dsp.h>

namespace nebula
{

/** A bank of presets with a cover image. LT COSMOS is the factory bank; the others are folders on disk. */
struct Bank
{
    juce::String name, author { "LT" };
    bool factory = false;
    juce::File folder;                      // user banks
    juce::Image cover;
    std::vector<lt::presets::ParamPreset> presets;
};

/**
    Banks live in  .../LT Audio/Presets/LT NEBULA/
      *.ltpreset           -> MY BANK
      <Bank name>/         -> a bank (bank.json, cover.png, *.ltpreset)
      favorites.json       -> starred presets ("bank/preset")
    A bank exports to a single .ltbank file (zip) that friends can import.
*/
class BankLibrary
{
public:
    explicit BankLibrary (std::vector<lt::presets::ParamPreset> factoryPresets);

    static juce::File root();
    void rescan();
    const std::vector<Bank>& banks() const noexcept { return list; }
    int indexOfBank (const juce::String&) const;

    bool isFavourite (const juce::String& bank, const juce::String& preset) const;
    void toggleFavourite (const juce::String& bank, const juce::String& preset);

    /** Creates an empty bank; returns its index or -1. */
    int createBank (const juce::String& name);
    bool deleteBank (int index);
    bool setCover (int index, const juce::File& image);
    bool savePreset (int bankIndex, lt::presets::ParamPreset);
    bool deletePreset (int bankIndex, const juce::String& name);
    bool exportBank (int index, const juce::File& destination) const;
    /** .ltbank (whole bank) or .ltpreset (into MY BANK). Returns the bank index it landed in, or -1. */
    int importFile (const juce::File&);

    static juce::Image makeCover (int style, int seed, int size);

private:
    Bank loadFolder (const juce::File&, const juce::String& name) const;
    void saveFavourites() const;

    std::vector<lt::presets::ParamPreset> factory;
    std::vector<Bank> list;
    juce::StringArray favourites;
    juce::Image factoryCover;
};

} // namespace nebula
