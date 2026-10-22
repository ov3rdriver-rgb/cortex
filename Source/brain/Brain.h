// Cortex: portions ported from Samplebrain (brain/src/brain.h),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.

#pragma once

#include "Block.h"
#include "SearchParams.h"

#include <juce_core/juce_core.h>

#include <functional>

namespace cortex::brain
{
struct BrainSettings
{
    int blockSize = 1000;
    float overlap = 0.0f;       // 0 .. 0.75 of a block
    double sampleRate = 48000.0;

    int hop() const noexcept { return juce::jmax (1, blockSize - (int) std::round (overlap * (float) blockSize)); }

    bool operator== (const BrainSettings& o) const noexcept
    {
        return blockSize == o.blockSize && overlap == o.overlap && sampleRate == o.sampleRate;
    }
};

struct SoundInfo
{
    juce::String path;
    std::uint32_t firstBlock = 0;
    std::uint32_t numBlocks = 0;
    bool enabled = true;
};

// A built brain: blocks, their analysis and synapse graph. Immutable once built
// and shared between threads; all per-playback state lives in BrainSearch.
struct Brain
{
    BrainSettings settings;
    std::vector<SoundInfo> sounds;
    std::vector<Block> blocks;

    std::uint32_t numBlocks() const noexcept { return (std::uint32_t) blocks.size(); }
    bool isBlockActive (std::uint32_t index) const noexcept;
};

inline constexpr int maxStoredSynapses = 64;

using ProgressFn = std::function<void (float progress, const juce::String& status)>;
using ShouldExitFn = std::function<bool()>;

// Precomputes each block's nearest neighbours (Samplebrain build_synapses_fixed),
// spread over the available cores. Returns false if cancelled.
bool buildSynapses (Brain&, const SearchParams&, const ProgressFn&, const ShouldExitFn&);

// Search state for one renderer (Samplebrain brain::search & friends).
// prepare() allocates; everything else is realtime-safe.
class BrainSearch
{
public:
    void prepare (const Brain&);
    void reset();

    std::uint32_t search (const Brain&, const BlockFeatures& f, const BlockFeatures& n, const SearchParams&);
    std::uint32_t reverseSearch (const Brain&, const BlockFeatures& f, const BlockFeatures& n, const SearchParams&);
    std::uint32_t searchSynapses (const Brain&, const BlockFeatures& f, const BlockFeatures& n, const SearchParams&);

    double getCurrentError() const noexcept { return currentError; }
    std::uint32_t getCurrentBlock() const noexcept { return currentIndex; }

private:
    double compare (const Brain&, std::uint32_t index, const BlockFeatures& f, const BlockFeatures& n, const SearchParams&) const noexcept;
    std::uint32_t stickify (const Brain&, std::uint32_t closest, double distance,
                            const BlockFeatures& f, const BlockFeatures& n, const SearchParams&) noexcept;
    void depleteUsage (float falloff) noexcept;

    std::vector<float> usage;
    std::uint32_t currentIndex = 0;
    double currentError = 0.0;
    juce::Random random;
};
} // namespace cortex::brain
