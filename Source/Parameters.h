#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// APVTS parameter IDs (BRIEF.md §5). Keep these strings stable: they are
// persisted in host sessions.
namespace cortex::ParamIDs
{
    // The two morph triangles (corners: Brain 1 top, Brain 2 bottom-left,
    // Input bottom-right), each as an x/y position.
    inline constexpr auto envX          = "envX";
    inline constexpr auto envY          = "envY";
    inline constexpr auto detailX       = "detailX";
    inline constexpr auto detailY       = "detailY";
    inline constexpr auto linkTriangles = "linkTriangles";

    // Per source: Input, Brain 1, Brain 2.
    inline constexpr auto cutoffIn      = "cutoffIn";
    inline constexpr auto cutoffB1      = "cutoffB1";
    inline constexpr auto cutoffB2      = "cutoffB2";
    inline constexpr auto formantIn     = "formantIn";
    inline constexpr auto formantB1     = "formantB1";
    inline constexpr auto formantB2     = "formantB2";
    inline constexpr auto freezeEnvIn   = "freezeEnvIn";
    inline constexpr auto freezeDetIn   = "freezeDetIn";
    inline constexpr auto freezeEnvB1   = "freezeEnvB1";
    inline constexpr auto freezeDetB1   = "freezeDetB1";
    inline constexpr auto freezeEnvB2   = "freezeEnvB2";
    inline constexpr auto freezeDetB2   = "freezeDetB2";

    inline constexpr auto phaseMode     = "phaseMode";
    inline constexpr auto fftSize       = "fftSize";
    inline constexpr auto inputSource   = "inputSource";   // effect only: Track / Sidechain
    inline constexpr auto matchTimbre   = "matchTimbre";
    inline constexpr auto matchPitch    = "matchPitch";
    inline constexpr auto envTransplant = "envTransplant";
    inline constexpr auto blockSize     = "blockSize";
    inline constexpr auto overlap       = "overlap";
    inline constexpr auto novelty       = "novelty";
    inline constexpr auto boredom       = "boredom";
    inline constexpr auto stickyness    = "stickyness";
    inline constexpr auto searchMode    = "searchMode";
    inline constexpr auto dryWet        = "dryWet";
    inline constexpr auto outGain       = "outGain";
    inline constexpr auto limiter       = "limiter";
    inline constexpr auto gateOn        = "gateOn";
    inline constexpr auto gateThresh    = "gateThresh";
    inline constexpr auto autoMute      = "autoMute";
    inline constexpr auto autoMuteMs    = "autoMuteMs";
    inline constexpr auto lowLatency    = "lowLatency";
    inline constexpr auto cepstralGlide = "cepstralGlide";
    inline constexpr auto chaos         = "chaos";
    inline constexpr auto memory        = "memory";
    inline constexpr auto lfoRate       = "lfoRate";
    inline constexpr auto lfoDepth      = "lfoDepth";
    inline constexpr auto lfoShape      = "lfoShape";
    inline constexpr auto lfoTarget     = "lfoTarget";
    inline constexpr auto envDepth      = "envDepth";
    inline constexpr auto envAttack     = "envAttack";
    inline constexpr auto envRelease    = "envRelease";
    inline constexpr auto theme         = "theme";          // UI only: Dark / Light

    // Cortex Instrument only.
    inline constexpr auto oscWave       = "oscWave";
    inline constexpr auto oscGlide      = "oscGlide";
    inline constexpr auto ampAttack     = "ampAttack";
    inline constexpr auto ampRelease    = "ampRelease";
}

namespace cortex
{
    // Index -> FFT size for the fftSize choice parameter.
    inline constexpr int fftSizes[] { 512, 1024, 2048, 4096 };
    inline constexpr int defaultFftSizeIndex = 2;
    inline constexpr int lowLatencyFftSize = 512;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (bool instrument);
}
