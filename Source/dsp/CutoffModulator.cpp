#include "CutoffModulator.h"

#include <cmath>

namespace cortex::dsp
{
namespace
{
    // Follower level is mapped from this range (dBFS) to 0..1.
    constexpr float floorDb = -60.0f;

    float coefficient (float ms, double sampleRate)
    {
        return ms <= 0.0f ? 1.0f : 1.0f - std::exp (-1.0f / (ms * 0.001f * (float) sampleRate));
    }
}

void CutoffModulator::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void CutoffModulator::reset()
{
    phase = 0.0;
    lfoValue = 0.0f;
    heldRandom = 0.0f;
    envelope = envLevel = 0.0f;
}

CutoffModulator::Offsets CutoffModulator::process (const Settings& s, const float* input, int numSamples) noexcept
{
    // LFO: evaluated at the block's start, advanced across it.
    const double p = phase;
    switch (s.shape)
    {
        case Shape::sine:          lfoValue = (float) std::sin (juce::MathConstants<double>::twoPi * p); break;
        case Shape::triangle:      lfoValue = (float) (1.0 - 4.0 * std::abs (p - 0.5)); break;
        case Shape::saw:           lfoValue = (float) (2.0 * p - 1.0); break;
        case Shape::square:        lfoValue = p < 0.5 ? 1.0f : -1.0f; break;
        case Shape::sampleAndHold: lfoValue = heldRandom; break;
    }

    phase += s.lfoRateHz * numSamples / sampleRate;
    if (phase >= 1.0)
    {
        phase -= std::floor (phase);
        heldRandom = random.nextFloat() * 2.0f - 1.0f;   // new step once per cycle
    }

    // Envelope follower on |A|.
    const float att = coefficient (s.envAttackMs, sampleRate);
    const float rel = coefficient (s.envReleaseMs, sampleRate);
    for (int i = 0; i < numSamples; ++i)
    {
        const float x = input != nullptr ? std::abs (input[i]) : 0.0f;
        envelope += (x - envelope) * (x > envelope ? att : rel);
    }
    const float db = juce::Decibels::gainToDecibels (envelope, floorDb);
    envLevel = juce::jlimit (0.0f, 1.0f, (db - floorDb) / -floorDb);

    const float lfoOffset = s.lfoDepth * lfoValue;
    const float envOffset = s.envDepth * envLevel;

    Offsets o {};
    for (int src = 0; src < numSources; ++src)
    {
        const bool targeted = s.target == Target::all || (int) s.target == src;
        o[(size_t) src] = envOffset + (targeted ? lfoOffset : 0.0f);
    }
    return o;
}
} // namespace cortex::dsp
