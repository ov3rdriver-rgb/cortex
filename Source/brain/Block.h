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
inline constexpr int numCepstrum  = 64;   // low-quefrency real cepstrum (envelope)
inline constexpr int numChroma    = 12;

// Split-cepstrum matching (BRIEF.md §4 idea 2).
// Timbre: cepstral envelope coefficients 1..numTimbre (c0, overall level, is left out).
// Pitch: 12-bin chroma, log2 f0 weighted by voicing, and voicing itself.
inline constexpr int numTimbre    = 24;
inline constexpr int numPitch     = numChroma + 2;

inline constexpr int minBlockSize = 256;
inline constexpr int maxBlockSize = 4096;

// Fixed-size, trivially copyable so it can be cached and copied on the audio thread.
struct BlockFeatures
{
    std::array<float, numFftBins> fft {};       // log10 band power
    std::array<float, numMfcc> mfcc {};
    std::array<float, numCepstrum> cepstrum {};
    std::array<float, numChroma> chroma {};     // pitch-class energy, sums to 1 (0 if silent)
    float f0 = 0.0f;                            // Hz, from the cepstral peak
    float voicing = 0.0f;                       // 0 = noise/unpitched .. 1 = clearly pitched
    float dominantFreq = 0.0f;

    std::array<float, numTimbre> timbreVector() const noexcept;
    std::array<float, numPitch> pitchVector() const noexcept;
};

struct Block
{
    std::vector<float> pcm;          // windowed, ready to overlap-add
    BlockFeatures features;
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
    void analyse (const float* raw, float* windowedOut, BlockFeatures& features);

private:
    using Complex = std::complex<float>;

    int blockSize = 0, fftSize = 0;
    double sampleRate = 48000.0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, windowed, magnitude, melEnergy, peakHeld;
    std::vector<Complex> timeBuf, specBuf;

    struct MelFilter { int firstBin = 0; std::vector<float> weights; };
    std::vector<MelFilter> melFilters;
    std::vector<float> dct;           // numMfcc x numMelBands
    std::vector<std::int8_t> pitchClassOfBin;   // -1 outside the chroma range
    int minPitchQuefrency = 1, maxPitchQuefrency = 1;
};
} // namespace cortex::brain
