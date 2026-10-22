#pragma once

#include "Cepstrum.h"

#include <vector>

namespace cortex::dsp
{
// Two-input, one-output overlap-add framework (BRIEF.md §2). Hann analysis and
// synthesis windows, overlap 8 (hop = N/8). Latency is exactly N samples.
//
// Every hop, the frame callback receives the last N samples of each input
// (analysis-windowed) and fills an N-sample output frame, which is then
// synthesis-windowed and overlap-added.
class STFT
{
public:
    static constexpr int overlap = 8;

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

    // FrameFn: void (const float* frameA, const float* frameB, float* frameOut)
    template <typename FrameFn>
    float processSample (float a, float b, FrameFn&& onFrame)
    {
        inA[(size_t) pos] = a;
        inB[(size_t) pos] = b;
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
                onFrame (frameA.data(), frameB.data(), frameOut.data());
                overlapAdd();
            }
        }

        return y;
    }

private:
    void gatherFrames() noexcept;
    void overlapAdd() noexcept;

    std::vector<float> inA, inB, outAcc, frameA, frameB, frameOut;
    std::array<std::vector<float>, maxFftOrder - minFftOrder + 1> windows;
    std::array<float, maxFftOrder - minFftOrder + 1> olaScales {};
    const float* window = nullptr;
    float olaScale = 1.0f;

    int n = 2048, pos = 0, hopCounter = 0;
    bool skipFrames = false;
};
} // namespace cortex::dsp
