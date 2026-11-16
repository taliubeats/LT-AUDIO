namespace lt::ui
{

ActivationComponent::ActivationComponent (licensing::LicenseManager& m, const Palette& p)
    : manager (m), palette (p)
{
    lnf.setPalette (palette);
    setLookAndFeel (&lnf);

    licenseEditor.setMultiLine (true, true);
    licenseEditor.setReturnKeyStartsNewLine (false);
    licenseEditor.setFont (Fonts::regular (13.0f));
    licenseEditor.setTextToShowWhenEmpty ("Paste your license key here, or drop your .lic file", palette.textDim);
    licenseEditor.setBorder ({ 10, 12, 10, 12 });
    licenseEditor.onReturnKey = [this] { activate (licenseEditor.getText()); };
    addAndMakeVisible (licenseEditor);

    activateButton.getProperties().set ("accent", true);
    activateButton.onClick = [this] { activate (licenseEditor.getText()); };
    addAndMakeVisible (activateButton);

    copyButton.onClick = [this]
    {
        juce::SystemClipboard::copyTextToClipboard (manager.getMachineId());
        message = "Machine ID copied. Send it to get your license.";
        messageIsError = false;
        repaint();
    };
    addAndMakeVisible (copyButton);

    pasteButton.onClick = [this] { licenseEditor.setText (juce::SystemClipboard::getTextFromClipboard().trim()); };
    addAndMakeVisible (pasteButton);

    trialButton.onClick = [this] { if (manager.isUsable() && onDismiss) onDismiss(); };
    addAndMakeVisible (trialButton);

    refresh();
}

void ActivationComponent::setPalette (const Palette& p)
{
    palette = p;
    lnf.setPalette (p);
    licenseEditor.setTextToShowWhenEmpty ("Paste your license key here, or drop your .lic file", palette.textDim);
    licenseEditor.applyColourToAllText (lnf.findColour (juce::TextEditor::textColourId));
    repaint();
}

void ActivationComponent::refresh()
{
    trialButton.setEnabled (manager.getState() == licensing::LicenseManager::State::trial);
    trialButton.setButtonText (manager.isLicensed() ? "CLOSE" : "CONTINUE TRIAL");
    if (manager.isLicensed())
        trialButton.setEnabled (true);
    repaint();
}

void ActivationComponent::activate (const juce::String& text)
{
    juce::String error;
    if (text.trim().isEmpty())
    {
        message = "Paste your license key first.";
        messageIsError = true;
    }
    else if (manager.activate (text, error))
    {
        message = "Activated for " + manager.getLicense().name + ". Thank you!";
        messageIsError = false;
        refresh();
        if (onDismiss)
            juce::Timer::callAfterDelay (900, [safe = juce::Component::SafePointer<ActivationComponent> (this)]
            {
                if (safe != nullptr && safe->onDismiss) safe->onDismiss();
            });
    }
    else
    {
        message = error;
        messageIsError = true;
    }
    repaint();
}

bool ActivationComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".lic") || f.endsWithIgnoreCase (".txt"))
            return true;
    return false;
}

void ActivationComponent::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
    {
        auto text = juce::File (f).loadFileAsString().trim();
        if (text.isNotEmpty())
        {
            licenseEditor.setText (text);
            activate (text);
            return;
        }
    }
}

juce::Rectangle<float> ActivationComponent::getCard() const
{
    auto b = getLocalBounds().toFloat();
    auto w = juce::jmin (620.0f, b.getWidth() - 40.0f);
    auto h = juce::jmin (520.0f, b.getHeight() - 30.0f);
    return b.withSizeKeepingCentre (w, h);
}

