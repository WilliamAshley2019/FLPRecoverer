#pragma once
#include <JuceHeader.h>
#include "FLPRecoveryScanner.h"
#include "NtfsScannerThread.h"

// Forward declaration
class BlockVisualizerComponent;

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
    juce::TextButton scanImageButton;
    juce::TextButton scanDriveButton;
    juce::TextButton scanDirectoryButton;
    juce::TextButton scanNtfsDeletedButton;
    juce::TextButton stopButton;
    juce::TextButton selectOutputFolderButton;
    juce::TextButton recoverButton;
    juce::TextButton exploreButton;

    juce::Label statusLabel;
    juce::Label progressLabel;

    double progressValue = 0.0;
    juce::ProgressBar progressBar;

    juce::TableListBox resultsTable;
    juce::TextEditor logBox;
    std::unique_ptr<BlockVisualizerComponent> blockVisualizer;

    // ─── Data ────────────────────────────────────────────────────────────
    FLPRecoveryScanner scanner;
    FLPRecovery::ScanResult currentResult;

    NtfsScannerThread ntfsScanner;
    std::vector<NtfsMftRecovery::DeletedFileCandidate> ntfsCandidates;

    // ─── Output Folder ──────────────────────────────────────────────────
    juce::File outputFolder;

    // FileChooser::launchAsync() is non-blocking — the dialog is torn down
    // if the FileChooser object is destroyed before the callback fires, so
    // this must be a persistent member, not a local stack variable.
    std::unique_ptr<juce::FileChooser> fileChooser;

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
    void scanNtfsDeletedButtonClicked();
    void stopButtonClicked();
    void selectOutputFolderButtonClicked();
    void recoverButtonClicked();
    void exploreButtonClicked();

    // ─── Logging ────────────────────────────────────────────────────────
    void logMessage(const juce::String& msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FLPRecoveryTool)
};