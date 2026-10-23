#pragma once

#include "Cepstrum.h"
#include "PhaseBlend.h"
#include "STFT.h"

#include <array>
#include <vector>

namespace cortex::dsp
{
template <typename T>
using PerSource = std::array<T, numSources>;

struct MorphParams
{
    // How much each source contributes to the output's spectral envelope
    // (formants, timbre) and to its detail (pitch, harmonics, texture).
    // Each set sums to 1; the defaults are pure input.
    PerSource<float> envWeights { 1.0f, 0.0f, 0.0f };
    PerSource<float> detailWeights { 1.0f, 0.0f, 0.0f };

    PerSource<float> cutoff { 25.0f, 25.0f, 25.0f };   // lifter cutoff quefrency (bins)
    PhaseSource phase = PhaseSource::followDetail;

    // Cepstral freeze (BRIEF.md §4 idea 6): hold a part of a source at the
    // cepstrum it had when the freeze was switched on, while the rest moves.
    PerSource<bool> freezeEnv {}, freezeDetail {};

    // Formant shift (BRIEF.md §4 idea 5), semitones, without changing pitch.
    PerSource<float> formant {};
};

// What the UI shows of the last processed frame: the low-quefrency cepstra of
// each source and of the morphed result, plus the settings they were made with.
struct MorphSnapshot
{
    static constexpr int numCoeffs = 256;
    PerSource<std::array<float, numCoeffs>> ceps {};
    std::array<float, numCoeffs> cepsOut {};
    PerSource<float> cutoff {}, envWeights {}, detailWeights {};
    PerSource<bool> used {};
    int fftSize = 2048;
    double sampleRate = 48000.0;
    bool valid = false;
};

// The cepstral morph core (BRIEF.md §3) for three sources, one channel.
// Works on windowed time-domain frames handed over by STFT. Each source is
// liftered into envelope and detail at its own cutoff; the output envelope is
// the envelope-weighted sum of the sources' envelopes, the output detail the
// detail-weighted sum of their details. A source with no weight (and not
// supplying the phase) is skipped, so it costs nothing.
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

    void processFrame (const STFT::Frames& frames, float* frameOut);

    // Filled in by every processFrame() (a few KB copy, no allocation).
    const MorphSnapshot& getSnapshot() const noexcept { return snapshot; }

private:
    void advanceSmoothing() noexcept;
    bool isUsed (int source) const noexcept;

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
    PerSource<Freeze> freezeEnv, freezeDetail;

    FFTBank fft;
    std::vector<Complex> timeBuf, scratch, output;
    PerSource<std::vector<Complex>> spectra, cepstra;
    std::vector<Complex> teCeps, teSpec;
    std::vector<float> teTarget, teEnvelope;
    MorphParams target, current;
    double sampleRate = 48000.0;
    float smoothCoeff = 0.0f;
    MorphSnapshot snapshot;
};
} // namespace cortex::dsp
