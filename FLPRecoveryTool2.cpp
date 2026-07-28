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
    int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll(juce::Colour(0xFF2A3A5A));
    }
    else if (rowNumber % 2 == 0)
    {
        g.fillAll(juce::Colour(0xFF1A1A2E));
    }
    else
    {
        g.fillAll(juce::Colour(0xFF222244));
    }
}

void FLPRecoveryTool::TableModel::paintCell(juce::Graphics& g,
    int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (rowNumber >= (int)m_owner.currentResult.candidates.size())
        return;

    const auto& candidate = m_owner.currentResult.candidates[rowNumber];
    juce::String text;
    juce::Colour textColour = rowIsSelected ? juce::Colours::white : juce::Colour(0xFFC0C0E0);

    switch (columnId)
    {
    case 1: // Index
        text = juce::String(rowNumber + 1);
        break;
    case 2: // Offset
        text = "0x" + juce::String::toHexString((int64_t)candidate.offset);
        break;
    case 3: // Size
        if (candidate.totalSize > 1024 * 1024)
            text = juce::String((int)(candidate.totalSize / (1024 * 1024))) + " MB";
        else if (candidate.totalSize > 1024)
            text = juce::String((int)(candidate.totalSize / 1024)) + " KB";
        else
            text = juce::String((int)candidate.totalSize) + " B";
        break;
    case 4: // Version
        text = candidate.version.isNotEmpty() ? candidate.version : "Unknown";
        if (candidate.version.isNotEmpty())
            textColour = juce::Colour(0xFF66DD88);
        break;
    case 5: // Status
        if (candidate.isValid && candidate.isComplete)
        {
            text = "Complete";
            textColour = juce::Colour(0xFF44DD66);
        }
        else if (candidate.isValid)
        {
            text = "Partial";
            textColour = juce::Colour(0xFFDDDD44);
        }
        else
        {
            text = "Invalid";
            textColour = juce::Colour(0xFFDD4444);
        }
        break;
    default:
        break;
    }

    g.setColour(textColour);

    // ✅ JUCE 8.0.12: FontOptions with style as string
    juce::Font font(juce::FontOptions(13.0f).withStyle("Bold"));
    g.setFont(font);

    juce::Rectangle<int> textRect(4, 0, width - 8, height);
    g.drawText(text, textRect, juce::Justification::centredLeft);
}

void FLPRecoveryTool::TableModel::cellClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent& /*event*/)
{
    if (rowNumber >= 0 && rowNumber < (int)m_owner.currentResult.candidates.size())
    {
        const auto& candidate = m_owner.currentResult.candidates[rowNumber];
        m_owner.logMessage("Selected: Offset 0x" +
            juce::String::toHexString((int64_t)candidate.offset) +
            " | Size: " + juce::String((int)(candidate.totalSize / 1024)) + " KB" +
            " | Version: " + candidate.version +
            " | Status: " + (candidate.isValid ? "Valid" : "Invalid"));
    }
}

// =============================================================================
// Block Visualizer Component
// =============================================================================
class BlockVisualizerComponent : public juce::Component
{
public:
    BlockVisualizerComponent()
    {
        setSize(300, 100);
    }

    void updateBlocks(const std::vector<FLPRecovery::BlockInfo>& blocks)
    {
        m_blocks = blocks;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(2);
        g.fillAll(juce::Colour(0xFF0A0A1A));

        g.setColour(juce::Colour(0xFF333355));
        g.drawRect(bounds, 1);

        if (m_blocks.empty())
        {
            g.setColour(juce::Colour(0xFF666688));
            g.setFont(juce::Font(juce::FontOptions(14.0f)));
            g.drawText("No blocks scanned yet", bounds, juce::Justification::centred);
            return;
        }

        int numBlocks = (int)m_blocks.size();
        int blockSize = juce::jlimit(4, 20, (bounds.getWidth() - 10) / numBlocks);
        int maxBlocksPerRow = (bounds.getWidth() - 10) / blockSize;
        int rows = (numBlocks + maxBlocksPerRow - 1) / maxBlocksPerRow;
        int rowHeight = juce::jlimit(4, 20, (bounds.getHeight() - 10) / rows);

        int x = bounds.getX() + 5;
        int y = bounds.getY() + 5;
        int blocksDrawn = 0;

        for (int row = 0; row < rows && blocksDrawn < numBlocks; ++row)
        {
            for (int col = 0; col < maxBlocksPerRow && blocksDrawn < numBlocks; ++col)
            {
                const auto& block = m_blocks[blocksDrawn];
                juce::Colour blockColour = getBlockColour(block);

                g.setColour(blockColour);
                g.fillRect(x, y, blockSize - 1, rowHeight - 1);

                if (block.hasFLPData)
                {
                    g.setColour(juce::Colours::white.withAlpha(0.3f));
                    g.drawRect(x, y, blockSize - 1, rowHeight - 1, 1);
                }

                x += blockSize;
                blocksDrawn++;
            }
            x = bounds.getX() + 5;
            y += rowHeight;
        }

        drawLegend(g, bounds);
    }

private:
    std::vector<FLPRecovery::BlockInfo> m_blocks;

