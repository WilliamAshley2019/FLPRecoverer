#pragma once
#include <JuceHeader.h>
#include "NtfsMftRecovery.h"

// =============================================================================
// NtfsScannerThread — runs an NtfsMftRecovery scan off the message thread,
// mirroring FLPRecoveryScanner's callback pattern so the UI wiring looks
// the same for both scan types.
// =============================================================================
class NtfsScannerThread : public juce::Thread
{
public:
    NtfsScannerThread() : juce::Thread("NtfsScannerThread") {}
    ~NtfsScannerThread() override { stopThread(10000); }

    void startScan(const juce::String& volumePath, const juce::String& nameFilterLowercase)
    {
        m_volumePath = volumePath;
        m_nameFilter = nameFilterLowercase;
        startThread();
    }

    NtfsMftRecovery& getEngine() { return engine; }
    const std::vector<NtfsMftRecovery::DeletedFileCandidate>& getResults() const { return results; }

    std::function<void(float, const juce::String&)> onProgressCallback;
    std::function<void(const NtfsMftRecovery::DeletedFileCandidate&)> onCandidateFoundCallback;
    std::function<void(const juce::String&)> onLogCallback;

protected:
    void run() override
    {
        results.clear();

        if (onLogCallback)
            onLogCallback("Opening volume " + m_volumePath + " (requires administrator privileges)...");

        if (!engine.openVolume(m_volumePath))
        {
            if (onLogCallback)
                onLogCallback("NTFS scan failed: " + engine.getLastError());
            return;
        }

        results = engine.scanForDeletedFiles(m_nameFilter,
            [this](float progress, const juce::String& status)
            {
                if (onProgressCallback) onProgressCallback(progress, status);
            },
            [this]() { return threadShouldExit(); });

        if (onLogCallback)
            onLogCallback(threadShouldExit()
                ? "NTFS scan stopped by user."
                : "NTFS scan complete: found " + juce::String((int)results.size()) + " deleted candidate(s).");

        // scanForDeletedFiles collects everything before returning rather
        // than streaming — fire the found-callback for each result now so
        // the table still fills in the same way the other scan types do.
        if (onCandidateFoundCallback)
            for (auto& candidate : results)
                onCandidateFoundCallback(candidate);
    }

private:
    juce::String m_volumePath;
    juce::String m_nameFilter;
    NtfsMftRecovery engine;
    std::vector<NtfsMftRecovery::DeletedFileCandidate> results;
};
