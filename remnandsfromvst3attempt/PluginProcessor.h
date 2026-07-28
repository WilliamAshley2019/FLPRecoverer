#pragma once
#include <JuceHeader.h>

class FLPRecoveryAudioProcessor : public juce::AudioProcessor
{
public:
    FLPRecoveryAudioProcessor();
    ~FLPRecoveryAudioProcessor() override;

    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FLP Recovery Tool"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // ─── Modern parameter tree (replaces deprecated getParameter/setParameter) ──
    juce::AudioProcessorValueTreeState apvts;

    // ─── VST3 category – only included when building the VST3 target ──
#if JUCE_VST3
    const juce::String getVST3Category() const override { return "Fx|Tools"; }
#endif

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::String lastOutputFolder;
    void logDebug(const juce::String& message);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FLPRecoveryAudioProcessor)
};