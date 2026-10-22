#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include "state/BrainCache.h"

namespace
{
    // Plugin state keeps a reference to the brain (file list + cache path),
    // never raw audio. Populated in Phase 2.
    const juce::Identifier brainStateId { "Brain" };
    const juce::Identifier sourceId { "Source" };
    const juce::Identifier pathId { "path" };

    constexpr double dcBlockHz = 5.0;
    constexpr double rampSeconds = 0.01;
    constexpr double crossfadeSeconds = 0.03;
}

juce::AudioProcessor::BusesProperties CortexAudioProcessor::busesFor (bool isInstrument)
{
    if (isInstrument)
        return BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true);

    return BusesProperties()
               .withInput ("Input", juce::AudioChannelSet::stereo(), true)
               .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
               .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false);
}

CortexAudioProcessor::CortexAudioProcessor (bool isInstrument)
    : AudioProcessor (busesFor (isInstrument)),
      apvts (*this, nullptr, "CortexParams", cortex::createParameterLayout (isInstrument)),
      instrument (isInstrument),
      builder ([this] (std::shared_ptr<const cortex::brain::Brain> b, const juce::String& m) { onBrainBuilt (std::move (b), m); })
{
    formatManager.registerBasicFormats();

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    params.morphX = get (cortex::ParamIDs::morphX);
    params.morphY = get (cortex::ParamIDs::morphY);
    params.cutoffA = get (cortex::ParamIDs::cutoffA);
    params.cutoffB = get (cortex::ParamIDs::cutoffB);
    params.phaseMode = get (cortex::ParamIDs::phaseMode);
    params.fftSize = get (cortex::ParamIDs::fftSize);
    params.bSource = get (cortex::ParamIDs::bSource);
    params.cornerGainTL = get (cortex::ParamIDs::cornerGainTL);
    params.cornerGainBR = get (cortex::ParamIDs::cornerGainBR);
    params.dryWet = get (cortex::ParamIDs::dryWet);
    params.outGain = get (cortex::ParamIDs::outGain);
    params.limiter = get (cortex::ParamIDs::limiter);
    params.gateOn = get (cortex::ParamIDs::gateOn);
    params.gateThresh = get (cortex::ParamIDs::gateThresh);
    params.autoMute = get (cortex::ParamIDs::autoMute);
    params.autoMuteMs = get (cortex::ParamIDs::autoMuteMs);
    params.lowLatency = get (cortex::ParamIDs::lowLatency);
    params.freezeEnvA = get (cortex::ParamIDs::freezeEnvA);
    params.freezeDetailA = get (cortex::ParamIDs::freezeDetailA);
    params.freezeEnvB = get (cortex::ParamIDs::freezeEnvB);
    params.freezeDetailB = get (cortex::ParamIDs::freezeDetailB);
    params.chaos = get (cortex::ParamIDs::chaos);
    params.memory = get (cortex::ParamIDs::memory);
    params.formantA = get (cortex::ParamIDs::formantA);
    params.formantB = get (cortex::ParamIDs::formantB);
    params.lfoRate = get (cortex::ParamIDs::lfoRate);
    params.lfoDepth = get (cortex::ParamIDs::lfoDepth);
    params.lfoShape = get (cortex::ParamIDs::lfoShape);
    params.lfoTarget = get (cortex::ParamIDs::lfoTarget);
    params.envDepth = get (cortex::ParamIDs::envDepth);
    params.envAttack = get (cortex::ParamIDs::envAttack);
    params.envRelease = get (cortex::ParamIDs::envRelease);
    params.matchTimbre = get (cortex::ParamIDs::matchTimbre);
    params.matchPitch = get (cortex::ParamIDs::matchPitch);
    params.envTransplant = get (cortex::ParamIDs::envTransplant);
    params.blockSize = get (cortex::ParamIDs::blockSize);
    params.overlap = get (cortex::ParamIDs::overlap);
    params.novelty = get (cortex::ParamIDs::novelty);
    params.boredom = get (cortex::ParamIDs::boredom);
    params.stickyness = get (cortex::ParamIDs::stickyness);
    params.searchMode = get (cortex::ParamIDs::searchMode);
    if (instrument)
    {
        params.oscWave = get (cortex::ParamIDs::oscWave);
        params.oscGlide = get (cortex::ParamIDs::oscGlide);
        params.ampAttack = get (cortex::ParamIDs::ampAttack);
        params.ampRelease = get (cortex::ParamIDs::ampRelease);
    }

    for (auto* id : { cortex::ParamIDs::fftSize, cortex::ParamIDs::blockSize, cortex::ParamIDs::overlap, cortex::ParamIDs::lowLatency })
        apvts.addParameterListener (id, this);
    updateLatency();
    startTimerHz (10);
}

