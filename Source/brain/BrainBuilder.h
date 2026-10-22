// Cortex: portions ported from Samplebrain (brain/src/brain.cpp: load_sound, init, chop_and_add),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.

#pragma once

#include "Brain.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <mutex>

namespace cortex::brain
{
struct BrainSource
{
    juce::String path;
    std::vector<float> mono;   // already at the brain's sample rate
};

// Expands folders (recursively) into the audio files JUCE can read, sorted by path.
juce::Array<juce::File> collectAudioFiles (const juce::StringArray& filesOrFolders, juce::AudioFormatManager&);

// Decodes a file, mixes it to mono and resamples it to `targetRate`.
bool decodeToMono (juce::AudioFormatManager&, const juce::File&, double targetRate, BrainSource&);

// Chops sources into blocks, analyses them and grows synapses. Thread-agnostic.
// Returns nullptr if cancelled.
std::shared_ptr<Brain> buildBrain (const std::vector<BrainSource>&, const BrainSettings&, const SearchParams&,
                                   const ProgressFn&, const ShouldExitFn&);

// Builds brains on a background thread: cache hit -> load, else decode, build, save.
class BrainBuilder : private juce::Thread
{
public:
    struct Request
    {
        juce::Array<juce::File> files;
        BrainSettings settings;
        juce::File cacheDirectory;
    };

    // Called on the builder thread when a build finishes. A cancelled build
    // (superseded or shut down) does not call it.
    using Callback = std::function<void (std::shared_ptr<const Brain>, const juce::String& message)>;

    explicit BrainBuilder (Callback);
    ~BrainBuilder() override;

    // Cancels any build in progress and starts this one.
    void start (Request);
    void cancel();

    bool isBuilding() const { return isThreadRunning(); }
    float getProgress() const { return progress.load(); }
    juce::String getStatus() const;

private:
    void run() override;
    void setStatus (float p, const juce::String& s);

    Callback callback;
    Request request;
    std::atomic<float> progress { 0.0f };
    mutable std::mutex statusLock;
    juce::String status;
};
} // namespace cortex::brain
