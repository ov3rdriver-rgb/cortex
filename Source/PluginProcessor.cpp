#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include "dsp/Triangle.h"
#include "state/BrainCache.h"

namespace
{
    // Plugin state keeps a reference to each brain (its source files), never
    // raw audio; the analysis comes back from the .cortexbrain cache.
    const juce::Identifier brainStateIds[] { "Brain", "Brain2" };
    const juce::Identifier sourceId { "Source" };
    const juce::Identifier pathId { "path" };

    constexpr double dcBlockHz = 5.0;
    constexpr double rampSeconds = 0.01;
    constexpr double crossfadeSeconds = 0.03;

    using cortex::dsp::Source;
    namespace id = cortex::ParamIDs;
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
      instrument (isInstrument)
{
    for (int b = 0; b < numBrains; ++b)
        slots[(size_t) b].builder = std::make_unique<cortex::brain::BrainBuilder> (
            [this, b] (std::shared_ptr<const cortex::brain::Brain> brain, const juce::String& m) { onBrainBuilt (b, std::move (brain), m); });

    formatManager.registerBasicFormats();

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    params.envX = get (id::envX);
    params.envY = get (id::envY);
    params.detailX = get (id::detailX);
    params.detailY = get (id::detailY);
    params.linkTriangles = get (id::linkTriangles);
    params.phaseMode = get (id::phaseMode);
    params.fftSize = get (id::fftSize);
    params.lowLatency = get (id::lowLatency);
    if (! instrument)
        params.inputSource = get (id::inputSource);
    params.cutoff = { get (id::cutoffIn), get (id::cutoffB1), get (id::cutoffB2) };
    params.formant = { get (id::formantIn), get (id::formantB1), get (id::formantB2) };
    params.freezeEnv = { get (id::freezeEnvIn), get (id::freezeEnvB1), get (id::freezeEnvB2) };
    params.freezeDetail = { get (id::freezeDetIn), get (id::freezeDetB1), get (id::freezeDetB2) };

    params.dryWet = get (id::dryWet);
    params.outGain = get (id::outGain);
    params.limiter = get (id::limiter);
    params.gateOn = get (id::gateOn);
    params.gateThresh = get (id::gateThresh);
    params.autoMute = get (id::autoMute);
    params.autoMuteMs = get (id::autoMuteMs);

    params.chaos = get (id::chaos);
    params.cepstralGlide = get (id::cepstralGlide);
    params.memory = get (id::memory);
    params.lfoRate = get (id::lfoRate);
    params.lfoDepth = get (id::lfoDepth);
    params.lfoShape = get (id::lfoShape);
    params.lfoTarget = get (id::lfoTarget);
    params.envDepth = get (id::envDepth);
    params.envAttack = get (id::envAttack);
    params.envRelease = get (id::envRelease);
    params.matchTimbre = get (id::matchTimbre);
    params.matchPitch = get (id::matchPitch);
    params.envTransplant = get (id::envTransplant);
    params.blockSize = get (id::blockSize);
    params.overlap = get (id::overlap);
    params.novelty = get (id::novelty);
    params.boredom = get (id::boredom);
    params.stickyness = get (id::stickyness);
    params.searchMode = get (id::searchMode);

    if (instrument)
    {
        params.oscWave = get (id::oscWave);
        params.oscGlide = get (id::oscGlide);
        params.ampAttack = get (id::ampAttack);
        params.ampRelease = get (id::ampRelease);
    }

    for (auto* param : { id::fftSize, id::blockSize, id::overlap, id::lowLatency })
        apvts.addParameterListener (param, this);
    updateLatency();
    startTimerHz (10);
}

CortexAudioProcessor::~CortexAudioProcessor()
{
    stopTimer();
    for (auto& s : slots)
        s.builder->cancel();
    for (auto* param : { id::fftSize, id::blockSize, id::overlap, id::lowLatency })
        apvts.removeParameterListener (param, this);
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
    p.envWeights = cortex::dsp::triangleWeights (params.envX->load(), params.envY->load());
    p.detailWeights = params.linkTriangles->load() > 0.5f
                          ? p.envWeights
                          : cortex::dsp::triangleWeights (params.detailX->load(), params.detailY->load());
    p.phase = (cortex::dsp::PhaseSource) juce::jlimit (0, 3, (int) params.phaseMode->load());
    for (int s = 0; s < numSources; ++s)
    {
        const auto i = (size_t) s;
        p.cutoff[i] = params.cutoff[i]->load();
        p.formant[i] = params.formant[i]->load();
        p.freezeEnv[i] = params.freezeEnv[i]->load() > 0.5f;
        p.freezeDetail[i] = params.freezeDetail[i]->load() > 0.5f;
    }
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
    {
        for (auto& h : history)
            h[(size_t) ch].assign ((size_t) historySize, 0.0f);
        historyDry[(size_t) ch].assign ((size_t) historySize, 0.0f);
    }
    historyPos = 0;
    fadeRemaining = 0;
    fadeLength = juce::roundToInt (crossfadeSeconds * sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), (juce::uint32) maxChannels };
    limiter.prepare (spec);
    limiter.setThreshold (-0.3f);
    limiter.setRelease (50.0f);

