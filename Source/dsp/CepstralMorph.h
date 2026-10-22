#pragma once

#include "Cepstrum.h"
#include "PhaseBlend.h"

#include <array>
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

    // Cepstral freeze (BRIEF.md §4 idea 6): hold a part of an input at the
    // cepstrum it had when the freeze was switched on, while the rest moves.
    bool freezeEnvA = false, freezeDetailA = false, freezeEnvB = false, freezeDetailB = false;

    // Formant shift (BRIEF.md §4 idea 5): move each input's formants by this
    // many semitones without changing its pitch.
    float formantA = 0.0f, formantB = 0.0f;
};

// What the UI shows of the last processed frame: the low-quefrency cepstra of
// A, B and the morphed result, plus the settings they were made with.
struct MorphSnapshot
{
    static constexpr int numCoeffs = 256;
    std::array<float, numCoeffs> cepsA {}, cepsB {}, cepsOut {};
    float cutoffA = 25.0f, cutoffB = 25.0f, x = 0.0f, y = 0.0f;
    int fftSize = 2048;
    double sampleRate = 48000.0;
    bool valid = false;
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

    // Filled in by every processFrame() (a few KB copy, no allocation).
    const MorphSnapshot& getSnapshot() const noexcept { return snapshot; }

private:
    void advanceSmoothing() noexcept;

    // Shifts the formants of `spectrum` (N bins, in place) by `factor` along
    // frequency, keeping phase and harmonic structure. The envelope is a
    // "true envelope" (iterated cepstral smoothing that rides the harmonic
    // peaks) at lifter `cutoff`, so harmonic sounds' formants actually move.
    void shiftFormants (std::vector<Complex>& spectrum, float cutoff, float factor) noexcept;

    // One frozen part: captured on the first frame after it is switched on.
    struct Freeze
    {
        std::vector<float> held;
        bool captured = false;

        void update (bool on, const std::vector<Complex>& live, int n) noexcept
        {
            if (! on)
                captured = false;
            else if (! captured)
            {
                for (int k = 0; k < n; ++k)
                    held[(size_t) k] = live[(size_t) k].real();
                captured = true;
            }
        }
        float value (bool on, float live, int k) const noexcept { return on ? held[(size_t) k] : live; }
    };
    Freeze freezeEnvA, freezeDetailA, freezeEnvB, freezeDetailB;

    FFTBank fft;
    std::vector<Complex> timeBuf, specA, specB, cepsA, cepsB, scratch;
    std::vector<Complex> teCeps, teSpec;
    std::vector<float> teTarget, teEnvelope;
    MorphParams target, current;
    double sampleRate = 48000.0;
    float smoothCoeff = 0.0f;
    MorphSnapshot snapshot;
};
} // namespace cortex::dsp
