#include "Cepstrum.h"

#include <vector>

namespace cortex::dsp
{
void FFTBank::prepare()
{
    for (int i = 0; i < numOrders; ++i)
    {
        const int o = minFftOrder + i;
        const int n = 1 << o;
        ffts[(size_t) i] = std::make_unique<juce::dsp::FFT> (o);

        // Calibrate the inverse scale so we don't depend on which FFT engine
        // JUCE picked (some normalise the inverse, some don't).
        std::vector<Complex> x ((size_t) n), X ((size_t) n), y ((size_t) n);
        x[1] = 1.0f;
        ffts[(size_t) i]->perform (x.data(), X.data(), false);
        ffts[(size_t) i]->perform (X.data(), y.data(), true);
        inverseScale[(size_t) i] = 1.0f / y[1].real();
    }
}

void FFTBank::setOrder (int newOrder)
{
    order = juce::jlimit (minFftOrder, maxFftOrder, newOrder);
}

void FFTBank::forward (const Complex* in, Complex* out) const
{
    ffts[(size_t) (order - minFftOrder)]->perform (in, out, false);
}

void FFTBank::inverse (const Complex* in, Complex* out) const
{
    const auto idx = (size_t) (order - minFftOrder);
    ffts[idx]->perform (in, out, true);

    const float s = inverseScale[idx];
    if (s != 1.0f)
        for (int i = 0, n = size(); i < n; ++i)
            out[i] *= s;
}

namespace Cepstrum
{
void realCepstrum (const FFTBank& fft, const Complex* spectrum, Complex* scratch, Complex* cepstrumOut)
{
    const int n = fft.size();
    for (int k = 0; k < n; ++k)
        scratch[k] = { std::log (std::abs (spectrum[k]) + epsilon), 0.0f };

    fft.inverse (scratch, cepstrumOut);
}

float envelopeWeight (int quefrency, float cutoff) noexcept
{
    // Taper width scales down for very low cutoffs so the envelope keeps c[0].
    const float halfWidth = juce::jmin (4.0f, cutoff * 0.5f);
    const float lo = cutoff - halfWidth;
    const float hi = cutoff + halfWidth;
    const auto q = (float) quefrency;

    if (q <= lo) return 1.0f;
    if (q >= hi) return 0.0f;
    return 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (q - lo) / (hi - lo));
}
} // namespace Cepstrum
} // namespace cortex::dsp
