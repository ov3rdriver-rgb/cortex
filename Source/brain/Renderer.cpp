// Cortex: portions ported from Samplebrain (brain/src/renderer.cpp, block_stream.cpp),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.
//
// Differences from Samplebrain: target blocks come only from the live stream;
// searching happens on a worker thread; the renderer's fixed 0.2 gain is
// replaced by overlap-add normalisation; the autotune/stretch/target-mix/
// normalised-pcm-mix controls are not ported (the cepstral morph covers that).

#include "Renderer.h"
#include "dsp/Cepstrum.h"

namespace cortex::brain
{
namespace
{
    // Time the worker gets to answer before the block is due (~23 ms at 44.1k).
    constexpr int minSearchDelay = 1024;

    // A glide reaches the grain's own envelope this far through the grain,
    // and holds it for the rest.
    constexpr float glideArrival = 0.75f;

    // Most a brain grain is boosted to match the input's level (+24 dB).
    constexpr float maxLevelMatchGain = 16.0f;

    // Below this a grain is treated as silent for gap filling (-60 dB).
    constexpr float audibleGrainGain = 0.001f;
}

Renderer::Renderer (std::shared_ptr<const Brain> b) : brain (std::move (b))
{
    jassert (brain != nullptr);
    blockSize = brain->settings.blockSize;
    hop = brain->settings.hop();
    normalSearchDelay = searchDelay = juce::jmax (hop, minSearchDelay);
    gapGainStep = (float) (1.0 / (0.005 * brain->settings.sampleRate));   // 5 ms fades

    analyser.prepare (blockSize, hop, brain->settings.sampleRate);
    search.prepare (*brain);

    // Normalise the overlap-add so a constant signal comes back at unity.
    double windowSum = 0.0;
    for (auto w : analyser.getWindow())
        windowSum += w;
    outputGain = windowSum > 0.0 ? (float) (hop / windowSum) : 1.0f;

    targetRing.assign ((size_t) blockSize, 0.0f);
    targetBlock.assign ((size_t) blockSize, 0.0f);
    windowedTarget.assign ((size_t) blockSize, 0.0f);

    grains.assign ((size_t) numGrains, std::vector<float> ((size_t) blockSize, 0.0f));

    const int order = (int) std::ceil (std::log2 ((double) blockSize)) + 1;
    transplantSize = 1 << order;
    transplantFft = std::make_unique<juce::dsp::FFT> (order);
    transplantTime.assign ((size_t) transplantSize, {});
    transplantSpec.assign ((size_t) transplantSize, {});
    glideInput.assign ((size_t) blockSize, 0.0f);
    glideSegment.assign ((size_t) blockSize, 0.0f);
    {
        // Calibrate the inverse scale, independent of JUCE's FFT engine.
        std::vector<std::complex<float>> x ((size_t) transplantSize), X ((size_t) transplantSize), y ((size_t) transplantSize);
        x[1] = 1.0f;
        transplantFft->perform (x.data(), X.data(), false);
        transplantFft->perform (X.data(), y.data(), true);
        transplantInverseScale = 1.0f / y[1].real();
    }
    const int bins = transplantSize / 2 + 1;
    cepstrumCos.resize ((size_t) (numCepstrum * bins));
    for (int q = 0; q < numCepstrum; ++q)
        for (int k = 0; k < bins; ++k)
            cepstrumCos[(size_t) (q * bins + k)] = (q == 0 ? 1.0f : 2.0f)
                * (float) std::cos (juce::MathConstants<double>::twoPi * q * k / transplantSize);

    worker = std::thread ([this] { workerLoop(); });
}

Renderer::~Renderer()
{
    stopWorker = true;
    ++wakeCounter;
    wakeCounter.notify_one();
    worker.join();
}

float Renderer::processSample (float target) noexcept
{
    startDueVoices();

    float out = 0.0f;
    bool anyVoice = false;
    for (auto& v : voices)
    {
        if (v.pcm == nullptr)
            continue;
        // Grains level-matched to silence don't count: they'd leave a gap.
        anyVoice = anyVoice || v.gain > audibleGrainGain;
        out += v.gain * v.pcm[v.position];
        if (++v.position >= blockSize)
        {
            v.pcm = nullptr;
            releaseGrain (v.grain);
        }
    }

    targetRing[(size_t) ringPos] = target;
    if (++ringPos == blockSize)
        ringPos = 0;

    // Target blocks start at 0, hop, 2*hop... exactly like the brain's own
    // blocks, so a brain built from the same audio lines up block for block.
    if (++samplesSeen >= blockSize && (samplesSeen - blockSize) % hop == 0)
        onTargetBlockComplete();

    const float gapTarget = fillGaps && ! anyVoice ? 1.0f : 0.0f;
    gapGain = gapGain < gapTarget ? juce::jmin (gapTarget, gapGain + gapGainStep)
                                  : juce::jmax (gapTarget, gapGain - gapGainStep);

    return out * outputGain + gapGain * target;
}

void Renderer::onTargetBlockComplete() noexcept
{
    if (brain->blocks.empty())
        return;

    // Unroll the ring, oldest sample first.
    for (int i = 0, idx = ringPos; i < blockSize; ++i)
    {
        targetBlock[(size_t) i] = targetRing[(size_t) idx];
        if (++idx == blockSize)
            idx = 0;
    }
    analyser.analyse (targetBlock.data(), windowedTarget.data(), targetFeatures);

    int start1, size1, start2, size2;
    requestFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 == 0)
        return;   // worker hopelessly behind: skip this hop, current voices keep playing

