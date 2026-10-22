// Cortex: portions ported from Samplebrain (brain/src/block.cpp, fft.cpp, mfcc.cpp, window.cpp),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.
//
// Differences from Samplebrain:
// - FFT bins use |X|^2 and are stored as log10 power (Samplebrain squares only the
//   real part and compares raw power), so distances are level-independent.
// - MFCCs take the log of the mel energies before the DCT (Aquila's MFCC skips the log).
// - Blocks are zero-padded to a power-of-two FFT.

#include "Block.h"
#include "SearchParams.h"

namespace cortex::brain
{
namespace
{
    constexpr int minRamp = 64;
    constexpr float logFloor = 1.0e-12f;

    double hzToMel (double hz) { return 2595.0 * std::log10 (1.0 + hz / 700.0); }
    double melToHz (double mel) { return 700.0 * (std::pow (10.0, mel / 2595.0) - 1.0); }
}

std::vector<float> makeBlockWindow (int blockSize, int hop)
{
    const int ramp = juce::jlimit (1, blockSize / 2, juce::jmax (minRamp, blockSize - hop));
    std::vector<float> w ((size_t) blockSize, 1.0f);
    for (int i = 0; i < ramp; ++i)
    {
        w[(size_t) i] = (float) i / (float) ramp;
        w[(size_t) (blockSize - 1 - i)] = (float) (i + 1) / (float) ramp;
    }
    return w;
}

void BlockAnalyser::prepare (int newBlockSize, int hop, double newSampleRate)
{
    blockSize = juce::jlimit (minBlockSize, maxBlockSize, newBlockSize);
    sampleRate = newSampleRate;

    const int order = juce::jmax (8, (int) std::ceil (std::log2 ((double) blockSize)));
    fftSize = 1 << order;
    fft = std::make_unique<juce::dsp::FFT> (order);

    window = makeBlockWindow (blockSize, hop);
    windowed.assign ((size_t) blockSize, 0.0f);
    normalisedPcm.assign ((size_t) blockSize, 0.0f);
    magnitude.assign ((size_t) (fftSize / 2 + 1), 0.0f);
    melEnergy.assign ((size_t) numMelBands, 0.0f);
    timeBuf.assign ((size_t) fftSize, {});
    specBuf.assign ((size_t) fftSize, {});

    // Triangular mel filterbank over 0 .. Nyquist.
    melFilters.assign ((size_t) numMelBands, {});
    const double melMax = hzToMel (sampleRate * 0.5);
    auto binOf = [this] (double mel) { return melToHz (mel) * fftSize / sampleRate; };

    for (int m = 0; m < numMelBands; ++m)
    {
        const double lo  = binOf (melMax * m / (numMelBands + 1));
        const double mid = binOf (melMax * (m + 1) / (numMelBands + 1));
        const double hi  = binOf (melMax * (m + 2) / (numMelBands + 1));

        auto& f = melFilters[(size_t) m];
        f.firstBin = (int) std::floor (lo);
        const int lastBin = juce::jmin (fftSize / 2, (int) std::ceil (hi));
        for (int k = f.firstBin; k <= lastBin; ++k)
        {
            const double w = k < mid ? (k - lo) / juce::jmax (1.0e-9, mid - lo)
                                     : (hi - k) / juce::jmax (1.0e-9, hi - mid);
            f.weights.push_back ((float) juce::jmax (0.0, w));
        }
    }

    // Orthonormal DCT-II, first numMfcc rows.
    dct.assign ((size_t) (numMfcc * numMelBands), 0.0f);
    for (int n = 0; n < numMfcc; ++n)
        for (int k = 0; k < numMelBands; ++k)
            dct[(size_t) (n * numMelBands + k)] = (float) (std::sqrt ((n == 0 ? 1.0 : 2.0) / numMelBands)
                                                    * std::cos (juce::MathConstants<double>::pi * n * (k + 0.5) / numMelBands));
}

void BlockAnalyser::analyse (const float* raw, float* windowedOut, BlockFeatures& features, BlockFeatures& normalised)
{
    for (int i = 0; i < blockSize; ++i)
        windowed[(size_t) i] = raw[i] * window[(size_t) i];
    analyseWindowed (windowed.data(), features);

    if (windowedOut != nullptr)
        std::copy (windowed.begin(), windowed.end(), windowedOut);

    // Samplebrain's normalise(): remove the min/max midpoint, scale peak to 1.
    auto [mn, mx] = std::minmax_element (raw, raw + blockSize);
    const float mid = *mn + (*mx - *mn) * 0.5f;
    const float peak = juce::jmax (std::abs (*mn - mid), std::abs (*mx - mid));
    const float scale = peak > 1.0e-9f ? 1.0f / peak : 0.0f;
    for (int i = 0; i < blockSize; ++i)
        normalisedPcm[(size_t) i] = (raw[i] - mid) * scale * window[(size_t) i];
    analyseWindowed (normalisedPcm.data(), normalised);
}

void BlockAnalyser::analyseWindowed (const float* pcm, BlockFeatures& out)
{
    const int half = fftSize / 2;

    for (int i = 0; i < fftSize; ++i)
        timeBuf[(size_t) i] = { i < blockSize ? pcm[i] : 0.0f, 0.0f };
    fft->perform (timeBuf.data(), specBuf.data(), false);

    float bestPower = 0.0f;
    int bestBin = 0;
    for (int k = 0; k <= half; ++k)
    {
        magnitude[(size_t) k] = std::abs (specBuf[(size_t) k]);
        const float p = magnitude[(size_t) k] * magnitude[(size_t) k];
        if (k < half && p > bestPower)
        {
            bestPower = p;
            bestBin = k;
        }
    }
    out.dominantFreq = juce::jmax (0.01f, (float) (bestBin * sampleRate / fftSize));

    // Linear power bins over 0 .. Nyquist (Samplebrain FFT::calculate_bins, inclusive ranges).
    for (int n = 0; n < numFftBins; ++n)
    {
        const int from = n * half / numFftBins;
        const int to = juce::jmin (half, (n + 1) * half / numFftBins);
        float power = 0.0f;
        for (int k = from; k <= to; ++k)
            power += magnitude[(size_t) k] * magnitude[(size_t) k];
        out.fft[(size_t) n] = std::log10 (power + logFloor);
    }

    // MFCC.
    for (int m = 0; m < numMelBands; ++m)
    {
        const auto& f = melFilters[(size_t) m];
        float e = 0.0f;
        for (size_t j = 0; j < f.weights.size(); ++j)
            e += magnitude[(size_t) f.firstBin + j] * f.weights[j];
        melEnergy[(size_t) m] = std::log (e + logFloor);
    }
    for (int n = 0; n < numMfcc; ++n)
    {
        float c = 0.0f;
        for (int k = 0; k < numMelBands; ++k)
            c += dct[(size_t) (n * numMelBands + k)] * melEnergy[(size_t) k];
        out.mfcc[(size_t) n] = c;
    }

    // Real cepstrum: forward FFT of the (real, even) log magnitude, divided by N.
    for (int k = 0; k < fftSize; ++k)
    {
        const int kk = k <= half ? k : fftSize - k;
        timeBuf[(size_t) k] = { std::log (magnitude[(size_t) kk] + 1.0e-6f), 0.0f };
    }
    fft->perform (timeBuf.data(), specBuf.data(), false);
    for (int q = 0; q < numCepstrum; ++q)
        out.cepstrum[(size_t) q] = q < fftSize ? specBuf[(size_t) q].real() / (float) fftSize : 0.0f;
}

double featureDistance (const BlockFeatures& a, const BlockFeatures& b, const SearchParams& p) noexcept
{
    const float totalWeight = p.fftWeight + p.mfccWeight;
    const float wFft = totalWeight > 0.0f ? p.fftWeight / totalWeight : 0.5f;
    const float wMfcc = 1.0f - wFft;

    double fftAcc = 0.0;
    if (wFft > 0.0f)
    {
        for (int i = 0; i < numFftBins; ++i)
        {
            const double d = a.fft[(size_t) i] - b.fft[(size_t) i];
            fftAcc += d * d;
        }
        fftAcc /= numFftBins;
    }

    double mfccAcc = 0.0;
    if (wMfcc > 0.0f)
    {
        for (int i = 0; i < numMfcc; ++i)
        {
            const double d = a.mfcc[(size_t) i] - b.mfcc[(size_t) i];
            mfccAcc += d * d;
        }
        mfccAcc /= numMfcc;
    }

    return wFft * fftAcc + wMfcc * mfccAcc;
}
} // namespace cortex::brain
