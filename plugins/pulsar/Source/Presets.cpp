#include "Presets.h"

namespace pulsar
{

const juce::StringArray& Rates::names()
{
    static const juce::StringArray n { "1/1", "1/2", "1/4", "1/8", "1/16" };
    return n;
}

double Rates::beats (int index) noexcept
{
    static constexpr double b[] = { 4.0, 2.0, 1.0, 0.5, 0.25 };
    return b[juce::jlimit (0, count - 1, index)];
}

juce::var Preset::toVar() const
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("product", "PULSAR");
    obj->setProperty ("version", 1);
    obj->setProperty ("name", name);
    obj->setProperty ("rate", Rates::names()[rate]);
    obj->setProperty ("mix", mix);
    obj->setProperty ("curve", curve.toVar());
    return juce::var (obj.release());
}

Preset Preset::fromVar (const juce::var& v)
{
    Preset p;
    p.name = v["name"].toString();
    auto r = Rates::names().indexOf (v["rate"].toString());
    p.rate = r >= 0 ? r : Rates::quarter;
    p.mix = juce::jlimit (0.0f, 100.0f, v.hasProperty ("mix") ? (float) v["mix"] : 75.0f);
    p.curve = Curve::fromVar (v["curve"]);
    return p;
}

std::vector<Preset> PresetLibrary::factory()
{
    using P = std::vector<CurvePoint>;
    auto make = [] (const char* name, int rate, float mix, P pts, float offset = 0.0f)
    {
        Preset p;
        p.name = name;
        p.rate = rate;
        p.mix = mix;
        p.curve = Curve (std::move (pts), offset);
        return p;
    };

    const P classic { { 0.0f, 0.0f, -0.5f }, { 0.55f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };

    return {
        make ("Default",         Rates::quarter,   75.0f, classic),
        make ("Classic Pump 1/4", Rates::quarter,  90.0f, classic),
        make ("Smooth S 1/4",    Rates::quarter,   90.0f, { { 0.0f, 0.0f, 0.0f }, { 0.27f, 0.0f, 0.5f }, { 0.35f, 0.5f, -0.5f },
                                                            { 0.43f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }),
        make ("Soft Duck 1/4",   Rates::quarter,   70.0f, { { 0.0f, 0.3f, -0.35f }, { 0.65f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }),
        make ("Hard Cut 1/4",    Rates::quarter,  100.0f, { { 0.0f, 0.0f, 0.0f }, { 0.3f, 0.0f, 0.0f }, { 0.32f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }),
        make ("Short Duck",      Rates::quarter,   90.0f, { { 0.0f, 0.0f, -0.45f }, { 0.22f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }),
        make ("Long Release",    Rates::quarter,   85.0f, { { 0.0f, 0.0f, -0.25f }, { 0.92f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }),
        make ("Double Pump 1/8", Rates::eighth,    80.0f, classic),
        make ("Offbeat 1/8",     Rates::quarter,   85.0f, classic, 0.5f),
        make ("Gate 1/16",       Rates::sixteenth, 100.0f, { { 0.0f, 1.0f, 0.0f }, { 0.48f, 1.0f, 0.0f }, { 0.52f, 0.0f, 0.0f },
                                                            { 0.96f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }),
        make ("Half Time 1/2",   Rates::half,      85.0f, { { 0.0f, 0.0f, -0.5f }, { 0.45f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }),
        make ("Subtle Glue",     Rates::quarter,   45.0f, { { 0.0f, 0.55f, -0.4f }, { 0.45f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }),
    };
}

juce::File PresetLibrary::userFolder()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("LT Audio").getChildFile ("Presets").getChildFile ("LT PULSAR");
}

PresetLibrary::PresetLibrary()
{
    rescan();
}

void PresetLibrary::rescan()
{
    presets = factory();

    auto files = userFolder().findChildFiles (juce::File::findFiles, false, "*.ltpreset");
    files.sort();
    for (auto& f : files)
    {
        auto v = juce::JSON::parse (f);
        if (! v.isObject())
            continue;
        auto p = Preset::fromVar (v);
        if (p.name.isEmpty())
            p.name = f.getFileNameWithoutExtension();
        p.factory = false;
        p.file = f;
        presets.push_back (p);
    }
}

int PresetLibrary::indexOf (const juce::String& name) const
{
    for (int i = 0; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].name == name)
            return i;
    return -1;
}

bool PresetLibrary::save (Preset p)
{
    p.name = p.name.trim();
    if (p.name.isEmpty())
        return false;

    auto folder = userFolder();
    if (! folder.createDirectory())
        return false;

    auto file = folder.getChildFile (juce::File::createLegalFileName (p.name) + ".ltpreset");
    auto ok = file.replaceWithText (juce::JSON::toString (p.toVar()));
    rescan();
    return ok;
}

bool PresetLibrary::remove (const juce::String& name)
{
    auto i = indexOf (name);
    if (i < 0 || presets[(size_t) i].factory)
        return false;
    auto ok = presets[(size_t) i].file.deleteFile();
    rescan();
    return ok;
}

} // namespace pulsar
