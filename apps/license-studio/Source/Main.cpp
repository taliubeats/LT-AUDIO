#include "MainComponent.h"

class StudioApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "LT License Studio"; }
    const juce::String getApplicationVersion() override { return LT_VERSION; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        // Preview mode: LTLicenseStudio --screenshot=/path/file.png [--view=settings|customers]
        if (commandLine.contains ("--screenshot="))
        {
            auto path = commandLine.fromFirstOccurrenceOf ("--screenshot=", false, false).upToFirstOccurrenceOf (" ", false, false);
            auto component = std::make_shared<studio::MainComponent>();
            component->setSize (1240, 780);
            juce::Timer::callAfterDelay (700, [component, path]
            {
                component->saveScreenshot (juce::File (path));
                juce::JUCEApplication::quit();
            });
            keepAlive = component;
            return;
        }

        window = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        window = nullptr;
        keepAlive = nullptr;
    }

    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colour (0xff07050d), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new studio::MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (1100, 720, 2400, 1600);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<MainWindow> window;
    std::shared_ptr<juce::Component> keepAlive;
};

START_JUCE_APPLICATION (StudioApplication)
