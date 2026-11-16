#pragma once

namespace lt::presets
{

/**
    A preset made only of parameter values (LT VOID, LT ORBIT...).
    Saved as JSON (.ltpreset) in "LT Audio/Presets/<product name>".
*/
struct ParamPreset
{
    juce::String name, group;                 // group = e.g. the reverb type it belongs to
    std::map<juce::String, float> values;     // parameter ID -> real (not normalised) value
    bool factory = true;
    juce::File file;

    juce::var toVar (const juce::String& productId) const;
    static ParamPreset fromVar (const juce::var&);

    /** Snapshot of every parameter of a state. */
    static ParamPreset capture (juce::AudioProcessorValueTreeState&, const juce::String& name, const juce::String& group);

    /** Applies the stored values (others keep their value unless resetOthers). */
    void apply (juce::AudioProcessorValueTreeState&, bool resetOthers = true) const;

    /** True when every stored value matches the state. */
    bool matches (juce::AudioProcessorValueTreeState&) const;
};

class ParamPresetLibrary
{
public:
    ParamPresetLibrary (juce::String productId, juce::String productName, std::vector<ParamPreset> factory);

    juce::File userFolder() const;
    void rescan();
    const std::vector<ParamPreset>& all() const noexcept { return presets; }
    int indexOf (const juce::String& name) const;
    bool save (ParamPreset);
    bool remove (const juce::String& name);

private:
    juce::String productId, productName;
    std::vector<ParamPreset> factory, presets;
};

} // namespace lt::presets
