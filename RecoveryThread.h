#pragma once
#include <JuceHeader.h>
#include "FLPRecovery.h"
#include "NtfsMftRecovery.h"

// =============================================================================
// RecoveryThread
//
// Recover All used to run entirely synchronously on the message thread.
// That was already not great, but became a real problem once recovery
// started also running FLPTOOL's full parser + stats + MIDI export per
// file (see FLPRecoveryTool::analyzeRecoveredFlpFile) — with a large batch
// of files, that's enough total blocking time to freeze the UI, which
// looks exactly like an unresponsive/stuck app rather than "still working."
// =============================================================================
class RecoveryThread : public juce::Thread
{
public:
    RecoveryThread() : juce::Thread("RecoveryThread") {}
    ~RecoveryThread() override { stopThread(10000); }

    struct Inputs
    {
        FLPRecovery* flpEngine = nullptr;       // e.g. &scanner (FLPRecoveryScanner IS-A FLPRecovery)
        NtfsMftRecovery* ntfsEngine = nullptr;  // e.g. &ntfsScanner.getEngine()
        FLPRecovery::ScanResult flpResult;
        std::vector<NtfsMftRecovery::DeletedFileCandidate> ntfsCandidates;
        juce::File outputFolder;
    };

    void startRecovery(Inputs newInputs)
    {
        inputs = std::move(newInputs);
        startThread();
    }

    // All callbacks fire from this background thread — callers must marshal
    // any UI touch back to the message thread themselves (as everywhere
    // else in this app).
    std::function<void(const juce::String&)> onLogCallback;
    std::function<void(const juce::File&)> onFileRecoveredCallback;
    std::function<void(int totalRecovered)> onCompleteCallback;

protected:
    void run() override
    {
        int totalRecovered = 0;

        if (!inputs.flpResult.candidates.empty() && inputs.flpEngine != nullptr)
        {
            if (onLogCallback)
                onLogCallback("Recovering " + juce::String((int)inputs.flpResult.candidates.size()) +
                    " carved file(s) to: " + inputs.outputFolder.getFullPathName());

            totalRecovered += inputs.flpEngine->recoverAllWithBlocks(inputs.flpResult, inputs.outputFolder, true,
                [this](const juce::File& f)
                {
                    if (onFileRecoveredCallback) onFileRecoveredCallback(f);
                });
        }

        if (!inputs.ntfsCandidates.empty() && inputs.ntfsEngine != nullptr)
        {
            if (onLogCallback)
                onLogCallback("Reconstructing " + juce::String((int)inputs.ntfsCandidates.size()) +
                    " NTFS-recovered file(s) to: " + inputs.outputFolder.getFullPathName());

            int ntfsRecovered = 0;
            for (const auto& candidate : inputs.ntfsCandidates)
            {
                if (threadShouldExit()) break;

                juce::String safeName = candidate.fileName.isNotEmpty()
                    ? candidate.fileName
                    : ("recovered_mft" + juce::String((int64_t)candidate.mftRecordNumber) + ".flp");
                safeName = safeName.replaceCharacters("\\/:?\"<>|", "________");

                juce::File outFile = inputs.outputFolder.getChildFile(safeName);
                if (outFile.existsAsFile())
                    outFile = inputs.outputFolder.getNonexistentChildFile(
                        outFile.getFileNameWithoutExtension(), outFile.getFileExtension());

                auto report = inputs.ntfsEngine->reconstructFile(candidate, outFile);

                if (report.fullyRecovered)
                {
                    ntfsRecovered++;
                    if (onLogCallback) onLogCallback("Recovered " + candidate.fileName + ": " + report.detail);
                    if (onFileRecoveredCallback) onFileRecoveredCallback(outFile);
                }
                else if (report.bytesRecovered > 0)
                {
                    ntfsRecovered++;
                    if (onLogCallback)
                        onLogCallback("PARTIAL recovery of " + candidate.fileName + " (" +
                            juce::String((int64_t)report.bytesRecovered) + " of " +
                            juce::String((int64_t)report.bytesExpected) + " bytes): " + report.detail);

                    if (inputs.flpEngine != nullptr)
                    {
                        auto integrity = inputs.flpEngine->analyzeEventStreamIntegrity(outFile);
                        if (onLogCallback)
                            onLogCallback("Content analysis for " + candidate.fileName + ": " + integrity.detail);
                    }

                    if (onFileRecoveredCallback) onFileRecoveredCallback(outFile);
                }
                else
                {
                    if (onLogCallback) onLogCallback("Failed to reconstruct " + candidate.fileName + ": " + report.detail);
                }
            }

            if (onLogCallback)
                onLogCallback("Reconstructed " + juce::String(ntfsRecovered) + " of " +
                    juce::String((int)inputs.ntfsCandidates.size()) + " NTFS file(s).");
            totalRecovered += ntfsRecovered;
        }

        if (onLogCallback)
            onLogCallback("Recovered " + juce::String(totalRecovered) + " files total.");

        if (onCompleteCallback)
            onCompleteCallback(totalRecovered);
    }

private:
    Inputs inputs;
};
