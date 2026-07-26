#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <memory>

// =============================================================================
// FLP File Recovery Tool - Enhanced for Raw Drive Scanning
// =============================================================================

class FLPRecovery
{
public:
    // ─── Block Information ──────────────────────────────────────────────
    struct BlockInfo
    {
        uint64_t blockIndex;
        uint64_t blockOffset;      // Offset in bytes from start
        uint64_t blockSize;        // Size of this block
        bool hasFLPData;           // Does this block contain FLP data?
        std::vector<uint64_t> foundOffsets; // Offsets within block where FLP was found
        juce::String status;       // "Empty", "Partial", "Complete"
        bool isBackedUp;           // Has this block been backed up?
    };

    // ─── Recovery Candidate (enhanced) ──────────────────────────────────
    struct Candidate
    {
        uint64_t offset;          // Starting offset of "FLhd"
        uint64_t dataOffset;      // Offset where "FLdt" was found
        uint64_t dataSize;        // Size from FLdt.size field
        uint64_t totalSize;       // Total recoverable file size
        uint64_t fileSize;        // Actual file size from filesystem (if known)
        juce::String version;     // FL Studio version (if found)
        bool isValid;             // Passed validation checks
        bool isComplete;          // All chunks found?
        juce::String error;       // Why invalid (if not valid)
        std::vector<BlockInfo> blocks; // Blocks that make up this file
        uint64_t chunksFound;     // Number of chunks found
        uint64_t totalChunks;     // Total chunks expected
    };

    // ─── Scan Results (enhanced) ──────────────────────────────────────
    struct ScanResult
    {
        std::vector<Candidate> candidates;
        std::vector<BlockInfo> allBlocks;     // All blocks scanned
        uint64_t bytesScanned = 0;
        uint64_t signatureHits = 0;
        uint64_t validFiles = 0;
        uint64_t totalBlocks = 0;
        uint64_t blocksWithFLP = 0;
        juce::String lastError;
    };

    // ─── Constructor / Destructor ──────────────────────────────────────
    FLPRecovery();
    ~FLPRecovery();

    // ─── Main Scanning Methods ──────────────────────────────────────────

    // Enhanced raw drive scan with block-level analysis
    ScanResult scanDriveRaw(const juce::String& drivePath,
        uint64_t startOffset = 0,
        uint64_t scanSize = 0);

    // Scan with block visualization
    ScanResult scanDriveWithBlocks(const juce::String& drivePath,
        uint64_t blockSize = 4096,
        uint64_t startOffset = 0,
        uint64_t scanSize = 0);

    // Passive real-time monitoring mode
    void startPassiveMode(const juce::String& drivePath,
        uint64_t blockSize = 4096,
        int updateIntervalMs = 100);

    void stopPassiveMode();

    // Scan a raw disk image file (existing)
    ScanResult scanImage(const juce::File& imageFile);

    // Scan a physical drive (Windows: \\.\PhysicalDriveX)
    ScanResult scanDrive(const juce::String& drivePath);

    // Scan a directory recursively (for intact files)
    ScanResult scanDirectory(const juce::File& directory);

    // ─── Recovery Methods ──────────────────────────────────────────────

    // Recover a single candidate to the output folder
    bool recoverCandidate(const Candidate& candidate,
        const juce::File& outputFolder,
        const juce::String& suggestedName = "");

    // Recover all valid candidates to the output folder
    int recoverAll(const ScanResult& result,
        const juce::File& outputFolder);

    // Recover with block-level backup
    bool recoverCandidateWithBlocks(const Candidate& candidate,
        const juce::File& outputFolder,
        bool backupBlocks = true,
        const juce::String& suggestedName = "");

    // Recover all with block backup
    int recoverAllWithBlocks(const ScanResult& result,
        const juce::File& outputFolder,
        bool backupBlocks = true);

    // ─── Validation ──────────────────────────────────────────────────────

    bool validateCandidate(Candidate& candidate, const uint8_t* data);

    // ─── Settings ──────────────────────────────────────────────────────

    void setMinFileSize(uint64_t minSize) { m_minFileSize = minSize; }
    void setMaxFileSize(uint64_t maxSize) { m_maxFileSize = maxSize; }
    void setVerbose(bool verbose) { m_verbose = verbose; }
    void setBlockSize(uint64_t blockSize) { m_blockSize = blockSize; }

    // ─── Progress Callbacks ────────────────────────────────────────────

    std::function<void(float progress, const juce::String& status)> onProgress;
    std::function<void(const Candidate& candidate)> onCandidateFound;
    std::function<void(const BlockInfo& block)> onBlockScanned;
    std::function<void(const juce::String& msg)> onLog;

private:
    // ─── Internal Scanning ──────────────────────────────────────────────

    std::vector<Candidate> findFLhdSignatures(const uint8_t* data, size_t size);
    bool validateFLdt(const uint8_t* data, Candidate& candidate);
    juce::String extractVersion(const uint8_t* data, uint64_t offset);

    // Enhanced scanning methods
    bool scanBlockForFLP(const uint8_t* blockData, uint64_t blockOffset,
        uint64_t blockSize, BlockInfo& blockInfo);
    bool reconstructFileFromBlocks(Candidate& candidate,
        const std::vector<BlockInfo>& blocks);
    bool backupBlock(const BlockInfo& block, const juce::File& backupFolder);

    // ─── Helpers ──────────────────────────────────────────────────────

    uint32_t readU32LE(const uint8_t* data);
    uint16_t readU16LE(const uint8_t* data);
    bool isOverlapping(const Candidate& a, const Candidate& b);

    // ─── Passive Mode Thread ──────────────────────────────────────────

    class PassiveScannerThread : public juce::Thread
    {
    public:
        PassiveScannerThread(FLPRecovery& owner);
        ~PassiveScannerThread() override;
        void run() override;
        void stopScanning();
        void setDrivePath(const juce::String& path) { m_drivePath = path; }
        void setBlockSize(uint64_t size) { m_blockSize = size; }
        void setUpdateInterval(int ms) { m_updateIntervalMs = ms; }

    private:
        FLPRecovery& m_owner;
        juce::String m_drivePath;
        uint64_t m_blockSize = 4096;
        int m_updateIntervalMs = 100;
        std::atomic<bool> m_shouldStop{ false };
        uint64_t m_currentOffset = 0;
    };

    std::unique_ptr<PassiveScannerThread> m_passiveThread;

    // ─── Settings ──────────────────────────────────────────────────────

    uint64_t m_minFileSize = 1024;
    uint64_t m_maxFileSize = 500 * 1024 * 1024; // 500MB
    uint64_t m_blockSize = 4096; // 4KB default
    bool m_verbose = false;

    // ─── Constants ──────────────────────────────────────────────────────

    static constexpr uint8_t FLHD_MAGIC[4] = { 0x46, 0x4C, 0x68, 0x64 };
    static constexpr uint8_t FLDT_MAGIC[4] = { 0x46, 0x4C, 0x64, 0x74 };
    static constexpr size_t HEADER_SIZE = 16;
    static constexpr size_t FLDT_HEADER_SIZE = 8;

    // ─── Internal Logging ──────────────────────────────────────────────

    void log(const juce::String& msg);
    void logVerbose(const juce::String& msg);
    void sendProgress(float progress, const juce::String& status);
};