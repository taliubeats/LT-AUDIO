#pragma once

namespace lt::ui
{

/**
    License activation screen shared by every LT plugin (design/licensing/activation.webp).
    Place it over the editor; it fills its bounds with a dimmed backdrop and a centred glass card.
    Accepts pasted license text or a dropped .lic file.
*/
class ActivationComponent : public juce::Component,
                            public juce::FileDragAndDropTarget
{
public:
    ActivationComponent (licensing::LicenseManager&, const Palette&);

    /** Called when the user continues the trial or activation succeeds. */
    std::function<void()> onDismiss;

    void setPalette (const Palette&);
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override            { dragOver = false; repaint(); }

private:
    void activate (const juce::String& text);
    juce::Rectangle<float> getCard() const;

    licensing::LicenseManager& manager;
    Palette palette;
    GlassLookAndFeel lnf;

    juce::TextButton copyButton { "COPY" }, pasteButton { "PASTE" }, activateButton { "ACTIVATE" }, trialButton { "CONTINUE TRIAL" };
    juce::TextEditor licenseEditor;
    juce::String message;
    bool messageIsError = false, dragOver = false;
    juce::Rectangle<float> machineIdBox, trialBar;
};

} // namespace lt::ui
