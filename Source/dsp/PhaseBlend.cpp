#include "PhaseBlend.h"

#include <cmath>

namespace cortex::dsp
{
float blendPhase (const std::array<float, numSources>& phases,
                  const std::array<float, numSources>& detailWeights,
                  PhaseSource mode) noexcept
{
    switch (mode)
    {
        case PhaseSource::input:  return phases[input];
        case PhaseSource::brain1: return phases[brain1];
        case PhaseSource::brain2: return phases[brain2];
        case PhaseSource::followDetail: break;
    }

    float re = 0.0f, im = 0.0f;
    int strongest = input;
    for (int s = 0; s < numSources; ++s)
    {
        const float w = detailWeights[(size_t) s] * detailWeights[(size_t) s];
        re += w * std::cos (phases[(size_t) s]);
        im += w * std::sin (phases[(size_t) s]);
        if (detailWeights[(size_t) s] > detailWeights[(size_t) strongest])
            strongest = s;
    }

    // Phasors that cancel (equal weights, opposite phases): fall back to the
    // strongest source rather than an arbitrary angle.
    if (re * re + im * im < 1.0e-12f)
        return phases[(size_t) strongest];
    return std::atan2 (im, re);
}
} // namespace cortex::dsp
