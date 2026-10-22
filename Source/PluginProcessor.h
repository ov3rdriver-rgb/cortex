#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "brain/BrainBuilder.h"
#include "brain/RealtimeExchange.h"
#include "brain/Renderer.h"
#include "dsp/CepstralMorph.h"
#include "dsp/STFT.h"
#include "dsp/CutoffModulator.h"
#include "dsp/SnapshotExchange.h"
#include "synth/MonoSynth.h"

#ifndef JucePlugin_IsSynth
 #define JucePlugin_IsSynth 0
#endif

class CortexAudioProcessor final : public juce::AudioProcessor,
                                   private juce::AudioProcessorValueTreeState::Listener,
                                   private juce::Timer
{
public:
    // The same processor builds both plugins: the effect (input A is the
    // track's audio) and Cortex Instrument (input A is a MIDI-played oscillator).
    explicit CortexAudioProcessor (bool isInstrument = JucePlugin_IsSynth != 0);

    bool isInstrument() const noexcept { return instrument; }
    ~CortexAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return instrument; }
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

    // What getLatencySamples() is heading for: FFT size plus the brain alignment
    // delay. The host is told on the message thread (timer), never from audio.
    int getAlignedLatencySamples() const { return currentFftSize() + reportedBrainDelay.load(); }

    // Brain management (message thread). Brain 1 (index 0) feeds input B;
    // Brain 2 (index 1) can feed input A instead of the live input (A Source),
    // so the morph runs between two brains' matches - the 4-corner brain.
    static constexpr int numBrains = 2;
    void loadBrainFrom (const juce::StringArray& filesOrFolders, int brain = 0);
    void clearBrain (int brain = 0);
    void rebuildBrain (int brain = 0);

    struct BrainStatus
    {
        bool building = false;
        float progress = 0.0f;
        juce::String message;
        int numBlocks = 0;
        int numSounds = 0;
        int playingBlock = -1;
    };
    BrainStatus getBrainStatus (int brain = 0) const;
    std::shared_ptr<const cortex::brain::Brain> getBrain (int brain = 0) const;

    // Latest morph frame (left channel) for the editor's displays.
    bool readMorphSnapshot (cortex::dsp::MorphSnapshot& out) const { return morphSnapshots.read (out); }
    juce::Array<juce::File> getBrainFiles (int brain = 0) const { return slots[(size_t) brain].files; }

private:
    static constexpr int maxChannels = 2;

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;
    void onBrainBuilt (int brain, std::shared_ptr<const cortex::brain::Brain>, const juce::String& message);
    cortex::brain::BrainSettings currentBrainSettings() const;
    cortex::brain::SearchParams readSearchParams() const;
    void updateLatency();
    cortex::dsp::MorphParams readMorphParams() const;

    static BusesProperties busesFor (bool isInstrument);

    const bool instrument;
    cortex::synth::MonoSynth monoSynth;
    cortex::dsp::CutoffModulator cutoffModulator;

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
        std::atomic<float>* lowLatency = nullptr;
        std::atomic<float>* aSource = nullptr;
        std::atomic<float>* freezeEnvA = nullptr;
        std::atomic<float>* freezeDetailA = nullptr;
        std::atomic<float>* freezeEnvB = nullptr;
        std::atomic<float>* freezeDetailB = nullptr;
        std::atomic<float>* chaos = nullptr;
        std::atomic<float>* cepstralGlide = nullptr;
        std::atomic<float>* memory = nullptr;
        std::atomic<float>* formantA = nullptr;
        std::atomic<float>* formantB = nullptr;
        std::atomic<float>* lfoRate = nullptr;
        std::atomic<float>* lfoDepth = nullptr;
        std::atomic<float>* lfoShape = nullptr;
        std::atomic<float>* lfoTarget = nullptr;
        std::atomic<float>* envDepth = nullptr;
        std::atomic<float>* envAttack = nullptr;
        std::atomic<float>* envRelease = nullptr;
        std::atomic<float>* matchTimbre = nullptr;
        std::atomic<float>* matchPitch = nullptr;
        std::atomic<float>* envTransplant = nullptr;
        std::atomic<float>* blockSize = nullptr;
        std::atomic<float>* overlap = nullptr;
        std::atomic<float>* novelty = nullptr;
        std::atomic<float>* boredom = nullptr;
        std::atomic<float>* stickyness = nullptr;
        std::atomic<float>* searchMode = nullptr;
        std::atomic<float>* oscWave = nullptr;
        std::atomic<float>* oscGlide = nullptr;
        std::atomic<float>* ampAttack = nullptr;
        std::atomic<float>* ampRelease = nullptr;
    } params;

    void switchEngine (int newOrder, int newBrainDelay, int newADelay, const cortex::dsp::MorphParams&);
    void primeEngine (int slot, int order, int aDelay, const cortex::dsp::MorphParams&);

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
    int activeBrainDelay = 0, previousBrainDelay = 0;   // brain latency: dry path and reported latency
    int activeADelay = 0, previousADelay = 0;           // samples A is delayed to line up with B
    std::atomic<int> reportedBrainDelay { 0 };

    // Input history: gated A (read at the alignment delay) and B for the engines
    // and priming, raw A for the dry path. Must cover 2N + the largest brain
    // delay (block 4096 + search delay 4096).
    static constexpr int historySize = 32768;
    std::array<std::vector<float>, maxChannels> historyA, historyB, historyDry;
    int historyPos = 0;

    juce::dsp::Limiter<float> limiter;

    juce::SmoothedValue<float> gateGainA, gateGainB, wetMix, outGain;
    std::array<float, maxChannels> dcX1 {}, dcY1 {};
    float dcCoeff = 0.9993f;
    int activeOrder = -1;
    int silentSamples = 0;
    double currentSampleRate = 48000.0;

    // Brains. Each is built on its builder's thread and handed to the audio
    // thread through its `renderer` exchange.
    struct BrainSlot
    {
        juce::Array<juce::File> files;
        std::unique_ptr<cortex::brain::BrainBuilder> builder;
        cortex::RealtimeExchange<cortex::brain::Renderer> renderer;
        std::atomic<int> playingBlock { -1 };
        cortex::brain::BrainSettings builtSettings;
        std::shared_ptr<const cortex::brain::Brain> current;   // guarded by brainLock
        juce::String message;                                  // guarded by brainLock
    };

    juce::AudioFormatManager formatManager;
    std::array<BrainSlot, numBrains> slots;
    cortex::SnapshotExchange<cortex::dsp::MorphSnapshot> morphSnapshots;
    std::atomic<bool> rebuildRequested { false };
    juce::uint32 rebuildDueMs = 0;
    mutable std::mutex brainLock;   // never taken on the audio thread

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CortexAudioProcessor)
};
