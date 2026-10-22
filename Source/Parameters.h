#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// APVTS parameter IDs (BRIEF.md §5). Keep these strings stable: they are
// persisted in host sessions.
namespace cortex::ParamIDs
{
    inline constexpr auto morphX        = "morphX";
    inline constexpr auto morphY        = "morphY";
    inline constexpr auto cutoffA       = "cutoffA";
    inline constexpr auto cutoffB       = "cutoffB";
    inline constexpr auto phaseMode     = "phaseMode";
    inline constexpr auto fftSize       = "fftSize";
    inline constexpr auto bSource       = "bSource";
    inline constexpr auto matchTimbre   = "matchTimbre";
    inline constexpr auto matchPitch    = "matchPitch";
    inline constexpr auto envTransplant = "envTransplant";
    inline constexpr auto blockSize     = "blockSize";
    inline constexpr auto overlap       = "overlap";
    inline constexpr auto novelty       = "novelty";
    inline constexpr auto boredom       = "boredom";
    inline constexpr auto stickyness    = "stickyness";
    inline constexpr auto searchMode    = "searchMode";
    inline constexpr auto cornerGainTL  = "cornerGainTL";
    inline constexpr auto cornerGainBR  = "cornerGainBR";
    inline constexpr auto dryWet        = "dryWet";
    inline constexpr auto outGain       = "outGain";
    inline constexpr auto limiter       = "limiter";
    inline constexpr auto gateOn        = "gateOn";
    inline constexpr auto gateThresh    = "gateThresh";
    inline constexpr auto autoMute      = "autoMute";
    inline constexpr auto autoMuteMs    = "autoMuteMs";
}

namespace cortex
{
    // Index -> FFT size for the fftSize choice parameter.
    inline constexpr int fftSizes[] { 512, 1024, 2048, 4096 };
    inline constexpr int defaultFftSizeIndex = 2;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
