#include "MainComponent.h"

class WarpApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "LT WARP"; }
    const juce::String getApplicationVersion() override { return LT_VERSION; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        // Preview: "LT WARP" --screenshot=/path.png [--skin=0..2]
        if (commandLine.contains ("--screenshot="))
        {
            auto path = commandLine.fromFirstOccurrenceOf ("--screenshot=", false, false).upToFirstOccurrenceOf (" ", false, false);
            auto c = std::make_shared<warp::MainComponent>();
            c->setSize (warp::MainComponent::baseWidth, warp::MainComponent::baseHeight);
            c->loadDemo();
            if (commandLine.contains ("--skin="))
                c->setSkin (commandLine.fromFirstOccurrenceOf ("--skin=", false, false).getIntValue());
            juce::Timer::callAfterDelay (600, [c, path] { c->saveScreenshot (juce::File (path)); juce::JUCEApplication::quit(); });
            keepAlive = c;
            return;
        }
        window = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override { window = nullptr; keepAlive = nullptr; }
    void systemRequestedQuit() override { quit(); }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        // files opened with the app while it runs are added to the queue
        if (window != nullptr)
            if (auto* mc = dynamic_cast<warp::MainComponent*> (window->getContentComponent()))
                mc->filesDropped (juce::StringArray::fromTokens (commandLine, true), 0, 0);
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colours::black, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new warp::MainComponent(), true);
            setResizable (true, true);
            if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) warp::MainComponent::baseWidth / warp::MainComponent::baseHeight);
            setResizeLimits (750, 495, 2000, 1320);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<MainWindow> window;
    std::shared_ptr<juce::Component> keepAlive;
};

START_JUCE_APPLICATION (WarpApplication)
