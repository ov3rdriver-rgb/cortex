#include "PhaseBlend.h"

#include <cmath>
#include <numbers>

namespace cortex::dsp
{
namespace
{
    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;

    float shortestArcLerp (float a, float b, float t) noexcept
    {
        float d = b - a;
        d -= twoPi * std::round (d / twoPi);
        return a + t * d;
    }

    // Width of the A/B crossfade zone around y = 0.5 in followDetail mode.
    constexpr float followZoneLo = 0.4f;
    constexpr float followZoneHi = 0.6f;
}

float blendPhase (float phaseA, float phaseB, PhaseMode mode, float y) noexcept
{
    switch (mode)
    {
        case PhaseMode::fromA:       return phaseA;
        case PhaseMode::fromB:       return phaseB;
        case PhaseMode::interpolate: return shortestArcLerp (phaseA, phaseB, y);
        case PhaseMode::followDetail:
        {
            if (y <= followZoneLo) return phaseA;
            if (y >= followZoneHi) return phaseB;
            const float t = (y - followZoneLo) / (followZoneHi - followZoneLo);
            return shortestArcLerp (phaseA, phaseB, t * t * (3.0f - 2.0f * t));
        }
    }
    return phaseA;
}
} // namespace cortex::dsp
