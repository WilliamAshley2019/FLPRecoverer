#pragma once
#include <JuceHeader.h>

// =============================================================================
// MainComponent — deliberately minimal.
//
// This is step 1 of a two-step rebuild: confirm this bare skeleton launches
// and shows a window on your machine. Once that's confirmed, the real
// FLPRecoveryTool UI and scanning engine get folded into this component.
// =============================================================================
class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::TextButton testButton{ "Click me" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
