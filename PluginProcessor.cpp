#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <fstream>

void FLPRecoveryAudioProcessor::logDebug(const juce::String& msg)
{
#if JUCE_WINDOWS
    std::ofstream file("C:\\temp\\flp_recovery_debug.txt", std::ios::app);
#else
    std::ofstream file("/tmp/flp_recovery_debug.txt", std::ios::app);
#endif
    if (file.is_open())
    {
        file << "[" << juce::Time::getCurrentTime().toString(true, true) << "] "
            << msg.toStdString() << std::endl;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
FLPRecoveryAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "dummy", "Dummy",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f));
    return layout;
}

FLPRecoveryAudioProcessor::FLPRecoveryAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), false) // optional
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    logDebug("=== FLPRecoveryAudioProcessor CONSTRUCTED ===");
#if JUCE_VST3
    logDebug("VST3 build confirmed");
#endif
}

FLPRecoveryAudioProcessor::~FLPRecoveryAudioProcessor()
{
    logDebug("=== FLPRecoveryAudioProcessor DESTROYED ===");
}

void FLPRecoveryAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    logDebug("prepareToPlay called - sampleRate: " + juce::String(sampleRate) +
        ", blockSize: " + juce::String(samplesPerBlock));
}

void FLPRecoveryAudioProcessor::releaseResources()
{
    logDebug("releaseResources called");
}

bool FLPRecoveryAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto inChans = layouts.getMainInputChannelSet();
    auto outChans = layouts.getMainOutputChannelSet();

    if (outChans != juce::AudioChannelSet::mono() &&
        outChans != juce::AudioChannelSet::stereo())
        return false;

    if (!inChans.isDisabled() && inChans.size() != outChans.size())
        return false;

    return true;
}

void FLPRecoveryAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    for (int ch = 0; ch < getTotalNumOutputChannels(); ++ch)
    {
        if (ch < getTotalNumInputChannels())
            buffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
        else
            buffer.clear(ch, 0, buffer.getNumSamples());
    }
}

juce::AudioProcessorEditor* FLPRecoveryAudioProcessor::createEditor()
{
    logDebug("=== createEditor CALLED ===");
    auto* editor = new FLPRecoveryAudioProcessorEditor(*this);
    logDebug("Editor created, returning");
    return editor;
}

void FLPRecoveryAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    logDebug("getStateInformation called");
    auto state = apvts.copyState();
    state.setProperty("lastOutputFolder", lastOutputFolder, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void FLPRecoveryAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    logDebug("setStateInformation called");
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid())
        {
            lastOutputFolder = state.getProperty("lastOutputFolder", juce::String());
            apvts.replaceState(state);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FLPRecoveryAudioProcessor();
}