CortexAudioProcessor::~CortexAudioProcessor()
{
    stopTimer();
    builder.cancel();
    for (auto* id : { cortex::ParamIDs::fftSize, cortex::ParamIDs::blockSize, cortex::ParamIDs::overlap, cortex::ParamIDs::lowLatency })
        apvts.removeParameterListener (id, this);
}

int CortexAudioProcessor::currentFftSize() const
{
    if (params.lowLatency->load() > 0.5f)
        return cortex::lowLatencyFftSize;
    const auto index = juce::jlimit (0, (int) std::size (cortex::fftSizes) - 1, (int) params.fftSize->load());
    return cortex::fftSizes[index];
}

void CortexAudioProcessor::updateLatency()
{
    setLatencySamples (getAlignedLatencySamples());
}

void CortexAudioProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == cortex::ParamIDs::fftSize || parameterID == cortex::ParamIDs::lowLatency)
        updateLatency();
    else
        rebuildRequested = true;   // block size / overlap: rebuilt (debounced) by the timer
}

cortex::dsp::MorphParams CortexAudioProcessor::readMorphParams() const
{
    cortex::dsp::MorphParams p;
    p.x = params.morphX->load();
    p.y = params.morphY->load();
    p.cutoffA = params.cutoffA->load();
    p.cutoffB = params.cutoffB->load();
    p.phaseMode = (cortex::dsp::PhaseMode) juce::jlimit (0, 3, (int) params.phaseMode->load());
    p.cornerGainTLdB = params.cornerGainTL->load();
    p.cornerGainBRdB = params.cornerGainBR->load();
    p.freezeEnvA = params.freezeEnvA->load() > 0.5f;
    p.freezeDetailA = params.freezeDetailA->load() > 0.5f;
    p.freezeEnvB = params.freezeEnvB->load() > 0.5f;
    p.freezeDetailB = params.freezeDetailB->load() > 0.5f;
    p.formantA = params.formantA->load();
    p.formantB = params.formantB->load();
    return p;
}

void CortexAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    monoSynth.prepare (sampleRate);
    cutoffModulator.prepare (sampleRate);

    for (auto& slot : engines)
        for (auto& e : slot)
        {
            e.stft.prepare();
            e.morph.prepare (sampleRate);
        }

    for (int ch = 0; ch < maxChannels; ++ch)
        for (auto* h : { &historyA, &historyB, &historyDry })
            (*h)[(size_t) ch].assign ((size_t) historySize, 0.0f);
    historyPos = 0;
    fadeRemaining = 0;
    fadeLength = juce::roundToInt (crossfadeSeconds * sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), (juce::uint32) maxChannels };
    limiter.prepare (spec);
    limiter.setThreshold (-0.3f);
    limiter.setRelease (50.0f);

    for (auto* s : { &gateGainA, &gateGainB, &wetMix, &outGain })
        s->reset (sampleRate, rampSeconds);
    gateGainA.setCurrentAndTargetValue (1.0f);
    gateGainB.setCurrentAndTargetValue (1.0f);
    wetMix.setCurrentAndTargetValue (params.dryWet->load());
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.outGain->load()));

    dcCoeff = (float) (1.0 - juce::MathConstants<double>::twoPi * dcBlockHz / sampleRate);
    dcX1.fill (0.0f);
    dcY1.fill (0.0f);
    silentSamples = 0;
    activeOrder = -1;   // forces an engine setup on the first block
    activeBrainDelay = previousBrainDelay = 0;

    // Brains are built at the host rate; a new rate needs a rebuild (usually a cache hit).
    {
        std::lock_guard<std::mutex> l (brainLock);
        if (! brainFiles.isEmpty() && builtSettings.sampleRate != sampleRate)
            rebuildRequested = true;
    }

    updateLatency();
}

