#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

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
    // Per brain: how its samples are sliced and how it searches them. Sessions
    // from before 1.2 saved one shared set (the ids without a suffix), which
    // is applied to both brains on load.
    inline constexpr auto blockSizeB1     = "blockSizeB1";
    inline constexpr auto overlapB1       = "overlapB1";
    inline constexpr auto chaosB1         = "chaosB1";
    inline constexpr auto memoryB1        = "memoryB1";
    inline constexpr auto cepstralGlideB1 = "cepstralGlideB1";
    inline constexpr auto matchTimbreB1   = "matchTimbreB1";
    inline constexpr auto matchPitchB1    = "matchPitchB1";
    inline constexpr auto envTransplantB1 = "envTransplantB1";
    inline constexpr auto searchModeB1    = "searchModeB1";
    inline constexpr auto noveltyB1       = "noveltyB1";
    inline constexpr auto boredomB1       = "boredomB1";
    inline constexpr auto stickynessB1    = "stickynessB1";
    inline constexpr auto blockSizeB2     = "blockSizeB2";
    inline constexpr auto overlapB2       = "overlapB2";
    inline constexpr auto chaosB2         = "chaosB2";
    inline constexpr auto memoryB2        = "memoryB2";
    inline constexpr auto cepstralGlideB2 = "cepstralGlideB2";
    inline constexpr auto matchTimbreB2   = "matchTimbreB2";
    inline constexpr auto matchPitchB2    = "matchPitchB2";
    inline constexpr auto envTransplantB2 = "envTransplantB2";
    inline constexpr auto searchModeB2    = "searchModeB2";
    inline constexpr auto noveltyB2       = "noveltyB2";
    inline constexpr auto boredomB2       = "boredomB2";
    inline constexpr auto stickynessB2    = "stickynessB2";

    // One brain's ids, so code can take "brain b" and index. `legacy` is the
    // pre-1.2 shared id of the same control.
    struct BrainParam { const char* id; const char* legacy; };
    struct BrainIds
    {
        BrainParam blockSize, overlap, chaos, memory, cepstralGlide, matchTimbre, matchPitch,
                   envTransplant, searchMode, novelty, boredom, stickyness;

        // The slicing pair first, then the search controls in the order the Search tab shows them.
        std::array<BrainParam, 12> all() const
        {
            return { blockSize, overlap, chaos, memory, cepstralGlide, matchTimbre, matchPitch,
                     envTransplant, searchMode, novelty, boredom, stickyness };
        }
    };
    inline constexpr BrainIds brainIds[2] {
        { { blockSizeB1, "blockSize" }, { overlapB1, "overlap" }, { chaosB1, "chaos" }, { memoryB1, "memory" },
          { cepstralGlideB1, "cepstralGlide" }, { matchTimbreB1, "matchTimbre" }, { matchPitchB1, "matchPitch" },
          { envTransplantB1, "envTransplant" }, { searchModeB1, "searchMode" }, { noveltyB1, "novelty" },
          { boredomB1, "boredom" }, { stickynessB1, "stickyness" } },
        { { blockSizeB2, "blockSize" }, { overlapB2, "overlap" }, { chaosB2, "chaos" }, { memoryB2, "memory" },
          { cepstralGlideB2, "cepstralGlide" }, { matchTimbreB2, "matchTimbre" }, { matchPitchB2, "matchPitch" },
          { envTransplantB2, "envTransplant" }, { searchModeB2, "searchMode" }, { noveltyB2, "novelty" },
          { boredomB2, "boredom" }, { stickynessB2, "stickyness" } },
    };

    inline constexpr auto dryWet        = "dryWet";
    inline constexpr auto outGain       = "outGain";
    inline constexpr auto limiter       = "limiter";
    inline constexpr auto gateOn        = "gateOn";
    inline constexpr auto gateThresh    = "gateThresh";
    inline constexpr auto autoMute      = "autoMute";
    inline constexpr auto autoMuteMs    = "autoMuteMs";
    inline constexpr auto lowLatency    = "lowLatency";
    inline constexpr auto lfoRate       = "lfoRate";
    inline constexpr auto lfoDepth      = "lfoDepth";
    inline constexpr auto lfoShape      = "lfoShape";
    inline constexpr auto lfoTarget     = "lfoTarget";
    inline constexpr auto envDepth      = "envDepth";
    inline constexpr auto envAttack     = "envAttack";
    inline constexpr auto envRelease    = "envRelease";
    inline constexpr auto theme         = "theme";          // UI only: Dark / Light
    inline constexpr auto captureSeconds = "captureSeconds"; // how much recent output Capture saves

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

    // The output capture ring (Capture button): always holds the last
    // maxCaptureSeconds of output; captureSeconds chooses how much is saved.
    inline constexpr int minCaptureSeconds = 5;
    inline constexpr int maxCaptureSeconds = 120;
    inline constexpr int defaultCaptureSeconds = 60;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (bool instrument);
}
