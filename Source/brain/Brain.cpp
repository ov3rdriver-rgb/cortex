// Cortex: portions ported from Samplebrain (brain/src/brain.cpp),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.
//
// Differences from Samplebrain: blocks are compared on split-cepstrum features
// (timbre and pitch vectors, standardised per brain) instead of FFT bins + MFCCs
// blended by `ratio`, and there is no normalised-block comparison.

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
    // standardised features, 1 per use puts usage on a comparable scale.
    constexpr float usageFactor = 1.0f;

    double blend (double a, double b, double t) noexcept { return a * (1.0 - t) + b * t; }

    // Sum of squared differences with independent accumulators, so it vectorises.
    template <int N>
    float squaredDistance (const float* a, const float* b) noexcept
    {
        float acc[4] {};
        int i = 0;
        for (; i + 4 <= N; i += 4)
            for (int j = 0; j < 4; ++j)
            {
                const float d = a[i + j] - b[i + j];
                acc[j] += d * d;
            }
        for (; i < N; ++i)
        {
            const float d = a[i] - b[i];
            acc[0] += d * d;
        }
        return (acc[0] + acc[1]) + (acc[2] + acc[3]);
    }

    // Per-dimension mean and 1/stddev over all blocks.
    template <int N, typename GetVector>
    void standardise (const std::vector<Block>& blocks, GetVector get, std::array<float, N>& mean, std::array<float, N>& scale)
    {
        std::array<double, N> sum {}, sumSq {};
        for (const auto& b : blocks)
        {
            const auto v = get (b.features);
            for (int d = 0; d < N; ++d)
            {
                sum[(size_t) d] += v[(size_t) d];
                sumSq[(size_t) d] += (double) v[(size_t) d] * v[(size_t) d];
            }
        }

        const double n = juce::jmax<double> (1.0, (double) blocks.size());
        for (int d = 0; d < N; ++d)
        {
            const double m = sum[(size_t) d] / n;
            const double var = juce::jmax (0.0, sumSq[(size_t) d] / n - m * m);
            mean[(size_t) d] = (float) m;
            // A dimension that never varies in this brain can't tell blocks
            // apart; keep it from dominating via a floor on the deviation.
            scale[(size_t) d] = (float) (1.0 / std::sqrt (juce::jmax (var, 1.0e-6)));
        }
    }
}

float rmsOf (const std::vector<float>& pcm) noexcept
{
    if (pcm.empty())
        return 1.0f;
    double sum = 0.0;
    for (auto v : pcm)
        sum += (double) v * v;
    return (float) std::sqrt (sum / (double) pcm.size());
}

bool Brain::isBlockActive (std::uint32_t index) const noexcept
{
    for (const auto& s : sounds)
        if (index >= s.firstBlock && index < s.firstBlock + s.numBlocks)
            return s.enabled;
    return false;
}

void Brain::finalise()
{
    standardise<numTimbre> (blocks, [] (const BlockFeatures& f) { return f.timbreVector(); }, timbreMean, timbreScale);
    standardise<numPitch> (blocks, [] (const BlockFeatures& f) { return f.pitchVector(); }, pitchMean, pitchScale);

    const auto n = blocks.size();
    blockRms.resize (n);
    for (size_t i = 0; i < n; ++i)
        blockRms[i] = rmsOf (blocks[i].pcm);

    timbreTable.resize (n * numTimbre);
    pitchTable.resize (n * numPitch);

    for (size_t i = 0; i < n; ++i)
    {
        const auto q = makeQuery (blocks[i].features);
        std::copy (q.timbre.begin(), q.timbre.end(), timbreTable.begin() + (std::ptrdiff_t) (i * numTimbre));
        std::copy (q.pitch.begin(), q.pitch.end(), pitchTable.begin() + (std::ptrdiff_t) (i * numPitch));
    }
}

MatchQuery Brain::makeQuery (const BlockFeatures& f) const noexcept
{
    MatchQuery q;
    const auto t = f.timbreVector();
    for (int d = 0; d < numTimbre; ++d)
        q.timbre[(size_t) d] = (t[(size_t) d] - timbreMean[(size_t) d]) * timbreScale[(size_t) d];

    const auto p = f.pitchVector();
    for (int d = 0; d < numPitch; ++d)
        q.pitch[(size_t) d] = (p[(size_t) d] - pitchMean[(size_t) d]) * pitchScale[(size_t) d];
    return q;
}