    auto& r = requests[(size_t) start1];
    r.features = targetFeatures;
    r.params = params;
    r.mode = mode;
    r.due = samplesSeen + searchDelay;
    r.targetRms = rmsOf (windowedTarget);
    requestFifo.finishedWrite (1);

    const auto sent = ++requestsSent;
    ++wakeCounter;
    wakeCounter.notify_one();

    if (blocking)
        while (resultsSent.load() < sent)
            std::this_thread::yield();
}

void Renderer::startDueVoices() noexcept
{
    for (;;)
    {
        int start1, size1, start2, size2;
        resultFifo.prepareToRead (1, start1, size1, start2, size2);
        if (size1 == 0)
            break;

        const auto& r = results[(size_t) start1];
        if (r.due > samplesSeen)
            break;

        startVoice (r);
        resultFifo.finishedRead (1);
    }
}

void Renderer::startVoice (const Result& r) noexcept
{
    // Free voice, or steal the one closest to finishing.
    Voice* slot = &voices[0];
    for (auto& v : voices)
    {
        if (v.pcm == nullptr)
        {
            slot = &v;
            break;
        }
        if (v.position > slot->position)
            slot = &v;
    }

    if (slot->pcm != nullptr)
        releaseGrain (slot->grain);

    slot->pcm = r.grain >= 0 ? grains[(size_t) r.grain].data() : brain->blocks[r.block].pcm.data();
    slot->grain = r.grain;
    slot->position = 0;
    slot->gain = r.gain;
    lastChosen.store ((int) r.block, std::memory_order_relaxed);
}

void Renderer::releaseGrain (int grain) noexcept
{
    if (grain >= 0)
        grainState[(size_t) grain].store (0, std::memory_order_release);
}

int Renderer::acquireGrain() noexcept
{
    for (int g = 0; g < numGrains; ++g)
    {
        int expected = 0;
        if (grainState[(size_t) g].compare_exchange_strong (expected, 1, std::memory_order_acq_rel))
            return g;
    }
    return -1;
}

