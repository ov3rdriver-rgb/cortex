#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

namespace
{
    // Plugin state keeps a reference to the brain (file list + cache path),
    // never raw audio. Populated in Phase 2.
    const juce::Identifier brainStateId { "Brain" };
}

CortexAudioProcessor::CortexAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "CortexParams", cortex::createParameterLayout())
{
    fftSizeParam = apvts.getRawParameterValue (cortex::ParamIDs::fftSize);
    dryWetParam  = apvts.getRawParameterValue (cortex::ParamIDs::dryWet);
    outGainParam = apvts.getRawParameterValue (cortex::ParamIDs::outGain);

    apvts.addParameterListener (cortex::ParamIDs::fftSize, this);
    updateLatency();
}

CortexAudioProcessor::~CortexAudioProcessor()
{
    apvts.removeParameterListener (cortex::ParamIDs::fftSize, this);
}

int CortexAudioProcessor::currentFftSize() const
{
    const auto index = juce::jlimit (0, (int) std::size (cortex::fftSizes) - 1, (int) fftSizeParam->load());
    return cortex::fftSizes[index];
}

void CortexAudioProcessor::updateLatency()
{
    // Phase 1+: STFT latency = FFT size (+ brain block size when the brain is in the path).
    setLatencySamples (currentFftSize());
}

void CortexAudioProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == cortex::ParamIDs::fftSize)
        updateLatency();
}

void CortexAudioProcessor::prepareToPlay (double, int)
{
    updateLatency();
}

void CortexAudioProcessor::releaseResources() {}

bool CortexAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;

    const auto sidechain = layouts.getChannelSet (true, 1);
    return sidechain.isDisabled()
        || sidechain == juce::AudioChannelSet::mono()
        || sidechain == juce::AudioChannelSet::stereo();
}

void CortexAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBus = getBusBuffer (buffer, false, 0);
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Phase 0: passthrough with output gain. The morph engine lands in Phase 1.
    // dryWet is read here so the parameter is live; with no wet path yet it is a no-op.
    juce::ignoreUnused (dryWetParam->load());
    mainBus.applyGain (juce::Decibels::decibelsToGain (outGainParam->load()));
}

juce::AudioProcessorEditor* CortexAudioProcessor::createEditor()
{
    return new CortexAudioProcessorEditor (*this);
}

void CortexAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (! state.getChildWithName (brainStateId).isValid())
        state.appendChild (juce::ValueTree (brainStateId), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CortexAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

    updateLatency();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CortexAudioProcessor();
}