    juce::Colour getBlockColour(const FLPRecovery::BlockInfo& block)
    {
        if (block.isBackedUp)
            return juce::Colour(0xFF44AADD);
        if (block.hasFLPData)
            return juce::Colour(0xFF44DD66);
        return juce::Colour(0xFF333355);
    }

    void drawLegend(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        auto legendBounds = bounds.removeFromBottom(20);
        int spacing = 60;
        int x = (bounds.getWidth() - spacing * 3) / 2 + bounds.getX();

        g.setColour(juce::Colour(0xFF44DD66));
        g.fillRect(x, legendBounds.getY() + 4, 12, 12);
        g.setColour(juce::Colour(0xFF8888AA));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText("FLP", x + 16, legendBounds.getY(), 40, 20, juce::Justification::centredLeft);
        x += spacing;

        g.setColour(juce::Colour(0xFF44AADD));
        g.fillRect(x, legendBounds.getY() + 4, 12, 12);
        g.setColour(juce::Colour(0xFF8888AA));
        g.drawText("Backup", x + 16, legendBounds.getY(), 50, 20, juce::Justification::centredLeft);
        x += spacing + 20;

        g.setColour(juce::Colour(0xFF333355));
        g.fillRect(x, legendBounds.getY() + 4, 12, 12);
        g.setColour(juce::Colour(0xFF8888AA));
        g.drawText("Empty", x + 16, legendBounds.getY(), 50, 20, juce::Justification::centredLeft);
    }
};

// =============================================================================
// Main UI Implementation
//
// BUILD_STAGE bisection — bump this number, rebuild, run. If it stops
// launching at some stage N, the problem is in whatever's gated to "== N".
//   1 = bare component, no children at all
//   2 = + buttons and labels (visible, not wired)
//   3 = + progress bar
//   4 = + results table and its model
//   5 = + block visualizer
//   6 = + log box
//   7 = + button onClick wiring
//   8 = + scanner member's callback lambdas wired
//   9 = + startTimerHz(30)   (full original behavior)
// =============================================================================
#define BUILD_STAGE 5

FLPRecoveryTool::FLPRecoveryTool()
    : progressBar(progressValue)
{
    setSize(1000, 750);
    setOpaque(true);

#if BUILD_STAGE >= 2
    auto setupButton = [](juce::TextButton& button, const juce::String& text, juce::Colour colour)
        {
            button.setButtonText(text);
            button.setColour(juce::TextButton::buttonColourId, colour);
            button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            button.setColour(juce::TextButton::buttonOnColourId, colour.brighter(0.3f));
            button.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        };

    setupButton(scanImageButton, "Scan Image File...", juce::Colour(0xFF2A4A6A));
    setupButton(scanDriveButton, "Scan Physical Drive...", juce::Colour(0xFF6A2A4A));
    setupButton(scanDirectoryButton, "Scan Directory...", juce::Colour(0xFF2A6A4A));
    setupButton(stopButton, "Stop", juce::Colour(0xFF8A2A2A));
    setupButton(selectOutputFolderButton, "Select Output Folder", juce::Colour(0xFF4A4A7A));
    setupButton(recoverButton, "Recover All", juce::Colour(0xFF2A8A5A));
    setupButton(exploreButton, "Open Output Folder", juce::Colour(0xFF4A6A4A));

    stopButton.setEnabled(false);

    addAndMakeVisible(scanImageButton);
    addAndMakeVisible(scanDriveButton);
    addAndMakeVisible(scanDirectoryButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(selectOutputFolderButton);
    addAndMakeVisible(recoverButton);
    addAndMakeVisible(exploreButton);

    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFAACCDD));
    statusLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    statusLabel.setText("Ready", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    progressLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFAACCDD));
    progressLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    progressLabel.setText("0%", juce::dontSendNotification);
    addAndMakeVisible(progressLabel);
#endif

