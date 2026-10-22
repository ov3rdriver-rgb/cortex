#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cortex::dsp
{
// Quefrency cutoff as a performance control (BRIEF.md §4 idea 7): an LFO and
// an envelope follower on input A, summed into a cutoff offset in bins.
// Block-rate; the morph's per-frame smoothing keeps sweeps click-free.
// Realtime-safe.
class CutoffModulator
{
public:
    enum class Shape { sine = 0, triangle, saw, square, sampleAndHold };
    enum class Target { a = 0, b, both };

    struct Settings
    {
        float lfoRateHz = 1.0f;
        float lfoDepth = 0.0f;       // bins, peak
        Shape shape = Shape::sine;
        Target target = Target::both;
        float envDepth = 0.0f;       // bins at full level (can be negative)
        float envAttackMs = 10.0f;
        float envReleaseMs = 200.0f;
    };

    struct Offsets
    {
        float a = 0.0f, b = 0.0f;
    };

    void prepare (double sampleRate);
    void reset();

    // Advances by one block. `input` is A (mono mix), used by the follower.
    Offsets process (const Settings&, const float* input, int numSamples) noexcept;

    float getLfoValue() const noexcept { return lfoValue; }       // -1..1
    float getEnvelopeLevel() const noexcept { return envLevel; }  // 0..1

private:
    double sampleRate = 48000.0;
    double phase = 0.0;
    float lfoValue = 0.0f, heldRandom = 0.0f;
    float envelope = 0.0f, envLevel = 0.0f;
    juce::Random random { 1234 };
};
} // namespace cortex::dsp
