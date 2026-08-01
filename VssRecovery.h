#pragma once
#include <JuceHeader.h>

#if JUCE_WINDOWS
#include <windows.h>
#endif

// =============================================================================
// VssRecovery
//
// Checks Windows' Volume Shadow Copy snapshots (the same mechanism behind
// "Previous Versions" / File History / System Restore) for an intact copy
// of a file — before falling back to any raw carving or $MFT work. This
// should always be tried first: it's completely non-invasive (read-only,
// doesn't touch the live volume), and when a snapshot exists it typically
// has a COMPLETE, unfragmented copy of the file as it was at snapshot
// time, which raw recovery can't promise.
//
// Requires administrator privileges, same as everything else here that
// touches raw volume data.
// =============================================================================
namespace VssRecovery
{
    struct ShadowCopy
    {
        juce::String deviceObjectPath; // e.g. "\\\\?\\GLOBALROOT\\Device\\HarddiskVolumeShadowCopy1"
        juce::String creationTime;     // as reported by vssadmin, informational only
    };

    // driveLetter e.g. "C:". Lists shadow copies for that volume via
    // `vssadmin list shadows` — the supported way to enumerate these
    // without pulling in the full VSS COM API.
    inline std::vector<ShadowCopy> listShadowCopies(const juce::String& driveLetter)
    {
        std::vector<ShadowCopy> results;

        juce::ChildProcess proc;
        if (!proc.start("vssadmin list shadows /for=" + driveLetter))
            return results;

        juce::String output = proc.readAllProcessOutput();
        proc.waitForProcessToFinish(10000);

        juce::StringArray lines;
        lines.addLines(output);

        juce::String pendingCreationTime;
        for (const auto& line : lines)
        {
            if (line.contains("creation time:"))
                pendingCreationTime = line.fromFirstOccurrenceOf("creation time:", false, false).trim();

            if (line.contains("Shadow Copy Volume:"))
            {
                ShadowCopy sc;
                sc.deviceObjectPath = line.fromFirstOccurrenceOf("Shadow Copy Volume:", false, false).trim();
                sc.creationTime = pendingCreationTime;
                if (sc.deviceObjectPath.isNotEmpty())
                    results.push_back(sc);
            }
        }

        return results;
    }

#if JUCE_WINDOWS
    // Checks whether `relativePathOnDrive` (e.g. "Users\\Name\\Documents\\
    // project.flp" — no drive letter, no leading slash) exists inside the
    // given shadow copy, and if so copies it to destinationFile.
    inline bool tryRecoverFromShadow(const ShadowCopy& shadow, const juce::String& relativePathOnDrive,
        const juce::File& destinationFile, juce::String& outError)
    {
        juce::String fullPath = shadow.deviceObjectPath + "\\" + relativePathOnDrive;

        HANDLE h = CreateFileA(
            fullPath.toRawUTF8(),
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        if (h == INVALID_HANDLE_VALUE)
        {
            outError = "Not found in this shadow copy (or administrator privileges required).";
            return false;
        }

        LARGE_INTEGER size{};
        if (!GetFileSizeEx(h, &size))
        {
            CloseHandle(h);
            outError = "Could not determine file size in shadow copy.";
            return false;
        }

        std::vector<uint8_t> buffer((size_t)size.QuadPart);
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(h, buffer.data(), (DWORD)size.QuadPart, &bytesRead, NULL);
        CloseHandle(h);

        if (!ok || (LONGLONG)bytesRead != size.QuadPart)
        {
            outError = "Could not read file contents from shadow copy.";
            return false;
        }

        juce::FileOutputStream out(destinationFile);
        if (!out.openedOk())
        {
            outError = "Could not create output file: " + destinationFile.getFullPathName();
            return false;
        }

        out.write(buffer.data(), buffer.size());
        out.flush();
        return true;
    }
#else
    inline bool tryRecoverFromShadow(const ShadowCopy&, const juce::String&, const juce::File&, juce::String& outError)
    {
        outError = "Shadow copy recovery is only implemented for Windows.";
        return false;
    }
#endif
}
