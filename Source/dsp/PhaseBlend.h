#pragma once

#include "STFT.h"

#include <array>

namespace cortex::dsp
{
// BRIEF.md §3 step 7, for three sources. Values match the phaseMode
// parameter's choice index.
enum class PhaseSource
{
    followDetail = 0,   // the sources' phases weighted by their detail weights
    input,
    brain1,
    brain2
};

// Output phase for one bin. followDetail takes the angle of the sum of each
// source's unit phasor weighted by its detail weight squared: a pure source
// gives exactly its own phase, and between sources the dominant one leads.
float blendPhase (const std::array<float, numSources>& phases,
                  const std::array<float, numSources>& detailWeights,
                  PhaseSource) noexcept;
} // namespace cortex::dsp
