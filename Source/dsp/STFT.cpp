#include "STFT.h"

namespace cortex::dsp
{
void STFT::prepare()
{
    for (int s = 0; s < numSources; ++s)
    {
        inputs[(size_t) s].assign ((size_t) maxFftSize, 0.0f);
        frames[(size_t) s].assign ((size_t) maxFftSize, 0.0f);
        framePointers[(size_t) s] = frames[(size_t) s].data();
    }
    outAcc.assign ((size_t) maxFftSize, 0.0f);
    frameOut.assign ((size_t) maxFftSize, 0.0f);

    for (int o = minFftOrder; o <= maxFftOrder; ++o)
    {
        const int size = 1 << o;
        const int h = size / overlap;
        auto& w = windows[(size_t) (o - minFftOrder)];
        w.resize ((size_t) size);

        // Periodic Hann, so shifted copies sum exactly.
        for (int i = 0; i < size; ++i)
            w[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size);

        // Analysis * synthesis window overlap-adds to a constant; normalise it out.
        double sum = 0.0;
        for (int i = 0; i < size; i += h)
            sum += (double) w[(size_t) i] * w[(size_t) i];
        olaScales[(size_t) (o - minFftOrder)] = (float) (1.0 / sum);
    }

    setOrder (juce::jlimit (minFftOrder, maxFftOrder, (int) std::log2 (n)));
}

void STFT::setOrder (int order)
{
    order = juce::jlimit (minFftOrder, maxFftOrder, order);
    n = 1 << order;
    window = windows[(size_t) (order - minFftOrder)].data();
    olaScale = olaScales[(size_t) (order - minFftOrder)];
    reset();
}

void STFT::reset()
{
    for (auto& v : inputs)
        std::fill (v.begin(), v.end(), 0.0f);
    std::fill (outAcc.begin(), outAcc.end(), 0.0f);
    pos = 0;
    hopCounter = 0;
}

void STFT::gatherFrames() noexcept
{
    // pos now points at the oldest sample in the ring.
    for (int s = 0; s < numSources; ++s)
    {
        const auto& in = inputs[(size_t) s];
        auto& frame = frames[(size_t) s];
        for (int i = 0, idx = pos; i < n; ++i)
        {
            frame[(size_t) i] = in[(size_t) idx] * window[i];
            if (++idx == n)
                idx = 0;
        }
    }
}

void STFT::overlapAdd() noexcept
{
    for (int i = 0, idx = pos; i < n; ++i)
    {
        outAcc[(size_t) idx] += frameOut[(size_t) i] * window[i] * olaScale;
        if (++idx == n)
            idx = 0;
    }
}
} // namespace cortex::dsp
