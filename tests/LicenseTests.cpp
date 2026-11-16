#include <lt_shared/lt_shared.h>

using namespace lt::licensing;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "FAIL: " #cond " (line " << __LINE__ << ")\n"; } } while (0)

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    juce::uint8 sk[secretKeySize], pk[publicKeySize], pk2[publicKeySize];
    generateKeyPair (sk, pk);
    derivePublicKey (sk, pk2);
    CHECK (std::memcmp (pk, pk2, publicKeySize) == 0);

    LicenseInfo info { "VOID", "Julian A.", "julian@example.com", "4f9a 22c1-8b7e-1234", "2026-11-16", "ABCD" };
    auto text = createLicenseText (info, sk);
    std::cout << "license: " << text << "\n(" << text.length() << " chars)\n";

    LicenseInfo out;
    CHECK (verifyLicenseText (text, pk, out));
    CHECK (out.name == "Julian A.");
    CHECK (out.machineId == "4F9A-22C1-8B7E-1234");
    CHECK (verifyLicenseText ("  " + text.substring (0, 40) + "\n" + text.substring (40) + "\n", pk, out)); // pasted with line breaks

    // tampering must fail
    auto tampered = text;
    {
        auto i = tampered.indexOfChar ('.') + 10;
        auto c = tampered[i];
        tampered = tampered.substring (0, i) + juce::String::charToString (c == 'A' ? 'B' : 'A') + tampered.substring (i + 1);
    }
    CHECK (tampered != text);
    CHECK (! verifyLicenseText (tampered, pk, out));
    auto parts = juce::StringArray::fromTokens (text, ".", "");
    auto otherSig = createLicenseText ({ "ALL", "X", "x@y.z", "0000-0000-0000-0000", "", "" }, sk);
    auto mixed = parts[0] + "." + parts[1] + "." + juce::StringArray::fromTokens (otherSig, ".", "")[2];
    CHECK (! verifyLicenseText (mixed, pk, out));

    // other key must fail
    juce::uint8 sk3[secretKeySize], pk3[publicKeySize];
    generateKeyPair (sk3, pk3);
    CHECK (! verifyLicenseText (text, pk3, out));

    CHECK (normaliseMachineId ("abcd1234ef567890") == "ABCD-1234-EF56-7890");
    auto mid = getMachineId();
    std::cout << "machine id: " << mid << "\n";
    CHECK (mid.length() == 19 && mid == getMachineId());

    // hex round trip
    juce::uint8 back[publicKeySize];
    CHECK (fromHex (toHex (pk, publicKeySize), back, publicKeySize) && std::memcmp (back, pk, publicKeySize) == 0);

    std::cout << (failures == 0 ? "ALL TESTS PASSED\n" : "TESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
