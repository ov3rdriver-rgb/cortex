// Cortex: portions ported from Samplebrain (brain/src/renderer.cpp, block_stream.cpp),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.
//
// Differences from Samplebrain: target blocks come only from the live stream;
// searching happens on a worker thread; the renderer's fixed 0.2 gain is
// replaced by overlap-add normalisation; the autotune/stretch/target-mix/
// normalised-pcm-mix controls are not ported (the cepstral morph covers that).

#include "Renderer.h"

namespace cortex::brain
{
namespace
{
    // Time the worker gets to answer before the block is due (~23 ms at 44.1k).
    constexpr int minSearchDelay = 1024;

    // Most a brain grain is boosted to match the input's level (+24 dB).
    constexpr float maxLevelMatchGain = 16.0f;
}

Renderer::Renderer (std::shared_ptr<const Brain> b) : brain (std::move (b))
{
    jassert (brain != nullptr);
    blockSize = brain->settings.blockSize;
    hop = brain->settings.hop();
    searchDelay = juce::jmax (hop, minSearchDelay);

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
    for (auto& v : voices)
    {
        if (v.block < 0)
            continue;
        out += v.gain * brain->blocks[(size_t) v.block].pcm[(size_t) v.position];
        if (++v.position >= blockSize)
            v.block = -1;
    }

    targetRing[(size_t) ringPos] = target;
    if (++ringPos == blockSize)
        ringPos = 0;

    // Target blocks start at 0, hop, 2*hop... exactly like the brain's own
    // blocks, so a brain built from the same audio lines up block for block.
    if (++samplesSeen >= blockSize && (samplesSeen - blockSize) % hop == 0)
        onTargetBlockComplete();

    return out * outputGain;
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

        startVoice (r.block, r.gain);
        resultFifo.finishedRead (1);
    }
}

void Renderer::startVoice (std::uint32_t blockIndex, float gain) noexcept
{
    // Free voice, or steal the one closest to finishing.
    Voice* slot = &voices[0];
    for (auto& v : voices)
    {
        if (v.block < 0)
        {
            slot = &v;
            break;
        }
        if (v.position > slot->position)
            slot = &v;
    }

    slot->block = (int) blockIndex;
    slot->position = 0;
    slot->gain = gain;
    lastChosen.store ((int) blockIndex, std::memory_order_relaxed);
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

        // Play the grain at the input's level (Samplebrain's normalised
        // playback, by RMS), bounded so near-silent blocks can't explode.
        const float gain = juce::jmin (maxLevelMatchGain, newest.targetRms / juce::jmax (1.0e-9f, brain->blockRms[chosen]));

        auto emit = [&] (int index)
        {
            int w1, s1, w2, s2;
            resultFifo.prepareToWrite (1, w1, s1, w2, s2);
            if (s1 > 0)
            {
                results[(size_t) w1] = { chosen, requests[(size_t) index].due, gain };
                resultFifo.finishedWrite (1);
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
