#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cstdint>

// =============================================================================
// NtfsMftRecovery
//
// Unlike FLPRecovery's raw byte-carving (which assumes a recovered file is
// one contiguous run of bytes), this reads NTFS's own bookkeeping — the
// Master File Table — to find a deleted file's *exact* cluster allocation,
// fragmented or not, and reconstructs it from the real map instead of
// guessing where fragments are.
//
// Trade-off vs. carving: this only works as long as the deleted file's MFT
// record hasn't itself been reused for a new file yet. Carving still finds
// things after that point (if the underlying bytes survive); this finds
// fragmented files carving can't reassemble, as long as its own metadata
// survives.
//
// Scope/limitations of this first pass:
//   - NTFS only (exFAT/FAT32/APFS/HFS+ are future work — see project notes)
//   - Assumes standard NTFS 3.x on-disk layout (stable since Windows XP)
//   - Does not yet handle $ATTRIBUTE_LIST (a $DATA run list so long it
//     spills into extra MFT records) — affects only very heavily
//     fragmented files, rare for typical .flp sizes
//   - Requires the process to be elevated (raw volume access, same
//     requirement as physical-drive scanning in FLPRecovery)
// =============================================================================
class NtfsMftRecovery
{
public:
    // One contiguous run of clusters on disk. A file's data may be made up
    // of several of these, in order.
    struct ClusterRun
    {
        uint64_t startLcn = 0;      // Logical Cluster Number where this run starts
        uint64_t clusterCount = 0;  // length of this run, in clusters
        bool isSparse = false;      // true = "hole", no physical clusters (rare for .flp)
    };

    struct DeletedFileCandidate
    {
        juce::String fileName;
        uint64_t mftRecordNumber = 0;
        uint64_t realSize = 0;          // actual file size in bytes
        uint64_t allocatedSize = 0;     // size rounded up to cluster boundary
        bool wasResident = false;       // true = tiny file, data stored inside the MFT record itself
        std::vector<uint8_t> residentData; // valid only if wasResident
        std::vector<ClusterRun> dataRuns;  // valid only if !wasResident
        int fragmentCount = 0;
    };

    NtfsMftRecovery();
    ~NtfsMftRecovery();

    // volumePath e.g. "\\\\.\\C:" — requires administrator privileges.
    bool openVolume(const juce::String& volumePath);
    void closeVolume();

    uint32_t getBytesPerCluster() const { return bytesPerSector * sectorsPerCluster; }
    juce::String getLastError() const { return lastError; }

    // Scans every MFT record for deleted entries whose filename matches
    // nameFilterLowercase (e.g. ".flp"). Calls onProgress periodically.
    std::vector<DeletedFileCandidate> scanForDeletedFiles(
        const juce::String& nameFilterLowercase,
        std::function<void(float progress, const juce::String& status)> onProgress = nullptr,
        std::function<bool()> shouldStop = nullptr);

    // Reads each cluster run in order directly off the volume and
    // concatenates them into outputFile — the actual fragment-stitching step.
    bool reconstructFile(const DeletedFileCandidate& candidate, const juce::File& outputFile);

private:
    void* volumeHandle = nullptr; // HANDLE, void* to keep this header includable cross-platform
    juce::String volumePathStored;
    juce::String lastError;

    uint16_t bytesPerSector = 512;
    uint8_t  sectorsPerCluster = 8;
    uint64_t mftStartLcn = 0;
    int32_t  rawClustersPerMftRecord = 0;
    uint32_t mftRecordSize = 1024;
    uint64_t totalMftRecords = 0;

    std::vector<ClusterRun> mftDataRuns; // the $MFT's own (possibly fragmented) layout

    bool readBootSector();
    bool readMftsOwnRunlist();

    // Reads absolute bytes from the open volume at a byte offset. Handles
    // the sector alignment raw volume handles require.
    bool readVolumeBytes(uint64_t byteOffset, uint64_t size, std::vector<uint8_t>& outBuffer);

    // Reads MFT record number `recordIndex` (0-based), following mftDataRuns
    // to translate a logical record position to a physical disk location,
    // and applies the update-sequence-array fixup.
    bool readMftRecord(uint64_t recordIndex, std::vector<uint8_t>& outRecord);

    static std::vector<ClusterRun> parseDataRuns(const uint8_t* data, size_t maxSize);
    bool applyFixups(std::vector<uint8_t>& record) const;
    bool parseFileRecord(const std::vector<uint8_t>& record, uint64_t recordIndex,
        const juce::String& nameFilterLowercase, DeletedFileCandidate& outCandidate, bool& outIsMatch);
};
