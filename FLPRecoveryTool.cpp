#include "FLPRecoveryTool.h"
#include <JuceHeader.h>

// =============================================================================
// Table Model Implementation
// =============================================================================

int FLPRecoveryTool::TableModel::getNumRows()
{
    return (int)m_owner.currentResult.candidates.size();
}

void FLPRecoveryTool::TableModel::paintRowBackground(juce::Graphics& g,
    int rowNumber, int width, int height, bool rowIsSelected)
{
    auto bg = rowIsSelected ? juce::Colour(0xFF2A2A3A) :
        (rowNumber % 2 ? juce::Colour(0xFF222222) : juce::Colour(0xFF1A1A1A));
    g.fillAll(bg);
}

void FLPRecoveryTool::TableModel::paintCell(juce::Graphics& g,
    int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (rowNumber >= (int)m_owner.currentResult.candidates.size())
        return;

    const auto& candidate = m_owner.currentResult.candidates[rowNumber];
    juce::String text;

    switch (columnId)
    {
    case 1: // Index
        text = juce::String(rowNumber + 1);
        break;
    case 2: // Offset
        text = "0x" + juce::String::toHexString((int64_t)candidate.offset);
        break;
    case 3: // Size
        text = juce::String((int)(candidate.totalSize / 1024)) + " KB";
        break;
    case 4: // Version
        text = candidate.version.isNotEmpty() ? candidate.version : "Unknown";
        break;
    case 5: // Status
        text = candidate.isValid ? "Valid" : "Invalid";
        break;
    default:
        break;
    }

    g.setColour(rowIsSelected ? juce::Colours::white : juce::Colours::lightgrey);
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft);
}

void FLPRecoveryTool::TableModel::cellClicked(int, int, const juce::MouseEvent&)
{
    // Could show details of the selected file
}

// =============================================================================
// Main UI Implementation
// =============================================================================

FLPRecoveryTool::FLPRecoveryTool()
{
    setSize(900, 700);

    // ─── Setup UI ────────────────────────────────────────────────────────

    addAndMakeVisible(scanImageButton);
    addAndMakeVisible(scanDriveButton);
    addAndMakeVisible(scanDirectoryButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(recoverButton);
    addAndMakeVisible(exploreButton);

    scanImageButton.onClick = [this] { scanImageButtonClicked(); };
    scanDriveButton.onClick = [this] { scanDriveButtonClicked(); };
    scanDirectoryButton.onClick = [this] { scanDirectoryButtonClicked(); };
    stopButton.onClick = [this] { stopButtonClicked(); };
    recoverButton.onClick = [this] { recoverButtonClicked(); };
    exploreButton.onClick = [this] { exploreButtonClicked(); };

    stopButton.setEnabled(false);

    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setText("Ready", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    progressLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    progressLabel.setText("0%", juce::dontSendNotification);
    addAndMakeVisible(progressLabel);

    addAndMakeVisible(progressBar);

    // ─── Results Table ──────────────────────────────────────────────────

    tableModel = std::make_unique<TableModel>(*this);
    resultsTable.setModel(tableModel.get());
    resultsTable.getHeader().addColumn("#", 1, 40);
    resultsTable.getHeader().addColumn("Offset", 2, 120);
    resultsTable.getHeader().addColumn("Size", 3, 100);
    resultsTable.getHeader().addColumn("Version", 4, 120);
    resultsTable.getHeader().addColumn("Status", 5, 80);
    addAndMakeVisible(resultsTable);

    // ─── Log Box ────────────────────────────────────────────────────────

    logBox.setMultiLine(true);
    logBox.setReadOnly(true);
    logBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF111111));
    logBox.setColour(juce::TextEditor::textColourId, juce::Colours::lightgrey);
    logBox.setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(logBox);

    // ─── Scanner Callbacks ──────────────────────────────────────────────

    scanner.onProgressCallback = [this](float progress, const juce::String& status)
        {
            progressValue = progress;
            juce::MessageManager::callAsync([this, status]
                {
                    statusLabel.setText(status, juce::dontSendNotification);
                    progressLabel.setText(juce::String((int)(progressValue * 100)) + "%", juce::dontSendNotification);
                });
        };

    scanner.onCandidateFoundCallback = [this](const FLPRecovery::Candidate& candidate)
        {
            juce::MessageManager::callAsync([this, candidate]
                {
                    currentResult.candidates.push_back(candidate);
                    resultsTable.updateContent();
                    logMessage("Found FLP at offset 0x" +
                        juce::String::toHexString((int64_t)candidate.offset) +
                        " (" + candidate.version + ")");
                });
        };

    scanner.onLogCallback = [this](const juce::String& msg)
        {
            juce::MessageManager::callAsync([this, msg]
                {
                    logMessage(msg);
                });
        };

    // ─── Timer ──────────────────────────────────────────────────────────

    startTimerHz(30);
}

