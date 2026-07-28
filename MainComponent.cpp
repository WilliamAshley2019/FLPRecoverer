#include "MainComponent.h"

MainComponent::MainComponent()
{
    titleLabel.setText("FLPRecoverTool — launch test", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::FontOptions(24.0f));
    addAndMakeVisible(titleLabel);

    testButton.onClick = [this]
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Working",
                "The window and event loop are both alive.");
        };
    addAndMakeVisible(testButton);

    setSize(600, 400);
}

MainComponent::~MainComponent() {}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(20);
    titleLabel.setBounds(area.removeFromTop(60));
    area.removeFromTop(20);
    testButton.setBounds(area.removeFromTop(40).withSizeKeepingCentre(200, 40));
}