void ActivationComponent::resized()
{
    auto card = getCard().reduced (40.0f, 0);
    auto y = card.getY() + 150.0f;

    machineIdBox = { card.getX(), y + 26.0f, card.getWidth(), 44.0f };
    copyButton.setBounds (machineIdBox.withLeft (machineIdBox.getRight() - 96.0f).reduced (6.0f).toNearestInt());

    y = machineIdBox.getBottom() + 22.0f;
    auto editorArea = juce::Rectangle<float> (card.getX(), y + 26.0f, card.getWidth(), 64.0f);
    licenseEditor.setBounds (editorArea.withTrimmedRight (90.0f).toNearestInt());
    pasteButton.setBounds (editorArea.withLeft (editorArea.getRight() - 84.0f).withSizeKeepingCentre (84.0f, 36.0f).toNearestInt());

    activateButton.setBounds (juce::Rectangle<float> (card.getX(), editorArea.getBottom() + 16.0f, card.getWidth(), 46.0f).toNearestInt());
    trialButton.setBounds (juce::Rectangle<float> (220.0f, 40.0f).withCentre ({ card.getCentreX(), (float) activateButton.getBottom() + 40.0f }).toNearestInt());
    trialBar = juce::Rectangle<float> (card.getWidth() * 0.6f, 4.0f).withCentre ({ card.getCentreX(), (float) trialButton.getBottom() + 34.0f });
}

void ActivationComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (palette.isLight ? 0.25f : 0.55f));

    auto card = getCard();
    Glass::drawPanel (g, card, 18.0f, palette, palette.isLight ? 2.5f : 3.0f, dragOver);

    auto inner = card.reduced (40.0f, 0);
    Logo::draw (g, { inner.getX(), card.getY() + 34.0f, inner.getWidth(), 46.0f },
                Logo::specFor (manager.getProductId()), palette);

    g.setColour (palette.textDim);
    g.setFont (Fonts::light (17.0f));
    g.drawText (manager.isLicensed() ? "Your license is active" : "Activate your license",
                juce::Rectangle<float> (inner.getX(), card.getY() + 86.0f, inner.getWidth(), 24.0f), juce::Justification::centred);

    g.setColour (palette.text);
    g.setFont (Fonts::regular (15.0f));
    g.drawText ("STEP 1: Send this Machine ID to get your license",
                machineIdBox.withY (machineIdBox.getY() - 26.0f).withHeight (22.0f), juce::Justification::centredLeft);

    Glass::drawPanel (g, machineIdBox, 8.0f, palette, 1.6f);
    g.setColour (palette.isLight ? palette.text : juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 17.0f, juce::Font::plain)));
    g.drawText ("Machine ID:  " + manager.getMachineId(), machineIdBox.reduced (14.0f, 0), juce::Justification::centredLeft);

    g.setColour (palette.text);
    g.setFont (Fonts::regular (15.0f));
    auto step2 = juce::Rectangle<float> (inner.getX(), (float) licenseEditor.getY() - 26.0f, inner.getWidth(), 22.0f);
    g.drawText ("STEP 2: Paste your license key here", step2, juce::Justification::centredLeft);

    // trial info
    auto stateText = manager.isLicensed() ? "Licensed to " + manager.getLicense().name
                   : manager.isUsable()   ? "Trial: " + juce::String (manager.getTrialDaysLeft()) + " days remaining"
                                          : juce::String ("Your trial has ended");
    g.setColour (palette.textDim);
    g.setFont (Fonts::regular (13.0f));
    g.drawText (stateText, trialBar.withHeight (20.0f).translated (0, -24.0f).expanded (100.0f, 0), juce::Justification::centred);

    if (! manager.isLicensed())
    {
        g.setColour (palette.text.withAlpha (0.15f));
        g.fillRoundedRectangle (trialBar, 2.0f);
        g.setColour (palette.accent);
        g.fillRoundedRectangle (trialBar.withWidth (trialBar.getWidth() * manager.getTrialProgress()), 2.0f);
    }

    if (message.isNotEmpty())
    {
        g.setColour (messageIsError ? palette.danger : palette.success);
        g.setFont (Fonts::regular (13.0f));
        g.drawText (message, juce::Rectangle<float> (inner.getX(), (float) activateButton.getBottom() + 2.0f, inner.getWidth(), 16.0f),
                    juce::Justification::centred);
    }

    g.setColour (palette.textDim.withMultipliedAlpha (0.8f));
    g.setFont (Fonts::regular (12.0f));
    g.drawText ("LT Audio  ·  2 computers per license", card.withTop (card.getBottom() - 34.0f), juce::Justification::centred);
}

} // namespace lt::ui