void CortexAudioProcessor::releaseResources() {}

bool CortexAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (instrument)
        return layouts.inputBuses.isEmpty() || mainIn.isDisabled();
    if (mainIn != mainOut)
        return false;

    const auto sidechain = layouts.getChannelSet (true, 1);
    return sidechain.isDisabled()
        || sidechain == juce::AudioChannelSet::mono()
        || sidechain == juce::AudioChannelSet::stereo();
}

void CortexAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (maxChannels, getMainBusNumOutputChannels(), buffer.getNumChannels());

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // Instrument: A is the MIDI-played oscillator, written where an effect's input would be.
    if (instrument && numChannels > 0)
    {
        monoSynth.setWaveform ((cortex::synth::Waveform) juce::jlimit (0, 3, (int) params.oscWave->load()));
        monoSynth.setGlide (params.oscGlide->load());
        monoSynth.setEnvelope (params.ampAttack->load(), params.ampRelease->load());
        monoSynth.render (midi, buffer.getWritePointer (0), numSamples);
        for (int ch = 1; ch < numChannels; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
    }

    // Input B: the sidechain, or the brain rendering a match for the live input.
    auto* brainRenderer = renderer.acquire();
    const bool useBrain = instrument || params.bSource->load() >= 0.5f;   // the instrument has no sidechain
    if (brainRenderer != nullptr)
    {
        brainRenderer->setBlocking (isNonRealtime());
        brainRenderer->setSearch (readSearchParams(), (cortex::brain::SearchMode) juce::jlimit (0, 3, (int) params.searchMode->load()));
    }
    if (! useBrain)
        brainRenderer = nullptr;

    // With B on Brain but no brain loaded yet, stay transparent rather than
    // morphing towards silence.
    auto morphParams = readMorphParams();
    if (useBrain && brainRenderer == nullptr)
        morphParams.x = morphParams.y = 0.0f;

    // Cutoff modulation: LFO + envelope follower on A (left channel).
    {
        cortex::dsp::CutoffModulator::Settings mod;
        mod.lfoRateHz = params.lfoRate->load();
        mod.lfoDepth = params.lfoDepth->load();
        mod.shape = (cortex::dsp::CutoffModulator::Shape) juce::jlimit (0, 4, (int) params.lfoShape->load());
        mod.target = (cortex::dsp::CutoffModulator::Target) juce::jlimit (0, 2, (int) params.lfoTarget->load());
        mod.envDepth = params.envDepth->load();
        mod.envAttackMs = params.envAttack->load();
        mod.envReleaseMs = params.envRelease->load();
        const auto offsets = cutoffModulator.process (mod, numChannels > 0 ? buffer.getReadPointer (0) : nullptr, numSamples);
        morphParams.cutoffA = juce::jlimit (4.0f, 200.0f, morphParams.cutoffA + offsets.a);
        morphParams.cutoffB = juce::jlimit (4.0f, 200.0f, morphParams.cutoffB + offsets.b);
    }

    // The brain answers one block (plus search time) late, so A is delayed by the
    // same amount to line the two up. A change of FFT size or of that delay
    // crossfades to a freshly primed engine. A change that arrives mid-crossfade
    // waits for the next block after it.
    // Low latency: smallest FFT, and A is not held back for the brain - your
    // note/attack comes straight through and the brain's timbre follows a
    // grain behind. Total latency is then just the 512-sample FFT.
    const bool lowLatency = params.lowLatency->load() > 0.5f;
    if (brainRenderer != nullptr)
        brainRenderer->setLowLatency (lowLatency);
    const int order = lowLatency ? cortex::dsp::minFftOrder
                                 : cortex::dsp::minFftOrder + juce::jlimit (0, 3, (int) params.fftSize->load());
    const int brainDelay = brainRenderer != nullptr && ! lowLatency ? brainRenderer->getLatencySamples() : 0;
    if ((order != activeOrder || brainDelay != activeBrainDelay) && fadeRemaining == 0)
        switchEngine (order, brainDelay, morphParams);

    for (auto& slot : engines)
        for (auto& e : slot)
            e.morph.setTargets (morphParams);

    std::array<const float*, maxChannels> sidechain {};
    const auto* scBus = getBus (true, 1);
    const int scChannels = scBus != nullptr && scBus->isEnabled() ? scBus->getNumberOfChannels() : 0;
    if (! useBrain && scChannels > 0)
        for (int ch = 0; ch < maxChannels; ++ch)
            sidechain[(size_t) ch] = buffer.getReadPointer (getChannelIndexInProcessBlockBuffer (true, 1, juce::jmin (ch, scChannels - 1)));

    // Gate and auto-mute, decided per block from peak levels.
    float peakA = 0.0f, peakB = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        peakA = juce::jmax (peakA, buffer.getMagnitude (ch, 0, numSamples));
        if (sidechain[(size_t) ch] != nullptr)
            peakB = juce::jmax (peakB, juce::FloatVectorOperations::findMaximum (sidechain[(size_t) ch], numSamples),
                                -juce::FloatVectorOperations::findMinimum (sidechain[(size_t) ch], numSamples));
    }
    if (brainRenderer != nullptr)
        peakB = peakA;   // the brain only sounds while it has input to follow

    const float threshold = juce::Decibels::decibelsToGain (params.gateThresh->load());
    const bool gateOn = params.gateOn->load() > 0.5f;
    gateGainA.setTargetValue (gateOn && peakA < threshold ? 0.0f : 1.0f);
    gateGainB.setTargetValue (gateOn && peakB < threshold ? 0.0f : 1.0f);

    bool muted = false;
    if (params.autoMute->load() > 0.5f)
    {
        silentSamples = (peakA < threshold && peakB < threshold) ? silentSamples + numSamples : 0;
        muted = silentSamples > (int) (params.autoMuteMs->load() * 0.001 * currentSampleRate);
    }
    else
    {
        silentSamples = 0;
    }
    for (auto& slot : engines)
        for (auto& e : slot)
            e.stft.setSkipFrames (muted);

    wetMix.setTargetValue (params.dryWet->load());
    outGain.setTargetValue (juce::Decibels::decibelsToGain (params.outGain->load()));

    std::array<float*, maxChannels> out {};
    for (int ch = 0; ch < numChannels; ++ch)
        out[(size_t) ch] = buffer.getWritePointer (ch);

    constexpr int historyMask = historySize - 1;
    const int newDelay = activeBrainDelay, oldDelay = previousBrainDelay;
    const int newLatency = (1 << activeOrder) + newDelay;
    const int oldLatency = (1 << previousOrder) + oldDelay;
    auto& active = engines[(size_t) activeSlot];
    auto& fading = engines[(size_t) (1 - activeSlot)];

    for (int i = 0; i < numSamples; ++i)
    {
        const float ga = gateGainA.getNextValue();
        const float gb = gateGainB.getNextValue();
        const float wet = wetMix.getNextValue();
        const float og = outGain.getNextValue();

        // Equal-power crossfade from the previous FFT size, if one is running.
        float gainNew = 1.0f, gainOld = 0.0f;
        if (fadeRemaining > 0)
        {
            const float t = 1.0f - (float) fadeRemaining / (float) fadeLength;
            gainNew = std::sin (t * juce::MathConstants<float>::halfPi);
            gainOld = std::cos (t * juce::MathConstants<float>::halfPi);
        }

        // The brain is mono: it matches the mono mix of A and feeds every channel's B.
        float brainOut = 0.0f;
        if (brainRenderer != nullptr)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                mono += out[(size_t) ch][i];
            brainOut = brainRenderer->processSample (mono / (float) juce::jmax (1, numChannels));
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto c = (size_t) ch;
            const float a = out[c][i];
            const float b = sidechain[c] != nullptr ? sidechain[c][i] : brainOut;
            const float gatedA = a * ga, gatedB = b * gb;

            historyA[c][(size_t) historyPos] = gatedA;
            historyB[c][(size_t) historyPos] = gatedB;
            historyDry[c][(size_t) historyPos] = a;

            // Each engine reads A at its own alignment delay.
            auto runEngine = [&] (Engine& e, int delay)
            {
                const float delayedA = historyA[c][(size_t) ((historyPos - delay) & historyMask)];
                return e.stft.processSample (delayedA, gatedB, [&e] (const float* fa, const float* fb, float* fo)
                                             { e.morph.processFrame (fa, fb, fo); });
            };

            float w = runEngine (active[c], newDelay);
            float dry = historyDry[c][(size_t) ((historyPos - newLatency) & historyMask)];

            if (fadeRemaining > 0)
            {
                w = gainNew * w + gainOld * runEngine (fading[c], oldDelay);
                dry = gainNew * dry + gainOld * historyDry[c][(size_t) ((historyPos - oldLatency) & historyMask)];
            }

            // DC blocker on the wet path.
            const float y = w - dcX1[c] + dcCoeff * dcY1[c];
            dcX1[c] = w;
            dcY1[c] = y;

            out[c][i] = (dry + wet * (y - dry)) * og;
        }

        historyPos = (historyPos + 1) & historyMask;
        if (fadeRemaining > 0)
            --fadeRemaining;
    }

    playingBlock.store (brainRenderer != nullptr ? brainRenderer->getLastChosenBlock() : -1, std::memory_order_relaxed);
    if (numChannels > 0)
        morphSnapshots.publish (engines[(size_t) activeSlot][0].morph.getSnapshot());

    if (params.limiter->load() > 0.5f && numChannels > 0)
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
        limiter.process (juce::dsp::ProcessContextReplacing<float> (block));
    }
}