FLPRecoveryTool::~FLPRecoveryTool() {}

void FLPRecoveryTool::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF181818));
}

void FLPRecoveryTool::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Top button row
    auto topRow = bounds.removeFromTop(35);
    scanImageButton.setBounds(topRow.removeFromLeft(140));
    topRow.removeFromLeft(8);
    scanDriveButton.setBounds(topRow.removeFromLeft(140));
    topRow.removeFromLeft(8);
    scanDirectoryButton.setBounds(topRow.removeFromLeft(140));
    topRow.removeFromLeft(20);
    stopButton.setBounds(topRow.removeFromLeft(80));
    topRow.removeFromLeft(20);
    recoverButton.setBounds(topRow.removeFromLeft(120));
    topRow.removeFromLeft(8);
    exploreButton.setBounds(topRow.removeFromLeft(130));

    bounds.removeFromTop(8);

    // Status and progress
    auto statusRow = bounds.removeFromTop(22);
    statusLabel.setBounds(statusRow.removeFromLeft(statusRow.getWidth() - 120));
    progressLabel.setBounds(statusRow.removeFromRight(60));
    bounds.removeFromTop(2);

    auto progressRow = bounds.removeFromTop(20);
    progressBar.setBounds(progressRow);

    bounds.removeFromTop(8);

    // Split: table (60%) + log (40%)
    auto tableArea = bounds.removeFromTop((int)(bounds.getHeight() * 0.6f));
    resultsTable.setBounds(tableArea);

    bounds.removeFromTop(4);
    logBox.setBounds(bounds);
}

void FLPRecoveryTool::timerCallback()
{
    // Update UI state
    if (scanner.isThreadRunning())
    {
        stopButton.setEnabled(true);
        scanImageButton.setEnabled(false);
        scanDriveButton.setEnabled(false);
        scanDirectoryButton.setEnabled(false);
    }
    else
    {
        stopButton.setEnabled(false);
        scanImageButton.setEnabled(true);
        scanDriveButton.setEnabled(true);
        scanDirectoryButton.setEnabled(true);
    }
}

// ─── Button Callbacks ──────────────────────────────────────────────────────

void FLPRecoveryTool::scanImageButtonClicked()
{
    juce::FileChooser chooser("Select Disk Image File",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.dmg;*.img;*.iso;*.bin;*.dd;*.raw;*");
    chooser.launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                currentResult = FLPRecovery::ScanResult();
                resultsTable.updateContent();
                logBox.clear();
                logMessage("Scanning: " + file.getFileName());
                scanner.startScanImage(file);
            }
        });
}

