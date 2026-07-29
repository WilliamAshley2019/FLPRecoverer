#pragma once
#include <JuceHeader.h>
#include <cstdint>

// =============================================================================
// DiskImager
//
// Byte-for-byte ("DD-style") imaging of a physical drive or volume to a
// destination file — the standard forensic first step: image once, then do
// all further scanning/recovery against the image, never the original.
//
// This deliberately duplicates FLPRecovery's raw-read alignment logic
// rather than depending on it, so imaging has no coupling to the FLP-
// specific scanning code — it's a general-purpose sector copier.
// =============================================================================
class DiskImager
{
public:
    DiskImager() = default;
    ~DiskImager() { closeAll(); }

    juce::String getLastError() const { return lastError; }

    // sourcePath: "\\\\.\\C:" (a volume) or "\\\\.\\PhysicalDrive0" (a whole disk).
    // Refuses to proceed if source and destination resolve to the same
    // physical device — imaging a drive onto itself would corrupt the very
    // data you're trying to preserve.
    bool imageToFile(const juce::String& sourcePath,
        const juce::File& destinationFile,
        std::function<void(float progress, const juce::String& status)> onProgress,
        std::function<bool()> shouldStop);

private:
    void closeAll();

    // Returns true and fills outDeviceNumber if the given path's underlying
    // physical device number could be determined (Windows only). Used to
    // catch "destination is on the same physical disk as source" even when
    // the paths look different (e.g. imaging \\.\PhysicalDrive0 to a file
    // on D:, where D: also happens to live on PhysicalDrive0).
    bool getPhysicalDeviceNumber(const juce::String& path, uint32_t& outDeviceNumber);

    juce::String lastError;
};
