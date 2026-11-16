namespace lt::licensing
{

namespace
{
    const char* trialSalt = "lt-audio::trial::7c1f3a9e";

    juce::String trialSignature (const juce::String& product, juce::int64 start, juce::int64 lastSeen)
    {
        juce::SHA256 sha ((juce::String (trialSalt) + product + juce::String (start) + "|" + juce::String (lastSeen)).toUTF8());
        return sha.toHexString().substring (0, 24);
    }

    struct TrialRecord { juce::int64 start = 0, lastSeen = 0; bool valid = false; };

    TrialRecord readTrial (const juce::File& f, const juce::String& product)
    {
        TrialRecord r;
        auto parts = juce::StringArray::fromTokens (f.loadFileAsString().trim(), "|", "");
        if (parts.size() == 3)
        {
            r.start    = parts[0].getLargeIntValue();
            r.lastSeen = parts[1].getLargeIntValue();
            r.valid    = r.start > 0 && parts[2] == trialSignature (product, r.start, r.lastSeen);
        }
        return r;
    }

    void writeTrial (const juce::File& f, const juce::String& product, const TrialRecord& r)
    {
        f.getParentDirectory().createDirectory();
        f.replaceWithText (juce::String (r.start) + "|" + juce::String (r.lastSeen) + "|"
                           + trialSignature (product, r.start, r.lastSeen));
    }

    juce::File secondaryTrialFile (const juce::String& product)
    {
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        base = base.getChildFile ("Application Support");
       #endif
        return base.getChildFile (".ltsys").getChildFile (juce::SHA256 (product.toUTF8()).toHexString().substring (0, 16));
    }
}

juce::File LicenseManager::getDataDirectory()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("LT Audio");
}

LicenseManager::LicenseManager (juce::String id, juce::String name)
    : productId (std::move (id)), productName (std::move (name)), machineId (getMachineId())
{
    refresh();
}

bool LicenseManager::loadLicenseFile (const juce::String& id)
{
    auto file = getDataDirectory().getChildFile ("Licenses").getChildFile (id + ".lic");
    if (! file.existsAsFile())
        return false;

    juce::uint8 pub[publicKeySize];
    LicenseInfo info;
    if (getEmbeddedPublicKey (pub)
         && verifyLicenseText (file.loadFileAsString(), pub, info)
         && normaliseMachineId (info.machineId) == machineId
         && (info.productId == productId || info.productId == Products::all))
    {
        license = info;
        return true;
    }
    return false;
}

void LicenseManager::refresh()
{
    if (loadLicenseFile (productId) || loadLicenseFile (Products::all))
    {
        state = State::licensed;
        return;
    }

    updateTrial();
    state = trialDaysLeft > 0 ? State::trial : State::expired;
}

void LicenseManager::updateTrial()
{
    auto now = juce::Time::currentTimeMillis();
    auto primaryFile = getDataDirectory().getChildFile ("Trials").getChildFile (productId + ".trial");
    auto secondaryFile = secondaryTrialFile (productId);

    auto a = readTrial (primaryFile, productId);
    auto b = readTrial (secondaryFile, productId);

    TrialRecord r;
    if (a.valid && b.valid)  r = { juce::jmin (a.start, b.start), juce::jmax (a.lastSeen, b.lastSeen), true };
    else if (a.valid)        r = a;
    else if (b.valid)        r = b;
    else if (primaryFile.existsAsFile() || secondaryFile.existsAsFile())
        r = { now - (juce::int64) trialDays * 86400000LL, now, true };  // tampered: trial over
    else
        r = { now, now, true };

    r.lastSeen = juce::jmax (r.lastSeen, now);  // clock rolled back? keep the latest time seen
    writeTrial (primaryFile, productId, r);
    writeTrial (secondaryFile, productId, r);

    auto elapsedDays = (int) ((r.lastSeen - r.start) / 86400000LL);
    trialDaysLeft = juce::jlimit (0, trialDays, trialDays - elapsedDays);
}

bool LicenseManager::activate (const juce::String& licenseText, juce::String& errorMessage)
{
    juce::uint8 pub[publicKeySize];
    if (! getEmbeddedPublicKey (pub))
    {
        errorMessage = "This build has no license key installed yet.";
        return false;
    }

    LicenseInfo info;
    if (! verifyLicenseText (licenseText, pub, info))
    {
        errorMessage = "This license key is not valid.";
        return false;
    }

    if (info.productId != productId && info.productId != Products::all)
    {
        errorMessage = "This license is for another LT plugin (" + info.productId + ").";
        return false;
    }

    if (normaliseMachineId (info.machineId) != machineId)
    {
        errorMessage = "This license was made for another computer.";
        return false;
    }

    auto file = getDataDirectory().getChildFile ("Licenses").getChildFile (info.productId + ".lic");
    file.getParentDirectory().createDirectory();
    if (! file.replaceWithText (licenseText.trim()))
    {
        errorMessage = "Could not save the license file.";
        return false;
    }

    refresh();
    return isLicensed();
}

} // namespace lt::licensing
