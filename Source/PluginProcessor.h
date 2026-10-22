#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/CepstralMorph.h"
#include "dsp/STFT.h"

class CortexAudioProcessor final : public juce::AudioProcessor,
                                   private juce::AudioProcessorValueTreeState::Listener
{
public:
    CortexAudioProcessor();
    ~CortexAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    int currentFftSize() const;

private:
    static constexpr int maxChannels = 2;

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void updateLatency();
    cortex::dsp::MorphParams readMorphParams() const;

    struct Params
    {
        std::atomic<float>* morphX = nullptr;
        std::atomic<float>* morphY = nullptr;
        std::atomic<float>* cutoffA = nullptr;
        std::atomic<float>* cutoffB = nullptr;
        std::atomic<float>* phaseMode = nullptr;
        std::atomic<float>* fftSize = nullptr;
        std::atomic<float>* bSource = nullptr;
        std::atomic<float>* cornerGainTL = nullptr;
        std::atomic<float>* cornerGainBR = nullptr;
        std::atomic<float>* dryWet = nullptr;
        std::atomic<float>* outGain = nullptr;
        std::atomic<float>* limiter = nullptr;
        std::atomic<float>* gateOn = nullptr;
        std::atomic<float>* gateThresh = nullptr;
        std::atomic<float>* autoMute = nullptr;
        std::atomic<float>* autoMuteMs = nullptr;
    } params;

    void switchFftOrder (int newOrder);
    void primeEngine (int slot, int order);

    struct Engine
    {
        cortex::dsp::STFT stft;
        cortex::dsp::CepstralMorph morph;
    };

    // Two engine slots, so an FFT size change can crossfade from the old size to
    // the new one instead of clicking (BRIEF.md §7).
    std::array<std::array<Engine, maxChannels>, 2> engines;
    int activeSlot = 0;
    int fadeRemaining = 0;
    int fadeLength = 1;
    int previousOrder = 11;

    // Input history: gated A/B to prime a newly sized engine, raw A for the dry path.
    static constexpr int historySize = 2 * cortex::dsp::maxFftSize;
    std::array<std::vector<float>, maxChannels> historyA, historyB, historyDry;
    int historyPos = 0;

    juce::dsp::Limiter<float> limiter;

    juce::SmoothedValue<float> gateGainA, gateGainB, wetMix, outGain;
    std::array<float, maxChannels> dcX1 {}, dcY1 {};
    float dcCoeff = 0.9993f;
    int activeOrder = -1;
    int silentSamples = 0;
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CortexAudioProcessor)
};
