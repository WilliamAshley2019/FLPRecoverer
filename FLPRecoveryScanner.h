#pragma once
#include <JuceHeader.h>
#include "FLPRecovery.h"
#include <atomic>

// =============================================================================
// FLPRecoveryScanner – Background scanning with threading
// =============================================================================
class FLPRecoveryScanner : public juce::Thread,
    public FLPRecovery
{
public:
    FLPRecoveryScanner()
        : juce::Thread("FLPRecoveryScanner")
    {
        // Forward callbacks to the scanner
        onProgress = [this](float progress, const juce::String& status)
            {
                if (this->onProgressCallback)
                    this->onProgressCallback(progress, status);
            };

        onCandidateFound = [this](const Candidate& candidate)
            {
                if (this->onCandidateFoundCallback)
                    this->onCandidateFoundCallback(candidate);
            };

        onLog = [this](const juce::String& msg)
            {
                if (this->onLogCallback)
                    this->onLogCallback(msg);
            };
    }

  
    // ─── Start Scanning ──────────────────────────────────────────────────
    void startScanImage(const juce::File& imageFile)
    {
        m_scanMode = ScanMode::Image;
        m_imageFile = imageFile;
        m_drivePath = "";
        m_directory = juce::File();
        m_stopScanning = false;
        startThread();
    }

    void startScanDrive(const juce::String& drivePath)
    {
        m_scanMode = ScanMode::Drive;
        m_drivePath = drivePath;
        m_imageFile = juce::File();
        m_directory = juce::File();
        m_stopScanning = false;
        startThread();
    }

    void startScanDirectory(const juce::File& directory)
    {
        m_scanMode = ScanMode::Directory;
        m_directory = directory;
        m_imageFile = juce::File();
        m_drivePath = "";
        m_stopScanning = false;
        startThread();
    }

    void stopScanning()
    {
        m_stopScanning = true;
    }

    // ─── Results ─────────────────────────────────────────────────────────
    FLPRecovery::ScanResult getResult() const { return m_result; }

    // ─── Callbacks ──────────────────────────────────────────────────────
    std::function<void(float, const juce::String&)> onProgressCallback;
    std::function<void(const FLPRecovery::Candidate&)> onCandidateFoundCallback;
    std::function<void(const juce::String&)> onLogCallback;

protected:
    void run() override
    {
        m_stopScanning = false;
        m_result = FLPRecovery::ScanResult();

        switch (m_scanMode)
        {
        case ScanMode::Image:
            m_result = scanImage(m_imageFile);
            break;
        case ScanMode::Drive:
            m_result = scanDrive(m_drivePath);
            break;
        case ScanMode::Directory:
            m_result = scanDirectory(m_directory);
            break;
        }

        if (onProgressCallback)
            onProgressCallback(1.0f, "Scan complete");
    }

private:
    enum class ScanMode { Image, Drive, Directory };

    ScanMode m_scanMode = ScanMode::Image;
    juce::File m_imageFile;
    juce::String m_drivePath;
    juce::File m_directory;
    FLPRecovery::ScanResult m_result;
    std::atomic<bool> m_stopScanning{ false };
};