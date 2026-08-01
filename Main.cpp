#include <JuceHeader.h>
#include "FLPRecoveryTool.h"
#include "AdminElevation.h"

class FLPRecoverToolApplication : public juce::JUCEApplication
{
public:
    FLPRecoverToolApplication() {}

    const juce::String getApplicationName() override { return "FLPRecoverTool"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        if (!AdminElevation::isRunningAsAdmin())
        {
            auto* alert = new juce::AlertWindow(
                "Administrator Privileges Recommended",
                "Most of this app's features (physical drive scanning, NTFS $MFT recovery, "
                "disk imaging, TRIM control, shadow copy access) require Administrator "
                "privileges to work at all.\n\n"
                "You can continue without them — directory scanning and image-file "
                "analysis will still work — but relaunching elevated now is recommended "
                "if you plan to use the drive-level features.",
                juce::AlertWindow::WarningIcon
            );
            alert->addButton("Relaunch as Administrator", 1, juce::KeyPress(juce::KeyPress::returnKey));
            alert->addButton("Continue without", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            alert->enterModalState(true,
                juce::ModalCallbackFunction::create(
                    [this, alert](int result)
                    {
                        delete alert;

                        if (result == 1)
                        {
                            if (AdminElevation::relaunchAsAdmin())
                            {
                                // The elevated instance is starting up separately —
                                // quit this one now that it's been handed off.
                                systemRequestedQuit();
                                return;
                            }
                            // User cancelled the UAC prompt, or it failed — fall
                            // through and just open the window unelevated.
                        }

                        mainWindow.reset(new MainWindow(getApplicationName()));
                    }
                ),
                true
            );
        }
        else
        {
            mainWindow.reset(new MainWindow(getApplicationName()));
        }
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override
    {
    }

    // ─── Main Window ─────────────────────────────────────────────────────
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(name,
                juce::Desktop::getInstance().getDefaultLookAndFeel()
                .findColour(juce::ResizableWindow::backgroundColourId),
                DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new FLPRecoveryTool(), true);

            setResizable(true, true);
            centreWithSize(1000, 750);

            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(FLPRecoverToolApplication)
