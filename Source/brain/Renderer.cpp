// Cortex: portions ported from Samplebrain (brain/src/renderer.cpp, block_stream.cpp),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.
//
// Differences from Samplebrain: target blocks come only from the live stream;
// the renderer's fixed 0.2 gain is replaced by overlap-add normalisation; the
// autotune/stretch/target-mix/normalised-pcm-mix controls are not ported (the
// cepstral morph covers that ground).

#include "Renderer.h"

namespace cortex::brain
{
Renderer::Renderer (std::shared_ptr<const Brain> b) : brain (std::move (b))
{
    jassert (brain != nullptr);
    blockSize = brain->settings.blockSize;
    hop = brain->settings.hop();

    analyser.prepare (blockSize, hop, brain->settings.sampleRate);
    search.prepare (*brain);

    // Normalise the overlap-add so a constant signal comes back at unity.
    double windowSum = 0.0;
    for (auto w : analyser.getWindow())
        windowSum += w;
    outputGain = windowSum > 0.0 ? (float) (hop / windowSum) : 1.0f;

    targetRing.assign ((size_t) blockSize, 0.0f);
    targetBlock.assign ((size_t) blockSize, 0.0f);
    reset();
}

void Renderer::reset()
{
    std::fill (targetRing.begin(), targetRing.end(), 0.0f);
    ringPos = 0;
    samplesSeen = 0;
    slideHeld = 0;
    for (auto& v : voices)
        v = {};
    search.reset();
    lastChosen = -1;
}

float Renderer::processSample (float target) noexcept
{
    float out = 0.0f;
    for (auto& v : voices)
    {
        if (v.block < 0)
            continue;
        out += brain->blocks[(size_t) v.block].pcm[(size_t) v.position];
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
    analyser.analyse (targetBlock.data(), nullptr, targetFeatures, targetNormalised);

    std::uint32_t chosen = 0;
    switch (mode)
    {
        case SearchMode::basic:
            chosen = search.search (*brain, targetFeatures, targetNormalised, params);
            break;

        case SearchMode::reverse:
            chosen = search.reverseSearch (*brain, targetFeatures, targetNormalised, params);
            break;

        case SearchMode::synaptic:
            chosen = search.searchSynapses (*brain, targetFeatures, targetNormalised, params);
            break;

        case SearchMode::synapticSlide:
        {
            // The target only advances once the brain has found a close enough
            // block; until then keep chasing the held one (bounded, since a live
            // target can't actually be paused).
            if (slideHeld == 0 || search.getCurrentError() < params.slideError || slideHeld >= params.maxSlideHold)
            {
                heldFeatures = targetFeatures;
                heldNormalised = targetNormalised;
                slideHeld = 0;
            }
            chosen = search.searchSynapses (*brain, heldFeatures, heldNormalised, params);
            ++slideHeld;
            break;
        }
    }

    startVoice (chosen);
}

void Renderer::startVoice (std::uint32_t blockIndex) noexcept
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
    lastChosen.store ((int) blockIndex, std::memory_order_relaxed);
}
} // namespace cortex::brain
