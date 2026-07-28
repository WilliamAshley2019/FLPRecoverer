#include "PluginEditor.h"

FLPRecoveryAudioProcessorEditor::FLPRecoveryAudioProcessorEditor(FLPRecoveryAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    addAndMakeVisible(toolComponent);
    setSize(1000, 750);
    setResizable(true, true);
    setResizeLimits(700, 500, 2000, 1400);
    processorRef.logDebug("Editor constructed");
}

FLPRecoveryAudioProcessorEditor::~FLPRecoveryAudioProcessorEditor()
{
    processorRef.logDebug("Editor destroyed");
}

void FLPRecoveryAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1A1A3A));
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("FLP Recovery Tool", getLocalBounds(), juce::Justification::centred, true);
}

void FLPRecoveryAudioProcessorEditor::resized()
{
    toolComponent.setBounds(getLocalBounds());
}

void FLPRecoveryAudioProcessorEditor::parentHierarchyChanged()
{
    if (getParentComponent() != nullptr)
    {
        processorRef.logDebug("Editor attached to host");
        setVisible(true);
        resized();
    }
}

void FLPRecoveryAudioProcessorEditor::visibilityChanged()
{
    processorRef.logDebug("Editor visibility: " + juce::String(isVisible() ? "visible" : "hidden"));
}