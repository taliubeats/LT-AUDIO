#pragma once

#include <lt_shared/lt_shared.h>

namespace studio
{

/** Folder with the signing key and license records. Overridable with LT_STUDIO_DATA_DIR (for tests). */
juce::File getDataDirectory();

/** The private signing key. Created on first launch, never leaves this Mac (except as a backup you export). */
class SigningKey
{
public:
    bool loadOrCreate();
    bool exportTo (const juce::File&) const;
    bool importFrom (const juce::File&, juce::String& error);

    const juce::uint8* secret() const noexcept { return secretKey; }
    const juce::uint8* publicKey() const noexcept { return pubKey; }
    juce::String publicKeyHex() const { return lt::licensing::toHex (pubKey, lt::licensing::publicKeySize); }
    bool isValid() const noexcept { return valid; }

    /** True when the plugins in this build already contain this public key. */
    bool matchesEmbeddedKey() const;

private:
    bool save (const juce::File&) const;
    bool load (const juce::File&);

    juce::uint8 secretKey[lt::licensing::secretKeySize] {};
    juce::uint8 pubKey[lt::licensing::publicKeySize] {};
    bool valid = false;
};

struct LicenseRecord
{
    lt::licensing::LicenseInfo info;
    juce::String licenseText;
};

/** Every license you generated, stored as JSON next to the key. */
class LicenseStore
{
public:
    void load();
    void add (const LicenseRecord&);
    void remove (const juce::String& licenseId);

    const std::vector<LicenseRecord>& all() const noexcept { return records; }

    /** Distinct computers already licensed for this email + product (an ALL license counts for every product). */
    juce::StringArray machinesFor (const juce::String& email, const juce::String& productId) const;

    std::function<void()> onChange;

private:
    void save() const;
    juce::File file() const { return getDataDirectory().getChildFile ("licenses.json"); }
    std::vector<LicenseRecord> records;
};

bool isValidEmail (const juce::String&);

} // namespace studio
