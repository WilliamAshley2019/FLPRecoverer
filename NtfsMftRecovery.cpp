#include "NtfsMftRecovery.h"

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace
{
    uint16_t readU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
    uint32_t readU32(const uint8_t* p) { return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)); }
    uint64_t readU64(const uint8_t* p)
    {
        uint64_t lo = readU32(p);
        uint64_t hi = readU32(p + 4);
        return lo | (hi << 32);
    }
    // Sign-extends a little-endian value of `numBytes` bytes (1-8) read from p.
    int64_t readSignedLE(const uint8_t* p, int numBytes)
    {
        if (numBytes <= 0) return 0;
        uint64_t value = 0;
        for (int i = 0; i < numBytes; ++i)
            value |= (uint64_t)p[i] << (8 * i);

        // Sign-extend if the top bit of the value's highest byte is set.
        if (numBytes < 8 && (p[numBytes - 1] & 0x80) != 0)
        {
            uint64_t signExtension = ~((uint64_t)0) << (8 * numBytes);
            value |= signExtension;
        }
        return (int64_t)value;
    }
    uint64_t readUnsignedLE(const uint8_t* p, int numBytes)
    {
        uint64_t value = 0;
        for (int i = 0; i < numBytes && i < 8; ++i)
            value |= (uint64_t)p[i] << (8 * i);
        return value;
    }
}

NtfsMftRecovery::NtfsMftRecovery() {}

NtfsMftRecovery::~NtfsMftRecovery()
{
    closeVolume();
}

void NtfsMftRecovery::closeVolume()
{
#if JUCE_WINDOWS
    if (volumeHandle != nullptr && volumeHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle((HANDLE)volumeHandle);
        volumeHandle = nullptr;
    }
#endif
}

