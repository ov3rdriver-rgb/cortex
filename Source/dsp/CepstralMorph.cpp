#include "CepstralMorph.h"

namespace cortex::dsp
{
namespace
{
    constexpr double smoothingSeconds = 0.02;

    // Formant shift gain limits: +24 dB / -60 dB, in nepers.
    constexpr float maxFormantBoostNepers = 2.763f;
    constexpr float maxFormantCutNepers = -6.908f;

    // Below this a weight counts as zero (the source can be skipped).
    constexpr float unusedWeight = 1.0e-4f;
}

void CepstralMorph::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    fft.prepare();
    for (auto* v : { &timeBuf, &scratch, &output, &teCeps, &teSpec })
        v->assign ((size_t) maxFftSize, {});
    for (int s = 0; s < numSources; ++s)
    {
        spectra[(size_t) s].assign ((size_t) maxFftSize, {});
        cepstra[(size_t) s].assign ((size_t) maxFftSize, {});
        freezeEnv[(size_t) s].held.assign ((size_t) maxFftSize, 0.0f);
        freezeDetail[(size_t) s].held.assign ((size_t) maxFftSize, 0.0f);
    }
    teTarget.assign ((size_t) (maxFftSize / 2 + 1), 0.0f);
    teEnvelope.assign ((size_t) (maxFftSize / 2 + 1), 0.0f);
    setOrder (11);
    snapToTargets();
}

void CepstralMorph::setOrder (int order)
{
    fft.setOrder (order);
    // Held cepstra belong to the old size; recapture on the next frame.
    for (int s = 0; s < numSources; ++s)
        freezeEnv[(size_t) s].captured = freezeDetail[(size_t) s].captured = false;
    const double hopSeconds = (double) (fft.size() / STFT::overlap) / sampleRate;
    smoothCoeff = (float) std::exp (-hopSeconds / smoothingSeconds);
    snapToTargets();
}

void CepstralMorph::advanceSmoothing() noexcept
{
    const float a = smoothCoeff;
    auto glide = [a] (float& c, float t) { c = t + a * (c - t); };

    for (int s = 0; s < numSources; ++s)
    {
        const auto i = (size_t) s;
        glide (current.envWeights[i], target.envWeights[i]);
        glide (current.detailWeights[i], target.detailWeights[i]);
        glide (current.cutoff[i], target.cutoff[i]);
        glide (current.formant[i], target.formant[i]);
        // Snap weights that have glided to (almost) nothing, so a source that
        // was faded out stops costing FFTs.
        if (target.envWeights[i] == 0.0f && current.envWeights[i] < unusedWeight)
            current.envWeights[i] = 0.0f;
        if (target.detailWeights[i] == 0.0f && current.detailWeights[i] < unusedWeight)
            current.detailWeights[i] = 0.0f;
    }
    current.phase = target.phase;
    current.freezeEnv = target.freezeEnv;
    current.freezeDetail = target.freezeDetail;
}

bool CepstralMorph::isUsed (int s) const noexcept
{
    const auto i = (size_t) s;
    return current.envWeights[i] > 0.0f || current.detailWeights[i] > 0.0f
        || (current.phase != PhaseSource::followDetail && (int) current.phase - 1 == s);
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

void CepstralMorph::processFrame (const STFT::Frames& frames, float* frameOut)
{
    advanceSmoothing();

    const int n = fft.size();
    const int half = n / 2;

    PerSource<bool> used {};
    for (int s = 0; s < numSources; ++s)
        used[(size_t) s] = isUsed (s);

    // 1-3. Spectrum (formant-shifted if asked) and real cepstrum of each used source.
    for (int s = 0; s < numSources; ++s)
    {
        if (! used[(size_t) s])
            continue;
        auto& spec = spectra[(size_t) s];
        for (int i = 0; i < n; ++i)
            timeBuf[(size_t) i] = { frames[(size_t) s][i], 0.0f };
        fft.forward (timeBuf.data(), spec.data());

        if (std::abs (current.formant[(size_t) s]) > 0.01f)
            shiftFormants (spec, current.cutoff[(size_t) s], std::exp2 (current.formant[(size_t) s] / 12.0f));

        Cepstrum::realCepstrum (fft, spec.data(), scratch.data(), cepstra[(size_t) s].data());
        freezeEnv[(size_t) s].update (current.freezeEnv[(size_t) s], cepstra[(size_t) s], n);
        freezeDetail[(size_t) s].update (current.freezeDetail[(size_t) s], cepstra[(size_t) s], n);
    }

    // 4-5. Lifter each source into envelope/detail at its own cutoff and sum
    // the parts with their own weights.
    for (int k = 0; k < n; ++k)
    {
        const int q = k <= half ? k : n - k;
        float env = 0.0f, det = 0.0f;
        for (int s = 0; s < numSources; ++s)
        {
            const auto i = (size_t) s;
            if (! used[i])
                continue;
            const float c = cepstra[i][(size_t) k].real();
            const float w = Cepstrum::envelopeWeight (q, current.cutoff[i]);
            env += current.envWeights[i] * w * freezeEnv[i].value (current.freezeEnv[i], c, k);
            det += current.detailWeights[i] * (1.0f - w) * freezeDetail[i].value (current.freezeDetail[i], c, k);
            if (k < MorphSnapshot::numCoeffs)
                snapshot.ceps[i][(size_t) k] = c;
        }
        scratch[(size_t) k] = { env + det, 0.0f };
        if (k < MorphSnapshot::numCoeffs)
            snapshot.cepsOut[(size_t) k] = env + det;
    }

    // 6. Back to log magnitude.
    fft.forward (scratch.data(), output.data());

    // 7-8. Phase, polar -> cartesian, Hermitian-symmetric spectrum, IFFT.
    PerSource<float> phases {};
    for (int k = 0; k <= half; ++k)
    {
        for (int s = 0; s < numSources; ++s)
            phases[(size_t) s] = used[(size_t) s] ? std::arg (spectra[(size_t) s][(size_t) k]) : 0.0f;
        const float mag = juce::jmax (0.0f, std::exp (output[(size_t) k].real()) - Cepstrum::epsilon);
        scratch[(size_t) k] = std::polar (mag, blendPhase (phases, current.detailWeights, current.phase));
    }
    for (int k = 1; k < half; ++k)
        scratch[(size_t) (n - k)] = std::conj (scratch[(size_t) k]);

    fft.inverse (scratch.data(), timeBuf.data());
    for (int i = 0; i < n; ++i)
        frameOut[i] = timeBuf[(size_t) i].real();

    snapshot.cutoff = current.cutoff;
    snapshot.envWeights = current.envWeights;
    snapshot.detailWeights = current.detailWeights;
    snapshot.used = used;
    snapshot.fftSize = n;
    snapshot.sampleRate = sampleRate;
    snapshot.valid = true;
}
} // namespace cortex::dsp