void CortexAudioProcessor::switchEngine (int newOrder, int newBrainDelay, const cortex::dsp::MorphParams& morphParams)
{
    if (activeOrder < 0)
    {
        // First block after prepare: nothing to fade from.
        primeEngine (activeSlot, newOrder, newBrainDelay, morphParams);
        previousOrder = activeOrder = newOrder;
        previousBrainDelay = activeBrainDelay = newBrainDelay;
    }
    else
    {
        previousOrder = activeOrder;
        previousBrainDelay = activeBrainDelay;
        activeSlot = 1 - activeSlot;
        primeEngine (activeSlot, newOrder, newBrainDelay, morphParams);
        activeOrder = newOrder;
        activeBrainDelay = newBrainDelay;
        fadeRemaining = fadeLength;
    }

    reportedBrainDelay.store (newBrainDelay);
}

void CortexAudioProcessor::primeEngine (int slot, int order, int brainDelay, const cortex::dsp::MorphParams& morphParams)
{
    // Run the last 2N input samples (A at this engine's alignment delay) through
    // the new engine so its overlap-add buffer is already in steady state when
    // the crossfade starts. Costs 16 frames once per switch.
    constexpr int historyMask = historySize - 1;
    const int n = 1 << order;

    for (int ch = 0; ch < maxChannels; ++ch)
    {
        auto& e = engines[(size_t) slot][(size_t) ch];
        e.stft.setOrder (order);
        e.morph.setOrder (order);
        e.morph.setTargets (morphParams);
        e.morph.snapToTargets();

        const auto& ha = historyA[(size_t) ch];
        const auto& hb = historyB[(size_t) ch];
        for (int i = 2 * n; i > 0; --i)
        {
            const auto idx = (size_t) ((historyPos - i) & historyMask);
            const auto idxA = (size_t) ((historyPos - i - brainDelay) & historyMask);
            e.stft.processSample (ha[idxA], hb[idx], [&e] (const float* fa, const float* fb, float* fo)
                                  { e.morph.processFrame (fa, fb, fo); });
        }
    }
}