#if BUILD_STAGE >= 3
    progressBar.setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xFF1A1A2A));
    progressBar.setColour(juce::ProgressBar::foregroundColourId, juce::Colour(0xFF44CC88));
    addAndMakeVisible(progressBar);
#endif

#if BUILD_STAGE >= 4
    tableModel = std::make_unique<TableModel>(*this);
    resultsTable.setModel(tableModel.get());
    resultsTable.setColour(juce::TableListBox::backgroundColourId, juce::Colour(0xFF0A0A1A));
    resultsTable.setColour(juce::TableListBox::textColourId, juce::Colour(0xFFC0C0E0));
    resultsTable.setColour(juce::TableListBox::outlineColourId, juce::Colour(0xFF333355));

    resultsTable.getHeader().setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour(0xFF222244));
    resultsTable.getHeader().setColour(juce::TableHeaderComponent::textColourId, juce::Colour(0xFFAACCDD));
    resultsTable.getHeader().setColour(juce::TableHeaderComponent::outlineColourId, juce::Colour(0xFF333355));

    resultsTable.getHeader().addColumn("#", 1, 40);
    resultsTable.getHeader().addColumn("Offset", 2, 130);
    resultsTable.getHeader().addColumn("Size", 3, 100);
    resultsTable.getHeader().addColumn("Version", 4, 120);
    resultsTable.getHeader().addColumn("Status", 5, 100);

    addAndMakeVisible(resultsTable);
#endif

#if BUILD_STAGE >= 5
    blockVisualizer = std::make_unique<BlockVisualizerComponent>();
    addAndMakeVisible(blockVisualizer.get());
#endif

#if BUILD_STAGE >= 6
    logBox.setMultiLine(true);
    logBox.setReadOnly(true);
    logBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF0A0A18));
    logBox.setColour(juce::TextEditor::textColourId, juce::Colour(0xFF88AACC));
    logBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF222244));
    logBox.setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(logBox);
#endif

#if BUILD_STAGE >= 7
    scanImageButton.onClick = [this] { scanImageButtonClicked(); };
    scanDriveButton.onClick = [this] { scanDriveButtonClicked(); };
    scanDirectoryButton.onClick = [this] { scanDirectoryButtonClicked(); };
    stopButton.onClick = [this] { stopButtonClicked(); };
    selectOutputFolderButton.onClick = [this] { selectOutputFolderButtonClicked(); };
    recoverButton.onClick = [this] { recoverButtonClicked(); };
    exploreButton.onClick = [this] { exploreButtonClicked(); };
#endif

#if BUILD_STAGE >= 8
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

                    if (!currentResult.allBlocks.empty())
                    {
                        blockVisualizer->updateBlocks(currentResult.allBlocks);
                    }
                });
        };

    scanner.onBlockScanned = [this](const FLPRecovery::BlockInfo& block)
        {
            juce::MessageManager::callAsync([this, block]
                {
                    currentResult.allBlocks.push_back(block);
                    blockVisualizer->updateBlocks(currentResult.allBlocks);
                });
        };

    scanner.onLogCallback = [this](const juce::String& msg)
        {
            juce::MessageManager::callAsync([this, msg]
                {
                    logMessage(msg);
                });
        };
#endif

#if BUILD_STAGE >= 9
    startTimerHz(30);
#endif
}

FLPRecoveryTool::~FLPRecoveryTool() {}

void FLPRecoveryTool::paint(juce::Graphics& g)
{
    juce::ColourGradient gradient(
        juce::Colour(0xFF0A0A1A), 0, 0,
        juce::Colour(0xFF1A1A3A), 0, (float)getHeight(),
        false
    );
    g.setGradientFill(gradient);
    g.fillAll();
}

void FLPRecoveryTool::resized()
{
    auto bounds = getLocalBounds().reduced(12);

    auto topRow = bounds.removeFromTop(40);
    int buttonWidth = 145;
    int gap = 6;

    scanImageButton.setBounds(topRow.removeFromLeft(buttonWidth));
    topRow.removeFromLeft(gap);
    scanDriveButton.setBounds(topRow.removeFromLeft(buttonWidth));
    topRow.removeFromLeft(gap);
    scanDirectoryButton.setBounds(topRow.removeFromLeft(buttonWidth));
    topRow.removeFromLeft(gap * 2);
    stopButton.setBounds(topRow.removeFromLeft(80));
    topRow.removeFromLeft(gap * 2);
    selectOutputFolderButton.setBounds(topRow.removeFromLeft(buttonWidth + 20));
    topRow.removeFromLeft(gap);
    recoverButton.setBounds(topRow.removeFromLeft(120));
    topRow.removeFromLeft(gap);
    exploreButton.setBounds(topRow.removeFromLeft(buttonWidth));

    bounds.removeFromTop(10);

    auto statusRow = bounds.removeFromTop(24);
    statusLabel.setBounds(statusRow.removeFromLeft(statusRow.getWidth() - 100));
    progressLabel.setBounds(statusRow.removeFromRight(70));
    bounds.removeFromTop(4);

    auto progressRow = bounds.removeFromTop(22);
    progressBar.setBounds(progressRow);
    bounds.removeFromTop(8);

    auto tableArea = bounds.removeFromTop((int)(bounds.getHeight() * 0.45f));
    resultsTable.setBounds(tableArea);
    bounds.removeFromTop(4);

    auto visualizerArea = bounds.removeFromTop((int)(bounds.getHeight() * 0.28f));
    if (blockVisualizer != nullptr)
        blockVisualizer->setBounds(visualizerArea);
    bounds.removeFromTop(4);

    logBox.setBounds(bounds);
}

