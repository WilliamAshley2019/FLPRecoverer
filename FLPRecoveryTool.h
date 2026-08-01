#pragma once
#include <JuceHeader.h>
#include "FLPRecoveryScanner.h"
#include "NtfsScannerThread.h"
#include "DiskImagerThread.h"
#include "TrimControl.h"
#include "flp.h"
#include "flphelper.h"
#include "RecoveryThread.h"
#include "VssRecovery.h"

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
    juce::TextButton imageDriveButton;
    juce::TextButton toggleTrimButton;
    juce::TextButton checkPreviousVersionsButton;
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
    std::vector<bool> flpSelected; // parallel to currentResult.candidates

    NtfsScannerThread ntfsScanner;
    std::vector<NtfsMftRecovery::DeletedFileCandidate> ntfsCandidates;
    std::vector<bool> ntfsSelected; // parallel to ntfsCandidates

    DiskImagerThread diskImager;
    RecoveryThread recoveryThread;

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
    void imageDriveButtonClicked();
    void toggleTrimButtonClicked();
    void checkPreviousVersionsButtonClicked();
    void stopButtonClicked();
    void selectOutputFolderButtonClicked();
    void recoverButtonClicked();
    void exploreButtonClicked();

    // ─── Logging ────────────────────────────────────────────────────────
    void logMessage(const juce::String& msg);

    // Runs FLPTOOL's semantic parser against a just-recovered file (full
    // or partial, carved or NTFS-reconstructed) and logs a real content
    // summary — pattern/note counts, sample references — plus exports a
    // .mid alongside it if any patterns parsed. Works on partial files
    // because FL::Project::loadPartial() never returns null; it just
    // reports how far it got.
    void analyzeRecoveredFlpFile(const juce::File& flpFile);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FLPRecoveryTool)
};