#pragma once
#include <JuceHeader.h>

// =============================================================================
// TrimControl
//
// Wraps `fsutil behavior query/set disabledeletenotify` — the supported way
// to toggle TRIM system-wide on Windows.
//
// IMPORTANT — read before using:
//   - This is a SYSTEM-WIDE setting affecting every SSD on the machine, not
//     a per-drive or per-scan option.
//   - Disabling TRIM does NOT recover data that's already been trimmed —
//     it only changes behavior for FUTURE deletions.
//   - Leaving TRIM disabled long-term hurts SSD performance and lifespan
//     (write amplification). This should be turned back on once you're
//     done with recovery-sensitive work, not left off permanently.
// =============================================================================
namespace TrimControl
{
    // Returns true if it could determine the current state; sets
    // outTrimEnabled to the result. Requires administrator privileges.
    inline bool queryTrimEnabled(bool& outTrimEnabled, juce::String& outRawOutput)
    {
        juce::ChildProcess proc;
        if (!proc.start("fsutil behavior query disabledeletenotify"))
            return false;

        outRawOutput = proc.readAllProcessOutput();
        proc.waitForProcessToFinish(5000);

        // fsutil prints a line containing "0" (TRIM enabled, notify-delete
        // NOT disabled) or "1" (TRIM disabled) somewhere in its output.
        if (outRawOutput.contains("= 1"))
        {
            outTrimEnabled = false;
            return true;
        }
        if (outRawOutput.contains("= 0"))
        {
            outTrimEnabled = true;
            return true;
        }
        return false;
    }

    // enableTrim = false to DISABLE TRIM (better recovery odds for future
    // deletions, worse SSD performance/lifespan while off).
    inline bool setTrimEnabled(bool enableTrim, juce::String& outRawOutput)
    {
        juce::String cmd = juce::String("fsutil behavior set disabledeletenotify ") + (enableTrim ? "0" : "1");
        juce::ChildProcess proc;
        if (!proc.start(cmd))
            return false;

        outRawOutput = proc.readAllProcessOutput();
        return proc.waitForProcessToFinish(5000);
    }
}
