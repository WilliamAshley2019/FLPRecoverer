#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

// =============================================================================
// FLP File Recovery Tool
// =============================================================================
// Scans raw disk images or drives for deleted FLP files by looking for
// the "FLhd" and "FLdt" magic markers. The FLdt chunk header contains
// a size field that tells us exactly how long the file should be,
// making FLP files excellent candidates for file carving.
// =============================================================================

class FLPRecovery
{
public:
    // ─── Recovery Candidate ──────────────────────────────────────────────
    struct Candidate
    {
        uint64_t offset;          // Starting offset of "FLhd"
        uint64_t dataOffset;      // Offset where "FLdt" was found
        uint64_t dataSize;        // Size from FLdt.size field
        uint64_t totalSize;       // Total recoverable file size
        juce::String version;     // FL Studio version (if found)
        bool isValid;             // Passed validation checks
        juce::String error;       // Why invalid (if not valid)
    };

    // ─── Scan Results ────────────────────────────────────────────────────
    struct ScanResult
    {
        std::vector<Candidate> candidates;
        uint64_t bytesScanned = 0;
        uint64_t signatureHits = 0;
        uint64_t validFiles = 0;
        juce::String lastError;
    };

    // ─── Constructor / Destructor ────────────────────────────────────────
    FLPRecovery();
    ~FLPRecovery();

    // ─── Main Scanning Methods ────────────────────────────────────────────

    // Scan a raw disk image file for FLP signatures
    // Returns: number of valid FLP files found
    ScanResult scanImage(const juce::File& imageFile);

    // Scan a physical drive (Windows: \\.\PhysicalDriveX)
    // Returns: number of valid FLP files found
    ScanResult scanDrive(const juce::String& drivePath);

    // Scan a directory recursively (for intact files, not carving)
    ScanResult scanDirectory(const juce::File& directory);

    // ─── Recovery Methods ────────────────────────────────────────────────

    // Recover a single candidate to the output folder
    // Returns: true if successfully recovered and validated
    bool recoverCandidate(const Candidate& candidate,
                          const juce::File& outputFolder,
                          const juce::String& suggestedName = "");

    // Recover all valid candidates to the output folder
    // Returns: number of successfully recovered files
    int recoverAll(const ScanResult& result,
                   const juce::File& outputFolder);

    // ─── Validation ──────────────────────────────────────────────────────

    // Validate a candidate's structure using the parser
    // Returns: true if the data appears to be a valid FLP
    bool validateCandidate(Candidate& candidate, const uint8_t* data);

    // ─── Settings ─────────────────────────────────────────────────────────

    // Minimum file size to consider (default: 1024 bytes)
    void setMinFileSize(uint64_t minSize) { m_minFileSize = minSize; }

    // Maximum file size to consider (default: 500MB)
    void setMaxFileSize(uint64_t maxSize) { m_maxFileSize = maxSize; }

    // Enable verbose logging
    void setVerbose(bool verbose) { m_verbose = verbose; }

    // ─── Progress Callback ───────────────────────────────────────────────

    // Called during scanning with progress (0.0 to 1.0)
    std::function<void(float progress, const juce::String& status)> onProgress;

    // Called when a candidate is found
    std::function<void(const Candidate& candidate)> onCandidateFound;

    // Called for log messages
    std::function<void(const juce::String& msg)> onLog;

private:
    // ─── Internal Scanning ───────────────────────────────────────────────

    // Scan raw data for FLhd signatures
    std::vector<Candidate> findFLhdSignatures(const uint8_t* data, size_t size);

    // Validate a candidate's FLdt chunk
    bool validateFLdt(const uint8_t* data, Candidate& candidate);

    // Extract version string from candidate data
    juce::String extractVersion(const uint8_t* data, uint64_t offset);

    // ─── Internal Helpers ────────────────────────────────────────────────

    // Read little-endian uint32 from raw data
    uint32_t readU32LE(const uint8_t* data);

    // Read little-endian uint16 from raw data
    uint16_t readU16LE(const uint8_t* data);

    // Check if a candidate likely overlaps with another
    bool isOverlapping(const Candidate& a, const Candidate& b);

    // ─── Settings ─────────────────────────────────────────────────────────

    uint64_t m_minFileSize = 1024;       // Minimum 1KB
    uint64_t m_maxFileSize = 500 * 1024 * 1024; // 500MB
    bool m_verbose = false;

    // ─── Constants ────────────────────────────────────────────────────────

    static constexpr uint8_t FLHD_MAGIC[4] = {0x46, 0x4C, 0x68, 0x64}; // "FLhd"
    static constexpr uint8_t FLDT_MAGIC[4] = {0x46, 0x4C, 0x64, 0x74}; // "FLdt"
    static constexpr size_t   HEADER_SIZE = 16;  // FLhd chunk size
    static constexpr size_t   FLDT_HEADER_SIZE = 8; // FLdt + size (4+4)

    // ─── Internal Logging ────────────────────────────────────────────────

    void log(const juce::String& msg);
    void logVerbose(const juce::String& msg);
    void sendProgress(float progress, const juce::String& status);
};