#pragma once
#include <JuceHeader.h>
#include "DiskImager.h"

class DiskImagerThread : public juce::Thread
{
public:
    DiskImagerThread() : juce::Thread("DiskImagerThread") {}
    ~DiskImagerThread() override { stopThread(10000); }

    void startImaging(const juce::String& sourcePath, const juce::File& destinationFile)
    {
        m_sourcePath = sourcePath;
        m_destinationFile = destinationFile;
        startThread();
    }

    std::function<void(float, const juce::String&)> onProgressCallback;
    std::function<void(const juce::String&)> onLogCallback;
    std::function<void(bool)> onCompleteCallback; // true = fully succeeded

protected:
    void run() override
    {
        if (onLogCallback)
            onLogCallback("Imaging " + m_sourcePath + " -> " + m_destinationFile.getFullPathName() +
                " (requires administrator privileges)...");

        bool ok = imager.imageToFile(m_sourcePath, m_destinationFile,
            [this](float progress, const juce::String& status)
            {
                if (onProgressCallback) onProgressCallback(progress, status);
            },
            [this]() { return threadShouldExit(); });

        if (onLogCallback)
            onLogCallback(imager.getLastError());

        if (onCompleteCallback)
            onCompleteCallback(ok);
    }

private:
    juce::String m_sourcePath;
    juce::File m_destinationFile;
    DiskImager imager;
};