bool NtfsMftRecovery::openVolume(const juce::String& volumePath)
{
#if JUCE_WINDOWS
    closeVolume();
    volumePathStored = volumePath;

    // Deliberately NOT using FILE_FLAG_NO_BUFFERING here — this lets us do
    // arbitrary-offset/arbitrary-size reads without hand-rolling sector
    // alignment, at the cost of going through the OS cache. Fine for a
    // scanning tool; a future perf pass could switch to unbuffered + aligned
    // reads if needed.
    HANDLE h = CreateFileA(
        volumePath.toRawUTF8(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (h == INVALID_HANDLE_VALUE)
    {
        lastError = "Could not open volume " + volumePath +
            " (administrator privileges are required to read raw volumes). Error: " +
            juce::String((int)GetLastError());
        return false;
    }

    volumeHandle = (void*)h;

    if (!readBootSector())
        return false;

    if (!readMftsOwnRunlist())
        return false;

    return true;
#else
    juce::ignoreUnused(volumePath);
    lastError = "NTFS $MFT recovery is only implemented for Windows in this build.";
    return false;
#endif
}

bool NtfsMftRecovery::readVolumeBytes(uint64_t byteOffset, uint64_t size, std::vector<uint8_t>& outBuffer)
{
#if JUCE_WINDOWS
    if (volumeHandle == nullptr)
    {
        lastError = "Volume not open.";
        return false;
    }

    LARGE_INTEGER pos;
    pos.QuadPart = (LONGLONG)byteOffset;
    if (!SetFilePointerEx((HANDLE)volumeHandle, pos, NULL, FILE_BEGIN))
    {
        lastError = "Failed to seek volume at offset " + juce::String((int64_t)byteOffset);
        return false;
    }

    outBuffer.resize((size_t)size);
    DWORD bytesRead = 0;
    if (!ReadFile((HANDLE)volumeHandle, outBuffer.data(), (DWORD)size, &bytesRead, NULL))
    {
        lastError = "Failed to read volume at offset " + juce::String((int64_t)byteOffset);
        return false;
    }

    if (bytesRead < size)
        outBuffer.resize(bytesRead);

    return bytesRead > 0;
#else
    juce::ignoreUnused(byteOffset, size, outBuffer);
    return false;
#endif
}

bool NtfsMftRecovery::readBootSector()
{
    std::vector<uint8_t> sector;
    if (!readVolumeBytes(0, 512, sector) || sector.size() < 512)
    {
        lastError = "Could not read boot sector.";
        return false;
    }

    if (memcmp(sector.data() + 3, "NTFS    ", 8) != 0)
    {
        lastError = "Not an NTFS volume (OEM ID mismatch).";
        return false;
    }

    bytesPerSector = readU16(sector.data() + 0x0B);
    sectorsPerCluster = sector[0x0D];
    mftStartLcn = readU64(sector.data() + 0x30);
    rawClustersPerMftRecord = (int8_t)sector[0x40];

    if (bytesPerSector == 0 || sectorsPerCluster == 0)
    {
        lastError = "Invalid boot sector geometry.";
        return false;
    }

    uint32_t clusterSize = (uint32_t)bytesPerSector * sectorsPerCluster;

    if (rawClustersPerMftRecord >= 0)
        mftRecordSize = (uint32_t)rawClustersPerMftRecord * clusterSize;
    else
        mftRecordSize = 1u << (uint32_t)(-rawClustersPerMftRecord);

    if (mftRecordSize == 0 || mftRecordSize > 65536)
    {
        lastError = "Unreasonable MFT record size decoded from boot sector.";
        return false;
    }

    return true;
}

bool NtfsMftRecovery::readMftsOwnRunlist()
{
    // The $MFT's own first record (record #0) is guaranteed to start
    // exactly at mftStartLcn — that's the one thing about $MFT's layout
    // that isn't itself described by a run list, since you need this
    // record to find the run list in the first place.
    uint32_t clusterSize = getBytesPerCluster();
    uint64_t byteOffset = mftStartLcn * clusterSize;

    std::vector<uint8_t> record;
    if (!readVolumeBytes(byteOffset, mftRecordSize, record) || record.size() < mftRecordSize)
    {
        lastError = "Could not read $MFT's own record 0.";
        return false;
    }

    if (!applyFixups(record))
    {
        lastError = "Fixup validation failed on $MFT record 0 (corrupt or misread).";
        return false;
    }

    if (memcmp(record.data(), "FILE", 4) != 0)
    {
        lastError = "$MFT record 0 does not have a valid FILE signature.";
        return false;
    }

    uint16_t attrOffset = readU16(record.data() + 0x14);
    uint32_t usedSize = readU32(record.data() + 0x18);

    size_t pos = attrOffset;
    bool foundData = false;

    while (pos + 4 <= usedSize && pos + 4 <= record.size())
    {
        const uint8_t* attr = record.data() + pos;
        uint32_t type = readU32(attr);
        if (type == 0xFFFFFFFF) break;

        uint32_t attrLen = readU32(attr + 4);
        if (attrLen == 0 || pos + attrLen > record.size()) break;

        if (type == 0x80 && attr[9] == 0) // unnamed $DATA
        {
            bool nonResident = attr[8] != 0;
            if (nonResident)
            {
                uint16_t runsOffset = readU16(attr + 0x20);
                mftDataRuns = parseDataRuns(attr + runsOffset, attrLen - runsOffset);
                foundData = true;
            }
            // (A resident $DATA on $MFT itself would be pathological — the
            // MFT is always large — so we don't handle that case.)
            break;
        }

        pos += attrLen;
    }

    if (!foundData || mftDataRuns.empty())
    {
        lastError = "Could not locate $MFT's own $DATA run list.";
        return false;
    }

    uint64_t totalMftClusters = 0;
    for (auto& run : mftDataRuns)
        totalMftClusters += run.clusterCount;

    totalMftRecords = (totalMftClusters * clusterSize) / mftRecordSize;
    return true;
}

bool NtfsMftRecovery::applyFixups(std::vector<uint8_t>& record) const
{
    if (record.size() < 8) return false;

    uint16_t usaOffset = readU16(record.data() + 0x04);
    uint16_t usaCount = readU16(record.data() + 0x06);

    if (usaCount == 0) return true; // nothing to fix up
    if ((size_t)usaOffset + (size_t)usaCount * 2 > record.size()) return false;

    const uint8_t* usa = record.data() + usaOffset;
    uint16_t updateSequenceNumber = readU16(usa);

    // usaCount includes the USN itself plus one entry per sector.
    int numSectors = usaCount - 1;
    if ((size_t)numSectors * bytesPerSector > record.size()) return false;

    for (int i = 0; i < numSectors; ++i)
    {
        size_t sectorLastWordOffset = (size_t)(i + 1) * bytesPerSector - 2;
        if (sectorLastWordOffset + 2 > record.size()) return false;

        uint16_t storedAtSectorEnd = readU16(record.data() + sectorLastWordOffset);
        if (storedAtSectorEnd != updateSequenceNumber)
        {
            // Fixup mismatch usually means a torn/corrupt record — bail
            // rather than silently parsing garbage.
            return false;
        }

        uint16_t realValue = readU16(usa + 2 + i * 2);
        record[sectorLastWordOffset] = (uint8_t)(realValue & 0xFF);
        record[sectorLastWordOffset + 1] = (uint8_t)((realValue >> 8) & 0xFF);
    }

    return true;
}

std::vector<NtfsMftRecovery::ClusterRun> NtfsMftRecovery::parseDataRuns(const uint8_t* data, size_t maxSize)
{
    std::vector<ClusterRun> runs;
    size_t pos = 0;
    int64_t currentLcn = 0;

    while (pos < maxSize)
    {
        uint8_t header = data[pos];
        if (header == 0) break; // end of run list
        pos++;

        int lengthBytes = header & 0x0F;
        int offsetBytes = (header >> 4) & 0x0F;

        if (pos + lengthBytes + offsetBytes > maxSize) break; // malformed, bail safely

        uint64_t runLength = readUnsignedLE(data + pos, lengthBytes);
        pos += lengthBytes;

        ClusterRun run;
        run.clusterCount = runLength;

        if (offsetBytes == 0)
        {
            // Sparse run: no physical clusters, "offset" is omitted entirely.
            run.isSparse = true;
            run.startLcn = 0;
        }
        else
        {
            int64_t delta = readSignedLE(data + pos, offsetBytes);
            pos += offsetBytes;
            currentLcn += delta;
            run.isSparse = false;
            run.startLcn = (uint64_t)currentLcn;
        }

        runs.push_back(run);
    }

    return runs;
}

bool NtfsMftRecovery::readMftRecord(uint64_t recordIndex, std::vector<uint8_t>& outRecord)
{
    uint32_t clusterSize = getBytesPerCluster();
    uint64_t recordByteOffsetInMft = recordIndex * mftRecordSize;
    uint64_t clusterIndexInMft = recordByteOffsetInMft / clusterSize;
    uint64_t offsetWithinCluster = recordByteOffsetInMft % clusterSize;

    // Walk the $MFT's own run list to translate this logical cluster
    // position into a physical LCN.
    uint64_t clustersWalked = 0;
    for (auto& run : mftDataRuns)
    {
        if (clusterIndexInMft < clustersWalked + run.clusterCount)
        {
            if (run.isSparse)
            {
                lastError = "MFT record " + juce::String((int64_t)recordIndex) + " falls in a sparse region.";
                return false;
            }

            uint64_t clusterOffsetInRun = clusterIndexInMft - clustersWalked;
            uint64_t physicalLcn = run.startLcn + clusterOffsetInRun;
            uint64_t physicalByteOffset = physicalLcn * clusterSize + offsetWithinCluster;

            // Common case: mftRecordSize <= clusterSize, so the record
            // can't straddle a run boundary. If it somehow needs more
            // clusters than remain in this run, bail rather than misread.
            uint64_t clustersNeeded = (offsetWithinCluster + mftRecordSize + clusterSize - 1) / clusterSize;
            if (clusterOffsetInRun + clustersNeeded > run.clusterCount)
            {
                lastError = "MFT record " + juce::String((int64_t)recordIndex) +
                    " straddles a fragmented $MFT run boundary (not supported yet).";
                return false;
            }

            return readVolumeBytes(physicalByteOffset, mftRecordSize, outRecord) &&
                outRecord.size() == mftRecordSize;
        }

        clustersWalked += run.clusterCount;
    }

    lastError = "MFT record " + juce::String((int64_t)recordIndex) + " is beyond the $MFT's own extents.";
    return false;
}

bool NtfsMftRecovery::parseFileRecord(const std::vector<uint8_t>& record, uint64_t recordIndex,
    const juce::String& nameFilterLowercase, DeletedFileCandidate& outCandidate, bool& outIsMatch)
{
    outIsMatch = false;

    if (record.size() < 0x30 || memcmp(record.data(), "FILE", 4) != 0)
        return false;

    uint16_t flags = readU16(record.data() + 0x16);
    bool inUse = (flags & 0x0001) != 0;
    bool isDirectory = (flags & 0x0002) != 0;

    if (inUse || isDirectory)
        return true; // parsed fine, just not a candidate — not an error

    uint16_t attrOffset = readU16(record.data() + 0x14);
    uint32_t usedSize = readU32(record.data() + 0x18);
    if (usedSize > record.size()) usedSize = (uint32_t)record.size();

    juce::String bestName;
    int bestNamespacePriority = -1; // higher = better; prefer Win32/Win32&DOS over plain DOS
    bool foundData = false;

    size_t pos = attrOffset;
    while (pos + 8 <= usedSize)
    {
        const uint8_t* attr = record.data() + pos;
        uint32_t type = readU32(attr);
        if (type == 0xFFFFFFFF) break;

        uint32_t attrLen = readU32(attr + 4);
        if (attrLen < 8 || pos + attrLen > record.size()) break;

        bool nonResident = attr[8] != 0;
        uint8_t nameLen = attr[9];

        if (type == 0x30 && !nonResident) // $FILE_NAME, always resident
        {
            uint16_t contentOffset = readU16(attr + 0x14);
            const uint8_t* content = attr + contentOffset;
            if (contentOffset + 0x42 <= attrLen)
            {
                uint8_t fnLen = content[0x40];
                uint8_t ns = content[0x41];
                size_t nameBytes = (size_t)fnLen * 2;
                if (contentOffset + 0x42 + nameBytes <= attrLen)
                {
                    juce::String name = juce::String(juce::CharPointer_UTF16(
                        (const juce::CharPointer_UTF16::CharType*)(content + 0x42)), fnLen);

                    // Namespace: 0=POSIX, 1=Win32, 2=DOS(8.3), 3=Win32&DOS.
                    // Prefer 1 or 3 (real long names) over 2 (short-name-only).
                    int priority = (ns == 2) ? 0 : 1;
                    if (priority > bestNamespacePriority)
                    {
                        bestNamespacePriority = priority;
                        bestName = name;
                    }
                }
            }
        }
        else if (type == 0x80 && nameLen == 0) // unnamed $DATA
        {
            if (!nonResident)
            {
                uint32_t contentLen = readU32(attr + 0x10);
                uint16_t contentOffset = readU16(attr + 0x14);
                if ((size_t)contentOffset + contentLen <= attrLen)
                {
                    outCandidate.wasResident = true;
                    outCandidate.residentData.assign(attr + contentOffset, attr + contentOffset + contentLen);
                    outCandidate.realSize = contentLen;
                    outCandidate.allocatedSize = contentLen;
                    foundData = true;
                }
            }
            else
            {
                outCandidate.allocatedSize = readU64(attr + 0x28);
                outCandidate.realSize = readU64(attr + 0x30);
                uint16_t runsOffset = readU16(attr + 0x20);
                if (runsOffset < attrLen)
                {
                    outCandidate.dataRuns = parseDataRuns(attr + runsOffset, attrLen - runsOffset);
                    outCandidate.wasResident = false;
                    foundData = true;
                }
            }
        }

        pos += attrLen;
    }

    if (foundData && bestName.isNotEmpty() &&
        bestName.toLowerCase().endsWith(nameFilterLowercase.toLowerCase()))
    {
        outCandidate.fileName = bestName;
        outCandidate.mftRecordNumber = recordIndex;
        outCandidate.fragmentCount = (int)outCandidate.dataRuns.size();
        outIsMatch = true;
    }

    return true;
}

std::vector<NtfsMftRecovery::DeletedFileCandidate> NtfsMftRecovery::scanForDeletedFiles(
    const juce::String& nameFilterLowercase,
    std::function<void(float, const juce::String&)> onProgress,
    std::function<bool()> shouldStop)
{
    std::vector<DeletedFileCandidate> results;

    if (totalMftRecords == 0)
    {
        lastError = "Volume not open or $MFT not parsed.";
        return results;
    }

    for (uint64_t i = 0; i < totalMftRecords; ++i)
    {
        if (shouldStop && shouldStop())
            break;

        std::vector<uint8_t> raw;
        if (!readMftRecord(i, raw))
            continue; // skip unreadable records rather than aborting the whole scan

        if (!applyFixups(raw))
            continue; // corrupt/torn record, skip

        DeletedFileCandidate candidate;
        bool isMatch = false;
        if (parseFileRecord(raw, i, nameFilterLowercase, candidate, isMatch) && isMatch)
            results.push_back(candidate);

        if (onProgress && (i % 500 == 0))
            onProgress((float)i / (float)totalMftRecords,
                "Scanning $MFT record " + juce::String((int64_t)i) + " of " + juce::String((int64_t)totalMftRecords));
    }

    return results;
}

bool NtfsMftRecovery::reconstructFile(const DeletedFileCandidate& candidate, const juce::File& outputFile)
{
    if (candidate.wasResident)
    {
        juce::FileOutputStream out(outputFile);
        if (!out.openedOk())
        {
            lastError = "Could not create output file: " + outputFile.getFullPathName();
            return false;
        }
        out.write(candidate.residentData.data(), candidate.residentData.size());
        out.flush();
        return true;
    }

    if (candidate.dataRuns.empty())
    {
        lastError = "No data runs to reconstruct from.";
        return false;
    }

    juce::FileOutputStream out(outputFile);
    if (!out.openedOk())
    {
        lastError = "Could not create output file: " + outputFile.getFullPathName();
        return false;
    }

    uint32_t clusterSize = getBytesPerCluster();
    uint64_t bytesWritten = 0;
    uint64_t targetSize = candidate.realSize;

    for (const auto& run : candidate.dataRuns)
    {
        if (bytesWritten >= targetSize) break;

        uint64_t runBytes = run.clusterCount * clusterSize;
        uint64_t bytesToUseFromRun = juce::jmin(runBytes, targetSize - bytesWritten);

        if (run.isSparse)
        {
            // Logically all zeros — write zero-filled bytes to preserve
            // correct file layout rather than skipping ahead.
            std::vector<uint8_t> zeros((size_t)bytesToUseFromRun, 0);
            out.write(zeros.data(), zeros.size());
        }
        else
        {
            uint64_t physicalByteOffset = run.startLcn * clusterSize;
            std::vector<uint8_t> buffer;
            if (!readVolumeBytes(physicalByteOffset, bytesToUseFromRun, buffer))
            {
                lastError = "Failed reading fragment at LCN " + juce::String((int64_t)run.startLcn) +
                    " while reconstructing " + candidate.fileName;
                return false;
            }
            out.write(buffer.data(), buffer.size());
        }

        bytesWritten += bytesToUseFromRun;
    }

    out.flush();

    if (bytesWritten < targetSize)
    {
        lastError = "Reconstructed " + candidate.fileName + " short by " +
            juce::String((int64_t)(targetSize - bytesWritten)) + " bytes — some fragments may be unreadable.";
        return false;
    }

    return true;
}
