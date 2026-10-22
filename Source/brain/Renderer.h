// Cortex: portions ported from Samplebrain (brain/src/renderer.h, block_stream.h),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.

#pragma once

#include "Brain.h"

#include <atomic>
#include <memory>

namespace cortex::brain
{
// Realtime brain playback. The live input is the target: it is chopped into
// blocks on the fly (Samplebrain's block_stream), each one is matched against
// the brain, and the chosen brain block is overlap-added to the output.
//
// Construct off the audio thread (it allocates); after that process() and
// reset() never allocate, lock or touch files. Samplebrain's std::list of
// render blocks is replaced by a fixed voice pool.
class Renderer
{
public:
    explicit Renderer (std::shared_ptr<const Brain>);

    const Brain& getBrain() const noexcept { return *brain; }
    std::shared_ptr<const Brain> getBrainPtr() const noexcept { return brain; }

    void reset();
    void setSearch (const SearchParams& p, SearchMode m) noexcept { params = p; mode = m; }

    // Takes one target sample, returns one sample of brain output.
    // Latency: one block (a target block must be complete before it can be matched).
    float processSample (float target) noexcept;

    int getLatencySamples() const noexcept { return blockSize; }

    // Most recently chosen brain block, for the UI (-1 before the first match).
    int getLastChosenBlock() const noexcept { return lastChosen.load (std::memory_order_relaxed); }

private:
    void onTargetBlockComplete() noexcept;
    void startVoice (std::uint32_t blockIndex) noexcept;

    struct Voice
    {
        int block = -1;
        int position = 0;
    };
    static constexpr int maxVoices = 8;

    std::shared_ptr<const Brain> brain;
    BlockAnalyser analyser;
    BrainSearch search;
    SearchParams params;
    SearchMode mode = SearchMode::basic;

    int blockSize = 0, hop = 0;
    float outputGain = 1.0f;

    std::vector<float> targetRing, targetBlock;
    int ringPos = 0;
    std::int64_t samplesSeen = 0;

    BlockFeatures targetFeatures, targetNormalised, heldFeatures, heldNormalised;
    int slideHeld = 0;

    std::array<Voice, maxVoices> voices {};
    std::atomic<int> lastChosen { -1 };
};
} // namespace cortex::brain
