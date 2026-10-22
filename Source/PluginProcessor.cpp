#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

namespace
{
    // Plugin state keeps a reference to the brain (file list + cache path),
    // never raw audio. Populated in Phase 2.
    const juce::Identifier brainStateId { "Brain" };

    constexpr double dcBlockHz = 5.0;
    constexpr double rampSeconds = 0.01;
    constexpr double crossfadeSeconds = 0.03;
}

CortexAudioProcessor::CortexAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "CortexParams", cortex::createParameterLayout())
{
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

    apvts.addParameterListener (cortex::ParamIDs::fftSize, this);
    updateLatency();
}

CortexAudioProcessor::~CortexAudioProcessor()
{
    apvts.removeParameterListener (cortex::ParamIDs::fftSize, this);
}

int CortexAudioProcessor::currentFftSize() const
{
    const auto index = juce::jlimit (0, (int) std::size (cortex::fftSizes) - 1, (int) params.fftSize->load());
    return cortex::fftSizes[index];
}

void CortexAudioProcessor::updateLatency()
{
    // STFT latency = FFT size. Phase 2+: add the brain block size when the brain is in the path.
    setLatencySamples (currentFftSize());
}

void CortexAudioProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == cortex::ParamIDs::fftSize)
        updateLatency();
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
    return p;
}

void CortexAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

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
    activeOrder = -1;   // forces setOrder() on the first block

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

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (maxChannels, getMainBusNumOutputChannels(), buffer.getNumChannels());

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // FFT size change. A change that arrives mid-crossfade waits for the next block after it.
    const int order = cortex::dsp::minFftOrder + juce::jlimit (0, 3, (int) params.fftSize->load());
    if (order != activeOrder && fadeRemaining == 0)
        switchFftOrder (order);

    const auto morphParams = readMorphParams();
    for (auto& slot : engines)
        for (auto& e : slot)
            e.morph.setTargets (morphParams);

    // Input B. bSource == Brain is wired up in Phase 3; until then it is silent.
    std::array<const float*, maxChannels> sidechain {};
    const auto* scBus = getBus (true, 1);
    const int scChannels = scBus != nullptr && scBus->isEnabled() ? scBus->getNumberOfChannels() : 0;
    if (params.bSource->load() < 0.5f && scChannels > 0)
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
    const int newLatency = 1 << activeOrder;
    const int oldLatency = 1 << previousOrder;
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

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto c = (size_t) ch;
            const float a = out[c][i];
            const float b = sidechain[c] != nullptr ? sidechain[c][i] : 0.0f;
            const float gatedA = a * ga, gatedB = b * gb;

            historyA[c][(size_t) historyPos] = gatedA;
            historyB[c][(size_t) historyPos] = gatedB;
            historyDry[c][(size_t) historyPos] = a;

            auto runEngine = [&] (Engine& e)
            {
                return e.stft.processSample (gatedA, gatedB, [&e] (const float* fa, const float* fb, float* fo)
                                             { e.morph.processFrame (fa, fb, fo); });
            };

            float w = runEngine (active[c]);
            float dry = historyDry[c][(size_t) ((historyPos - newLatency) & historyMask)];

            if (fadeRemaining > 0)
            {
                w = gainNew * w + gainOld * runEngine (fading[c]);
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

    if (params.limiter->load() > 0.5f && numChannels > 0)
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
        limiter.process (juce::dsp::ProcessContextReplacing<float> (block));
    }
}

void CortexAudioProcessor::switchFftOrder (int newOrder)
{
    if (activeOrder < 0)
    {
        // First block after prepare: nothing to fade from.
        primeEngine (activeSlot, newOrder);
        previousOrder = activeOrder = newOrder;
        return;
    }

    previousOrder = activeOrder;
    activeSlot = 1 - activeSlot;
    primeEngine (activeSlot, newOrder);
    activeOrder = newOrder;
    fadeRemaining = fadeLength;
}

void CortexAudioProcessor::primeEngine (int slot, int order)
{
    // Run the last 2N input samples through the new engine so its overlap-add
    // buffer is already in steady state when the crossfade starts. Costs 16
    // frames once per FFT size change.
    constexpr int historyMask = historySize - 1;
    const int n = 1 << order;
    const auto morphParams = readMorphParams();

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
            e.stft.processSample (ha[idx], hb[idx], [&e] (const float* fa, const float* fb, float* fo)
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
