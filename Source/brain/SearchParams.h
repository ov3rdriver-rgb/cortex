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

    int numSynapses = 20;           // how many stored synapses synaptic search looks at
    double slideError = 1.0;        // synaptic slide: hold the target until error drops below this
    int maxSlideHold = 16;          // ...but never for more than this many hops
};
} // namespace cortex::brain
