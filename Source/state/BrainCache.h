#pragma once

#include "brain/Brain.h"

#include <juce_core/juce_core.h>

// Serialises a built brain (.cortexbrain) so projects reopen without
// re-analysing their samples (BRIEF.md §2, §7).
namespace cortex::BrainCache
{
    // Where cached brains live by default.
    juce::File defaultDirectory();

    // Identifies a brain build: the source files (path, size, modification time)
    // plus every setting that changes the analysis. Same key -> same brain.
    juce::String keyFor (const juce::Array<juce::File>& files, const brain::BrainSettings&);

    juce::File fileFor (const juce::File& directory, const juce::String& key);

    bool save (const brain::Brain&, const juce::File&);
    std::shared_ptr<brain::Brain> load (const juce::File&);

    // Deletes least-recently-used caches until the directory is under maxBytes.
    // `keep` is never deleted. load() refreshes a file's timestamp.
    inline constexpr juce::int64 defaultMaxBytes = (juce::int64) 3 * 1024 * 1024 * 1024;
    void prune (const juce::File& directory, juce::int64 maxBytes, const juce::File& keep);
}
