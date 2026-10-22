#include "CepstralMorph.h"
#include "STFT.h"

namespace cortex::dsp
{
namespace
{
    constexpr double smoothingSeconds = 0.02;

    // Formant shift gain limits: +24 dB / -60 dB, in nepers.
    constexpr float maxFormantBoostNepers = 2.763f;
    constexpr float maxFormantCutNepers = -6.908f;

    float lerp (float a, float b, float t) noexcept { return a + t * (b - a); }
}

void CepstralMorph::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    fft.prepare();
    for (auto* v : { &timeBuf, &specA, &specB, &cepsA, &cepsB, &scratch, &teCeps, &teSpec })
        v->assign ((size_t) maxFftSize, {});
    teTarget.assign ((size_t) (maxFftSize / 2 + 1), 0.0f);
    teEnvelope.assign ((size_t) (maxFftSize / 2 + 1), 0.0f);
    for (auto* f : { &freezeEnvA, &freezeDetailA, &freezeEnvB, &freezeDetailB })
        f->held.assign ((size_t) maxFftSize, 0.0f);
    setOrder (11);
    snapToTargets();
}

void CepstralMorph::setOrder (int order)
{
    fft.setOrder (order);
    // Held cepstra belong to the old size; recapture on the next frame.
    for (auto* f : { &freezeEnvA, &freezeDetailA, &freezeEnvB, &freezeDetailB })
        f->captured = false;
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
    glide (current.formantA, target.formantA);
    glide (current.formantB, target.formantB);
    current.phaseMode = target.phaseMode;
    current.freezeEnvA = target.freezeEnvA;
    current.freezeDetailA = target.freezeDetailA;
    current.freezeEnvB = target.freezeEnvB;
    current.freezeDetailB = target.freezeDetailB;
}

void CepstralMorph::shiftFormants (std::vector<Complex>& spectrum, float cutoff, float factor) noexcept
{
    // Iterations of the true-envelope estimate; it converges within a few dB
    // of the harmonic peaks by about here.
    constexpr int trueEnvelopeIterations = 8;

    const int n = fft.size();
    const int half = n / 2;

    for (int k = 0; k <= half; ++k)
        teTarget[(size_t) k] = std::log (std::abs (spectrum[(size_t) k]) + Cepstrum::epsilon);

    // True envelope: lifter, then raise the target to wherever the smooth
    // curve is above it, and repeat. The curve climbs onto the peaks.
    for (int it = 0; it < trueEnvelopeIterations; ++it)
    {
        for (int k = 0; k < n; ++k)
            teSpec[(size_t) k] = { teTarget[(size_t) (k <= half ? k : n - k)], 0.0f };
        fft.inverse (teSpec.data(), teCeps.data());
        for (int k = 0; k < n; ++k)
            teCeps[(size_t) k] *= Cepstrum::envelopeWeight (k <= half ? k : n - k, cutoff);
        fft.forward (teCeps.data(), teSpec.data());

        for (int k = 0; k <= half; ++k)
        {
            teEnvelope[(size_t) k] = teSpec[(size_t) k].real();
            teTarget[(size_t) k] = juce::jmax (teTarget[(size_t) k], teEnvelope[(size_t) k]);
        }
    }

    // Gain = E(f / factor) - E(f), in log terms. Past Nyquist, hold the edge.
    for (int k = 0; k <= half; ++k)
    {
        const float src = (float) k / factor;
        const int i0 = (int) src;
        float warped;
        if (i0 >= half)
            warped = teEnvelope[(size_t) half];
        else
        {
            const float frac = src - (float) i0;
            warped = teEnvelope[(size_t) i0] + frac * (teEnvelope[(size_t) (i0 + 1)] - teEnvelope[(size_t) i0]);
        }
        // Bounded: where the envelope falls to the noise floor (above the last
        // harmonic, say) the raw difference would ask for absurd boosts.
        const float gain = std::exp (juce::jlimit (maxFormantCutNepers, maxFormantBoostNepers, warped - teEnvelope[(size_t) k]));
        spectrum[(size_t) k] *= gain;
        if (k > 0 && k < half)
            spectrum[(size_t) (n - k)] *= gain;
    }
}

void CepstralMorph::processFrame (const float* frameA, const float* frameB, float* frameOut)
{
    advanceSmoothing();
    snapshot.cutoffA = current.cutoffA;
    snapshot.cutoffB = current.cutoffB;
    snapshot.x = current.x;
    snapshot.y = current.y;
    snapshot.fftSize = fft.size();
    snapshot.sampleRate = sampleRate;
    snapshot.valid = true;

    const int n = fft.size();
    const int half = n / 2;
    const float x = current.x, y = current.y;

    // 1. Spectra of both inputs.
    for (int i = 0; i < n; ++i) timeBuf[(size_t) i] = { frameA[i], 0.0f };
    fft.forward (timeBuf.data(), specA.data());
    for (int i = 0; i < n; ++i) timeBuf[(size_t) i] = { frameB[i], 0.0f };
    fft.forward (timeBuf.data(), specB.data());

    // Formant shift, applied to the input spectra before anything else.
    if (std::abs (current.formantA) > 0.01f)
        shiftFormants (specA, current.cutoffA, std::exp2 (current.formantA / 12.0f));
    if (std::abs (current.formantB) > 0.01f)
        shiftFormants (specB, current.cutoffB, std::exp2 (current.formantB / 12.0f));

    // 2-3. Real cepstra.
    Cepstrum::realCepstrum (fft, specA.data(), scratch.data(), cepsA.data());
    Cepstrum::realCepstrum (fft, specB.data(), scratch.data(), cepsB.data());

    freezeEnvA.update (current.freezeEnvA, cepsA, n);
    freezeDetailA.update (current.freezeDetailA, cepsA, n);
    freezeEnvB.update (current.freezeEnvB, cepsB, n);
    freezeDetailB.update (current.freezeDetailB, cepsB, n);

    // 4-5. Lifter each input into envelope/detail and crossfade the parts separately.
    for (int k = 0; k < n; ++k)
    {
        const int q = k <= half ? k : n - k;
        const float wA = Cepstrum::envelopeWeight (q, current.cutoffA);
        const float wB = Cepstrum::envelopeWeight (q, current.cutoffB);
        const float ca = cepsA[(size_t) k].real();
        const float cb = cepsB[(size_t) k].real();

        const float envA = freezeEnvA.value (current.freezeEnvA, ca, k);
        const float detA = freezeDetailA.value (current.freezeDetailA, ca, k);
        const float envB = freezeEnvB.value (current.freezeEnvB, cb, k);
        const float detB = freezeDetailB.value (current.freezeDetailB, cb, k);

        const float env = lerp (wA * envA, wB * envB, x);
        const float det = lerp ((1.0f - wA) * detA, (1.0f - wB) * detB, y);
        scratch[(size_t) k] = { env + det, 0.0f };

        if (k < MorphSnapshot::numCoeffs)
        {
            snapshot.cepsA[(size_t) k] = ca;
            snapshot.cepsB[(size_t) k] = cb;
            snapshot.cepsOut[(size_t) k] = env + det;
        }
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