    for (auto* s : { &gateGain, &wetMix, &outGain })
        s->reset (sampleRate, rampSeconds);
    gateGain.setCurrentAndTargetValue (1.0f);
    wetMix.setCurrentAndTargetValue (params.dryWet->load());
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.outGain->load()));

    dcCoeff = (float) (1.0 - juce::MathConstants<double>::twoPi * dcBlockHz / sampleRate);
    dcX1.fill (0.0f);
    dcY1.fill (0.0f);
    silentSamples = 0;
    activeOrder = -1;   // forces an engine setup on the first block
    activeBrainDelay = previousBrainDelay = 0;
    activeDelays = previousDelays = {};

    // Brains are built at the host rate; a new rate needs a rebuild (usually a cache hit).
    {
        std::lock_guard<std::mutex> l (brainLock);
        for (const auto& s : slots)
            if (! s.files.isEmpty() && s.builtSettings.sampleRate != sampleRate)
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

    // Instrument: the Input source is the MIDI-played oscillator, written where
    // an effect's input would be (so it is also the dry signal).
    if (instrument && numChannels > 0)
    {
        monoSynth.setWaveform ((cortex::synth::Waveform) juce::jlimit (0, 3, (int) params.oscWave->load()));
        monoSynth.setGlide (params.oscGlide->load());
        monoSynth.setEnvelope (params.ampAttack->load(), params.ampRelease->load());
        monoSynth.render (midi, buffer.getWritePointer (0), numSamples);
        for (int ch = 1; ch < numChannels; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
    }

    // Effect: the Input source is the track, or the sidechain if chosen (and
    // connected). The dry signal is always the track.
    std::array<const float*, maxChannels> input {};
    for (int ch = 0; ch < numChannels; ++ch)
        input[(size_t) ch] = buffer.getReadPointer (ch);
    if (! instrument && params.inputSource->load() >= 0.5f)
    {
        const auto* scBus = getBus (true, 1);
        const int scChannels = scBus != nullptr && scBus->isEnabled() ? scBus->getNumberOfChannels() : 0;
        if (scChannels > 0)
            for (int ch = 0; ch < numChannels; ++ch)
                input[(size_t) ch] = buffer.getReadPointer (getChannelIndexInProcessBlockBuffer (true, 1, juce::jmin (ch, scChannels - 1)));
    }

    // Brains: each renders its best match for the live input.
    std::array<cortex::brain::Renderer*, numBrains> brains { slots[0].renderer.acquire(), slots[1].renderer.acquire() };
    const auto searchMode = (cortex::brain::SearchMode) juce::jlimit (0, 3, (int) params.searchMode->load());
    const bool lowLatency = params.lowLatency->load() > 0.5f;
    for (int b = 0; b < numBrains; ++b)
        if (auto* r = brains[(size_t) b])
        {
            r->setBlocking (isNonRealtime());
            r->setSearch (readSearchParams (b), searchMode);
            r->setLowLatency (lowLatency);
        }

    // Morph settings. A brain that isn't loaded has nothing to give: its share
    // of each triangle goes back to the Input, so an empty corner is transparent.
    auto morphParams = readMorphParams();
    for (auto* weights : { &morphParams.envWeights, &morphParams.detailWeights })
        for (int b = 0; b < numBrains; ++b)
            if (brains[(size_t) b] == nullptr)
            {
                (*weights)[Source::input] += (*weights)[(size_t) (Source::brain1 + b)];
                (*weights)[(size_t) (Source::brain1 + b)] = 0.0f;
            }
    // ...and phase pinned to a missing brain follows the detail instead.
    for (int b = 0; b < numBrains; ++b)
        if (brains[(size_t) b] == nullptr && (int) morphParams.phase == (int) cortex::dsp::PhaseSource::brain1 + b)
            morphParams.phase = cortex::dsp::PhaseSource::followDetail;

    // Cutoff modulation: LFO + envelope follower on the Input.
    {
        cortex::dsp::CutoffModulator::Settings mod;
        mod.lfoRateHz = params.lfoRate->load();
        mod.lfoDepth = params.lfoDepth->load();
        mod.shape = (cortex::dsp::CutoffModulator::Shape) juce::jlimit (0, 4, (int) params.lfoShape->load());
        mod.target = (cortex::dsp::CutoffModulator::Target) juce::jlimit (0, 3, (int) params.lfoTarget->load());
        mod.envDepth = params.envDepth->load();
        mod.envAttackMs = params.envAttack->load();
        mod.envReleaseMs = params.envRelease->load();
        const auto offsets = cutoffModulator.process (mod, numChannels > 0 ? input[0] : nullptr, numSamples);
        for (int s = 0; s < numSources; ++s)
            morphParams.cutoff[(size_t) s] = juce::jlimit (4.0f, 200.0f, morphParams.cutoff[(size_t) s] + offsets[(size_t) s]);
    }

    // Alignment. A brain answers a block (plus search time) after the input it
    // matched, so everything is lined up with the latest loaded brain: the
    // Input waits the full brain latency, each brain only for any difference.
    // Low latency: smallest FFT and nothing waits - your playing comes straight
    // through and the brains' timbre follows a grain behind.
    const int order = lowLatency ? cortex::dsp::minFftOrder
                                 : cortex::dsp::minFftOrder + juce::jlimit (0, 3, (int) params.fftSize->load());
    PerSourceInt latency {};
    for (int b = 0; b < numBrains; ++b)
        if (brains[(size_t) b] != nullptr && ! lowLatency)
            latency[(size_t) (Source::brain1 + b)] = brains[(size_t) b]->getLatencySamples();
    const int brainDelay = juce::jmax (latency[Source::brain1], latency[Source::brain2]);
    PerSourceInt delays {};
    for (int s = 0; s < numSources; ++s)
        delays[(size_t) s] = brainDelay - latency[(size_t) s];

    // A change of FFT size or alignment crossfades to a freshly primed engine;
    // one that arrives mid-crossfade waits for the next block after it.
    if ((order != activeOrder || brainDelay != activeBrainDelay || delays != activeDelays) && fadeRemaining == 0)
        switchEngine (order, brainDelay, delays, morphParams);

    for (auto& slot : engines)
        for (auto& e : slot)
            e.morph.setTargets (morphParams);

    // Gate and auto-mute, from the Input's level (the brains follow it).
    float peak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        peak = juce::jmax (peak, juce::FloatVectorOperations::findMaximum (input[(size_t) ch], numSamples),
                           -juce::FloatVectorOperations::findMinimum (input[(size_t) ch], numSamples));

    const float threshold = juce::Decibels::decibelsToGain (params.gateThresh->load());
    gateGain.setTargetValue (params.gateOn->load() > 0.5f && peak < threshold ? 0.0f : 1.0f);

    bool muted = false;
    if (params.autoMute->load() > 0.5f)
    {
        silentSamples = peak < threshold ? silentSamples + numSamples : 0;
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
    const int newLatency = (1 << activeOrder) + activeBrainDelay;
    const int oldLatency = (1 << previousOrder) + previousBrainDelay;
    auto& active = engines[(size_t) activeSlot];
    auto& fading = engines[(size_t) (1 - activeSlot)];

    for (int i = 0; i < numSamples; ++i)
    {
        const float gate = gateGain.getNextValue();
        const float wet = wetMix.getNextValue();
        const float og = outGain.getNextValue();

        // Equal-power crossfade from the previous engine setup, if one is running.
        float gainNew = 1.0f, gainOld = 0.0f;
        if (fadeRemaining > 0)
        {
            const float t = 1.0f - (float) fadeRemaining / (float) fadeLength;
            gainNew = std::sin (t * juce::MathConstants<float>::halfPi);
            gainOld = std::cos (t * juce::MathConstants<float>::halfPi);
        }

        // The brains are mono: each matches the mono mix of the Input and
        // feeds every channel.
        std::array<float, numBrains> brainOut {};
        if (brains[0] != nullptr || brains[1] != nullptr)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                mono += input[(size_t) ch][i];
            mono /= (float) juce::jmax (1, numChannels);
            for (int b = 0; b < numBrains; ++b)
                if (brains[(size_t) b] != nullptr)
                    brainOut[(size_t) b] = brains[(size_t) b]->processSample (mono);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto c = (size_t) ch;
            const float dryNow = out[c][i];

            history[Source::input][c][(size_t) historyPos] = input[c][i] * gate;
            history[Source::brain1][c][(size_t) historyPos] = brainOut[0];
            history[Source::brain2][c][(size_t) historyPos] = brainOut[1];
            historyDry[c][(size_t) historyPos] = dryNow;

            // Each engine reads every source at its own alignment delay.
            auto runEngine = [&] (Engine& e, const PerSourceInt& d)
            {
                std::array<float, numSources> in {};
                for (int s = 0; s < numSources; ++s)
                    in[(size_t) s] = history[(size_t) s][c][(size_t) ((historyPos - d[(size_t) s]) & historyMask)];
                return e.stft.processSample (in, [&e] (const cortex::dsp::STFT::Frames& f, float* fo) { e.morph.processFrame (f, fo); });
            };

            float w = runEngine (active[c], activeDelays);
            float dry = historyDry[c][(size_t) ((historyPos - newLatency) & historyMask)];

            if (fadeRemaining > 0)
            {
                w = gainNew * w + gainOld * runEngine (fading[c], previousDelays);
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

    for (int b = 0; b < numBrains; ++b)
        slots[(size_t) b].playingBlock.store (brains[(size_t) b] != nullptr ? brains[(size_t) b]->getLastChosenBlock() : -1,
                                              std::memory_order_relaxed);
    if (numChannels > 0)
        morphSnapshots.publish (engines[(size_t) activeSlot][0].morph.getSnapshot());

    if (params.limiter->load() > 0.5f && numChannels > 0)
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
        limiter.process (juce::dsp::ProcessContextReplacing<float> (block));
    }
}

void CortexAudioProcessor::switchEngine (int newOrder, int newBrainDelay, const PerSourceInt& newDelays,
                                         const cortex::dsp::MorphParams& morphParams)
{
    if (activeOrder < 0)
    {
        // First block after prepare: nothing to fade from.
        primeEngine (activeSlot, newOrder, newDelays, morphParams);
        previousOrder = activeOrder = newOrder;
        previousBrainDelay = activeBrainDelay = newBrainDelay;
        previousDelays = activeDelays = newDelays;
    }
    else
    {
        previousOrder = activeOrder;
        previousBrainDelay = activeBrainDelay;
        previousDelays = activeDelays;
        activeSlot = 1 - activeSlot;
        primeEngine (activeSlot, newOrder, newDelays, morphParams);
        activeOrder = newOrder;
        activeBrainDelay = newBrainDelay;
        activeDelays = newDelays;
        fadeRemaining = fadeLength;
    }

    reportedBrainDelay.store (newBrainDelay);
}

void CortexAudioProcessor::primeEngine (int slot, int order, const PerSourceInt& delays, const cortex::dsp::MorphParams& morphParams)
{
    // Run the last 2N samples of each source (at its alignment delay) through
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

        for (int i = 2 * n; i > 0; --i)
        {
            std::array<float, numSources> in {};
            for (int s = 0; s < numSources; ++s)
                in[(size_t) s] = history[(size_t) s][(size_t) ch][(size_t) ((historyPos - i - delays[(size_t) s]) & historyMask)];
            e.stft.processSample (in, [&e] (const cortex::dsp::STFT::Frames& f, float* fo) { e.morph.processFrame (f, fo); });
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

    for (int b = 0; b < numBrains; ++b)
    {
        state.removeChild (state.getChildWithName (brainStateIds[b]), nullptr);
        juce::ValueTree brainTree (brainStateIds[b]);
        for (const auto& f : slots[(size_t) b].files)
            brainTree.appendChild (juce::ValueTree (sourceId, { { pathId, f.getFullPathName() } }), nullptr);
        state.appendChild (brainTree, nullptr);
    }

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
            std::array<juce::StringArray, numBrains> paths;
            for (int b = 0; b < numBrains; ++b)
            {
                for (const auto& source : state.getChildWithName (brainStateIds[b]))
                    paths[(size_t) b].add (source[pathId].toString());
                state.removeChild (state.getChildWithName (brainStateIds[b]), nullptr);
            }
            apvts.replaceState (state);

            // Hosts never call this on the audio thread. The builds themselves
            // are asynchronous, and usually fast cache loads.
            for (int b = 0; b < numBrains; ++b)
            {
                if (paths[(size_t) b].isEmpty())
                    clearBrain (b);
                else
                    loadBrainFrom (paths[(size_t) b], b);
            }
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

cortex::brain::SearchParams CortexAudioProcessor::readSearchParams (int brain) const
{
    cortex::brain::SearchParams p;
    p.timbreWeight = params.matchTimbre->load();
    p.pitchWeight = params.matchPitch->load();
    p.envTransplant = params.envTransplant->load();
    p.transplantCutoff = params.cutoff[(size_t) (Source::brain1 + brain)]->load();
    p.cepstralGlide = params.cepstralGlide->load();
    return cortex::brain::applySearchKnobs (p, { params.novelty->load(), params.boredom->load(), params.stickyness->load() },
                                            params.chaos->load(), params.memory->load());
}

void CortexAudioProcessor::loadBrainFrom (const juce::StringArray& filesOrFolders, int brain)
{
    slots[(size_t) brain].files = cortex::brain::collectAudioFiles (filesOrFolders, formatManager);
    rebuildBrain (brain);
}

void CortexAudioProcessor::clearBrain (int brain)
{
    auto& s = slots[(size_t) brain];
    s.builder->cancel();
    s.files.clear();
    s.renderer.publishClear();
    std::lock_guard<std::mutex> l (brainLock);
    s.current = nullptr;
    s.message = {};
}

void CortexAudioProcessor::rebuildBrain (int brain)
{
    auto& s = slots[(size_t) brain];
    if (s.files.isEmpty())
    {
        clearBrain (brain);
        return;
    }
    s.builder->start ({ s.files, currentBrainSettings(), cortex::BrainCache::defaultDirectory() });
}

void CortexAudioProcessor::onBrainBuilt (int brain, std::shared_ptr<const cortex::brain::Brain> built, const juce::String& message)
{
    // Builder thread: the renderer allocates here, never on the audio thread.
    auto& s = slots[(size_t) brain];
    s.renderer.publish (std::make_unique<cortex::brain::Renderer> (built));

    std::lock_guard<std::mutex> l (brainLock);
    s.builtSettings = built->settings;
    s.current = std::move (built);
    s.message = message;
}

void CortexAudioProcessor::timerCallback()
{
    for (auto& s : slots)
        s.renderer.collectGarbage();

    if (getLatencySamples() != getAlignedLatencySamples())
        updateLatency();

    // Debounce block size / overlap / sample-rate changes so dragging a knob
    // doesn't restart the build on every step.
    const auto now = juce::Time::getMillisecondCounter();
    if (rebuildRequested.exchange (false))
        rebuildDueMs = now + 600;
    if (rebuildDueMs != 0 && now >= rebuildDueMs)
    {
        rebuildDueMs = 0;
        for (int b = 0; b < numBrains; ++b)
            if (! slots[(size_t) b].files.isEmpty())
                rebuildBrain (b);
    }
}

CortexAudioProcessor::BrainStatus CortexAudioProcessor::getBrainStatus (int brain) const
{
    const auto& slot = slots[(size_t) brain];
    BrainStatus s;
    s.building = slot.builder->isBuilding();
    s.progress = slot.builder->getProgress();

    std::lock_guard<std::mutex> l (brainLock);
    s.playingBlock = slot.playingBlock.load (std::memory_order_relaxed);
    s.message = s.building ? slot.builder->getStatus() : slot.message;
    if (slot.current != nullptr)
    {
        s.numBlocks = (int) slot.current->numBlocks();
        s.numSounds = (int) slot.current->sounds.size();
    }
    return s;
}

std::shared_ptr<const cortex::brain::Brain> CortexAudioProcessor::getBrain (int brain) const
{
    std::lock_guard<std::mutex> l (brainLock);
    return slots[(size_t) brain].current;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CortexAudioProcessor (JucePlugin_IsSynth != 0);
}
