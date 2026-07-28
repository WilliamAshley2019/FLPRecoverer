#include <JuceHeader.h>
#include "FLPRecoveryTool.h"

// =============================================================================
// Main application window
class FLPRecoveryMainWindow : public juce::DocumentWindow
{
public:
    FLPRecoveryMainWindow()
        : DocumentWindow("FLP Recovery Tool",
                         juce::Colours::black,
                         DocumentWindow::allButtons)
    {
        // Create and add the main tool component
        auto* tool = new FLPRecoveryTool();
        setContentOwned(tool, true);
        
        // Set initial size
        setResizable(true, true);
        setResizeLimits(700, 500, 2000, 1400);
        centreWithSize(1000, 750);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        // This will call JUCEApplication::quit()
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FLPRecoveryMainWindow)
};

// =============================================================================
// Application class
class FLPRecoveryStandaloneApp : public juce::JUCEApplication
{
public:
    FLPRecoveryStandaloneApp() {}

    const juce::String getApplicationName() override       { return "FLP Recovery Tool"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& commandLine) override
    {
        mainWindow.reset(new FLPRecoveryMainWindow());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

private:
    std::unique_ptr<FLPRecoveryMainWindow> mainWindow;
};

// =============================================================================
// Startup point
START_JUCE_APPLICATION(FLPRecoveryStandaloneApp)