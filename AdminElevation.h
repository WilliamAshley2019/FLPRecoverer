#pragma once
#include <JuceHeader.h>

#if JUCE_WINDOWS
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

// =============================================================================
// AdminElevation
//
// Nearly everything this app does that touches raw drives/volumes (physical
// drive scanning, NTFS $MFT parsing, DD imaging, TRIM toggling, shadow copy
// access) requires the process to be elevated. Rather than making the user
// remember to right-click -> Run as Administrator, this checks at startup
// and offers to relaunch itself elevated via the standard UAC prompt.
// =============================================================================
namespace AdminElevation
{
    inline bool isRunningAsAdmin()
    {
#if JUCE_WINDOWS
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

        if (AllocateAndInitializeSid(&ntAuthority, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &adminGroup))
        {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }

        return isAdmin != FALSE;
#else
        return false;
#endif
    }

    // Relaunches the current executable elevated (triggers the standard UAC
    // prompt) and returns true if the relaunch was successfully initiated —
    // the caller should quit this (non-elevated) instance immediately after.
    // Returns false if the user cancelled the UAC prompt or it failed.
    inline bool relaunchAsAdmin()
    {
#if JUCE_WINDOWS
        wchar_t exePath[MAX_PATH];
        if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0)
            return false;

        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas";
        sei.lpFile = exePath;
        sei.nShow = SW_NORMAL;
        sei.fMask = SEE_MASK_NOASYNC;

        if (!ShellExecuteExW(&sei))
        {
            // ERROR_CANCELLED means the user clicked "No" on the UAC prompt —
            // not a real failure, just a declined elevation.
            return false;
        }

        return true;
#else
        return false;
#endif
    }
}
