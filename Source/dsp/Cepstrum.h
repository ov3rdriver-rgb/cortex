#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <complex>
#include <memory>

namespace cortex::dsp
{
using Complex = std::complex<float>;

inline constexpr int minFftOrder = 9;   // 512
inline constexpr int maxFftOrder = 12;  // 4096
inline constexpr int maxFftSize  = 1 << maxFftOrder;

// Pre-built FFT plans for every supported size. prepare() allocates; setOrder()
// only switches plans, so it is safe on the audio thread.
class FFTBank
{
public:
    void prepare();
    void setOrder (int order);
    int size() const noexcept { return 1 << order; }

    // Unnormalised forward transform.
    void forward (const Complex* in, Complex* out) const;
    // Normalised inverse: inverse (forward (x)) == x.
    void inverse (const Complex* in, Complex* out) const;

private:
    static constexpr int numOrders = maxFftOrder - minFftOrder + 1;
    std::array<std::unique_ptr<juce::dsp::FFT>, numOrders> ffts;
    std::array<float, numOrders> inverseScale {};
    int order = 11;
};

// Real-cepstrum helpers (BRIEF.md §3 steps 2-4, 6).
namespace Cepstrum
{
    // Floor added before the log so silence never hits log(0).
    inline constexpr float epsilon = 1.0e-6f;

    // spectrum (N bins) -> log magnitude (N bins, real) -> real cepstrum (N, real).
    // `scratch` must hold N values.
    void realCepstrum (const FFTBank&, const Complex* spectrum, Complex* scratch, Complex* cepstrumOut);

    // Envelope weight for quefrency q (0..N/2) at cutoff `cutoff`: 1 below the
    // cutoff, 0 above, with a short raised-cosine taper centred on it. The detail
    // weight is 1 - this, so env + detail always sums back to the input cepstrum.
    float envelopeWeight (int quefrency, float cutoff) noexcept;
}
} // namespace cortex::dsp
