#pragma once

namespace cortex::dsp
{
// BRIEF.md §3 step 7. Values match the phaseMode parameter's choice index.
enum class PhaseMode
{
    fromA = 0,
    fromB,
    interpolate,   // shortest-arc lerp A -> B, weighted by morph Y
    followDetail   // A when Y < 0.5, B above, crossfaded around the middle
};

// Returns the output phase for one bin. `y` is the detail morph amount (0..1).
float blendPhase (float phaseA, float phaseB, PhaseMode mode, float y) noexcept;
} // namespace cortex::dsp
