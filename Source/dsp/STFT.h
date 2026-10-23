#pragma once

#include "Cepstrum.h"

#include <array>
#include <vector>

namespace cortex::dsp
{
// The morph's sources (BRIEF.md §3, extended to three): the live input and two brains.
inline constexpr int numSources = 3;
enum Source { input = 0, brain1 = 1, brain2 = 2 };

// Multi-input, one-output overlap-add framework (BRIEF.md §2). Hann analysis
// and synthesis windows, overlap 8 (hop = N/8). Latency is exactly N samples.
//
// Every hop, the frame callback receives the last N samples of each input
// (analysis-windowed) and fills an N-sample output frame, which is then
// synthesis-windowed and overlap-added.
class STFT
{
public:
    static constexpr int overlap = 8;
    using Frames = std::array<const float*, numSources>;

    // Allocates for the largest FFT size. Not realtime-safe.
    void prepare();
    // Switches size and clears all state. Realtime-safe (no allocation).
    void setOrder (int order);
    void reset();

    int size() const noexcept { return n; }
    int hop() const noexcept  { return n / overlap; }

    // While set, frames are not analysed (CPU saver for auto-mute); the
    // overlap-add tail drains and the output falls to silence.
    void setSkipFrames (bool shouldSkip) noexcept { skipFrames = shouldSkip; }

    // FrameFn: void (const Frames& frames, float* frameOut)
    template <typename FrameFn>
    float processSample (const std::array<float, numSources>& in, FrameFn&& onFrame)
    {
        for (int s = 0; s < numSources; ++s)
            inputs[(size_t) s][(size_t) pos] = in[(size_t) s];
        const float y = outAcc[(size_t) pos];
        outAcc[(size_t) pos] = 0.0f;

        if (++pos == n)
            pos = 0;

        if (++hopCounter == hop())
        {
            hopCounter = 0;
            if (! skipFrames)
            {
                gatherFrames();
                onFrame (framePointers, frameOut.data());
                overlapAdd();
            }
        }

        return y;
    }

private:
    void gatherFrames() noexcept;
    void overlapAdd() noexcept;

    std::array<std::vector<float>, numSources> inputs, frames;
    Frames framePointers {};
    std::vector<float> outAcc, frameOut;
    std::array<std::vector<float>, maxFftOrder - minFftOrder + 1> windows;
    std::array<float, maxFftOrder - minFftOrder + 1> olaScales {};
    const float* window = nullptr;
    float olaScale = 1.0f;

    int n = 2048, pos = 0, hopCounter = 0;
    bool skipFrames = false;
};
} // namespace cortex::dsp
