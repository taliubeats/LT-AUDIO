namespace lt::presets
{

juce::var ParamPreset::toVar (const juce::String& productId) const
{
    auto vals = std::make_unique<juce::DynamicObject>();
    for (auto& [id, v] : values)
        vals->setProperty (id, v);

    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("product", productId);
    obj->setProperty ("version", 1);
    obj->setProperty ("name", name);
    obj->setProperty ("group", group);
    obj->setProperty ("values", juce::var (vals.release()));
    return juce::var (obj.release());
}

ParamPreset ParamPreset::fromVar (const juce::var& v)
{
    ParamPreset p;
    p.name = v["name"].toString();
    p.group = v["group"].toString();
    if (auto* obj = v["values"].getDynamicObject())
        for (auto& prop : obj->getProperties())
            p.values[prop.name.toString()] = (float) prop.value;
    return p;
}

ParamPreset ParamPreset::capture (juce::AudioProcessorValueTreeState& s, const juce::String& name, const juce::String& group)
{
    ParamPreset p;
    p.name = name;
    p.group = group;
    p.factory = false;
    for (auto* param : s.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (param))
            p.values[r->getParameterID()] = r->convertFrom0to1 (r->getValue());
    return p;
}

void ParamPreset::apply (juce::AudioProcessorValueTreeState& s, bool resetOthers) const
{
    for (auto* param : s.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            auto it = values.find (r->getParameterID());
            float norm;
            if (it != values.end())
                norm = r->convertTo0to1 (it->second);
            else if (resetOthers)
                norm = r->getDefaultValue();
            else
                continue;
            r->beginChangeGesture();
            r->setValueNotifyingHost (norm);
            r->endChangeGesture();
        }
}

bool ParamPreset::matches (juce::AudioProcessorValueTreeState& s) const
{
    for (auto& [id, v] : values)
        if (auto* r = s.getParameter (id))
            if (std::abs (r->getValue() - r->convertTo0to1 (v)) > 0.002f)
                return false;
    return true;
}

// ------------------------------------------------------------------

ParamPresetLibrary::ParamPresetLibrary (juce::String id, juce::String name, std::vector<ParamPreset> f)
    : productId (std::move (id)), productName (std::move (name)), factory (std::move (f))
{
    rescan();
}

juce::File ParamPresetLibrary::userFolder() const
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("LT Audio").getChildFile ("Presets").getChildFile (productName);
}

void ParamPresetLibrary::rescan()
{
    presets = factory;
    auto files = userFolder().findChildFiles (juce::File::findFiles, false, "*.ltpreset");
    files.sort();
    for (auto& f : files)
    {
        auto v = juce::JSON::parse (f);
        if (! v.isObject()) continue;
        auto p = ParamPreset::fromVar (v);
        if (p.name.isEmpty()) p.name = f.getFileNameWithoutExtension();
        p.factory = false;
        p.file = f;
        presets.push_back (p);
    }
}

int ParamPresetLibrary::indexOf (const juce::String& name) const
{
    for (int i = 0; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].name == name)
            return i;
    return -1;
}

bool ParamPresetLibrary::save (ParamPreset p)
{
    p.name = p.name.trim();
    if (p.name.isEmpty() || ! userFolder().createDirectory())
        return false;
    auto file = userFolder().getChildFile (juce::File::createLegalFileName (p.name) + ".ltpreset");
    auto ok = file.replaceWithText (juce::JSON::toString (p.toVar (productId)));
    rescan();
    return ok;
}

bool ParamPresetLibrary::remove (const juce::String& name)
{
    auto i = indexOf (name);
    if (i < 0 || presets[(size_t) i].factory) return false;
    auto ok = presets[(size_t) i].file.deleteFile();
    rescan();
    return ok;
}

} // namespace lt::presets
