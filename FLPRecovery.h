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
        uint64_t blockIndex = 0;
        uint64_t blockOffset = 0;
        uint64_t blockSize = 0;
        bool hasFLPData = false;
        bool isBackedUp = false;
        std::vector<uint64_t> foundOffsets;
        juce::String status;
    };

    // ─── Recovery Candidate ──────────────────────────────────────────────
    struct Candidate
    {
        uint64_t offset = 0;
        uint64_t dataOffset = 0;
        uint64_t dataSize = 0;
        uint64_t totalSize = 0;
        uint64_t fileSize = 0;
        uint64_t chunksFound = 0;
        uint64_t totalChunks = 0;
        juce::String version;
        bool isValid = false;
        bool isComplete = false;
        juce::String error;
        std::vector<BlockInfo> blocks;

        // Where the candidate's bytes actually live, so recovery can reopen
        // the source and read them back out.
        juce::String sourcePath;
        bool isPhysicalDrive = false;
    };

    // ─── Scan Results ────────────────────────────────────────────────────
    struct ScanResult
    {
        std::vector<Candidate> candidates;
        std::vector<BlockInfo> allBlocks;
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
    ScanResult scanDriveRaw(const juce::String& drivePath,
        uint64_t startOffset = 0,
        uint64_t scanSize = 0);

    ScanResult scanDriveWithBlocks(const juce::String& drivePath,
        uint64_t blockSize = 4096,
        uint64_t startOffset = 0,
        uint64_t scanSize = 0);

    void startPassiveMode(const juce::String& drivePath,
        uint64_t blockSize = 4096,
        int updateIntervalMs = 100);
    void stopPassiveMode();

    ScanResult scanImage(const juce::File& imageFile);
    ScanResult scanDrive(const juce::String& drivePath);
    ScanResult scanDirectory(const juce::File& directory);

    // ─── Recovery Methods ──────────────────────────────────────────────
    bool recoverCandidate(const Candidate& candidate,
        const juce::File& outputFolder,
        const juce::String& suggestedName = "");

    int recoverAll(const ScanResult& result,
        const juce::File& outputFolder);

    bool recoverCandidateWithBlocks(const Candidate& candidate,
        const juce::File& outputFolder,
        bool backupBlocks = true,
        const juce::String& suggestedName = "");

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

    bool scanBlockForFLP(const uint8_t* blockData, uint64_t blockOffset,
        uint64_t blockSize, BlockInfo& blockInfo);
    bool reconstructFileFromBlocks(Candidate& candidate,
        const std::vector<BlockInfo>& blocks);
    bool backupBlock(const BlockInfo& block, const juce::File& backupFolder);

    // Reads `size` bytes starting at `offset` from either a regular file or
    // (on Windows) a raw physical drive path, handling the sector alignment
    // FILE_FLAG_NO_BUFFERING requires. Used by recovery to pull the actual
    // candidate bytes back out of the source.
    bool readSourceBytes(const juce::String& sourcePath, bool isPhysicalDrive,
        uint64_t offset, uint64_t size, std::vector<uint8_t>& outBuffer);

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
    uint64_t m_maxFileSize = 500 * 1024 * 1024;
    uint64_t m_blockSize = 4096;
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