void FLPRecoveryTool::scanDriveButtonClicked()
{
#if JUCE_WINDOWS
    auto* alert = new juce::AlertWindow(
        "Scan Physical Drive",
        "Enter the drive path (e.g., \\\\.\\PhysicalDrive0 or C:):\n\n"
        "Common drive paths:\n"
        "  \\\\.\\PhysicalDrive0  - First physical disk\n"
        "  \\\\.\\PhysicalDrive1  - Second physical disk\n"
        "  C:                   - C: drive",
        juce::AlertWindow::InfoIcon
    );

    alert->addTextEditor("drive", "\\\\.\\PhysicalDrive0");
    alert->addButton("Scan", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alert->enterModalState(true,
        juce::ModalCallbackFunction::create(
            [this, alert](int result)
            {
                if (result == 1)
                {
                    juce::String drivePath = alert->getTextEditorContents("drive").trim();
                    if (drivePath.isNotEmpty())
                    {
                        currentResult = FLPRecovery::ScanResult();
                        resultsTable.updateContent();
                        logBox.clear();
                        logMessage("Scanning drive: " + drivePath);
                        logMessage("WARNING: Scanning physical drives requires administrator privileges.");
                        scanner.startScanDrive(drivePath);
                    }
                    else
                    {
                        logMessage("No drive path entered.");
                    }
                }
            }
        ),
        true
    );
#else
    auto* alert = new juce::AlertWindow(
        "Scan Physical Drive",
        "Enter the drive path (e.g., /dev/sda, /dev/disk0):\n\n"
        "Common drive paths:\n"
        "  /dev/sda   - First SCSI/SATA disk\n"
        "  /dev/sdb   - Second SCSI/SATA disk\n"
        "  /dev/disk0 - First disk on macOS",
        juce::AlertWindow::InfoIcon
    );

    alert->addTextEditor("drive", "/dev/sda");
    alert->addButton("Scan", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alert->enterModalState(true,
        juce::ModalCallbackFunction::create(
            [this, alert](int result)
            {
                if (result == 1)
                {
                    juce::String drivePath = alert->getTextEditorContents("drive").trim();
                    if (drivePath.isNotEmpty())
                    {
                        currentResult = FLPRecovery::ScanResult();
                        resultsTable.updateContent();
                        logBox.clear();
                        logMessage("Scanning drive: " + drivePath);
                        logMessage("WARNING: Scanning physical drives requires root privileges.");
                        scanner.startScanDrive(drivePath);
                    }
                    else
                    {
                        logMessage("No drive path entered.");
                    }
                }
            }
        ),
        true
    );
#endif
}

void FLPRecoveryTool::scanDirectoryButtonClicked()
{
    juce::FileChooser chooser("Select Directory to Scan",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*");
    chooser.launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir.isDirectory())
            {
                currentResult = FLPRecovery::ScanResult();
                resultsTable.updateContent();
                logBox.clear();
                logMessage("Scanning directory: " + dir.getFullPathName());
                scanner.startScanDirectory(dir);
            }
        });
}

void FLPRecoveryTool::stopButtonClicked()
{
    scanner.stopScanning();
    logMessage("Stopping scan...");
    statusLabel.setText("Stopped", juce::dontSendNotification);
}

void FLPRecoveryTool::recoverButtonClicked()
{
    if (currentResult.candidates.empty())
    {
        logMessage("No candidates to recover.");
        return;
    }

    juce::FileChooser chooser("Select Output Folder",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*");
    chooser.launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir.isDirectory())
            {
                outputFolder = dir;
                int count = scanner.recoverAll(currentResult, outputFolder);
                logMessage("Recovered " + juce::String(count) + " files to " + outputFolder.getFullPathName());
            }
        });
}

void FLPRecoveryTool::exploreButtonClicked()
{
    if (outputFolder.isDirectory())
    {
        outputFolder.startAsProcess();
    }
    else
    {
        logMessage("No output folder selected yet.");
    }
}

// ─── Logging ──────────────────────────────────────────────────────────────

void FLPRecoveryTool::logMessage(const juce::String& msg)
{
    logBox.moveCaretToEnd();
    logBox.insertTextAtCaret("[" + juce::Time::getCurrentTime().toString(true, true) + "] " + msg + "\n");
}

// ─── File Drag and Drop ────────────────────────────────────────────────────

bool FLPRecoveryTool::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() == 1 && (files[0].endsWithIgnoreCase(".img") ||
        files[0].endsWithIgnoreCase(".dmg") ||
        files[0].endsWithIgnoreCase(".dd") ||
        files[0].endsWithIgnoreCase(".iso") ||
        files[0].endsWithIgnoreCase(".bin"));
}

void FLPRecoveryTool::filesDropped(const juce::StringArray& files, int, int)
{
    if (files.size() == 1)
    {
        juce::File file(files[0]);
        if (file.existsAsFile())
        {
            currentResult = FLPRecovery::ScanResult();
            resultsTable.updateContent();
            logBox.clear();
            logMessage("Scanning dropped: " + file.getFileName());
            scanner.startScanImage(file);
        }
    }
}