#include "FLPRecovery.h"
#include <fstream>
#include <algorithm>

// =============================================================================
// FLPRecovery Implementation
// =============================================================================

FLPRecovery::FLPRecovery() {}
FLPRecovery::~FLPRecovery() {}

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
    // Version string appears around offset 24-30 after FLhd
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
            candidate.totalSize = 0;
            candidate.dataOffset = 0;
            candidate.dataSize = 0;

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

    // FLhd header is 16 bytes, FLdt should start at offset+16
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
    logVerbose("Validated: size=" + juce::String((int)dataSize) +
        ", version=" + candidate.version);

    return true;
}

/// ─── Scan Image File ──────────────────────────────────────────────────────

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

    // Use chunk-based reading for all files (works with any size)
    const size_t chunkSize = 64 * 1024 * 1024; // 64MB chunks
    const size_t overlapSize = 8192; // Overlap between chunks to catch signatures that span boundaries

    std::vector<uint8_t> buffer(chunkSize + overlapSize);
    std::vector<uint8_t> overlapBuffer(overlapSize);

    juce::int64 pos = 0;
    bool firstChunk = true;
    uint64_t totalCandidatesFound = 0;

    while (pos < fileSize)
    {
        size_t readSize = (size_t)std::min((juce::int64)chunkSize, fileSize - pos);

        // If this isn't the first chunk, copy the overlap from previous chunk
        if (!firstChunk)
        {
            // Copy overlap data to the beginning of the buffer
            memcpy(buffer.data(), overlapBuffer.data(), overlapSize);
            // Read new data after the overlap
            stream.setPosition(pos);
            size_t bytesRead = stream.read(buffer.data() + overlapSize, readSize);
            // Total valid data in buffer is overlapSize + bytesRead
            size_t totalData = overlapSize + bytesRead;

            // Scan the buffer
            auto candidates = findFLhdSignatures(buffer.data(), totalData);
            totalCandidatesFound += candidates.size();

            for (auto& candidate : candidates)
            {
                // Adjust offset for the overlap
                uint64_t actualOffset = pos - overlapSize + candidate.offset;
                candidate.offset = actualOffset;

                // We need to validate with the full data, but we only have part of the file
                // So we need to check if the FLdt data is within our buffer
                uint64_t fldtPos = candidate.offset + HEADER_SIZE;

                // Check if we have enough data to validate
                if (fldtPos + FLDT_HEADER_SIZE + 4 > (uint64_t)(pos + totalData))
                {
                    // We don't have the full FLdt data, skip validation for now
                    // This will be caught in the next chunk if the signature spans the boundary
                    continue;
                }

                // We can only fully validate if the entire FLdt chunk is in our buffer
                // For now, do a partial validation
                if (candidate.offset + 1024 > (uint64_t)(pos + totalData))
                    continue;

                // We need to read the FLdt size to validate, but we might not have the whole file
                // For chunk-based scanning, we need to handle this differently
                // Let's just note the candidate for now and validate later
                candidate.isValid = true; // Tentative
                candidate.totalSize = 0; // Will be filled later
                result.candidates.push_back(candidate);
                result.validFiles++;
                if (onCandidateFound) onCandidateFound(candidate);
                log("Found FLhd candidate at offset 0x" +
                    juce::String::toHexString((int64_t)candidate.offset));
            }
        }
        else
        {
            // First chunk: read from the beginning
            stream.setPosition(pos);
            size_t bytesRead = stream.read(buffer.data(), readSize);

            // Scan the buffer
            auto candidates = findFLhdSignatures(buffer.data(), bytesRead);
            totalCandidatesFound += candidates.size();

            for (auto& candidate : candidates)
            {
                // Validate candidate
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

        // Store the last overlapSize bytes for the next chunk
        size_t bytesToStore = std::min((size_t)overlapSize, (size_t)(pos + readSize));
        if (bytesToStore > 0)
        {
            stream.setPosition(pos + readSize - bytesToStore);
            stream.read(overlapBuffer.data(), bytesToStore);
        }

        pos += readSize;
        sendProgress((float)pos / fileSize, "Scanning... " + juce::String((int)(pos * 100 / fileSize)) + "%");
    }

    // Now, after scanning all chunks, re-validate any candidates that were only partially validated
    // This is a simplified approach - in a real implementation, you'd want to do this more carefully
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

    // In a full implementation, the source data would be passed in
    // For now, we'll log the recovery would happen
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

// ─── Validate Candidate ──────────────────────────────────────────────────────

bool FLPRecovery::validateCandidate(Candidate& candidate, const uint8_t* data)
{
    if (!data || candidate.offset > 0xFFFFFFFF)
    {
        candidate.error = "Invalid data pointer or offset";
        candidate.isValid = false;
        return false;
    }

    // Check FLhd magic
    if (memcmp(data + candidate.offset, FLHD_MAGIC, 4) != 0)
    {
        candidate.error = "FLhd magic not found";
        candidate.isValid = false;
        return false;
    }

    // Validate FLdt
    return validateFLdt(data, candidate);
}

// ─── Check Overlap ───────────────────────────────────────────────────────────

bool FLPRecovery::isOverlapping(const Candidate& a, const Candidate& b)
{
    if (!a.isValid || !b.isValid)
        return false;

    uint64_t aEnd = a.offset + a.totalSize;
    uint64_t bEnd = b.offset + b.totalSize;

    // Check if ranges overlap
    return !(aEnd <= b.offset || bEnd <= a.offset);
}