juce::AudioProcessorEditor* CortexAudioProcessor::createEditor()
{
    return new CortexAudioProcessorEditor (*this);
}

void CortexAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName (brainStateId), nullptr);

    // The brain is stored by reference (source files); the analysis comes back
    // from the .cortexbrain cache, which is keyed on those files and settings.
    juce::ValueTree brainTree (brainStateId);
    for (const auto& f : brainFiles)
        brainTree.appendChild (juce::ValueTree (sourceId, { { pathId, f.getFullPathName() } }), nullptr);
    state.appendChild (brainTree, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CortexAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            juce::StringArray paths;
            for (const auto& source : state.getChildWithName (brainStateId))
                paths.add (source[pathId].toString());
            state.removeChild (state.getChildWithName (brainStateId), nullptr);
            apvts.replaceState (state);

            // Hosts never call this on the audio thread. The build itself is
            // asynchronous, and usually a fast cache load.
            if (paths.isEmpty())
                clearBrain();
            else
                loadBrainFrom (paths);
        }
    }

    updateLatency();
}

//==============================================================================
// Brain management

cortex::brain::BrainSettings CortexAudioProcessor::currentBrainSettings() const
{
    cortex::brain::BrainSettings s;
    s.blockSize = (int) params.blockSize->load();
    s.overlap = params.overlap->load();
    s.sampleRate = currentSampleRate;
    return s;
}

