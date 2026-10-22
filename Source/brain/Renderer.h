// Cortex: portions ported from Samplebrain (brain/src/renderer.h, block_stream.h),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.

#pragma once

#include "Brain.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <complex>
#include <memory>
#include <thread>

namespace cortex::brain
{
// Realtime brain playback. The live input is the target: it is chopped into
// blocks on the fly (Samplebrain's block_stream), each one is matched against
// the brain, and the chosen brain block is overlap-added to the output.
//
// Matching runs on a worker thread so a large brain can never overrun the
// audio callback: the audio thread analyses the target block, queues it, and
// starts the chosen block `searchDelay` samples later. If a search is late the
// block starts as soon as it arrives; if the worker falls behind it answers only
// the newest request and repeats that choice for the ones it skipped.
//
// Construct and destroy off the audio thread. processSample() never allocates,
// locks or blocks (except in blocking mode, meant for offline rendering).
class Renderer
{
public:
    explicit Renderer (std::shared_ptr<const Brain>);
    ~Renderer();

    const Brain& getBrain() const noexcept { return *brain; }
    std::shared_ptr<const Brain> getBrainPtr() const noexcept { return brain; }

    void setSearch (const SearchParams& p, SearchMode m) noexcept { params = p; mode = m; }

    // Offline rendering: wait for each search so the result is deterministic.
    void setBlocking (bool shouldBlock) noexcept { blocking = shouldBlock; }

    // Takes one target sample, returns one sample of brain output.
    float processSample (float target) noexcept;

    // A target block must be complete, then searched, before it can play.
    int getLatencySamples() const noexcept { return blockSize + searchDelay; }

    // Most recently started brain block, for the UI (-1 before the first).
    int getLastChosenBlock() const noexcept { return lastChosen.load (std::memory_order_relaxed); }

private:
    struct Request
    {
        BlockFeatures features;
        SearchParams params;
        SearchMode mode = SearchMode::basic;
        std::int64_t due = 0;
        float targetRms = 0.0f;
    };

    struct Result
    {
        std::uint32_t block = 0;
        std::int64_t due = 0;
        float gain = 1.0f;
        int grain = -1;   // transplanted copy in `grains`, or -1 to play the brain block as-is
    };

    void onTargetBlockComplete() noexcept;
    void startDueVoices() noexcept;
    void startVoice (const Result&) noexcept;
    void releaseGrain (int grain) noexcept;

    // Worker thread.
    void workerLoop();
    std::uint32_t choose (const Request&);
    int acquireGrain() noexcept;
    void transplantEnvelope (const Block&, const BlockFeatures& target, float amount, float cutoff, float* out);

    struct Voice
    {
        const float* pcm = nullptr;   // nullptr = idle
        int grain = -1;
        int position = 0;
        float gain = 1.0f;
    };
    static constexpr int maxVoices = 8;
    static constexpr int queueSize = 32;
    static constexpr int numGrains = queueSize + maxVoices + 2;

    std::shared_ptr<const Brain> brain;
    BlockAnalyser analyser;
    SearchParams params;
    SearchMode mode = SearchMode::basic;
    bool blocking = false;

    int blockSize = 0, hop = 0, searchDelay = 0;
    float outputGain = 1.0f;

    std::vector<float> targetRing, targetBlock, windowedTarget;
    int ringPos = 0;
    std::int64_t samplesSeen = 0;
    BlockFeatures targetFeatures;

    std::array<Voice, maxVoices> voices {};
    std::atomic<int> lastChosen { -1 };

    // Audio -> worker requests, worker -> audio results (single producer/consumer each).
    juce::AbstractFifo requestFifo { queueSize }, resultFifo { queueSize };
    std::array<Request, queueSize> requests;
    std::array<Result, queueSize> results;
    std::atomic<std::uint32_t> wakeCounter { 0 };
    std::atomic<std::int64_t> requestsSent { 0 }, resultsSent { 0 };
    std::atomic<bool> stopWorker { false };

    // Envelope-transplanted grains: written by the worker, played by voices.
    // State 0 = free, 1 = owned (queued or playing); a voice frees it when done.
    std::vector<std::vector<float>> grains;
    std::array<std::atomic<int>, numGrains> grainState {};

    // Worker-only: transplant FFT (2x the block, zero-padded, so the envelope
    // filter's short impulse response doesn't wrap) and its cosine table.
    std::unique_ptr<juce::dsp::FFT> transplantFft;
    int transplantSize = 0;
    float transplantInverseScale = 1.0f;
    std::vector<std::complex<float>> transplantTime, transplantSpec;
    std::vector<float> cepstrumCos;   // numCepstrum x (transplantSize/2 + 1)

    // Worker-only search state.
    BrainSearch search;
    MatchQuery heldQuery;
    int slideHeld = 0;

    std::thread worker;
};
} // namespace cortex::brain
