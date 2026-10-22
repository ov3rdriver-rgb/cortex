// Cortex: portions ported from Samplebrain (brain/src/block.h, fft.h, mfcc.h, window.h),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.

#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <complex>
#include <cstdint>
#include <memory>
#include <vector>

namespace cortex::brain
{
inline constexpr int numFftBins   = 100;  // Samplebrain's linear power bins
inline constexpr int numMfcc      = 13;
inline constexpr int numMelBands  = 24;
inline constexpr int numCepstrum  = 64;   // low-quefrency real cepstrum, for Phase 3 matching/transplant

inline constexpr int minBlockSize = 256;
inline constexpr int maxBlockSize = 4096;

// Fixed-size, trivially copyable so it can be cached and copied on the audio thread.
struct BlockFeatures
{
    std::array<float, numFftBins> fft {};       // log10 band power
    std::array<float, numMfcc> mfcc {};
    std::array<float, numCepstrum> cepstrum {};
    float dominantFreq = 0.0f;
};

struct Block
{
    std::vector<float> pcm;          // windowed, ready to overlap-add
    BlockFeatures features;          // analysis of the block as-is
    BlockFeatures normalised;        // analysis of the DC-removed, peak-normalised block
    std::vector<std::uint32_t> synapses;  // nearest neighbours, closest first
};

// Window used for both analysis and playback: a trapezoid whose linear ramps
// span the overlap, so blocks overlap-add to a constant (exact while the overlap
// is at most half a block). Ramps are at least 64 samples to avoid clicks.
std::vector<float> makeBlockWindow (int blockSize, int hop);

// Analyses blocks. prepare() allocates; analyse() is realtime-safe so the same
// code runs on the brain builder and on live input.
class BlockAnalyser
{
public:
    void prepare (int blockSize, int hop, double sampleRate);

    int getBlockSize() const noexcept { return blockSize; }
    const std::vector<float>& getWindow() const noexcept { return window; }

    // `raw` holds blockSize unwindowed samples. `windowedOut` (optional) receives
    // the windowed block.
    void analyse (const float* raw, float* windowedOut, BlockFeatures& features, BlockFeatures& normalised);

private:
    void analyseWindowed (const float* windowed, BlockFeatures& out);

    using Complex = std::complex<float>;

    int blockSize = 0, fftSize = 0;
    double sampleRate = 48000.0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, windowed, normalisedPcm, magnitude, melEnergy;
    std::vector<Complex> timeBuf, specBuf;

    struct MelFilter { int firstBin = 0; std::vector<float> weights; };
    std::vector<MelFilter> melFilters;
    std::vector<float> dct;   // numMfcc x numMelBands
};

// Weighted feature distance (Samplebrain block::_compare), no usage term.
struct SearchParams;
double featureDistance (const BlockFeatures& a, const BlockFeatures& b, const SearchParams& p) noexcept;
} // namespace cortex::brain