void Renderer::applyCepstralGain (const float* in, int length, const float* logGainCeps, int numQ, float* out)
{
    const int n = transplantSize;
    const int bins = n / 2 + 1;

    for (int i = 0; i < n; ++i)
        transplantTime[(size_t) i] = { i < length ? in[i] : 0.0f, 0.0f };
    transplantFft->perform (transplantTime.data(), transplantSpec.data(), false);

    for (int k = 0; k < bins; ++k)
    {
        float logGain = 0.0f;
        for (int q = 0; q < numQ; ++q)
            logGain += logGainCeps[q] * cepstrumCos[(size_t) (q * bins + k)];
        const float g = std::exp (juce::jlimit (-12.0f, 12.0f, logGain));
        transplantSpec[(size_t) k] *= g;
        if (k > 0 && k < n / 2)
            transplantSpec[(size_t) (n - k)] *= g;
    }

    transplantFft->perform (transplantSpec.data(), transplantTime.data(), true);
    for (int i = 0; i < length; ++i)
        out[i] = transplantTime[(size_t) i].real() * transplantInverseScale;
}

void Renderer::transplantEnvelope (const Block& block, const BlockFeatures& target, float amount, float cutoff, float* out)
{
    // Log-gain = amount * (target envelope - block envelope), both rebuilt
    // from their low-quefrency cepstra with the morph's tapered lifter.
    const int numQ = juce::jmin (numCepstrum, (int) std::ceil (cutoff) + 5);
    float diff[numCepstrum] {};
    for (int q = 0; q < numQ; ++q)
        diff[q] = amount * cortex::dsp::Cepstrum::envelopeWeight (q, cutoff)
                * (target.cepstrum[(size_t) q] - block.features.cepstrum[(size_t) q]);

    applyCepstralGain (block.pcm.data(), blockSize, diff, numQ, out);
}

void Renderer::glideEnvelope (const float* in, const float* fromCeps, const float* toCeps, float amount, float cutoff, float* out)
{
    // The grain is cut into Hann segments at 50% overlap (they sum to 1).
    // Segment s is pulled towards the previous grain's envelope by
    // amount * (1 - t), t = its centre's position in the grain (reaching 1 at
    // glideArrival), so the envelope glides from where the last grain ended
    // to this one's own.
    const int segmentLength = 2 * blockSize / (glideSegments + 1);
    const int segmentHop = segmentLength / 2;
    const int numQ = juce::jmin (numCepstrum, (int) std::ceil (cutoff) + 5);

    std::fill (out, out + blockSize, 0.0f);
    for (int s = 0; s <= glideSegments + 1; ++s)
    {
        const int start = (s - 1) * segmentHop;
        for (int i = 0; i < segmentLength; ++i)
        {
            const int pos = start + i;
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) segmentLength);
            glideSegment[(size_t) i] = pos >= 0 && pos < blockSize ? in[pos] * w : 0.0f;
        }

        const float t = juce::jlimit (0.0f, 1.0f, (float) (start + segmentHop) / (glideArrival * (float) blockSize));
        float diff[numCepstrum] {};
        for (int q = 0; q < numQ; ++q)
            diff[q] = amount * (1.0f - t) * cortex::dsp::Cepstrum::envelopeWeight (q, cutoff) * (fromCeps[q] - toCeps[q]);

        applyCepstralGain (glideSegment.data(), segmentLength, diff, numQ, glideSegment.data());
        for (int i = 0; i < segmentLength; ++i)
        {
            const int pos = start + i;
            if (pos >= 0 && pos < blockSize)
                out[pos] += glideSegment[(size_t) i];
        }
    }
}

