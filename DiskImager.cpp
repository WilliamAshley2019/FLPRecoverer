#include "DiskImager.h"

#if JUCE_WINDOWS
#include <windows.h>
#include <winioctl.h>
#endif

void DiskImager::closeAll() {}

bool DiskImager::getPhysicalDeviceNumber(const juce::String& path, uint32_t& outDeviceNumber)
{
#if JUCE_WINDOWS
    HANDLE h = CreateFileA(
        path.toRawUTF8(),
        0, // metadata query only, no read/write access needed
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (h == INVALID_HANDLE_VALUE)
        return false;

    STORAGE_DEVICE_NUMBER sdn{};
    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER,
        NULL, 0, &sdn, sizeof(sdn), &bytesReturned, NULL);
    CloseHandle(h);

    if (!ok) return false;
    outDeviceNumber = sdn.DeviceNumber;
    return true;
#else
    juce::ignoreUnused(path, outDeviceNumber);
    return false;
#endif
}

bool DiskImager::imageToFile(const juce::String& sourcePath,
    const juce::File& destinationFile,
    std::function<void(float, const juce::String&)> onProgress,
    std::function<bool()> shouldStop)
{
#if JUCE_WINDOWS
    // ─── Safety check: refuse to image a drive onto itself ─────────────
    juce::String destRoot = destinationFile.getFullPathName().substring(0, 2); // e.g. "D:"
    juce::String destVolumePath = "\\\\.\\" + destRoot;

    uint32_t sourceDeviceNumber = 0, destDeviceNumber = 0;
    bool haveSourceDevice = getPhysicalDeviceNumber(sourcePath, sourceDeviceNumber);
    bool haveDestDevice = getPhysicalDeviceNumber(destVolumePath, destDeviceNumber);

    if (haveSourceDevice && haveDestDevice && sourceDeviceNumber == destDeviceNumber)
    {
        lastError = "Refusing to image " + sourcePath + " onto " + destRoot +
            " — they are the same physical disk. Choose a destination on a different physical drive.";
        return false;
    }

    if (!haveSourceDevice || !haveDestDevice)
    {
        // Best-effort check only — couldn't fully verify, but don't block
        // outright (e.g. network destinations won't resolve to a device
        // number at all). The caller's UI should still warn the user.
    }

    // ─── Open source ─────────────────────────────────────────────────
    HANDLE hSource = CreateFileA(
        sourcePath.toRawUTF8(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING,
        NULL);

    if (hSource == INVALID_HANDLE_VALUE)
    {
        lastError = "Could not open source " + sourcePath +
            " (administrator privileges required). Error: " + juce::String((int)GetLastError());
        return false;
    }

    // ─── Determine source size ─────────────────────────────────────────
    LARGE_INTEGER sourceSize{};
    if (!GetFileSizeEx(hSource, &sourceSize) || sourceSize.QuadPart <= 0)
    {
        // Fall back to the disk-length IOCTL for physical drive handles
        // where GetFileSizeEx doesn't apply.
        GET_LENGTH_INFORMATION lengthInfo{};
        DWORD bytesReturned = 0;
        if (!DeviceIoControl(hSource, IOCTL_DISK_GET_LENGTH_INFO,
            NULL, 0, &lengthInfo, sizeof(lengthInfo), &bytesReturned, NULL))
        {
            CloseHandle(hSource);
            lastError = "Could not determine size of " + sourcePath;
            return false;
        }
        sourceSize.QuadPart = lengthInfo.Length.QuadPart;
    }

    uint64_t totalSize = (uint64_t)sourceSize.QuadPart;

    // ─── Open destination ───────────────────────────────────────────────
    if (destinationFile.existsAsFile())
        destinationFile.deleteFile();

    juce::FileOutputStream out(destinationFile);
    if (!out.openedOk())
    {
        CloseHandle(hSource);
        lastError = "Could not create destination file: " + destinationFile.getFullPathName();
        return false;
    }

    // ─── Copy loop ──────────────────────────────────────────────────────
    const uint64_t sectorAlign = 4096; // safe alignment for FILE_FLAG_NO_BUFFERING
    const uint64_t chunkSize = 8 * 1024 * 1024; // 8 MB per read, already sector-aligned
    std::vector<uint8_t> buffer(chunkSize);

    uint64_t bytesCopied = 0;
    juce::int64 startTime = juce::Time::getMillisecondCounter();

    while (bytesCopied < totalSize)
    {
        if (shouldStop && shouldStop())
        {
            lastError = "Imaging stopped by user at " + juce::String((int64_t)bytesCopied) + " of " +
                juce::String((int64_t)totalSize) + " bytes.";
            CloseHandle(hSource);
            return false;
        }

        uint64_t remaining = totalSize - bytesCopied;
        uint64_t requestSize = juce::jmin(chunkSize, ((remaining + sectorAlign - 1) / sectorAlign) * sectorAlign);

        DWORD bytesRead = 0;
        if (!ReadFile(hSource, buffer.data(), (DWORD)requestSize, &bytesRead, NULL) || bytesRead == 0)
        {
            lastError = "Read failed at offset " + juce::String((int64_t)bytesCopied) +
                " — source may have a bad sector here. Stopping (partial image is on disk up to this point).";
            CloseHandle(hSource);
            return false;
        }

        uint64_t bytesToWrite = juce::jmin((uint64_t)bytesRead, remaining);
        if (!out.write(buffer.data(), (size_t)bytesToWrite))
        {
            lastError = "Write failed at offset " + juce::String((int64_t)bytesCopied) +
                " — destination may be out of space.";
            CloseHandle(hSource);
            return false;
        }

        bytesCopied += bytesToWrite;

        if (onProgress)
        {
            juce::int64 elapsedMs = juce::Time::getMillisecondCounter() - startTime;
            double mbPerSec = elapsedMs > 0 ? (bytesCopied / 1024.0 / 1024.0) / (elapsedMs / 1000.0) : 0.0;
            onProgress((float)((double)bytesCopied / (double)totalSize),
                juce::String((int64_t)(bytesCopied / (1024 * 1024))) + " / " +
                juce::String((int64_t)(totalSize / (1024 * 1024))) + " MB (" +
                juce::String(mbPerSec, 1) + " MB/s)");
        }
    }

    out.flush();
    CloseHandle(hSource);

    lastError = "Image complete: " + juce::String((int64_t)bytesCopied) + " bytes written to " +
        destinationFile.getFullPathName();
    return true;
#else
    juce::ignoreUnused(sourcePath, destinationFile, onProgress, shouldStop);
    lastError = "Raw imaging is only implemented for Windows in this build.";
    return false;
#endif
}
