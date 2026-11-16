#include "monocypher.h"
#include "LTPublicKey.h"

#include <random>

namespace lt::licensing
{

namespace
{
    const char* licensePrefix = "LT1";

    juce::String base64UrlEncode (const void* data, size_t size)
    {
        auto s = juce::Base64::toBase64 (data, size);
        return s.replaceCharacter ('+', '-').replaceCharacter ('/', '_').trimCharactersAtEnd ("=");
    }

    bool base64UrlDecode (juce::String s, juce::MemoryBlock& out)
    {
        s = s.replaceCharacter ('-', '+').replaceCharacter ('_', '/');
        while (s.length() % 4 != 0)
            s << "=";

        juce::MemoryOutputStream mo (out, false);
        return juce::Base64::convertFromBase64 (mo, s);
    }

    juce::String stripWhitespace (const juce::String& s)
    {
        juce::String r;
        for (auto c : s)
            if (! juce::CharacterFunctions::isWhitespace (c))
                r << juce::String::charToString (c);
        return r;
    }
}

juce::String toHex (const juce::uint8* data, size_t size)
{
    return juce::String::toHexString (data, (int) size, 0).toUpperCase();
}

bool fromHex (const juce::String& hexIn, juce::uint8* out, size_t size)
{
    auto hex = stripWhitespace (hexIn);
    if ((size_t) hex.length() != size * 2 || ! hex.containsOnly ("0123456789abcdefABCDEF"))
        return false;

    for (size_t i = 0; i < size; ++i)
        out[i] = (juce::uint8) hex.substring ((int) i * 2, (int) i * 2 + 2).getHexValue32();

    return true;
}

bool secureRandomBytes (juce::uint8* out, size_t size)
{
   #if JUCE_WINDOWS
    // MSVC's random_device is backed by the OS cryptographic generator
    try
    {
        std::random_device rd;
        for (size_t i = 0; i < size; ++i)
            out[i] = (juce::uint8) (rd() & 0xff);
        return true;
    }
    catch (...) { return false; }
   #else
    juce::FileInputStream in (juce::File ("/dev/urandom"));
    return in.openedOk() && in.read (out, (int) size) == (int) size;
   #endif
}

juce::String normaliseMachineId (const juce::String& s)
{
    auto raw = stripWhitespace (s).removeCharacters ("-").toUpperCase();
    juce::String r;
    for (int i = 0; i < raw.length(); ++i)
    {
        if (i > 0 && i % 4 == 0)
            r << "-";
        r << juce::String::charToString (raw[i]);
    }
    return r;
}

juce::String getMachineId()
{
    auto id = juce::SystemStats::getUniqueDeviceID();

    if (id.isEmpty())
        id = juce::SystemStats::getComputerName() + juce::SystemStats::getCpuModel();

    juce::SHA256 sha (("lt-audio/" + id).toUTF8());
    auto hash = sha.getRawData();
    return normaliseMachineId (toHex ((const juce::uint8*) hash.getData(), 8));
}

void generateKeyPair (juce::uint8* secretKey64, juce::uint8* publicKey32)
{
    juce::uint8 seed[32];
    if (! secureRandomBytes (seed, sizeof (seed)))
    {
        juce::Random r;
        r.setSeedRandomly();
        for (auto& b : seed)
            b = (juce::uint8) r.nextInt (256);
    }

    crypto_eddsa_key_pair (secretKey64, publicKey32, seed); // wipes seed
}

void derivePublicKey (const juce::uint8* secretKey64, juce::uint8* publicKey32)
{
    // Monocypher secret keys store the public key in their last 32 bytes.
    std::memcpy (publicKey32, secretKey64 + 32, 32);
}

juce::String createLicenseText (const LicenseInfo& info, const juce::uint8* secretKey64)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("p", info.productId);
    obj->setProperty ("n", info.name);
    obj->setProperty ("e", info.email);
    obj->setProperty ("m", normaliseMachineId (info.machineId));
    obj->setProperty ("i", info.issued);
    obj->setProperty ("id", info.licenseId);

    auto payload = juce::JSON::toString (juce::var (obj.release()), true);
    auto utf8 = payload.toUTF8();
    auto len = (size_t) payload.getNumBytesAsUTF8();

    juce::uint8 sig[signatureSize];
    crypto_eddsa_sign (sig, secretKey64, (const juce::uint8*) utf8.getAddress(), len);

    return juce::String (licensePrefix) + "." + base64UrlEncode (utf8.getAddress(), len)
            + "." + base64UrlEncode (sig, sizeof (sig));
}

bool verifyLicenseText (const juce::String& textIn, const juce::uint8* publicKey32, LicenseInfo& out)
{
    auto parts = juce::StringArray::fromTokens (stripWhitespace (textIn), ".", "");
    if (parts.size() != 3 || parts[0] != licensePrefix)
        return false;

    juce::MemoryBlock payload, sig;
    if (! base64UrlDecode (parts[1], payload) || ! base64UrlDecode (parts[2], sig) || sig.getSize() != signatureSize)
        return false;

    if (crypto_eddsa_check ((const juce::uint8*) sig.getData(), publicKey32,
                            (const juce::uint8*) payload.getData(), payload.getSize()) != 0)
        return false;

    auto json = juce::JSON::parse (payload.toString());
    if (! json.isObject())
        return false;

    out.productId = json["p"].toString();
    out.name      = json["n"].toString();
    out.email     = json["e"].toString();
    out.machineId = json["m"].toString();
    out.issued    = json["i"].toString();
    out.licenseId = json["id"].toString();
    return out.productId.isNotEmpty() && out.machineId.isNotEmpty();
}

bool getEmbeddedPublicKey (juce::uint8* publicKey32)
{
    return fromHex (LT_LICENSE_PUBLIC_KEY_HEX, publicKey32, publicKeySize);
}

} // namespace lt::licensing
