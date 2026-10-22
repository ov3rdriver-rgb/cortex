#pragma once

#include "Cepstrum.h"
#include "PhaseBlend.h"

#include <vector>

namespace cortex::dsp
{
struct MorphParams
{
    float x = 0.0f;           // envelope A -> B
    float y = 0.0f;           // detail A -> B
    float cutoffA = 25.0f;    // lifter cutoff quefrency (bins)
    float cutoffB = 25.0f;
    PhaseMode phaseMode = PhaseMode::fromA;
    float cornerGainTLdB = 0.0f;  // A envelope + B detail
    float cornerGainBRdB = 0.0f;  // B envelope + A detail
};

// The A/B cepstral morph core (BRIEF.md §3), one channel. Works on windowed
// time-domain frames handed over by STFT.
class CepstralMorph
{
public:
    // Allocates for the largest FFT size. Not realtime-safe.
    void prepare (double sampleRate);
    // Realtime-safe.
    void setOrder (int order);

    // Parameters glide towards their targets once per frame (~20 ms).
    void setTargets (const MorphParams& p) noexcept { target = p; }
    void snapToTargets() noexcept { current = target; }
    const MorphParams& getCurrent() const noexcept { return current; }

    void processFrame (const float* frameA, const float* frameB, float* frameOut);

private:
    void advanceSmoothing() noexcept;

    FFTBank fft;
    std::vector<Complex> timeBuf, specA, specB, cepsA, cepsB, scratch;
    MorphParams target, current;
    double sampleRate = 48000.0;
    float smoothCoeff = 0.0f;
};
} // namespace cortex::dsp
