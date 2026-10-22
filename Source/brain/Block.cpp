// Cortex: portions ported from Samplebrain (brain/src/block.cpp, fft.cpp, mfcc.cpp, window.cpp),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.
//
// Differences from Samplebrain:
// - FFT bins use |X|^2 and are stored as log10 power (Samplebrain squares only the
//   real part and compares raw power), so they are level-independent.
// - MFCCs take the log of the mel energies before the DCT (Aquila's MFCC skips the log).
// - Blocks are zero-padded to a power-of-two FFT.
// - No peak-normalised twin analysis; matching uses split-cepstrum features instead
//   (timbre from the cepstral envelope, pitch from the cepstral peak and chroma).

#include "Block.h"

namespace cortex::brain
{
namespace
{
    constexpr int minRamp = 64;
    constexpr float logFloor = 1.0e-12f;

    // Pitch search range, and the chroma range.
    constexpr double minF0 = 50.0, maxF0 = 1000.0;
    constexpr double minChromaHz = 55.0, maxChromaHz = 5000.0;

    // Cepstral peak heights mapped to voicing 0..1. Clearly pitched blocks
    // peak well above 0.1; noise stays around 0.02.
    constexpr float unvoicedPeak = 0.03f, voicedPeak = 0.12f;

    double hzToMel (double hz) { return 2595.0 * std::log10 (1.0 + hz / 700.0); }
    double melToHz (double mel) { return 700.0 * (std::pow (10.0, mel / 2595.0) - 1.0); }
}

std::array<float, numTimbre> BlockFeatures::timbreVector() const noexcept
{
    std::array<float, numTimbre> t {};
    std::copy (cepstrum.begin() + 1, cepstrum.begin() + 1 + numTimbre, t.begin());
    return t;
}

std::array<float, numPitch> BlockFeatures::pitchVector() const noexcept
{
    std::array<float, numPitch> p {};
    std::copy (chroma.begin(), chroma.end(), p.begin());
    p[numChroma] = f0 > 0.0f ? voicing * std::log2 (f0) : 0.0f;
    p[numChroma + 1] = voicing;
    return p;
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

    // Chroma: each bin's nearest pitch class (A = 0).
    pitchClassOfBin.assign ((size_t) (fftSize / 2 + 1), (std::int8_t) -1);
    for (int k = 1; k <= fftSize / 2; ++k)
    {
        const double hz = k * sampleRate / fftSize;
        if (hz >= minChromaHz && hz <= maxChromaHz)
        {
            const int semis = (int) std::lround (12.0 * std::log2 (hz / 440.0));
            pitchClassOfBin[(size_t) k] = (std::int8_t) (((semis % 12) + 12) % 12);
        }
    }

    // Cepstral pitch range, limited by what the FFT can see.
    minPitchQuefrency = juce::jmax (2, (int) std::floor (sampleRate / maxF0));
    maxPitchQuefrency = juce::jmin (fftSize / 2 - 2, (int) std::ceil (sampleRate / minF0));
}

void BlockAnalyser::analyse (const float* raw, float* windowedOut, BlockFeatures& out)
{
    for (int i = 0; i < blockSize; ++i)
        windowed[(size_t) i] = raw[i] * window[(size_t) i];
    if (windowedOut != nullptr)
        std::copy (windowed.begin(), windowed.end(), windowedOut);

    const int half = fftSize / 2;

    for (int i = 0; i < fftSize; ++i)
        timeBuf[(size_t) i] = { i < blockSize ? windowed[(size_t) i] : 0.0f, 0.0f };
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

    // Chroma from spectral peaks. At low frequencies FFT bins are wider than a
    // semitone, so each peak's true frequency is found by parabolic interpolation
    // on the log magnitude and the peak's energy goes to that pitch class.
    out.chroma.fill (0.0f);
    float chromaTotal = 0.0f;
    for (int k = 2; k < half; ++k)
    {
        const float m = magnitude[(size_t) k];
        if (pitchClassOfBin[(size_t) k] < 0 || m <= magnitude[(size_t) (k - 1)] || m < magnitude[(size_t) (k + 1)])
            continue;

        const float l = std::log (magnitude[(size_t) (k - 1)] + logFloor);
        const float c = std::log (m + logFloor);
        const float r = std::log (magnitude[(size_t) (k + 1)] + logFloor);
        const float denom = l - 2.0f * c + r;
        const float shift = std::abs (denom) > 1.0e-9f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (l - r) / denom) : 0.0f;

        const double hz = ((double) k + shift) * sampleRate / fftSize;
        const int semis = (int) std::lround (12.0 * std::log2 (hz / 440.0));
        const float p = magnitude[(size_t) (k - 1)] * magnitude[(size_t) (k - 1)] + m * m
                      + magnitude[(size_t) (k + 1)] * magnitude[(size_t) (k + 1)];
        out.chroma[(size_t) (((semis % 12) + 12) % 12)] += p;
        chromaTotal += p;
    }
    if (chromaTotal > logFloor)
        for (auto& c : out.chroma)
            c /= chromaTotal;
    else
        out.chroma.fill (0.0f);

    // Real cepstrum: forward FFT of the (real, even) log magnitude, divided by N.
    for (int k = 0; k < fftSize; ++k)
    {
        const int kk = k <= half ? k : fftSize - k;
        timeBuf[(size_t) k] = { std::log (magnitude[(size_t) kk] + 1.0e-6f), 0.0f };
    }
    fft->perform (timeBuf.data(), specBuf.data(), false);
    const float invN = 1.0f / (float) fftSize;
    for (int q = 0; q < numCepstrum; ++q)
        out.cepstrum[(size_t) q] = q < fftSize ? specBuf[(size_t) q].real() * invN : 0.0f;

    // Pitch from the cepstral peak, refined by parabolic interpolation.
    out.f0 = 0.0f;
    out.voicing = 0.0f;
    if (bestPower > logFloor && maxPitchQuefrency > minPitchQuefrency)
    {
        int peakQ = minPitchQuefrency;
        float peak = -1.0e9f;
        for (int q = minPitchQuefrency; q <= maxPitchQuefrency; ++q)
        {
            const float c = specBuf[(size_t) q].real() * invN;
            if (c > peak)
            {
                peak = c;
                peakQ = q;
            }
        }

        const float l = specBuf[(size_t) (peakQ - 1)].real() * invN;
        const float r = specBuf[(size_t) (peakQ + 1)].real() * invN;
        const float denom = l - 2.0f * peak + r;
        const float shift = std::abs (denom) > 1.0e-12f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (l - r) / denom) : 0.0f;

        out.f0 = (float) (sampleRate / ((float) peakQ + shift));
        out.voicing = juce::jlimit (0.0f, 1.0f, (peak - unvoicedPeak) / (voicedPeak - unvoicedPeak));
    }
}
} // namespace cortex::brain