cortex::brain::SearchParams CortexAudioProcessor::readSearchParams() const
{
    cortex::brain::SearchParams p;
    p.timbreWeight = params.matchTimbre->load();
    p.pitchWeight = params.matchPitch->load();
    p.envTransplant = params.envTransplant->load();
    p.transplantCutoff = params.cutoffB->load();
    return cortex::brain::applySearchKnobs (p, { params.novelty->load(), params.boredom->load(), params.stickyness->load() },
                                            params.chaos->load(), params.memory->load());
}

void CortexAudioProcessor::loadBrainFrom (const juce::StringArray& filesOrFolders)
{
    brainFiles = cortex::brain::collectAudioFiles (filesOrFolders, formatManager);
    rebuildBrain();
}

void CortexAudioProcessor::clearBrain()
{
    builder.cancel();
    brainFiles.clear();
    renderer.publishClear();
    std::lock_guard<std::mutex> l (brainLock);
    currentBrain = nullptr;
    brainMessage = {};
}

void CortexAudioProcessor::rebuildBrain()
{
    rebuildRequested = false;
    rebuildDueMs = 0;

    if (brainFiles.isEmpty())
    {
        clearBrain();
        return;
    }

    builder.start ({ brainFiles, currentBrainSettings(), cortex::BrainCache::defaultDirectory() });
}

void CortexAudioProcessor::onBrainBuilt (std::shared_ptr<const cortex::brain::Brain> brain, const juce::String& message)
{
    // Builder thread: the renderer allocates here, never on the audio thread.
    renderer.publish (std::make_unique<cortex::brain::Renderer> (brain));

    std::lock_guard<std::mutex> l (brainLock);
    builtSettings = brain->settings;
    currentBrain = std::move (brain);
    brainMessage = message;
}

void CortexAudioProcessor::timerCallback()
{
    renderer.collectGarbage();

    if (getLatencySamples() != getAlignedLatencySamples())
        updateLatency();

    // Debounce block size / overlap / sample-rate changes so dragging a knob
    // doesn't restart the build on every step.
    const auto now = juce::Time::getMillisecondCounter();
    if (rebuildRequested.exchange (false))
        rebuildDueMs = now + 600;
    if (rebuildDueMs != 0 && now >= rebuildDueMs)
        rebuildBrain();
}

CortexAudioProcessor::BrainStatus CortexAudioProcessor::getBrainStatus() const
{
    BrainStatus s;
    s.building = builder.isBuilding();
    s.progress = builder.getProgress();

    std::lock_guard<std::mutex> l (brainLock);
    s.playingBlock = playingBlock.load (std::memory_order_relaxed);
    s.message = s.building ? builder.getStatus() : brainMessage;
    if (currentBrain != nullptr)
    {
        s.numBlocks = (int) currentBrain->numBlocks();
        s.numSounds = (int) currentBrain->sounds.size();
    }
    return s;
}

std::shared_ptr<const cortex::brain::Brain> CortexAudioProcessor::getBrain() const
{
    std::lock_guard<std::mutex> l (brainLock);
    return currentBrain;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CortexAudioProcessor (JucePlugin_IsSynth != 0);
}
