#include "FLPRecoveryTool.h"
#include <JuceHeader.h>

// =============================================================================
// Table Model Implementation
// =============================================================================
int FLPRecoveryTool::TableModel::getNumRows()
{
    return (int)m_owner.currentResult.candidates.size() + (int)m_owner.ntfsCandidates.size();
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
    const int flpCount = (int)m_owner.currentResult.candidates.size();
    const int ntfsCount = (int)m_owner.ntfsCandidates.size();
    if (rowNumber >= flpCount + ntfsCount)
        return;

    if (columnId == 6)
    {
        bool checked = (rowNumber < flpCount)
            ? (rowNumber < (int)m_owner.flpSelected.size() && m_owner.flpSelected[rowNumber])
            : ((rowNumber - flpCount) < (int)m_owner.ntfsSelected.size() && m_owner.ntfsSelected[rowNumber - flpCount]);

        g.setColour(checked ? juce::Colour(0xFF44DD66) : juce::Colour(0xFF666688));
        juce::Font checkFont(juce::FontOptions(16.0f));
        g.setFont(checkFont);
        g.drawText(checked ? juce::String(juce::CharPointer_UTF8("\xE2\x98\x91")) // ☑
                            : juce::String(juce::CharPointer_UTF8("\xE2\x98\x90")), // ☐
            juce::Rectangle<int>(0, 0, width, height), juce::Justification::centred);
        return;
    }

    juce::String text;
    juce::Colour textColour = rowIsSelected ? juce::Colours::white : juce::Colour(0xFFC0C0E0);

    if (rowNumber < flpCount)
    {
        const auto& candidate = m_owner.currentResult.candidates[rowNumber];

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
    }
    else
    {
        const auto& candidate = m_owner.ntfsCandidates[rowNumber - flpCount];

        switch (columnId)
        {
        case 1: // Index
            text = juce::String(rowNumber + 1);
            break;
        case 2: // Offset — shown as MFT record number for this row type
            text = "MFT#" + juce::String((int64_t)candidate.mftRecordNumber);
            break;
        case 3: // Size
            if (candidate.realSize > 1024 * 1024)
                text = juce::String((int)(candidate.realSize / (1024 * 1024))) + " MB";
            else if (candidate.realSize > 1024)
                text = juce::String((int)(candidate.realSize / 1024)) + " KB";
            else
                text = juce::String((int)candidate.realSize) + " B";
            break;
        case 4: // Version column repurposed to show original filename for this row type
            text = candidate.fileName;
            textColour = juce::Colour(0xFF66AADD);
            break;
        case 5: // Status
            text = "Deleted (NTFS" + (candidate.fragmentCount > 1
                ? juce::String(", ") + juce::String(candidate.fragmentCount) + " fragments)"
                : juce::String(")"));
            textColour = juce::Colour(0xFFDD9944);
            break;
        default:
            break;
        }
    }

    g.setColour(textColour);

    // ✅ JUCE 8.0.12: FontOptions with style as string
    juce::Font font(juce::FontOptions(13.0f).withStyle("Bold"));
    g.setFont(font);

    juce::Rectangle<int> textRect(4, 0, width - 8, height);
    g.drawText(text, textRect, juce::Justification::centredLeft);
}