void Renderer::workerLoop()
{
    std::uint32_t seen = wakeCounter.load();

    while (! stopWorker)
    {
        const int pending = requestFifo.getNumReady();
        if (pending == 0)
        {
            wakeCounter.wait (seen);
            seen = wakeCounter.load();
            continue;
        }

        // Answer only the newest request; older ones get the same block.
        int start1, size1, start2, size2;
        requestFifo.prepareToRead (pending, start1, size1, start2, size2);
        const auto& newest = requests[(size_t) (size2 > 0 ? start2 + size2 - 1 : start1 + size1 - 1)];
        const auto chosen = choose (newest);
        const auto& block = brain->blocks[chosen];

        // This grain's envelope after any transplant: where a glide ends, and
        // where the next grain's glide starts.
        const float transplant = newest.params.envTransplant;
        std::array<float, numCepstrum> grainEnvelope {};
        for (int q = 0; q < numCepstrum; ++q)
            grainEnvelope[(size_t) q] = block.features.cepstrum[(size_t) q]
                                      + transplant * (newest.features.cepstrum[(size_t) q] - block.features.cepstrum[(size_t) q]);
        const bool glide = newest.params.cepstralGlide > 0.0f && hasPreviousEnvelope;

        // Transplant and/or glide into a pooled grain (if one is free;
        // otherwise the block plays untouched this time).
        int grain = -1;
        float grainRms = brain->blockRms[chosen];
        if ((transplant > 0.0f || glide) && (grain = acquireGrain()) >= 0)
        {
            auto& g = grains[(size_t) grain];
            const float* source = block.pcm.data();
            if (transplant > 0.0f)
            {
                transplantEnvelope (block, newest.features, transplant, newest.params.transplantCutoff, g.data());
                source = g.data();
            }
            if (glide)
            {
                std::copy (source, source + blockSize, glideInput.begin());
                glideEnvelope (glideInput.data(), previousEnvelope.data(), grainEnvelope.data(),
                               newest.params.cepstralGlide, newest.params.transplantCutoff, g.data());
            }
            grainRms = rmsOf (g);
        }
        previousEnvelope = grainEnvelope;
        hasPreviousEnvelope = true;

        // Play the grain at the input's level (Samplebrain's normalised
        // playback, by RMS), bounded so near-silent blocks can't explode.
        const float gain = juce::jmin (maxLevelMatchGain, newest.targetRms / juce::jmax (1.0e-9f, grainRms));
        const float plainGain = juce::jmin (maxLevelMatchGain, newest.targetRms / juce::jmax (1.0e-9f, brain->blockRms[chosen]));

        // Older, skipped requests repeat the choice as a plain block; only the
        // newest gets the transplanted grain, so each grain has one owner.
        const int newestIndex = size2 > 0 ? start2 + size2 - 1 : start1 + size1 - 1;
        auto emit = [&] (int index)
        {
            const bool isNewest = index == newestIndex;
            int w1, s1, w2, s2;
            resultFifo.prepareToWrite (1, w1, s1, w2, s2);
            if (s1 > 0)
            {
                results[(size_t) w1] = { chosen, requests[(size_t) index].due,
                                         isNewest ? gain : plainGain, isNewest ? grain : -1 };
                resultFifo.finishedWrite (1);
            }
            else if (isNewest)
            {
                releaseGrain (grain);
            }
        };
        for (int i = 0; i < size1; ++i) emit (start1 + i);
        for (int i = 0; i < size2; ++i) emit (start2 + i);

        requestFifo.finishedRead (pending);
        resultsSent += pending;
    }
}

std::uint32_t Renderer::choose (const Request& r)
{
    const auto query = brain->makeQuery (r.features);

    switch (r.mode)
    {
        case SearchMode::basic:    return search.search (*brain, query, r.params);
        case SearchMode::reverse:  return search.reverseSearch (*brain, query, r.params);
        case SearchMode::synaptic: return search.searchSynapses (*brain, query, r.params);
        case SearchMode::synapticSlide:
        {
            // The target only advances once the brain has found a close enough
            // block; until then keep chasing the held one (bounded, since a live
            // target can't actually be paused).
            if (slideHeld == 0 || search.getCurrentError() < r.params.slideError || slideHeld >= r.params.maxSlideHold)
            {
                heldQuery = query;
                slideHeld = 0;
            }
            ++slideHeld;
            return search.searchSynapses (*brain, heldQuery, r.params);
        }
    }
    return 0;
}
} // namespace cortex::brain
