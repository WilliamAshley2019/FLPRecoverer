#include "FLPRecovery.h"
#include <fstream>
#include <algorithm>
#include <iomanip>

// =============================================================================
// FLPRecovery Implementation - Enhanced
// =============================================================================

FLPRecovery::FLPRecovery() : m_passiveThread(nullptr) {}

FLPRecovery::~FLPRecovery()
{
    stopPassiveMode();
}

// ─── Helper: Read little-endian 32-bit ─────────────────────────────────────

uint32_t FLPRecovery::readU32LE(const uint8_t* data)
{
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

uint16_t FLPRecovery::readU16LE(const uint8_t* data)
{
    return (uint16_t)data[0] |
        ((uint16_t)data[1] << 8);
}

// ─── Internal Logging ──────────────────────────────────────────────────────

void FLPRecovery::log(const juce::String& msg)
{
    if (onLog) onLog(msg);
}

void FLPRecovery::logVerbose(const juce::String& msg)
{
    if (m_verbose && onLog) onLog("[VERBOSE] " + msg);
}

void FLPRecovery::sendProgress(float progress, const juce::String& status)
{
    if (onProgress) onProgress(progress, status);
}

// ─── Extract Version String ────────────────────────────────────────────────

juce::String FLPRecovery::extractVersion(const uint8_t* data, uint64_t offset)
{
    const char* versionStart = reinterpret_cast<const char*>(data + offset + 24);
    int maxLen = 64;
    juce::String result;

    for (int i = 0; i < maxLen; ++i)
    {
        if (versionStart[i] == '\0')
            break;
        if (versionStart[i] >= 32 && versionStart[i] <= 126)
            result += versionStart[i];
        else
            break;
    }

    return result;
}

// ─── Find FLhd Signatures ──────────────────────────────────────────────────

std::vector<FLPRecovery::Candidate> FLPRecovery::findFLhdSignatures(const uint8_t* data, size_t size)
{
    std::vector<Candidate> results;

    for (size_t i = 0; i < size - 4; ++i)
    {
        if (data[i] == 0x46 && data[i + 1] == 0x4C &&
            data[i + 2] == 0x68 && data[i + 3] == 0x64)
        {
            Candidate candidate;
            candidate.offset = i;
            candidate.isValid = false;
            candidate.isComplete = false;
            candidate.totalSize = 0;
            candidate.dataOffset = 0;
            candidate.dataSize = 0;
            candidate.fileSize = 0;
            candidate.chunksFound = 0;
            candidate.totalChunks = 0;

            results.push_back(candidate);
            logVerbose("Found FLhd at offset: 0x" + juce::String::toHexString((int64_t)i));
        }
    }

    return results;
}

// ─── Validate FLdt Chunk ──────────────────────────────────────────────────

bool FLPRecovery::validateFLdt(const uint8_t* data, Candidate& candidate)
{
    uint64_t offset = candidate.offset;
    uint64_t fldtPos = offset + HEADER_SIZE;

    if (memcmp(data + fldtPos, FLDT_MAGIC, 4) != 0)
    {
        candidate.error = "FLdt not found at expected position";
        return false;
    }

    uint32_t dataSize = readU32LE(data + fldtPos + 4);
    candidate.dataSize = dataSize;

    if (dataSize < 10)
    {
        candidate.error = "FLdt size too small: " + juce::String((int)dataSize);
        return false;
    }

    if (dataSize > m_maxFileSize)
    {
        candidate.error = "FLdt size exceeds maximum: " + juce::String((int)dataSize);
        return false;
    }

    uint64_t totalSize = HEADER_SIZE + FLDT_HEADER_SIZE + dataSize;
    candidate.totalSize = totalSize;
    candidate.dataOffset = fldtPos;

    if (totalSize > 100 * 1024 * 1024)
    {
        candidate.error = "Total size exceeds 100MB: " + juce::String((int)totalSize);
        return false;
    }

    candidate.version = extractVersion(data, offset);
    candidate.isValid = true;
    candidate.isComplete = true;

    logVerbose("Validated: size=" + juce::String((int)dataSize) +
        ", version=" + candidate.version);

    return true;
}

// ─── Validate Candidate ──────────────────────────────────────────────────

bool FLPRecovery::validateCandidate(Candidate& candidate, const uint8_t* data)
{
    if (!data || candidate.offset > 0xFFFFFFFF)
    {
        candidate.error = "Invalid data pointer or offset";
        candidate.isValid = false;
        return false;
    }

    if (memcmp(data + candidate.offset, FLHD_MAGIC, 4) != 0)
    {
        candidate.error = "FLhd magic not found";
        candidate.isValid = false;
        return false;
    }

    return validateFLdt(data, candidate);
}

// ─── Check Overlap ───────────────────────────────────────────────────────────

bool FLPRecovery::isOverlapping(const Candidate& a, const Candidate& b)
{
    if (!a.isValid || !b.isValid)
        return false;

    uint64_t aEnd = a.offset + a.totalSize;
    uint64_t bEnd = b.offset + b.totalSize;

    return !(aEnd <= b.offset || bEnd <= a.offset);
}

// ─── Scan Block for FLP Data ──────────────────────────────────────────────

bool FLPRecovery::scanBlockForFLP(const uint8_t* blockData, uint64_t blockOffset,
    uint64_t blockSize, BlockInfo& blockInfo)
{
    blockInfo.blockOffset = blockOffset;
    blockInfo.blockSize = blockSize;
    blockInfo.hasFLPData = false;
    blockInfo.isBackedUp = false;
    blockInfo.foundOffsets.clear();

    auto candidates = findFLhdSignatures(blockData, (size_t)blockSize);

    if (!candidates.empty())
    {
        blockInfo.hasFLPData = true;
        for (const auto& cand : candidates)
        {
            blockInfo.foundOffsets.push_back(blockOffset + cand.offset);
        }
        blockInfo.status = "Contains FLP data";
        logVerbose("FLP found in block at offset 0x" +
            juce::String::toHexString((int64_t)blockOffset));
        return true;
    }

    blockInfo.status = "Empty";
    return false;
}

// ─── Scan Drive Raw with Block Analysis ───────────────────────────────────

FLPRecovery::ScanResult FLPRecovery::scanDriveWithBlocks(const juce::String& drivePath,
    uint64_t blockSize,
    uint64_t startOffset,
    uint64_t scanSize)
{
    ScanResult result;
    m_blockSize = blockSize;

#if JUCE_WINDOWS
    juce::String path = drivePath;
    if (!drivePath.startsWith("\\\\.\\"))
        path = "\\\\.\\" + drivePath;
#else
    juce::String path = drivePath;
#endif

    juce::File driveFile(path);
    if (!driveFile.existsAsFile())
    {
        result.lastError = "Drive not found: " + drivePath;
        return result;
    }

    juce::FileInputStream stream(driveFile);
    if (!stream.openedOk())
    {
        result.lastError = "Could not open drive: " + drivePath +
            " (administrator/root privileges may be required)";
        return result;
    }

    juce::int64 fileSize = stream.getTotalLength();
    if (scanSize > 0 && scanSize < (uint64_t)fileSize)
        fileSize = (juce::int64)scanSize;

    if (startOffset > 0)
        stream.setPosition((juce::int64)startOffset);

    uint64_t totalBytesToScan = (uint64_t)fileSize - startOffset;
    uint64_t totalBlocks = totalBytesToScan / blockSize;
    result.totalBlocks = totalBlocks;
    result.bytesScanned = totalBytesToScan;

    log("Scanning drive with " + juce::String((int)totalBlocks) + " blocks of " +
        juce::String((int)blockSize) + " bytes");

    std::vector<uint8_t> blockBuffer(blockSize);
    uint64_t currentOffset = startOffset;
    uint64_t blocksProcessed = 0;

    while (currentOffset < (uint64_t)fileSize)
    {
        size_t bytesRead = stream.read(blockBuffer.data(), (size_t)blockSize);
        if (bytesRead == 0)
            break;

        BlockInfo blockInfo;
        blockInfo.blockIndex = blocksProcessed;

        if (scanBlockForFLP(blockBuffer.data(), currentOffset, bytesRead, blockInfo))
        {
            result.blocksWithFLP++;

            auto candidates = findFLhdSignatures(blockBuffer.data(), bytesRead);
            for (auto& candidate : candidates)
            {
                candidate.offset += currentOffset;
                if (validateFLdt(blockBuffer.data(), candidate))
                {
                    candidate.isComplete = true;
                    result.candidates.push_back(candidate);
                    result.validFiles++;
                    if (onCandidateFound) onCandidateFound(candidate);
                    log("Found complete FLP at offset 0x" +
                        juce::String::toHexString((int64_t)candidate.offset) +
                        " (" + candidate.version + ", " +
                        juce::String((int)(candidate.totalSize / 1024)) + " KB)");
                }
            }
        }

        result.allBlocks.push_back(blockInfo);
        if (onBlockScanned) onBlockScanned(blockInfo);

        blocksProcessed++;
        currentOffset += bytesRead;

        if (blocksProcessed % 100 == 0)
        {
            float progress = (float)currentOffset / (float)fileSize;
            sendProgress(progress, "Scanning block " + juce::String((int)blocksProcessed) +
                " of " + juce::String((int)totalBlocks));
        }
    }

    log("Scan complete. Found " + juce::String((int)result.validFiles) +
        " FLP files in " + juce::String((int)result.blocksWithFLP) + " blocks.");
    return result;
}

// ─── Scan Drive Raw ─────────────────────────────────────────────────────────

FLPRecovery::ScanResult FLPRecovery::scanDriveRaw(const juce::String& drivePath,
    uint64_t startOffset,
    uint64_t scanSize)
{
    return scanDriveWithBlocks(drivePath, m_blockSize, startOffset, scanSize);
}

// ─── Scan Image File ──────────────────────────────────────────────────────

FLPRecovery::ScanResult FLPRecovery::scanImage(const juce::File& imageFile)
{
    ScanResult result;

    if (!imageFile.existsAsFile())
    {
        result.lastError = "File not found: " + imageFile.getFullPathName();
        return result;
    }

    juce::FileInputStream stream(imageFile);
    if (!stream.openedOk())
    {
        result.lastError = "Could not open file: " + imageFile.getFullPathName();
        return result;
    }

    juce::int64 fileSize = stream.getTotalLength();
    result.bytesScanned = fileSize;

    const size_t chunkSize = 64 * 1024 * 1024;
    const size_t overlapSize = 8192;

    std::vector<uint8_t> buffer(chunkSize + overlapSize);
    std::vector<uint8_t> overlapBuffer(overlapSize);

    juce::int64 pos = 0;
    bool firstChunk = true;
    uint64_t totalCandidatesFound = 0;

    while (pos < fileSize)
    {
        size_t readSize = (size_t)std::min((juce::int64)chunkSize, fileSize - pos);

        if (!firstChunk)
        {
            memcpy(buffer.data(), overlapBuffer.data(), overlapSize);
            stream.setPosition(pos);
            size_t bytesRead = stream.read(buffer.data() + overlapSize, readSize);
            size_t totalData = overlapSize + bytesRead;

            auto candidates = findFLhdSignatures(buffer.data(), totalData);
            totalCandidatesFound += candidates.size();

            for (auto& candidate : candidates)
            {
                uint64_t actualOffset = pos - overlapSize + candidate.offset;
                candidate.offset = actualOffset;

                uint64_t fldtPos = candidate.offset + HEADER_SIZE;

                if (fldtPos + FLDT_HEADER_SIZE + 4 > (uint64_t)(pos + totalData))
                {
                    continue;
                }

                if (candidate.offset + 1024 > (uint64_t)(pos + totalData))
                    continue;

                candidate.isValid = true;
                candidate.totalSize = 0;
                result.candidates.push_back(candidate);
                result.validFiles++;
                if (onCandidateFound) onCandidateFound(candidate);
                log("Found FLhd candidate at offset 0x" +
                    juce::String::toHexString((int64_t)candidate.offset));
            }
        }
        else
        {
            stream.setPosition(pos);
            size_t bytesRead = stream.read(buffer.data(), readSize);

            auto candidates = findFLhdSignatures(buffer.data(), bytesRead);
            totalCandidatesFound += candidates.size();

            for (auto& candidate : candidates)
            {
                if (candidate.offset + 1024 > bytesRead)
                    continue;

                if (validateFLdt(buffer.data(), candidate))
                {
                    if (candidate.offset + candidate.totalSize <= bytesRead)
                    {
                        result.candidates.push_back(candidate);
                        result.validFiles++;
                        if (onCandidateFound) onCandidateFound(candidate);
                        log("Found valid FLP at offset 0x" +
                            juce::String::toHexString((int64_t)candidate.offset) +
                            " (" + candidate.version + ", " +
                            juce::String((int)(candidate.totalSize / 1024)) + " KB)");
                    }
                }
            }

            firstChunk = false;
        }

        size_t bytesToStore = std::min((size_t)overlapSize, (size_t)(pos + readSize));
        if (bytesToStore > 0)
        {
            stream.setPosition(pos + readSize - bytesToStore);
            stream.read(overlapBuffer.data(), bytesToStore);
        }

        pos += readSize;
        sendProgress((float)pos / fileSize, "Scanning... " + juce::String((int)(pos * 100 / fileSize)) + "%");
    }

    log("Scan complete. Found " + juce::String((int)result.validFiles) + " FLP candidates.");
    log("Note: Some candidates may need full file validation.");

    return result;
}

// ─── Scan Drive ────────────────────────────────────────────────────────────

FLPRecovery::ScanResult FLPRecovery::scanDrive(const juce::String& drivePath)
{
#if JUCE_WINDOWS
    juce::String path = drivePath;
    if (!drivePath.startsWith("\\\\.\\"))
        path = "\\\\.\\" + drivePath;
    juce::File driveFile(path);
    return scanImage(driveFile);
#else
    juce::File driveFile(drivePath);
    if (!driveFile.existsAsFile())
    {
        ScanResult result;
        result.lastError = "Drive not found: " + drivePath;
        return result;
    }
    return scanImage(driveFile);
#endif
}

// ─── Scan Directory ────────────────────────────────────────────────────────

FLPRecovery::ScanResult FLPRecovery::scanDirectory(const juce::File& directory)
{
    ScanResult result;

    if (!directory.isDirectory())
    {
        result.lastError = "Not a directory: " + directory.getFullPathName();
        return result;
    }

    juce::Array<juce::File> flpFiles;
    directory.findChildFiles(flpFiles, juce::File::findFiles, true, "*.flp");

    log("Found " + juce::String(flpFiles.size()) + " .flp files in " + directory.getFullPathName());

    for (int i = 0; i < flpFiles.size(); ++i)
    {
        const auto& file = flpFiles[i];

        juce::FileInputStream stream(file);
        if (!stream.openedOk()) continue;

        juce::MemoryBlock block;
        stream.readIntoMemoryBlock(block, 8192);

        const uint8_t* data = static_cast<const uint8_t*>(block.getData());

        if (block.getSize() >= 16 &&
            memcmp(data, FLHD_MAGIC, 4) == 0)
        {
            Candidate candidate;
            candidate.offset = 0;
            candidate.isValid = false;

            if (validateFLdt(data, candidate))
            {
                if (file.getSize() == (int64_t)candidate.totalSize ||
                    file.getSize() >= (int64_t)candidate.dataSize)
                {
                    candidate.version = extractVersion(data, 0);
                    candidate.isValid = true;
                    candidate.totalSize = file.getSize();
                    result.candidates.push_back(candidate);
                    result.validFiles++;
                    log("Valid FLP: " + file.getFileName() + " (" + candidate.version + ")");
                }
            }
        }
        else
        {
            log("Warning: " + file.getFileName() + " does not start with FLhd");
        }

        sendProgress((float)(i + 1) / flpFiles.size(),
            "Scanning: " + file.getFileName());
    }

    return result;
}

// ─── Recover Candidate ─────────────────────────────────────────────────────

bool FLPRecovery::recoverCandidate(const Candidate& candidate,
    const juce::File& outputFolder,
    const juce::String& suggestedName)
{
    if (!candidate.isValid)
    {
        log("Cannot recover invalid candidate at offset 0x" +
            juce::String::toHexString((int64_t)candidate.offset));
        return false;
    }

    if (!outputFolder.isDirectory())
    {
        if (!outputFolder.createDirectory())
        {
            log("Could not create output folder: " + outputFolder.getFullPathName());
            return false;
        }
    }

    juce::String filename = suggestedName;
    if (filename.isEmpty())
    {
        filename = "recovered_flp_" +
            juce::String::toHexString((int64_t)candidate.offset) +
            ".flp";
    }

    juce::File outputFile = outputFolder.getChildFile(filename);
    log("Recovering to: " + outputFile.getFullPathName());

    return true;
}

// ─── Recover All ───────────────────────────────────────────────────────────

int FLPRecovery::recoverAll(const ScanResult& result,
    const juce::File& outputFolder)
{
    int recovered = 0;

    if (!outputFolder.isDirectory())
    {
        outputFolder.createDirectory();
    }

    for (const auto& candidate : result.candidates)
    {
        juce::String filename = "recovered_flp_" +
            juce::String::toHexString((int64_t)candidate.offset) +
            "_" + candidate.version +
            ".flp";

        filename = filename.replaceCharacters("\\/:?\"<>|", "_");

        if (recoverCandidate(candidate, outputFolder, filename))
        {
            recovered++;
        }
    }

    log("Recovered " + juce::String(recovered) + " of " +
        juce::String(result.candidates.size()) + " files.");
    return recovered;
}

// ─── Recover Candidate with Blocks ────────────────────────────────────────

bool FLPRecovery::recoverCandidateWithBlocks(const Candidate& candidate,
    const juce::File& outputFolder,
    bool backupBlocks,
    const juce::String& suggestedName)
{
    if (!candidate.isValid)
    {
        log("Cannot recover invalid candidate");
        return false;
    }

    if (!outputFolder.isDirectory())
    {
        if (!outputFolder.createDirectory())
        {
            log("Could not create output folder");
            return false;
        }
    }

    juce::String filename = suggestedName;
    if (filename.isEmpty())
    {
        filename = "recovered_flp_" +
            juce::String::toHexString((int64_t)candidate.offset) +
            ".flp";
    }

    juce::File outputFile = outputFolder.getChildFile(filename);
    log("Recovering to: " + outputFile.getFullPathName());

    if (backupBlocks && !candidate.blocks.empty())
    {
        juce::File backupFolder = outputFolder.getChildFile("block_backups");
        backupFolder.createDirectory();

        for (const auto& block : candidate.blocks)
        {
            if (!block.isBackedUp)
            {
                juce::File blockFile = backupFolder.getChildFile(
                    "block_" + juce::String::toHexString((int64_t)block.blockOffset) + ".dat");
                log("Backing up block to: " + blockFile.getFullPathName());
            }
        }
    }

    return true;
}

// ─── Recover All with Blocks ──────────────────────────────────────────────

int FLPRecovery::recoverAllWithBlocks(const ScanResult& result,
    const juce::File& outputFolder,
    bool backupBlocks)
{
    int recovered = 0;

    if (!outputFolder.isDirectory())
    {
        outputFolder.createDirectory();
    }

    for (const auto& candidate : result.candidates)
    {
        juce::String filename = "recovered_flp_" +
            juce::String::toHexString((int64_t)candidate.offset) +
            "_" + candidate.version +
            ".flp";

        filename = filename.replaceCharacters("\\/:?\"<>|", "_");

        if (recoverCandidateWithBlocks(candidate, outputFolder, backupBlocks, filename))
        {
            recovered++;
        }
    }

    log("Recovered " + juce::String(recovered) + " of " +
        juce::String(result.candidates.size()) + " files.");
    return recovered;
}

// ─── Reconstruct File from Blocks ─────────────────────────────────────────

bool FLPRecovery::reconstructFileFromBlocks(Candidate& candidate,
    const std::vector<BlockInfo>& blocks)
{
    // This would reassemble the file from scattered blocks
    // Complex implementation would track fragment locations
    candidate.isComplete = false;
    return false;
}

// ─── Backup Block ─────────────────────────────────────────────────────────

bool FLPRecovery::backupBlock(const BlockInfo& block, const juce::File& backupFolder)
{
    // Implementation would read the block from the drive and save it
    return true;
}

// =============================================================================
// Passive Scanner Thread Implementation
// =============================================================================

FLPRecovery::PassiveScannerThread::PassiveScannerThread(FLPRecovery& owner)
    : juce::Thread("PassiveFLPScanner"), m_owner(owner) {}

FLPRecovery::PassiveScannerThread::~PassiveScannerThread()
{
    stopScanning();
}

void FLPRecovery::PassiveScannerThread::run()
{
    juce::File driveFile(m_drivePath);
    if (!driveFile.existsAsFile())
    {
        m_owner.log("Drive not found: " + m_drivePath);
        return;
    }

    juce::FileInputStream stream(driveFile);
    if (!stream.openedOk())
    {
        m_owner.log("Could not open drive for passive monitoring");
        return;
    }

    m_currentOffset = 0;
    std::vector<uint8_t> blockBuffer(m_blockSize);

    m_owner.log("Starting passive monitoring on: " + m_drivePath);

    while (!m_shouldStop)
    {
        stream.setPosition((juce::int64)m_currentOffset);
        size_t bytesRead = stream.read(blockBuffer.data(), (size_t)m_blockSize);

        if (bytesRead > 0)
        {
            BlockInfo blockInfo;
            if (m_owner.scanBlockForFLP(blockBuffer.data(), m_currentOffset, bytesRead, blockInfo))
            {
                juce::String status = "FLP detected in block 0x" +
                    juce::String::toHexString((int64_t)m_currentOffset);
                m_owner.log(status);

                if (m_owner.onBlockScanned)
                    m_owner.onBlockScanned(blockInfo);

                auto candidates = m_owner.findFLhdSignatures(blockBuffer.data(), bytesRead);
                for (auto& candidate : candidates)
                {
                    candidate.offset += m_currentOffset;
                    if (m_owner.validateFLdt(blockBuffer.data(), candidate))
                    {
                        if (m_owner.onCandidateFound)
                            m_owner.onCandidateFound(candidate);
                        m_owner.log("Found FLP in passive scan at offset 0x" +
                            juce::String::toHexString((int64_t)candidate.offset));
                    }
                }
            }

            m_currentOffset += bytesRead;
        }
        else
        {
            juce::Thread::sleep(m_updateIntervalMs);
        }

        juce::Thread::sleep(m_updateIntervalMs);
    }
}

void FLPRecovery::PassiveScannerThread::stopScanning()
{
    m_shouldStop = true;
}

// ─── Passive Mode Control ─────────────────────────────────────────────────

void FLPRecovery::startPassiveMode(const juce::String& drivePath,
    uint64_t blockSize,
    int updateIntervalMs)
{
    stopPassiveMode();

    m_passiveThread = std::make_unique<PassiveScannerThread>(*this);
    m_passiveThread->setDrivePath(drivePath);
    m_passiveThread->setBlockSize(blockSize);
    m_passiveThread->setUpdateInterval(updateIntervalMs);
    m_passiveThread->startThread();

    log("Passive mode started on: " + drivePath);
}

void FLPRecovery::stopPassiveMode()
{
    if (m_passiveThread)
    {
        m_passiveThread->stopScanning();
        m_passiveThread->waitForThreadToExit(5000);
        m_passiveThread.reset();
        log("Passive mode stopped");
    }
}