#pragma once

namespace lt::licensing
{

/** Product identifiers. A license for "ALL" unlocks every product. */
namespace Products
{
    inline constexpr const char* typeBeats = "TYPEBEATS";
    inline constexpr const char* pulsar    = "PULSAR";
    inline constexpr const char* knock     = "KNOCK";
    inline constexpr const char* nebula    = "NEBULA";
    inline constexpr const char* orbit     = "ORBIT";
    inline constexpr const char* voidVerb  = "VOID";
    inline constexpr const char* tune      = "TUNE";
    inline constexpr const char* nova      = "NOVA";
    inline constexpr const char* vocalSuite = "VOCALSUITE";
    inline constexpr const char* all       = "ALL";
}

struct LicenseInfo
{
    juce::String productId;   // e.g. "VOID" or "ALL"
    juce::String name;
    juce::String email;
    juce::String machineId;   // "XXXX-XXXX-XXXX-XXXX"
    juce::String issued;      // ISO date
    juce::String licenseId;   // random id, for bookkeeping
};

static constexpr int publicKeySize = 32;
static constexpr int secretKeySize = 64;
static constexpr int signatureSize = 64;

/** Short, stable id of this computer, formatted "XXXX-XXXX-XXXX-XXXX". */
juce::String getMachineId();

/** Normalises user-typed machine ids (upper case, dashes, no spaces). */
juce::String normaliseMachineId (const juce::String&);

/** Signs a license. The secret key never leaves the author's Mac (LT License Studio). */
juce::String createLicenseText (const LicenseInfo&, const juce::uint8* secretKey64);

/** Verifies the signature and decodes the license. Returns false if invalid or tampered. */
bool verifyLicenseText (const juce::String& text, const juce::uint8* publicKey32, LicenseInfo& out);

/** The public key compiled into this build (from shared/license_public_key.txt). */
bool getEmbeddedPublicKey (juce::uint8* publicKey32);

void generateKeyPair (juce::uint8* secretKey64, juce::uint8* publicKey32);
void derivePublicKey (const juce::uint8* secretKey64, juce::uint8* publicKey32);

juce::String toHex (const juce::uint8* data, size_t size);
bool fromHex (const juce::String& hex, juce::uint8* out, size_t size);

bool secureRandomBytes (juce::uint8* out, size_t size);

} // namespace lt::licensing
