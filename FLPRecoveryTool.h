#pragma once

#include <JuceHeader.h>
#include "FLPRecoveryScanner.h"

// =============================================================================
// FLPRecoveryTool – Main Application
// =============================================================================

class FLPRecoveryTool : public juce::Component,
    public juce::FileDragAndDropTarget,
    private juce::Timer
{
public:
    FLPRecoveryTool();
    ~FLPRecoveryTool() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void timerCallback() override;

private:
    // ─── UI Components ──────────────────────────────────────────────────

    juce::TextButton scanImageButton{ "Scan Image File..." };
    juce::TextButton scanDriveButton{ "Scan Physical Drive..." };
    juce::TextButton scanDirectoryButton{ "Scan Directory..." };
    juce::TextButton stopButton{ "Stop" };
    juce::TextButton recoverButton{ "Recover All" };
    juce::TextButton exploreButton{ "Open Output Folder" };

    juce::Label statusLabel;
    juce::Label progressLabel;
    juce::ProgressBar progressBar{ progressValue };

    juce::TableListBox resultsTable;
    juce::TextEditor logBox;

    // ─── Data ────────────────────────────────────────────────────────────

    FLPRecoveryScanner scanner;
    FLPRecovery::ScanResult currentResult;
    double progressValue = 0.0;

    // ─── Output Folder ──────────────────────────────────────────────────

    juce::File outputFolder;

    // ─── Table Model ────────────────────────────────────────────────────

    class TableModel : public juce::TableListBoxModel
    {
    public:
        TableModel(FLPRecoveryTool& owner) : m_owner(owner) {}

        int getNumRows() override;
        void paintRowBackground(juce::Graphics&, int rowNumber, int width, int height, bool rowIsSelected) override;
        void paintCell(juce::Graphics&, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
        void cellClicked(int rowNumber, int columnId, const juce::MouseEvent&) override;

    private:
        FLPRecoveryTool& m_owner;
    };

    std::unique_ptr<TableModel> tableModel;

    // ─── Callbacks ──────────────────────────────────────────────────────

    void scanImageButtonClicked();
    void scanDriveButtonClicked();
    void scanDirectoryButtonClicked();
    void stopButtonClicked();
    void recoverButtonClicked();
    void exploreButtonClicked();

    void updateUI();

    // ─── Logging ────────────────────────────────────────────────────────

    void logMessage(const juce::String& msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FLPRecoveryTool)
};


//block visualization
class BlockVisualizer : public juce::Component
{
public:
    BlockVisualizer();
    void paint(juce::Graphics& g) override;
    void updateBlocks(const std::vector<FLPRecovery::BlockInfo>& blocks);
    void setBlockSize(int size) { m_blockSize = size; }

private:
    std::vector<FLPRecovery::BlockInfo> m_blocks;
    int m_blockSize = 10; // pixels per block
    juce::Colour getBlockColour(const FLPRecovery::BlockInfo& block);
};


// ─── Main Application Class ────────────────────────────────────────────────

class FLPRecoveryApplication : public juce::JUCEApplication
{
public:
    FLPRecoveryApplication() = default;

    const juce::String getApplicationName() override { return "FLP Recovery Tool"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), new FLPRecoveryTool(), *this);
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, juce::Component* c, JUCEApplication& a)
            : DocumentWindow(name, juce::Colours::darkgrey, DocumentWindow::allButtons)
            , app(a)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(c, true);
            centreWithSize(900, 700);
            setResizable(true, true);
            setVisible(true);
        }

        void closeButtonPressed() override { app.systemRequestedQuit(); }

    private:
        JUCEApplication& app;
    };

    std::unique_ptr<MainWindow> mainWindow;
};