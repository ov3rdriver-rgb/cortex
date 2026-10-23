#pragma once

#include "STFT.h"

#include <array>

namespace cortex::dsp
{
// The morph triangles: one source per corner, positions in a unit square.
struct TrianglePoint { float x, y; };

inline constexpr int brain1Corner = 0, brain2Corner = 1, inputCorner = 2;
inline constexpr TrianglePoint triangleCorner[3] {
    { 0.5f, 1.0f },   // Brain 1, top
    { 0.0f, 0.0f },   // Brain 2, bottom left
    { 1.0f, 0.0f },   // Input, bottom right
};

// Barycentric weights (indexed by Source) of a point. Points outside the
// triangle are pulled onto it: negative weights are dropped and the rest
// renormalised, so the weights always sum to 1.
inline std::array<float, numSources> triangleWeights (float x, float y) noexcept
{
    const auto& a = triangleCorner[brain1Corner];
    const auto& b = triangleCorner[brain2Corner];
    const auto& c = triangleCorner[inputCorner];

    const float det = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    float wA = ((b.y - c.y) * (x - c.x) + (c.x - b.x) * (y - c.y)) / det;
    float wB = ((c.y - a.y) * (x - c.x) + (a.x - c.x) * (y - c.y)) / det;
    float wC = 1.0f - wA - wB;

    wA = wA > 0.0f ? wA : 0.0f;
    wB = wB > 0.0f ? wB : 0.0f;
    wC = wC > 0.0f ? wC : 0.0f;
    const float sum = wA + wB + wC;

    std::array<float, numSources> w {};
    w[input] = sum > 0.0f ? wC / sum : 1.0f;
    w[brain1] = sum > 0.0f ? wA / sum : 0.0f;
    w[brain2] = sum > 0.0f ? wB / sum : 0.0f;
    return w;
}
} // namespace cortex::dsp
