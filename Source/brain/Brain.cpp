// Cortex: portions ported from Samplebrain (brain/src/brain.cpp),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.

#include "Brain.h"

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <thread>

namespace cortex::brain
{
namespace
{
    // Samplebrain adds 1000 per use and compares it against raw FFT power; with
    // log features, 1 per use puts usage on a comparable scale.
    constexpr float usageFactor = 1.0f;

    double blend (double a, double b, double t) noexcept { return a * (1.0 - t) + b * t; }
}

bool Brain::isBlockActive (std::uint32_t index) const noexcept
{
    for (const auto& s : sounds)
        if (index >= s.firstBlock && index < s.firstBlock + s.numBlocks)
            return s.enabled;
    return false;
}

bool buildSynapses (Brain& brain, const SearchParams& params, const ProgressFn& progress, const ShouldExitFn& shouldExit)
{
    const auto n = brain.numBlocks();
    if (n < 2)
    {
        for (auto& b : brain.blocks)
            b.synapses.clear();
        return true;
    }

    const auto keep = (std::uint32_t) juce::jmin<std::uint32_t> (maxStoredSynapses, n - 1);
    std::atomic<std::uint32_t> next { 0 }, done { 0 };
    std::atomic<bool> cancelled { false };

    auto worker = [&]
    {
        std::vector<std::pair<double, std::uint32_t>> collect;
        collect.reserve (n);

        for (std::uint32_t i = next++; i < n && ! cancelled; i = next++)
        {
            collect.clear();
            const auto& a = brain.blocks[i];
            for (std::uint32_t j = 0; j < n; ++j)
                if (j != i)
                    collect.emplace_back (blend (featureDistance (a.features, brain.blocks[j].features, params),
                                                 featureDistance (a.normalised, brain.blocks[j].normalised, params),
                                                 params.normalisedMix),
                                          j);

            std::partial_sort (collect.begin(), collect.begin() + keep, collect.end());
            auto& syn = brain.blocks[i].synapses;
            syn.resize (keep);
            for (std::uint32_t k = 0; k < keep; ++k)
                syn[k] = collect[k].second;

            ++done;
        }
    };

    const int numThreads = juce::jmax (1, (int) std::thread::hardware_concurrency() - 1);
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t)
        threads.emplace_back (worker);

    while (done < n && ! cancelled)
    {
        if (shouldExit && shouldExit())
            cancelled = true;
        if (progress)
            progress ((float) done / (float) n, "Growing synapses");
        std::this_thread::sleep_for (std::chrono::milliseconds (20));
    }

    for (auto& t : threads)
        t.join();

    return ! cancelled;
}

void BrainSearch::prepare (const Brain& brain)
{
    usage.assign (brain.blocks.size(), 0.0f);
    reset();
}

void BrainSearch::reset()
{
    std::fill (usage.begin(), usage.end(), 0.0f);
    // Samplebrain's jiggle(): start the synaptic walk somewhere random.
    currentIndex = usage.empty() ? 0 : (std::uint32_t) random.nextInt ((int) usage.size());
    currentError = 0.0;
}

double BrainSearch::compare (const Brain& brain, std::uint32_t index, const BlockFeatures& f, const BlockFeatures& n,
                             const SearchParams& p) const noexcept
{
    const auto& b = brain.blocks[index];
    return blend (blend (featureDistance (f, b.features, p), featureDistance (n, b.normalised, p), p.normalisedMix),
                  usage[index], p.usageImportance);
}

void BrainSearch::depleteUsage (float falloff) noexcept
{
    for (auto& u : usage)
        u *= falloff;
}

std::uint32_t BrainSearch::stickify (const Brain& brain, std::uint32_t closest, double distance,
                                     const BlockFeatures& f, const BlockFeatures& n, const SearchParams& p) noexcept
{
    const auto nextIndex = currentIndex + 1;
    if (p.stickyness > 0.0f && nextIndex < brain.numBlocks())
    {
        const double toNext = compare (brain, nextIndex, f, n, p);
        if (toNext * (1.0 - p.stickyness) < distance * p.stickyness)
            return currentIndex = nextIndex;
    }
    return currentIndex = closest;
}

std::uint32_t BrainSearch::search (const Brain& brain, const BlockFeatures& f, const BlockFeatures& n, const SearchParams& p)
{
    double closest = DBL_MAX;
    std::uint32_t closestIndex = 0;

    for (const auto& s : brain.sounds)
    {
        if (! s.enabled)
            continue;
        for (auto i = s.firstBlock; i < s.firstBlock + s.numBlocks; ++i)
        {
            const double d = compare (brain, i, f, n, p);
            if (d < closest)
            {
                closest = d;
                closestIndex = i;
            }
        }
    }

    depleteUsage (p.usageFalloff);
    usage[closestIndex] += usageFactor;
    currentError = closest;
    return stickify (brain, closestIndex, closest, f, n, p);
}

std::uint32_t BrainSearch::reverseSearch (const Brain& brain, const BlockFeatures& f, const BlockFeatures& n, const SearchParams& p)
{
    double furthest = -1.0;
    std::uint32_t furthestIndex = 0;

    for (const auto& s : brain.sounds)
    {
        if (! s.enabled)
            continue;
        for (auto i = s.firstBlock; i < s.firstBlock + s.numBlocks; ++i)
        {
            const double d = compare (brain, i, f, n, p);
            if (d > furthest)
            {
                furthest = d;
                furthestIndex = i;
            }
        }
    }

    depleteUsage (p.usageFalloff);
    usage[furthestIndex] += usageFactor;
    currentError = furthest;
    return currentIndex = furthestIndex;
}

std::uint32_t BrainSearch::searchSynapses (const Brain& brain, const BlockFeatures& f, const BlockFeatures& n, const SearchParams& p)
{
    const auto& synapses = brain.blocks[currentIndex].synapses;
    if (synapses.empty())
        return search (brain, f, n, p);

    double closest = DBL_MAX;
    std::uint32_t closestIndex = currentIndex;
    const auto count = juce::jmin (synapses.size(), (size_t) juce::jmax (1, p.numSynapses));

    for (size_t k = 0; k < count; ++k)
    {
        const auto i = synapses[k];
        if (! brain.isBlockActive (i))
            continue;
        const double d = compare (brain, i, f, n, p);
        if (d < closest)
        {
            closest = d;
            closestIndex = i;
        }
    }

    depleteUsage (p.usageFalloff);
    usage[currentIndex] += usageFactor;

    if (closest == DBL_MAX)   // every neighbour disabled: jump via a full search
        return search (brain, f, n, p);

    currentError = closest;
    return stickify (brain, closestIndex, closest, f, n, p);
}
} // namespace cortex::brain
