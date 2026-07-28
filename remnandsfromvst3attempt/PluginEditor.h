#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FLPRecoveryTool.h"

class FLPRecoveryAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit FLPRecoveryAudioProcessorEditor(FLPRecoveryAudioProcessor&);
    ~FLPRecoveryAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

private:
    FLPRecoveryAudioProcessor& processorRef;
    FLPRecoveryTool toolComponent;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FLPRecoveryAudioProcessorEditor)
};