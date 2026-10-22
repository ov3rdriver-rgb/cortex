// Phase 5 (BRIEF.md §4 ideas 4-9) feature tests.

#include "dsp/CepstralMorph.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <complex>
#include <vector>

using namespace cortex::dsp;

namespace
{
constexpr int order = 11, frameSize = 1 << order;

std::vector<float> windowedFrame (int seed, bool tonal)
{
    juce::Random rng (seed);
    std::vector<float> f ((size_t) frameSize);
    for (int i = 0; i < frameSize; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / frameSize);
        const float s = tonal ? (float) std::sin (i * 0.07 + seed) + 0.5f * (float) std::sin (i * 0.21)
                              : rng.nextFloat() - 0.5f;
        f[(size_t) i] = w * s;
    }
    return f;
}

std::vector<float> logMagnitude (const std::vector<float>& frame, bool floorBelowPeak = true)
{
    juce::dsp::FFT fft (order);
    std::vector<std::complex<float>> in ((size_t) frameSize), out ((size_t) frameSize);
    for (int i = 0; i < frameSize; ++i)
        in[(size_t) i] = frame[(size_t) i];
    fft.perform (in.data(), out.data(), false);
    // Floor 80 dB below the peak: deeper valleys are below the transforms'
    // float round-off and their logs would just be noise.
    float peak = 0.0f;
    for (int k = 0; k <= frameSize / 2; ++k)
        peak = std::max (peak, std::abs (out[(size_t) k]));
    std::vector<float> m ((size_t) (frameSize / 2 + 1));
    for (int k = 0; k <= frameSize / 2; ++k)
        m[(size_t) k] = std::log (std::abs (out[(size_t) k]) + (floorBelowPeak ? peak * 1.0e-4f : 0.0f) + 1.0e-6f);
    return m;
}

// Low-quefrency real cepstrum of a frame (the envelope part).
std::vector<float> lowCepstrum (const std::vector<float>& frame, int count)
{
    const auto lm = logMagnitude (frame, false);   // the same log the morph itself uses
    std::vector<float> c ((size_t) count, 0.0f);
    for (int q = 0; q < count; ++q)
    {
        double sum = 0.0;
        for (int k = 0; k < frameSize; ++k)
            sum += lm[(size_t) (k <= frameSize / 2 ? k : frameSize - k)] * std::cos (juce::MathConstants<double>::twoPi * k * q / frameSize);
        c[(size_t) q] = (float) (sum / frameSize);
    }
    return c;
}

float maxAbsDiff (const std::vector<float>& a, const std::vector<float>& b, size_t from = 1)
{
    float d = 0.0f;
    for (size_t i = from; i < a.size(); ++i)
        d = std::max (d, std::abs (a[i] - b[i]));
    return d;
}
} // namespace

class CepstralFreezeTests final : public juce::UnitTest
{
public:
    CepstralFreezeTests() : juce::UnitTest ("Cepstral freeze", "Cortex") {}

    void runTest() override
    {
        const auto a1 = windowedFrame (1, true);
        const auto a2 = windowedFrame (2, false);
        const std::vector<float> silence ((size_t) frameSize, 0.0f);

        auto run = [&] (MorphParams p, const std::vector<std::vector<float>>& frames)
        {
            CepstralMorph m;
            m.prepare (48000.0);
            m.setOrder (order);
            m.setTargets (p);
            m.snapToTargets();
            std::vector<std::vector<float>> outs;
            for (const auto& f : frames)
            {
                std::vector<float> out ((size_t) frameSize);
                m.processFrame (f.data(), silence.data(), out.data());
                outs.push_back (out);
            }
            return outs;
        };

        beginTest ("Freezing all of A holds its magnitude spectrum while the input changes");
        {
            MorphParams p;
            p.freezeEnvA = p.freezeDetailA = true;
            const auto outs = run (p, { a1, a2 });
            const float held = maxAbsDiff (logMagnitude (outs[0]), logMagnitude (outs[1]));
            logMessage ("  frozen: max log-magnitude change " + juce::String (held, 5));
            expectLessThan (held, 1.0e-3f);

            const auto live = run (MorphParams {}, { a1, a2 });
            expectGreaterThan (maxAbsDiff (logMagnitude (live[0]), logMagnitude (live[1])), 1.0f, "sanity: without freeze it changes");
        }

        beginTest ("Freezing A's envelope holds the envelope, detail keeps following the input");
        {
            MorphParams p;
            p.freezeEnvA = true;
            const auto outs = run (p, { a1, a2 });
            const auto envBefore = lowCepstrum (outs[0], 18);   // well inside the 25-bin cutoff
            const auto envAfter = lowCepstrum (outs[1], 18);
            const float envChange = maxAbsDiff (envBefore, envAfter);

            const auto live = run (MorphParams {}, { a1, a2 });
            const float liveEnvChange = maxAbsDiff (lowCepstrum (live[0], 18), lowCepstrum (live[1], 18));
            logMessage ("  envelope change: frozen " + juce::String (envChange, 4) + ", live " + juce::String (liveEnvChange, 4));
            expectLessThan (envChange, liveEnvChange * 0.1f);
            expectGreaterThan (maxAbsDiff (logMagnitude (outs[0]), logMagnitude (outs[1])), 1.0f, "detail still moves");
        }

        beginTest ("Switching freeze off releases it");
        {
            CepstralMorph m;
            m.prepare (48000.0);
            m.setOrder (order);
            MorphParams p;
            p.freezeEnvA = p.freezeDetailA = true;
            m.setTargets (p);
            m.snapToTargets();
            std::vector<float> o1 ((size_t) frameSize), o2 ((size_t) frameSize), ref ((size_t) frameSize);
            m.processFrame (a1.data(), silence.data(), o1.data());
            p.freezeEnvA = p.freezeDetailA = false;
            m.setTargets (p);
            m.processFrame (a2.data(), silence.data(), o2.data());

            CepstralMorph plain;
            plain.prepare (48000.0);
            plain.setOrder (order);
            plain.processFrame (a2.data(), silence.data(), ref.data());
            expectLessThan (maxAbsDiff (logMagnitude (o2), logMagnitude (ref)), 1.0e-3f);
        }
    }
};

static CepstralFreezeTests cepstralFreezeTests;
