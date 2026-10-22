// Cortex: portions ported from Samplebrain (brain/src/search_params.h),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.

#pragma once

#include "Block.h"

namespace cortex::brain
{
enum class SearchMode
{
    basic = 0,
    reverse,
    synaptic,
    synapticSlide
};

struct SearchParams
{
    // Split-cepstrum matching (BRIEF.md §4 idea 2), replacing Samplebrain's single
    // FFT-vs-MFCC `ratio`: how much the spectral envelope and the pitch content
    // each count. Both zero counts them equally.
    float timbreWeight = 0.5f;
    float pitchWeight = 0.5f;

    float usageImportance = 0.0f;   // "novelty": penalise recently used blocks
    float usageFalloff = 0.9f;      // per-search decay of usage ("boredom" memory)
    float stickyness = 0.0f;        // prefer the block that follows the last one

    // Envelope transplant (BRIEF.md §4 idea 3): pull each matched grain's
    // spectral envelope towards the input block's, so the brain "sings with
    // your mouth". The envelope/detail split uses this lifter cutoff.
    float envTransplant = 0.0f;
    float transplantCutoff = 25.0f;

    // Synaptic drift with cepstral glide (BRIEF.md §4 idea 4): each new grain
    // starts from the previous grain's spectral envelope and glides to its
    // own, so jumps between blocks become timbral slides.
    float cepstralGlide = 0.0f;

    // Chance per search of jumping to a random block instead of the match.
    float randomJump = 0.0f;

    int numSynapses = 20;           // how many stored synapses synaptic search looks at
    double slideError = 1.0;        // synaptic slide: hold the target until error drops below this
    int maxSlideHold = 16;          // ...but never for more than this many hops
};

// The user-facing search knobs, as Samplebrain names them.
struct SearchKnobs
{
    float novelty = 0.0f, boredom = 0.0f, stickyness = 0.0f;
};

// Chaos / Memory macros (BRIEF.md §4 idea 9) on top of the individual knobs.
// Both at 0 leave the knobs exactly as set.
//   Chaos  - more novelty, less stickyness, and random jumps (up to 30%).
//   Memory - longer usage memory (boredom) and more stickyness, so the brain
//            remembers what it played and follows sequences in the source.
inline SearchParams applySearchKnobs (SearchParams p, SearchKnobs k, float chaos, float memory) noexcept
{
    auto clamp01 = [] (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
    const float novelty = clamp01 (k.novelty + 0.8f * chaos);
    const float boredom = clamp01 (k.boredom + 0.5f * memory);
    const float stickyness = clamp01 (k.stickyness + 0.8f * memory - 0.5f * chaos);

    p.usageImportance = novelty;
    p.usageFalloff = 0.5f + 0.49f * boredom;
    p.stickyness = stickyness;
    p.randomJump = 0.3f * chaos * chaos;
    return p;
}
} // namespace cortex::brain