void FLPRecoveryTool::TableModel::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& /*event*/)
{
    const int flpCount = (int)m_owner.currentResult.candidates.size();
    const int ntfsCount = (int)m_owner.ntfsCandidates.size();
    if (rowNumber < 0 || rowNumber >= flpCount + ntfsCount)
        return;

    if (columnId == 6)
    {
        if (rowNumber < flpCount)
        {
            if (rowNumber < (int)m_owner.flpSelected.size())
                m_owner.flpSelected[rowNumber] = !m_owner.flpSelected[rowNumber];
        }
        else
        {
            int idx = rowNumber - flpCount;
            if (idx < (int)m_owner.ntfsSelected.size())
                m_owner.ntfsSelected[idx] = !m_owner.ntfsSelected[idx];
        }
        m_owner.resultsTable.repaint();
        return;
    }

    if (rowNumber < flpCount)
    {
        const auto& candidate = m_owner.currentResult.candidates[rowNumber];
        m_owner.logMessage("Selected: Offset 0x" +
            juce::String::toHexString((int64_t)candidate.offset) +
            " | Size: " + juce::String((int)(candidate.totalSize / 1024)) + " KB" +
            " | Version: " + candidate.version +
            " | Status: " + (candidate.isValid ? "Valid" : "Invalid"));
    }
    else
    {
        const auto& candidate = m_owner.ntfsCandidates[rowNumber - flpCount];
        m_owner.logMessage("Selected (NTFS): " + candidate.fileName +
            " | Size: " + juce::String((int)(candidate.realSize / 1024)) + " KB" +
            " | Fragments: " + juce::String(candidate.fragmentCount) +
            " | MFT record: " + juce::String((int64_t)candidate.mftRecordNumber));
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
// =============================================================================
FLPRecoveryTool::FLPRecoveryTool()
    : progressBar(progressValue)
{
    setSize(1000, 750);
    setOpaque(true);

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
    setupButton(scanNtfsDeletedButton, "Scan Deleted (NTFS)...", juce::Colour(0xFF6A5A2A));
    setupButton(imageDriveButton, "Image Drive (DD)...", juce::Colour(0xFF4A2A6A));
    setupButton(toggleTrimButton, "TRIM Status...", juce::Colour(0xFF6A4A2A));
    setupButton(checkPreviousVersionsButton, "Check Previous Versions...", juce::Colour(0xFF2A6A6A));
    setupButton(stopButton, "Stop", juce::Colour(0xFF8A2A2A));
    setupButton(selectOutputFolderButton, "Select Output Folder", juce::Colour(0xFF4A4A7A));
    setupButton(recoverButton, "Recover All", juce::Colour(0xFF2A8A5A));
    setupButton(exploreButton, "Open Output Folder", juce::Colour(0xFF4A6A4A));

    stopButton.setEnabled(false);

    addAndMakeVisible(scanImageButton);
    addAndMakeVisible(scanDriveButton);
    addAndMakeVisible(scanDirectoryButton);
    addAndMakeVisible(scanNtfsDeletedButton);
    addAndMakeVisible(imageDriveButton);
    addAndMakeVisible(toggleTrimButton);
    addAndMakeVisible(checkPreviousVersionsButton);
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

    progressBar.setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xFF1A1A2A));
    progressBar.setColour(juce::ProgressBar::foregroundColourId, juce::Colour(0xFF44CC88));
    addAndMakeVisible(progressBar);

    tableModel = std::make_unique<TableModel>(*this);
    resultsTable.setModel(tableModel.get());
    resultsTable.setColour(juce::TableListBox::backgroundColourId, juce::Colour(0xFF0A0A1A));
    resultsTable.setColour(juce::TableListBox::textColourId, juce::Colour(0xFFC0C0E0));
    resultsTable.setColour(juce::TableListBox::outlineColourId, juce::Colour(0xFF333355));

    resultsTable.getHeader().setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour(0xFF222244));
    resultsTable.getHeader().setColour(juce::TableHeaderComponent::textColourId, juce::Colour(0xFFAACCDD));
    resultsTable.getHeader().setColour(juce::TableHeaderComponent::outlineColourId, juce::Colour(0xFF333355));

    resultsTable.getHeader().addColumn("", 6, 32); // checkbox column
    resultsTable.getHeader().addColumn("#", 1, 40);
    resultsTable.getHeader().addColumn("Offset", 2, 130);
    resultsTable.getHeader().addColumn("Size", 3, 100);
    resultsTable.getHeader().addColumn("Version", 4, 120);
    resultsTable.getHeader().addColumn("Status", 5, 100);

    addAndMakeVisible(resultsTable);

    blockVisualizer = std::make_unique<BlockVisualizerComponent>();
    addAndMakeVisible(blockVisualizer.get());

    logBox.setMultiLine(true);
    logBox.setReadOnly(true);
    logBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF0A0A18));
    logBox.setColour(juce::TextEditor::textColourId, juce::Colour(0xFF88AACC));
    logBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF222244));
    logBox.setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(logBox);

    scanImageButton.onClick = [this] { scanImageButtonClicked(); };
    scanDriveButton.onClick = [this] { scanDriveButtonClicked(); };
    scanDirectoryButton.onClick = [this] { scanDirectoryButtonClicked(); };
    scanNtfsDeletedButton.onClick = [this] { scanNtfsDeletedButtonClicked(); };
    imageDriveButton.onClick = [this] { imageDriveButtonClicked(); };
    toggleTrimButton.onClick = [this] { toggleTrimButtonClicked(); };
    checkPreviousVersionsButton.onClick = [this] { checkPreviousVersionsButtonClicked(); };
    stopButton.onClick = [this] { stopButtonClicked(); };
    selectOutputFolderButton.onClick = [this] { selectOutputFolderButtonClicked(); };
    recoverButton.onClick = [this] { recoverButtonClicked(); };
    exploreButton.onClick = [this] { exploreButtonClicked(); };

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
                    flpSelected.push_back(true);
                    resultsTable.updateContent();
                    resultsTable.repaint();
                    logMessage(">>> TABLE ROW ADDED (" + juce::String((int)currentResult.candidates.size()) +
                        " total) — offset 0x" + juce::String::toHexString((int64_t)candidate.offset) +
                        " (" + candidate.version + ")");

                    if (!currentResult.allBlocks.empty())
                    {
                        if (blockVisualizer != nullptr)
                            blockVisualizer->updateBlocks(currentResult.allBlocks);
                    }
                });
        };

    scanner.onBlockScanned = [this](const FLPRecovery::BlockInfo& block)
        {
            juce::MessageManager::callAsync([this, block]
                {
                    currentResult.allBlocks.push_back(block);
                    if (blockVisualizer != nullptr)
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

    ntfsScanner.onProgressCallback = [this](float progress, const juce::String& status)
        {
            progressValue = progress;
            juce::MessageManager::callAsync([this, status]
                {
                    statusLabel.setText(status, juce::dontSendNotification);
                    progressLabel.setText(juce::String((int)(progressValue * 100)) + "%", juce::dontSendNotification);
                });
        };

    ntfsScanner.onCandidateFoundCallback = [this](const NtfsMftRecovery::DeletedFileCandidate& candidate)
        {
            juce::MessageManager::callAsync([this, candidate]
                {
                    ntfsCandidates.push_back(candidate);
                    ntfsSelected.push_back(true);
                    resultsTable.updateContent();
                    logMessage("Found deleted file: " + candidate.fileName +
                        " (" + juce::String(candidate.fragmentCount) + " fragment(s))");
                });
        };

    ntfsScanner.onLogCallback = [this](const juce::String& msg)
        {
            juce::MessageManager::callAsync([this, msg]
                {
                    logMessage(msg);
                });
        };

    diskImager.onProgressCallback = [this](float progress, const juce::String& status)
        {
            progressValue = progress;
            juce::MessageManager::callAsync([this, status]
                {
                    statusLabel.setText(status, juce::dontSendNotification);
                    progressLabel.setText(juce::String((int)(progressValue * 100)) + "%", juce::dontSendNotification);
                });
        };

    diskImager.onLogCallback = [this](const juce::String& msg)
        {
            juce::MessageManager::callAsync([this, msg]
                {
                    logMessage(msg);
                });
        };

    diskImager.onCompleteCallback = [this](bool success)
        {
            juce::MessageManager::callAsync([this, success]
                {
                    statusLabel.setText(success ? "Imaging complete" : "Imaging stopped/failed", juce::dontSendNotification);
                });
        };

    recoveryThread.onLogCallback = [this](const juce::String& msg)
        {
            juce::MessageManager::callAsync([this, msg]
                {
                    logMessage(msg);
                });
        };

    recoveryThread.onFileRecoveredCallback = [this](const juce::File& f)
        {
            juce::MessageManager::callAsync([this, f]
                {
                    analyzeRecoveredFlpFile(f);
                });
        };

    recoveryThread.onCompleteCallback = [this](int /*totalRecovered*/)
        {
            juce::MessageManager::callAsync([this]
                {
                    statusLabel.setText("Recovery complete", juce::dontSendNotification);
                });
        };

    startTimerHz(30);

    // The very first resized() fired synchronously back at setSize(1000, 750)
    // above, before blockVisualizer (a unique_ptr, created later in this
    // constructor) existed — so it never got positioned. Re-running layout
    // now, with everything actually constructed, fixes that.
    resized();
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

    int gap = 6;

    // Row 1: the four scan-source buttons
    auto scanRow = bounds.removeFromTop(36);
    int scanButtonWidth = (scanRow.getWidth() - gap * 3) / 4;
    scanImageButton.setBounds(scanRow.removeFromLeft(scanButtonWidth));
    scanRow.removeFromLeft(gap);
    scanDriveButton.setBounds(scanRow.removeFromLeft(scanButtonWidth));
    scanRow.removeFromLeft(gap);
    scanDirectoryButton.setBounds(scanRow.removeFromLeft(scanButtonWidth));
    scanRow.removeFromLeft(gap);
    scanNtfsDeletedButton.setBounds(scanRow);

    bounds.removeFromTop(6);

    // Row 2: forensics utilities — VSS, imaging, TRIM, stop
    auto utilRow = bounds.removeFromTop(36);
    int utilButtonWidth = (utilRow.getWidth() - gap * 3 - 90) / 3;
    checkPreviousVersionsButton.setBounds(utilRow.removeFromLeft(utilButtonWidth));
    utilRow.removeFromLeft(gap);
    imageDriveButton.setBounds(utilRow.removeFromLeft(utilButtonWidth));
    utilRow.removeFromLeft(gap);
    toggleTrimButton.setBounds(utilRow.removeFromLeft(utilButtonWidth));
    utilRow.removeFromLeft(gap);
    stopButton.setBounds(utilRow);

    bounds.removeFromTop(6);

    // Row 3: output folder / recover / explore
    auto actionRow = bounds.removeFromTop(36);
    int remainingWidth = actionRow.getWidth();
    int outputFolderWidth = (int)(remainingWidth * 0.40f);
    int recoverWidth = (int)(remainingWidth * 0.30f);
    selectOutputFolderButton.setBounds(actionRow.removeFromLeft(outputFolderWidth));
    actionRow.removeFromLeft(gap);
    recoverButton.setBounds(actionRow.removeFromLeft(recoverWidth));
    actionRow.removeFromLeft(gap);
    exploreButton.setBounds(actionRow);

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
    const bool anyRunning = scanner.isThreadRunning() || ntfsScanner.isThreadRunning() ||
        diskImager.isThreadRunning() || recoveryThread.isThreadRunning();

    if (anyRunning)
    {
        stopButton.setEnabled(true);
        scanImageButton.setEnabled(false);
        scanDriveButton.setEnabled(false);
        scanDirectoryButton.setEnabled(false);
        scanNtfsDeletedButton.setEnabled(false);
        imageDriveButton.setEnabled(false);
        checkPreviousVersionsButton.setEnabled(false);
        selectOutputFolderButton.setEnabled(false);
        recoverButton.setEnabled(false);
    }
    else
    {
        stopButton.setEnabled(false);
        scanImageButton.setEnabled(true);
        scanDriveButton.setEnabled(true);
        scanDirectoryButton.setEnabled(true);
        scanNtfsDeletedButton.setEnabled(true);
        imageDriveButton.setEnabled(true);
        checkPreviousVersionsButton.setEnabled(true);
        selectOutputFolderButton.setEnabled(true);
        recoverButton.setEnabled(!currentResult.candidates.empty() || !ntfsCandidates.empty());
    }
}

void FLPRecoveryTool::scanImageButtonClicked()
{
    fileChooser = std::make_unique<juce::FileChooser>("Select Disk Image File",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.dmg;*.img;*.iso;*.bin;*.dd;*.raw;*");

    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                currentResult = FLPRecovery::ScanResult();
                ntfsCandidates.clear();
                flpSelected.clear();
                ntfsSelected.clear();
                resultsTable.updateContent();
                if (blockVisualizer != nullptr)
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
                        ntfsCandidates.clear();
                        flpSelected.clear();
                        ntfsSelected.clear();
                        resultsTable.updateContent();
                        if (blockVisualizer != nullptr)
                            blockVisualizer->updateBlocks(currentResult.allBlocks);
                        logBox.clear();
                        logMessage("Scanning drive: " + drivePath);
                        logMessage("(Raw drive access needs Administrator privileges -- if this fails, the log below will say specifically why.)");
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
                        ntfsCandidates.clear();
                        flpSelected.clear();
                        ntfsSelected.clear();
                        resultsTable.updateContent();
                        if (blockVisualizer != nullptr)
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
    fileChooser = std::make_unique<juce::FileChooser>("Select Directory to Scan",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*");

    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir.isDirectory())
            {
                currentResult = FLPRecovery::ScanResult();
                ntfsCandidates.clear();
                flpSelected.clear();
                ntfsSelected.clear();
                resultsTable.updateContent();
                if (blockVisualizer != nullptr)
                    blockVisualizer->updateBlocks(currentResult.allBlocks);
                logBox.clear();
                logMessage("Scanning directory: " + dir.getFullPathName());
                scanner.startScanDirectory(dir);
            }
        });
}

void FLPRecoveryTool::scanNtfsDeletedButtonClicked()
{
#if JUCE_WINDOWS
    auto* alert = new juce::AlertWindow(
        "Scan Deleted Files (NTFS)",
        "Enter the drive letter to scan for deleted .flp files:\n"
        "Example:\n"
        "  C:\n\n"
        "This reads the NTFS $MFT directly to find deleted files and their\n"
        "exact (possibly fragmented) location on disk, rather than guessing\n"
        "from raw bytes.\n"
        "Requires Administrator privileges. NTFS volumes only.",
        juce::AlertWindow::InfoIcon
    );
    alert->addTextEditor("drive", "C:");
    alert->addButton("Scan", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alert->enterModalState(true,
        juce::ModalCallbackFunction::create(
            [this, alert](int result)
            {
                if (result == 1)
                {
                    juce::String driveLetter = alert->getTextEditorContents("drive").trim();
                    if (driveLetter.isNotEmpty())
                    {
                        if (!driveLetter.endsWithChar(':'))
                            driveLetter += ":";

                        juce::String volumePath = "\\\\.\\" + driveLetter;

                        currentResult = FLPRecovery::ScanResult();
                        ntfsCandidates.clear();
                        flpSelected.clear();
                        ntfsSelected.clear();
                        resultsTable.updateContent();
                        logBox.clear();
                        logMessage("Scanning " + volumePath + " for deleted .flp files (NTFS)...");
                        ntfsScanner.startScan(volumePath, ".flp");
                    }
                    else
                    {
                        logMessage("No drive letter entered.");
                    }
                }
                delete alert;
            }
        ),
        true
    );
#else
    logMessage("NTFS $MFT recovery is only implemented for Windows in this build.");
#endif
}

void FLPRecoveryTool::imageDriveButtonClicked()
{
#if JUCE_WINDOWS
    auto* alert = new juce::AlertWindow(
        "Create Raw Image (DD)",
        "Enter what to image:\n"
        "  A volume, e.g.  C:\n"
        "  Or a whole physical drive, e.g.  PhysicalDrive0\n\n"
        "You'll be asked to choose a destination file next \u2014 pick a\n"
        "location on a DIFFERENT physical drive than the source.\n"
        "Requires Administrator privileges.",
        juce::AlertWindow::InfoIcon
    );
    alert->addTextEditor("source", "C:");
    alert->addButton("Next", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alert->enterModalState(true,
        juce::ModalCallbackFunction::create(
            [this, alert](int result)
            {
                juce::String sourceInput = alert->getTextEditorContents("source").trim();
                delete alert;

                if (result != 1 || sourceInput.isEmpty())
                {
                    logMessage("Imaging cancelled.");
                    return;
                }

                juce::String sourcePath = sourceInput.startsWith("\\\\.\\")
                    ? sourceInput
                    : ("\\\\.\\" + sourceInput);

                fileChooser = std::make_unique<juce::FileChooser>(
                    "Choose destination image file (must be on a different physical drive)",
                    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("image.dd"),
                    "*.dd;*.img;*.raw");

                fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                    [this, sourcePath](const juce::FileChooser& fc)
                    {
                        auto destFile = fc.getResult();
                        if (destFile == juce::File())
                        {
                            logMessage("Imaging cancelled — no destination chosen.");
                            return;
                        }

                        logBox.clear();
                        logMessage("Starting raw image: " + sourcePath + " -> " + destFile.getFullPathName());
                        diskImager.startImaging(sourcePath, destFile);
                    });
            }
        ),
        true
    );
#else
    logMessage("Raw imaging is only implemented for Windows in this build.");
#endif
}

void FLPRecoveryTool::checkPreviousVersionsButtonClicked()
{
#if JUCE_WINDOWS
    auto* alert = new juce::AlertWindow(
        "Check Previous Versions (Volume Shadow Copy)",
        "Enter the FULL original path of the file you're looking for, e.g.:\n"
        "  C:\\Users\\Name\\Documents\\Image-Line\\FL Studio\\Projects\\MySong.flp\n\n"
        "This checks Windows' shadow copy snapshots for an intact earlier\n"
        "version, completely separately from raw carving/NTFS scanning \u2014\n"
        "worth trying first, since a snapshot (if one exists) is typically\n"
        "a complete, unfragmented copy. Requires Administrator privileges.",
        juce::AlertWindow::InfoIcon
    );
    alert->addTextEditor("path", "");
    alert->addButton("Check", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alert->enterModalState(true,
        juce::ModalCallbackFunction::create(
            [this, alert](int result)
            {
                juce::String fullPath = alert->getTextEditorContents("path").trim();
                delete alert;

                if (result != 1 || fullPath.isEmpty())
                {
                    logMessage("Previous-versions check cancelled.");
                    return;
                }

                if (fullPath.length() < 3 || fullPath[1] != ':')
                {
                    logMessage("Please enter a full path starting with a drive letter, e.g. C:\\...");
                    return;
                }

                juce::String driveLetter = fullPath.substring(0, 2);
                juce::String relativePath = fullPath.substring(3); // past "C:\"

                if (!outputFolder.isDirectory())
                {
                    logMessage("Please select an output folder first using 'Select Output Folder'.");
                    return;
                }

                logMessage("Listing shadow copies for " + driveLetter + " ...");
                auto shadows = VssRecovery::listShadowCopies(driveLetter);

                if (shadows.empty())
                {
                    logMessage("No shadow copies found for " + driveLetter +
                        " (System Restore/File History may be off for this drive, or none exist yet).");
                    return;
                }

                logMessage("Found " + juce::String((int)shadows.size()) +
                    " shadow copy/copies — checking each for the file...");

                int foundCount = 0;
                for (size_t i = 0; i < shadows.size(); ++i)
                {
                    const auto& shadow = shadows[i];
                    juce::String errorMsg;
                    juce::String destName = juce::File(fullPath).getFileNameWithoutExtension() +
                        "_shadow" + juce::String((int)i) + "." + juce::File(fullPath).getFileExtension();
                    juce::File destFile = outputFolder.getChildFile(destName.replaceCharacters("\\/:?\"<>|", "________"));

                    if (VssRecovery::tryRecoverFromShadow(shadow, relativePath, destFile, errorMsg))
                    {
                        foundCount++;
                        logMessage("FOUND in shadow copy from " + shadow.creationTime +
                            " -> " + destFile.getFileName());
                        analyzeRecoveredFlpFile(destFile);
                    }
                    else
                    {
                        logMessage("Shadow copy from " + shadow.creationTime + ": " + errorMsg);
                    }
                }

                logMessage(foundCount > 0
                    ? ("Recovered " + juce::String(foundCount) + " previous version(s) — check the output folder.")
                    : juce::String("File not found in any shadow copy. It may never have been snapshotted, "
                        "or the snapshots covering when it existed have since been cycled out."));
            }
        ),
        true
    );
#else
    logMessage("Shadow copy recovery is only implemented for Windows in this build.");
#endif
}

void FLPRecoveryTool::toggleTrimButtonClicked()
{
    bool trimEnabled = false;
    juce::String rawOutput;

    if (!TrimControl::queryTrimEnabled(trimEnabled, rawOutput))
    {
        logMessage("Could not query TRIM status (requires administrator privileges).");
        return;
    }

    juce::String message = trimEnabled
        ? "TRIM is currently ENABLED system-wide.\n\n"
          "Disabling it may improve recovery odds for FUTURE accidental "
          "deletions on SSDs, at the cost of some SSD performance/lifespan "
          "while it's off. It will NOT recover anything already trimmed.\n\n"
          "Disable TRIM now?"
        : "TRIM is currently DISABLED system-wide.\n\n"
          "This is not meant to be left off permanently \u2014 it increases "
          "write amplification and reduces SSD lifespan over time.\n\n"
          "Re-enable TRIM now?";

    auto* alert = new juce::AlertWindow("TRIM Status", message, juce::AlertWindow::QuestionIcon);
    alert->addButton(trimEnabled ? "Disable TRIM" : "Re-enable TRIM", 1);
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alert->enterModalState(true,
        juce::ModalCallbackFunction::create(
            [this, alert, trimEnabled](int result)
            {
                delete alert;
                if (result != 1) return;

                juce::String setOutput;
                if (TrimControl::setTrimEnabled(!trimEnabled, setOutput))
                    logMessage(juce::String("TRIM ") + (trimEnabled ? "disabled." : "re-enabled.") +
                        " Note: this affects the whole system, not just one drive.");
                else
                    logMessage("Failed to change TRIM setting (requires administrator privileges).");
            }
        ),
        true
    );
}

void FLPRecoveryTool::stopButtonClicked()
{
    scanner.stopScanning();
    ntfsScanner.signalThreadShouldExit();
    diskImager.signalThreadShouldExit();
    recoveryThread.signalThreadShouldExit();
    logMessage("Stopping...");
    statusLabel.setText("Stopped", juce::dontSendNotification);
}

void FLPRecoveryTool::selectOutputFolderButtonClicked()
{
    fileChooser = std::make_unique<juce::FileChooser>("Select Output Folder for Recovered Files",
        outputFolder.isDirectory() ? outputFolder : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*");

    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
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
    // Only recover checked rows.
    FLPRecovery::ScanResult selectedFlpResult;
    for (size_t i = 0; i < currentResult.candidates.size(); ++i)
        if (i < flpSelected.size() && flpSelected[i])
            selectedFlpResult.candidates.push_back(currentResult.candidates[i]);

    std::vector<NtfsMftRecovery::DeletedFileCandidate> selectedNtfsCandidates;
    for (size_t i = 0; i < ntfsCandidates.size(); ++i)
        if (i < ntfsSelected.size() && ntfsSelected[i])
            selectedNtfsCandidates.push_back(ntfsCandidates[i]);

    if (selectedFlpResult.candidates.empty() && selectedNtfsCandidates.empty())
    {
        logMessage((currentResult.candidates.empty() && ntfsCandidates.empty())
            ? "No candidates to recover."
            : "No candidates are checked — tick the checkbox next to the files you want to recover.");
        return;
    }

    if (!outputFolder.isDirectory())
    {
        logMessage("Please select an output folder first using 'Select Output Folder'.");
        return;
    }

    // Recovery (including the FLPTOOL content analysis that now runs per
    // file) can take a while over a large batch — runs on a background
    // thread so the UI stays responsive instead of appearing to freeze.
    RecoveryThread::Inputs inputs;
    inputs.flpEngine = &scanner;
    inputs.ntfsEngine = &ntfsScanner.getEngine();
    inputs.flpResult = selectedFlpResult;
    inputs.ntfsCandidates = selectedNtfsCandidates;
    inputs.outputFolder = outputFolder;

    recoveryThread.startRecovery(std::move(inputs));
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

void FLPRecoveryTool::analyzeRecoveredFlpFile(const juce::File& flpFile)
{
    FL::Project::LoadReport report;
    auto project = FL::Project::loadPartial(flpFile, report);

    if (project == nullptr)
    {
        // Couldn't even get started (not FLP data at all, or unreadable) —
        // nothing more to say here.
        logMessage("  Content analysis: " + report.stopReason);
        return;
    }

    if (!report.fullyParsed)
        logMessage("  Structure: parsed " + juce::String(report.bytesParsed) + " of " +
            juce::String(report.totalDataSize) + " bytes (" + report.stopReason + ")");

    auto stats = FL::StatsGenerator::generate(*project);
    logMessage("  Content: " + juce::String(stats.totalChannels) + " channel(s), " +
        juce::String(stats.totalPatterns) + " pattern(s), " +
        juce::String(stats.totalNotes) + " note(s), " +
        juce::String(stats.totalPlaylistItems) + " playlist item(s)" +
        (stats.samplerChannels > 0 ? (", " + juce::String(stats.samplerChannels) + " sampler channel(s)") : juce::String()));

    // If any patterns parsed at all, a MIDI export is essentially free and
    // is real usable content even when the rest of the file didn't survive.
    if (stats.totalNotes > 0)
    {
        juce::File midiOut = flpFile.getSiblingFile(flpFile.getFileNameWithoutExtension() + ".mid");
        if (FL::MidiBridge::exportProjectToMidi(*project, midiOut))
            logMessage("  Exported recovered notes to: " + midiOut.getFileName());
    }

    // Resolved sample references, cross-checked against what's actually
    // still on disk — tells you which samples you'd still need to hunt
    // down even if the .flp itself is only partially recoverable.
    FL::SampleScanner sampleScanner(*project);
    auto sampleFiles = sampleScanner.scan();
    if (!sampleFiles.empty())
    {
        int missing = 0;
        for (auto& s : sampleFiles) if (!s.exists) missing++;
        logMessage("  Samples referenced: " + juce::String((int)sampleFiles.size()) +
            " (" + juce::String(missing) + " not found at their recorded path)");
    }
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
            ntfsCandidates.clear();
            flpSelected.clear();
            ntfsSelected.clear();
            resultsTable.updateContent();
            if (blockVisualizer != nullptr)
                blockVisualizer->updateBlocks(currentResult.allBlocks);
            logBox.clear();
            logMessage("Scanning dropped: " + file.getFileName());
            scanner.startScanImage(file);
        }
    }
}