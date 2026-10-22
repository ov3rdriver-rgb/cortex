#include "CepstralMorph.h"
#include "STFT.h"

namespace cortex::dsp
{
namespace
{
    constexpr double smoothingSeconds = 0.02;

    float lerp (float a, float b, float t) noexcept { return a + t * (b - a); }
}

void CepstralMorph::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    fft.prepare();
    for (auto* v : { &timeBuf, &specA, &specB, &cepsA, &cepsB, &scratch })
        v->assign ((size_t) maxFftSize, {});
    setOrder (11);
    snapToTargets();
}

void CepstralMorph::setOrder (int order)
{
    fft.setOrder (order);
    const double hopSeconds = (double) (fft.size() / STFT::overlap) / sampleRate;
    smoothCoeff = (float) std::exp (-hopSeconds / smoothingSeconds);
    snapToTargets();
}

void CepstralMorph::advanceSmoothing() noexcept
{
    const float a = smoothCoeff;
    auto glide = [a] (float& c, float t) { c = t + a * (c - t); };

    glide (current.x, target.x);
    glide (current.y, target.y);
    glide (current.cutoffA, target.cutoffA);
    glide (current.cutoffB, target.cutoffB);
    glide (current.cornerGainTLdB, target.cornerGainTLdB);
    glide (current.cornerGainBRdB, target.cornerGainBRdB);
    current.phaseMode = target.phaseMode;
}

void CepstralMorph::processFrame (const float* frameA, const float* frameB, float* frameOut)
{
    advanceSmoothing();

    const int n = fft.size();
    const int half = n / 2;
    const float x = current.x, y = current.y;

    // 1. Spectra of both inputs.
    for (int i = 0; i < n; ++i) timeBuf[(size_t) i] = { frameA[i], 0.0f };
    fft.forward (timeBuf.data(), specA.data());
    for (int i = 0; i < n; ++i) timeBuf[(size_t) i] = { frameB[i], 0.0f };
    fft.forward (timeBuf.data(), specB.data());

    // 2-3. Real cepstra.
    Cepstrum::realCepstrum (fft, specA.data(), scratch.data(), cepsA.data());
    Cepstrum::realCepstrum (fft, specB.data(), scratch.data(), cepsB.data());

    // 4-5. Lifter each input into envelope/detail and crossfade the parts separately.
    for (int k = 0; k < n; ++k)
    {
        const int q = k <= half ? k : n - k;
        const float wA = Cepstrum::envelopeWeight (q, current.cutoffA);
        const float wB = Cepstrum::envelopeWeight (q, current.cutoffB);
        const float ca = cepsA[(size_t) k].real();
        const float cb = cepsB[(size_t) k].real();

        const float env = lerp (wA * ca, wB * cb, x);
        const float det = lerp ((1.0f - wA) * ca, (1.0f - wB) * cb, y);
        scratch[(size_t) k] = { env + det, 0.0f };
    }

    // 6. Back to log magnitude.
    fft.forward (scratch.data(), cepsA.data());   // cepsA now holds L'

    // Off-diagonal corner trims, bilinear over the pad so pure corners are untouched.
    const float cornerDb = current.cornerGainTLdB * (1.0f - x) * y
                         + current.cornerGainBRdB * x * (1.0f - y);
    const float gain = juce::Decibels::decibelsToGain (cornerDb, -200.0f);

    // 7-8. Phase, polar -> cartesian, Hermitian-symmetric spectrum, IFFT.
    for (int k = 0; k <= half; ++k)
    {
        const float mag = juce::jmax (0.0f, std::exp (cepsA[(size_t) k].real()) - Cepstrum::epsilon) * gain;
        const float ph = blendPhase (std::arg (specA[(size_t) k]), std::arg (specB[(size_t) k]), current.phaseMode, y);
        scratch[(size_t) k] = std::polar (mag, ph);
    }
    for (int k = 1; k < half; ++k)
        scratch[(size_t) (n - k)] = std::conj (scratch[(size_t) k]);

    fft.inverse (scratch.data(), timeBuf.data());
    for (int i = 0; i < n; ++i)
        frameOut[i] = timeBuf[(size_t) i].real();
}
} // namespace cortex::dsp
