#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
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

// Three sources feed one cepstral morph: the live Input (the track, the
// sidechain, or the instrument's oscillator), Brain 1 and Brain 2 - each brain
// rendering its best match for the live input. Two triangles set how much each
// source gives to the output's envelope and to its detail.
class CortexAudioProcessor final : public juce::AudioProcessor,
                                   private juce::AudioProcessorValueTreeState::Listener,
                                   private juce::Timer
{
public:
    // The same processor builds both plugins: the effect (Input is the track
    // or sidechain) and Cortex Instrument (Input is a MIDI-played oscillator).
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

    // Sets the theme parameter from the per-user setting (Settings.h). The
    // theme is never part of the saved state.
    void syncThemeFromSettings();

    juce::AudioProcessorValueTreeState apvts;

    int currentFftSize() const;

    // What getLatencySamples() is heading for: FFT size plus the brain alignment
    // delay. The host is told on the message thread (timer), never from audio.
    int getAlignedLatencySamples() const { return currentFftSize() + reportedBrainDelay.load(); }

    // Brain management (message thread). Brain 1 is index 0, Brain 2 index 1.
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
    juce::Array<juce::File> getBrainFiles (int brain = 0) const { return slots[(size_t) brain].files; }

    // Links between the brains' controls (message thread). A linked control
    // (index into ParamIDs::BrainIds::all()) moves on both brains together:
    // whichever brain's knob you turn, the other follows to the same value.
    // Saved with the session. Switching a link on copies `fromBrain`'s value
    // to the other brain.
    static constexpr int numBrainControls = 13;
    bool isLinked (int control) const { return linked[(size_t) control].load (std::memory_order_relaxed); }
    void setLinked (int control, bool shouldLink, int fromBrain = 0);
    bool allLinked() const;
    void setAllLinked (bool shouldLink, int fromBrain = 0);

    // What a brain listens to right now (its Listen parameter decoded for
    // this plugin). The audio thread uses the same mapping.
    cortex::Listen listenSource (int brain) const
    {
        return cortex::listenFromChoice ((int) params.listen[(size_t) brain]->load(), instrument);
    }

    // Output capture (message thread). Writes the last `getCaptureSeconds()`
    // of output (less, if the plugin has run for less) as a 24-bit WAV at the
    // host rate. Returns the number of seconds written, or 0 with `error` set.
    double saveCapture (const juce::File& wav, juce::String& error) const;
    int getCaptureSeconds() const { return (int) params.captureSeconds->load(); }
    double getCaptureSampleRate() const { return currentSampleRate; }

    // Latest morph frame (left channel) for the editor's displays.
    bool readMorphSnapshot (cortex::dsp::MorphSnapshot& out) const { return morphSnapshots.read (out); }

    // Per-block peak levels and gate state for the editor's meters.
    struct Levels
    {
        float inputPeak = 0.0f;    // linear, the Input before the gate
        float outputPeak = 0.0f;   // linear, after output gain and limiter
        bool gateOpen = true;
    };
    Levels getLevels() const
    {
        return { inputPeakLevel.load (std::memory_order_relaxed), outputPeakLevel.load (std::memory_order_relaxed),
                 gateOpenFlag.load (std::memory_order_relaxed) };
    }

private:
    static constexpr int maxChannels = 2;
    static constexpr int numSources = cortex::dsp::numSources;
    using PerSourceInt = std::array<int, numSources>;

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;
    void onBrainBuilt (int brain, std::shared_ptr<const cortex::brain::Brain>, const juce::String& message);
    cortex::brain::BrainSettings currentBrainSettings (int brain) const;
    cortex::brain::SearchParams readSearchParams (int brain) const;
    void updateLatency();
    cortex::dsp::MorphParams readMorphParams() const;

    static BusesProperties busesFor (bool isInstrument);

    const bool instrument;
    cortex::synth::MonoSynth monoSynth;
    cortex::dsp::CutoffModulator cutoffModulator;

    struct Params
    {
        std::atomic<float>* envX = nullptr;
        std::atomic<float>* envY = nullptr;
        std::atomic<float>* detailX = nullptr;
        std::atomic<float>* detailY = nullptr;
        std::atomic<float>* linkTriangles = nullptr;
        std::atomic<float>* phaseMode = nullptr;
        std::atomic<float>* fftSize = nullptr;
        std::atomic<float>* lowLatency = nullptr;
        std::atomic<float>* inputSource = nullptr;   // effect only
        std::array<std::atomic<float>*, cortex::dsp::numSources> cutoff {}, formant {}, freezeEnv {}, freezeDetail {};

        std::atomic<float>* dryWet = nullptr;
        std::atomic<float>* outGain = nullptr;
        std::atomic<float>* limiter = nullptr;
        std::atomic<float>* gateOn = nullptr;
        std::atomic<float>* gateThresh = nullptr;
        std::atomic<float>* autoMute = nullptr;
        std::atomic<float>* autoMuteMs = nullptr;

        std::atomic<float>* lfoRate = nullptr;
        std::atomic<float>* lfoDepth = nullptr;
        std::atomic<float>* lfoShape = nullptr;
        std::atomic<float>* lfoTarget = nullptr;
        std::atomic<float>* envDepth = nullptr;
        std::atomic<float>* envAttack = nullptr;
        std::atomic<float>* envRelease = nullptr;
        // Per brain: slicing and search.
        template <typename T> using PerBrain = std::array<T, numBrains>;
        PerBrain<std::atomic<float>*> blockSize {}, overlap {}, chaos {}, memory {}, cepstralGlide {},
                                      matchTimbre {}, matchPitch {}, envTransplant {}, searchMode {},
                                      novelty {}, boredom {}, stickyness {}, listen {};
        std::atomic<float>* captureSeconds = nullptr;

        std::atomic<float>* oscWave = nullptr;      // instrument only
        std::atomic<float>* oscGlide = nullptr;
        std::atomic<float>* ampAttack = nullptr;
        std::atomic<float>* ampRelease = nullptr;
    } params;

    void switchEngine (int newOrder, int newBrainDelay, const PerSourceInt& newDelays, const cortex::dsp::MorphParams&);
    void primeEngine (int slot, int order, const PerSourceInt& delays, const cortex::dsp::MorphParams&);

    struct Engine
    {
        cortex::dsp::STFT stft;
        cortex::dsp::CepstralMorph morph;
    };

    // Two engine slots, so an FFT size or alignment change can crossfade from
    // the old setup to the new one instead of clicking (BRIEF.md §7).
    std::array<std::array<Engine, maxChannels>, 2> engines;
    int activeSlot = 0;
    int fadeRemaining = 0;
    int fadeLength = 1;
    int activeOrder = -1, previousOrder = 11;
    int activeBrainDelay = 0, previousBrainDelay = 0;   // brain latency: dry path and reported latency
    PerSourceInt activeDelays {}, previousDelays {};    // per source, to line all three up
    std::atomic<int> reportedBrainDelay { 0 };

    // Per-source history (gated, read at each source's alignment delay) for the
    // engines and priming, plus the dry signal. Must cover 2N + the largest
    // brain delay (block 4096 + search delay 4096).
    static constexpr int historySize = 32768;
    std::array<std::array<std::vector<float>, maxChannels>, cortex::dsp::numSources> history;
    std::array<std::vector<float>, maxChannels> historyDry;
    int historyPos = 0;

    // The previous sample of each brain and of the output (mono), for a brain
    // that listens to the other brain or to Cortex's output: one sample behind,
    // so the order the brains run in never matters and a loop is harmless.
    std::array<float, numBrains> lastBrainOut {};
    float lastOutMono = 0.0f;

    juce::dsp::Limiter<float> limiter;

    juce::SmoothedValue<float> gateGain, wetMix, outGain;
    std::atomic<float> inputPeakLevel { 0.0f }, outputPeakLevel { 0.0f };
    std::atomic<bool> gateOpenFlag { true };
    std::array<float, maxChannels> dcX1 {}, dcY1 {};
    float dcCoeff = 0.9993f;
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
    std::array<std::atomic<bool>, numBrains> rebuildRequested {};
    std::array<juce::uint32, numBrains> rebuildDueMs {};

    // Links live in a "Links" child of the state tree (so they save with the
    // session) and in this atomic mirror for the parameter listener, which a
    // host may call from the audio thread.
    std::array<std::atomic<bool>, numBrainControls> linked {};
    void readLinksFromState();
    void mirrorLinked (const juce::String& changedId);
    std::atomic<int> mirrorPending { -1 };   // a control to mirror from the timer (change came off the message thread)

    // Output capture: a ring of the last maxCaptureSeconds of output (after
    // gain and limiter), written by the audio thread. saveCapture() copies the
    // most recent captureSeconds out of it on the message thread; the write
    // head only reaches that region again after a full lap, seconds later.
    std::array<std::vector<float>, maxChannels> captureRing;
    int captureCapacity = 0;
    std::atomic<int> capturePos { 0 };
    std::atomic<std::int64_t> captureWritten { 0 };
    mutable std::mutex brainLock;   // never taken on the audio thread

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CortexAudioProcessor)
};
