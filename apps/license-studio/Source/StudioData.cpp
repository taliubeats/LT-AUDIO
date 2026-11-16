#include "StudioData.h"

#if JUCE_MAC || JUCE_LINUX
 #include <sys/stat.h>
#endif

namespace studio
{

using namespace lt::licensing;

juce::File getDataDirectory()
{
    auto overrideDir = juce::SystemStats::getEnvironmentVariable ("LT_STUDIO_DATA_DIR", {});
    if (overrideDir.isNotEmpty())
        return juce::File (overrideDir);

    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("LT License Studio");
}

// ------------------------------------------------------------------ SigningKey

bool SigningKey::save (const juce::File& f) const
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("format", "lt-signing-key-1");
    obj->setProperty ("secret", toHex (secretKey, secretKeySize));
    obj->setProperty ("public", toHex (pubKey, publicKeySize));
    obj->setProperty ("created", juce::Time::getCurrentTime().toISO8601 (true));

    f.getParentDirectory().createDirectory();
    if (! f.replaceWithText (juce::JSON::toString (juce::var (obj.release()))))
        return false;

   #if JUCE_MAC || JUCE_LINUX
    ::chmod (f.getFullPathName().toRawUTF8(), 0600);
   #endif
    return true;
}

bool SigningKey::load (const juce::File& f)
{
    auto json = juce::JSON::parse (f.loadFileAsString());
    if (! json.isObject() || json["format"].toString() != "lt-signing-key-1")
        return false;

    juce::uint8 s[secretKeySize], p[publicKeySize], derived[publicKeySize];
    if (! fromHex (json["secret"].toString(), s, secretKeySize) || ! fromHex (json["public"].toString(), p, publicKeySize))
        return false;

    derivePublicKey (s, derived);
    if (std::memcmp (derived, p, publicKeySize) != 0)
        return false;

    std::memcpy (secretKey, s, secretKeySize);
    std::memcpy (pubKey, p, publicKeySize);
    valid = true;
    return true;
}

bool SigningKey::loadOrCreate()
{
    auto f = getDataDirectory().getChildFile ("signing-key.json");
    if (f.existsAsFile())
        return load (f);

    generateKeyPair (secretKey, pubKey);
    valid = save (f);
    return valid;
}

bool SigningKey::exportTo (const juce::File& f) const
{
    return valid && save (f);
}

bool SigningKey::importFrom (const juce::File& f, juce::String& error)
{
    SigningKey other;
    if (! other.load (f))
    {
        error = "That file is not a valid LT signing key backup.";
        return false;
    }

    *this = other;
    if (! save (getDataDirectory().getChildFile ("signing-key.json")))
    {
        error = "Could not save the imported key.";
        return false;
    }
    return true;
}

bool SigningKey::matchesEmbeddedKey() const
{
    juce::uint8 embedded[publicKeySize];
    return valid && getEmbeddedPublicKey (embedded) && std::memcmp (embedded, pubKey, publicKeySize) == 0;
}

// ------------------------------------------------------------------ LicenseStore

void LicenseStore::load()
{
    records.clear();
    auto json = juce::JSON::parse (file().loadFileAsString());
    if (auto* arr = json.getArray())
    {
        for (auto& item : *arr)
        {
            LicenseRecord r;
            r.info.productId = item["product"].toString();
            r.info.name      = item["name"].toString();
            r.info.email     = item["email"].toString();
            r.info.machineId = item["machine"].toString();
            r.info.issued    = item["issued"].toString();
            r.info.licenseId = item["id"].toString();
            r.licenseText    = item["license"].toString();
            records.push_back (r);
        }
    }
}

void LicenseStore::save() const
{
    juce::Array<juce::var> arr;
    for (auto& r : records)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty ("product", r.info.productId);
        obj->setProperty ("name", r.info.name);
        obj->setProperty ("email", r.info.email);
        obj->setProperty ("machine", r.info.machineId);
        obj->setProperty ("issued", r.info.issued);
        obj->setProperty ("id", r.info.licenseId);
        obj->setProperty ("license", r.licenseText);
        arr.add (juce::var (obj.release()));
    }

    file().getParentDirectory().createDirectory();
    file().replaceWithText (juce::JSON::toString (juce::var (arr)));
}

void LicenseStore::add (const LicenseRecord& r)
{
    records.insert (records.begin(), r);
    save();
    if (onChange) onChange();
}

void LicenseStore::remove (const juce::String& id)
{
    records.erase (std::remove_if (records.begin(), records.end(),
                                   [&] (const LicenseRecord& r) { return r.info.licenseId == id; }),
                   records.end());
    save();
    if (onChange) onChange();
}

juce::StringArray LicenseStore::machinesFor (const juce::String& email, const juce::String& productId) const
{
    juce::StringArray machines;
    for (auto& r : records)
        if (r.info.email.equalsIgnoreCase (email.trim())
             && (r.info.productId == productId || r.info.productId == Products::all || productId == Products::all))
            machines.addIfNotAlreadyThere (normaliseMachineId (r.info.machineId));
    return machines;
}

bool isValidEmail (const juce::String& s)
{
    auto e = s.trim();
    auto at = e.indexOfChar ('@');
    return at > 0 && e.lastIndexOfChar ('.') > at + 1 && ! e.endsWithChar ('.')
            && ! e.containsAnyOf (" ,;") && e.indexOfChar (at + 1, '@') < 0;
}

} // namespace studio