void FLPRecoveryTool::timerCallback()
{
    if (scanner.isThreadRunning())
    {
        stopButton.setEnabled(true);
        scanImageButton.setEnabled(false);
        scanDriveButton.setEnabled(false);
        scanDirectoryButton.setEnabled(false);
        selectOutputFolderButton.setEnabled(false);
        recoverButton.setEnabled(false);
    }
    else
    {
        stopButton.setEnabled(false);
        scanImageButton.setEnabled(true);
        scanDriveButton.setEnabled(true);
        scanDirectoryButton.setEnabled(true);
        selectOutputFolderButton.setEnabled(true);
        recoverButton.setEnabled(!currentResult.candidates.empty());
    }
}

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
                blockVisualizer->updateBlocks(currentResult.allBlocks);
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
        "Enter the drive path:\n"
        "Examples:\n"
        "  \\\\.\\PhysicalDrive0  - First physical disk\n"
        "  \\\\.\\PhysicalDrive1  - Second physical disk\n"
        "  C:                   - C: drive\n"
        "Requires Administrator privileges",
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
                        blockVisualizer->updateBlocks(currentResult.allBlocks);
                        logBox.clear();
                        logMessage("Scanning drive: " + drivePath);
                        logMessage("Scanning physical drives requires administrator privileges.");
                        scanner.startScanDrive(drivePath);
                    }
                    else
                    {
                        logMessage("No drive path entered.");
                    }
                }
                delete alert;
            }
        ),
        true
    );
#else
    auto* alert = new juce::AlertWindow(
        "Scan Physical Drive",
        "Enter the drive path (e.g., /dev/sda, /dev/disk0):\n"
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
                        blockVisualizer->updateBlocks(currentResult.allBlocks);
                        logBox.clear();
                        logMessage("Scanning drive: " + drivePath);
                        logMessage("Scanning physical drives requires root privileges.");
                        scanner.startScanDrive(drivePath);
                    }
                    else
                    {
                        logMessage("No drive path entered.");
                    }
                }
                delete alert;
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
                blockVisualizer->updateBlocks(currentResult.allBlocks);
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

void FLPRecoveryTool::selectOutputFolderButtonClicked()
{
    juce::FileChooser chooser("Select Output Folder for Recovered Files",
        outputFolder.isDirectory() ? outputFolder : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*");

    chooser.launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir.isDirectory())
            {
                outputFolder = dir;
                logMessage("Output folder set to: " + outputFolder.getFullPathName());
            }
            else
            {
                logMessage("No output folder selected.");
            }
        });
}

void FLPRecoveryTool::recoverButtonClicked()
{
    if (currentResult.candidates.empty())
    {
        logMessage("No candidates to recover.");
        return;
    }

    if (!outputFolder.isDirectory())
    {
        logMessage("Please select an output folder first using 'Select Output Folder'.");
        return;
    }

    logMessage("Recovering " + juce::String((int)currentResult.candidates.size()) + " files to: " + outputFolder.getFullPathName());
    int count = scanner.recoverAllWithBlocks(currentResult, outputFolder, true);
    logMessage("Recovered " + juce::String(count) + " files.");
}

void FLPRecoveryTool::exploreButtonClicked()
{
    if (outputFolder.isDirectory())
    {
        outputFolder.startAsProcess();
    }
    else
    {
        logMessage("No output folder selected. Click 'Select Output Folder' first.");
    }
}

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
            blockVisualizer->updateBlocks(currentResult.allBlocks);
            logBox.clear();
            logMessage("Scanning dropped: " + file.getFileName());
            scanner.startScanImage(file);
        }
    }
}