float Brain::distanceTo (std::uint32_t index, const MatchQuery& q, const SearchParams& p) const noexcept
{
    const float total = p.timbreWeight + p.pitchWeight;
    const float wTimbre = total > 0.0f ? p.timbreWeight / total : 0.5f;
    const float wPitch = 1.0f - wTimbre;

    float d = 0.0f;
    if (wTimbre > 0.0f)
        d += wTimbre * squaredDistance<numTimbre> (timbreTable.data() + (size_t) index * numTimbre, q.timbre.data()) / numTimbre;
    if (wPitch > 0.0f)
        d += wPitch * squaredDistance<numPitch> (pitchTable.data() + (size_t) index * numPitch, q.pitch.data()) / numPitch;
    return d;
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
        std::vector<std::pair<float, std::uint32_t>> collect;
        collect.reserve (n);

        for (std::uint32_t i = next++; i < n && ! cancelled; i = next++)
        {
            collect.clear();
            const auto q = brain.makeQuery (brain.blocks[i].features);
            for (std::uint32_t j = 0; j < n; ++j)
                if (j != i)
                    collect.emplace_back (brain.distanceTo (j, q, params), j);

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

double BrainSearch::compare (const Brain& brain, std::uint32_t index, const MatchQuery& q, const SearchParams& p) const noexcept
{
    return blend (brain.distanceTo (index, q, p), usage[index], p.usageImportance);
}

void BrainSearch::depleteUsage (float falloff) noexcept
{
    for (auto& u : usage)
        u *= falloff;
}

std::uint32_t BrainSearch::stickify (const Brain& brain, std::uint32_t closest, double distance,
                                     const MatchQuery& q, const SearchParams& p) noexcept
{
    const auto nextIndex = currentIndex + 1;
    if (p.stickyness > 0.0f && nextIndex < brain.numBlocks())
    {
        const double toNext = compare (brain, nextIndex, q, p);
        if (toNext * (1.0 - p.stickyness) < distance * p.stickyness)
            return currentIndex = nextIndex;
    }
    return currentIndex = closest;
}

std::uint32_t BrainSearch::search (const Brain& brain, const MatchQuery& q, const SearchParams& p)
{
    double closest = DBL_MAX;
    std::uint32_t closestIndex = 0;

    for (const auto& s : brain.sounds)
    {
        if (! s.enabled)
            continue;
        for (auto i = s.firstBlock; i < s.firstBlock + s.numBlocks; ++i)
        {
            const double d = compare (brain, i, q, p);
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
    return stickify (brain, closestIndex, closest, q, p);
}

std::uint32_t BrainSearch::reverseSearch (const Brain& brain, const MatchQuery& q, const SearchParams& p)
{
    double furthest = -1.0;
    std::uint32_t furthestIndex = 0;

    for (const auto& s : brain.sounds)
    {
        if (! s.enabled)
            continue;
        for (auto i = s.firstBlock; i < s.firstBlock + s.numBlocks; ++i)
        {
            const double d = compare (brain, i, q, p);
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

std::uint32_t BrainSearch::searchSynapses (const Brain& brain, const MatchQuery& q, const SearchParams& p)
{
    const auto& synapses = brain.blocks[currentIndex].synapses;
    if (synapses.empty())
        return search (brain, q, p);

    double closest = DBL_MAX;
    std::uint32_t closestIndex = currentIndex;
    const auto count = juce::jmin (synapses.size(), (size_t) juce::jmax (1, p.numSynapses));

    for (size_t k = 0; k < count; ++k)
    {
        const auto i = synapses[k];
        if (! brain.isBlockActive (i))
            continue;
        const double d = compare (brain, i, q, p);
        if (d < closest)
        {
            closest = d;
            closestIndex = i;
        }
    }

    depleteUsage (p.usageFalloff);
    usage[currentIndex] += usageFactor;

    if (closest == DBL_MAX)   // every neighbour disabled: jump via a full search
        return search (brain, q, p);

    currentError = closest;
    return stickify (brain, closestIndex, closest, q, p);
}
} // namespace cortex::brain
