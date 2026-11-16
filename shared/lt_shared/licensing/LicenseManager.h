#pragma once

namespace lt::licensing
{

/** Per-product license state used by the plugins: 10-day trial, then a machine-bound license. */
class LicenseManager
{
public:
    static constexpr int trialDays = 10;

    enum class State { licensed, trial, expired };

    LicenseManager (juce::String productId, juce::String productName);

    void refresh();

    State getState() const noexcept           { return state; }
    bool isUsable() const noexcept            { return state != State::expired; }
    bool isLicensed() const noexcept          { return state == State::licensed; }
    int getTrialDaysLeft() const noexcept     { return trialDaysLeft; }
    float getTrialProgress() const noexcept   { return (float) trialDaysLeft / (float) trialDays; }

    const juce::String& getProductId() const noexcept   { return productId; }
    const juce::String& getProductName() const noexcept { return productName; }
    const juce::String& getMachineId() const noexcept   { return machineId; }
    const LicenseInfo& getLicense() const noexcept      { return license; }

    /** Verifies and stores a license (text or the contents of a .lic file). */
    bool activate (const juce::String& licenseText, juce::String& errorMessage);

    static juce::File getDataDirectory();

private:
    bool loadLicenseFile (const juce::String& id);
    void updateTrial();

    juce::String productId, productName, machineId;
    LicenseInfo license;
    State state = State::trial;
    int trialDaysLeft = trialDays;
};

} // namespace lt::licensing
