#include "PluginProcessor.h"
#include "Parameters.h"
#include "dsp/CepstralMorph.h"
#include "dsp/STFT.h"

#include <cmath>
#include <vector>

using namespace cortex::dsp;

namespace
{
constexpr double testSampleRate = 48000.0;

std::vector<float> runMorph (const std::vector<float>& a, const std::vector<float>& b, const MorphParams& p, int order)
{
    STFT stft;
    stft.prepare();
    stft.setOrder (order);

    CepstralMorph morph;
    morph.prepare (testSampleRate);
    morph.setOrder (order);
    morph.setTargets (p);
    morph.snapToTargets();

    std::vector<float> out (a.size());
    for (size_t i = 0; i < a.size(); ++i)
        out[i] = stft.processSample (a[i], b[i], [&] (const float* fa, const float* fb, float* fo)
                                     { morph.processFrame (fa, fb, fo); });
    return out;
}

// Residual of `out` against `ref` delayed by `latency`, in dB relative to ref.
double nullDb (const std::vector<float>& out, const std::vector<float>& ref, int latency)
{
    double err = 0.0, sig = 0.0;
    for (size_t i = (size_t) latency * 2; i < out.size(); ++i)
    {
        const double r = ref[i - (size_t) latency];
        err += (out[i] - r) * (out[i] - r);
        sig += r * r;
    }
    return 10.0 * std::log10 (err / sig + 1.0e-30);
}

std::vector<float> whiteNoise (size_t n, int seed, float level)
{
    juce::Random rng (seed);
    std::vector<float> v (n);
    for (auto& s : v)
        s = level * (rng.nextFloat() * 2.0f - 1.0f);
    return v;
}

std::vector<float> pinkNoise (size_t n, int seed)
{
    // Paul Kellet's economy pink filter.
    juce::Random rng (seed);
    std::vector<float> v (n);
    float b0 = 0, b1 = 0, b2 = 0;
    for (auto& s : v)
    {
        const float w = rng.nextFloat() * 2.0f - 1.0f;
        b0 = 0.99765f * b0 + w * 0.0990460f;
        b1 = 0.96300f * b1 + w * 0.2965164f;
        b2 = 0.57000f * b2 + w * 1.0526913f;
        s = 0.1f * (b0 + b1 + b2 + w * 0.1848f);
    }
    return v;
}

std::vector<float> sawtooth (size_t n, double f0)
{
    // Band-limited: additive harmonics below 20 kHz.
    std::vector<float> v (n, 0.0f);
    for (int h = 1; h * f0 < 20000.0; ++h)
        for (size_t i = 0; i < n; ++i)
            v[i] += (float) (0.3 / h * std::sin (juce::MathConstants<double>::twoPi * h * f0 * (double) i / testSampleRate));
    return v;
}

// Averaged power spectrum (Hann, 8192-point) over `from`..end.
std::vector<double> powerSpectrum (const std::vector<float>& x, size_t from)
{
    constexpr int order = 13, n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<double> power (n / 2 + 1, 0.0);
    std::vector<float> buf (2 * n);

    for (size_t start = from; start + n <= x.size(); start += n / 2)
    {
        for (int i = 0; i < n; ++i)
            buf[(size_t) i] = x[start + (size_t) i] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / n));
        std::fill (buf.begin() + n, buf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        for (int k = 0; k <= n / 2; ++k)
            power[(size_t) k] += (double) buf[(size_t) k] * buf[(size_t) k];
    }
    return power;
}

double binHz (size_t k) { return (double) k * testSampleRate / 8192.0; }

double centroidHz (const std::vector<double>& p)
{
    double num = 0.0, den = 0.0;
    for (size_t k = 1; k < p.size(); ++k)
    {
        num += binHz (k) * p[k];
        den += p[k];
    }
    return num / den;
}

// Fraction of energy within +-15 Hz of the harmonics of f0 (below 5 kHz).
double harmonicFraction (const std::vector<double>& p, double f0)
{
    double onHarm = 0.0, total = 0.0;
    for (size_t k = 1; k < p.size() && binHz (k) < 5000.0; ++k)
    {
        const double f = binHz (k);
        const double distance = std::abs (f - f0 * std::round (f / f0));
        total += p[k];
        if (distance < 15.0)
            onHarm += p[k];
    }
    return onHarm / total;
}
} // namespace

class CepstralMorphTests final : public juce::UnitTest
{
public:
    CepstralMorphTests() : juce::UnitTest ("Cepstral morph", "Cortex") {}

    void runTest() override
    {
        const size_t len = 48000;
        const auto a = whiteNoise (len, 1, 0.5f);
        const auto b = pinkNoise (len, 2);

        for (int order = minFftOrder; order <= maxFftOrder; ++order)
        {
            const int n = 1 << order;

            beginTest ("XY (0,0), phase A reproduces A, N=" + juce::String (n));
            MorphParams p;
            p.phaseMode = PhaseMode::fromA;
            const double dbA = nullDb (runMorph (a, b, p, order), a, n);
            logMessage ("  null A: " + juce::String (dbA, 1) + " dB");
            expectLessThan (dbA, -60.0);

            beginTest ("XY (1,1), phase B reproduces B, N=" + juce::String (n));
            p.x = p.y = 1.0f;
            p.phaseMode = PhaseMode::fromB;
            const double dbB = nullDb (runMorph (a, b, p, order), b, n);
            logMessage ("  null B: " + juce::String (dbB, 1) + " dB");
            expectLessThan (dbB, -60.0);

            beginTest ("Impulse comes out exactly N samples late, N=" + juce::String (n));
            std::vector<float> imp (len, 0.0f), zeros (len, 0.0f);
            imp[10000] = 1.0f;
            const auto out = runMorph (imp, zeros, MorphParams {}, order);
            const auto peak = std::max_element (out.begin(), out.end(), [] (float l, float r) { return std::abs (l) < std::abs (r); });
            expectEquals ((int) std::distance (out.begin(), peak), 10000 + n);
        }

        beginTest ("Pitched A + pink B at B-envelope/A-detail corner: keeps A's pitch, takes B's colour");
        {
            // BRIEF.md §6 describes this as "top-left" with a sine, but by the §3 corner
            // definitions A's pitch + B's colour is bottom-right (x=1, y=0). A pure sine
            // has no spectral envelope to swap, so a band-limited saw is used for A.
            const double f0 = 220.0;
            const auto saw = sawtooth (len, f0);
            const auto pink = pinkNoise (len, 3);

            MorphParams p;
            p.x = 1.0f;
            p.y = 0.0f;
            p.phaseMode = PhaseMode::fromA;
            const auto out = runMorph (saw, pink, p, 11);

            const auto pa = powerSpectrum (saw, 4096);
            const auto pb = powerSpectrum (pink, 4096);
            const auto po = powerSpectrum (out, 4096);

            const double ca = centroidHz (pa), cb = centroidHz (pb), co = centroidHz (po);
            logMessage ("  centroid A " + juce::String (ca, 0) + " Hz, B " + juce::String (cb, 0)
                        + " Hz, out " + juce::String (co, 0) + " Hz");
            expectGreaterThan (co, ca * 1.5, "centroid should move from A towards B");

            const double harm = harmonicFraction (po, f0);
            logMessage ("  energy on 220 Hz harmonics: " + juce::String (harm * 100.0, 1) + "%");
            expectGreaterThan (harm, 0.5, "output should stay pitched at A's f0");
            expectLessThan (harmonicFraction (pb, f0), 0.3, "sanity: pink noise is not harmonic");
        }

        beginTest ("Silence in: finite, silent, no denormals");
        {
            std::vector<float> zeros (len, 0.0f);
            for (auto mode : { PhaseMode::fromA, PhaseMode::fromB, PhaseMode::interpolate, PhaseMode::followDetail })
            {
                MorphParams p;
                p.x = 0.3f;
                p.y = 0.7f;
                p.phaseMode = mode;
                p.cornerGainTLdB = 12.0f;
                const auto out = runMorph (zeros, zeros, p, 11);

                bool clean = true;
                for (auto s : out)
                    clean &= std::isfinite (s) && std::abs (s) < 1.0e-4f && std::fpclassify (s) != FP_SUBNORMAL;
                expect (clean);
            }
        }

        beginTest ("Mid-pad morph of noise stays finite and bounded");
        {
            MorphParams p;
            p.x = 0.5f;
            p.y = 0.5f;
            p.phaseMode = PhaseMode::interpolate;
            const auto out = runMorph (a, b, p, 11);
            float peak = 0.0f;
            bool finite = true;
            for (auto s : out)
            {
                finite &= std::isfinite (s);
                peak = std::max (peak, std::abs (s));
            }
            expect (finite);
            expectLessThan (peak, 4.0f);
        }
    }
};

static CepstralMorphTests cepstralMorphTests;

class ProcessorMorphTests final : public juce::UnitTest
{
public:
    ProcessorMorphTests() : juce::UnitTest ("Processor morph path", "Cortex") {}

    static void set (CortexAudioProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    // Feeds an impulse through the processor and returns the index of the output peak.
    static int impulsePeak (CortexAudioProcessor& proc, int blockSize, int totalSamples)
    {
        juce::AudioBuffer<float> buf (2, blockSize);
        juce::MidiBuffer midi;
        float peak = 0.0f;
        int peakIndex = -1;

        for (int start = 0; start < totalSamples; start += blockSize)
        {
            buf.clear();
            if (start <= 1000 && 1000 < start + blockSize)
                for (int ch = 0; ch < 2; ++ch)
                    buf.setSample (ch, 1000 - start, 1.0f);

            proc.processBlock (buf, midi);

            for (int i = 0; i < blockSize; ++i)
                if (std::abs (buf.getSample (0, i)) > peak)
                {
                    peak = std::abs (buf.getSample (0, i));
                    peakIndex = start + i;
                }
        }
        return peakIndex;
    }

    void runTest() override
    {
        using namespace cortex::ParamIDs;

        for (int sizeIndex = 0; sizeIndex < 4; ++sizeIndex)
        {
            const int n = cortex::fftSizes[sizeIndex];

            for (float wetAmount : { 0.0f, 1.0f })
            {
                beginTest ("Reported latency matches " + juce::String (wetAmount == 0.0f ? "dry" : "wet")
                           + " path, N=" + juce::String (n));
                CortexAudioProcessor proc;
                set (proc, fftSize, (float) sizeIndex);
                set (proc, dryWet, wetAmount);
                set (proc, limiter, 0.0f);
                set (proc, autoMute, 0.0f);
                proc.prepareToPlay (testSampleRate, 480);

                expectEquals (proc.getLatencySamples(), n);
                expectEquals (impulsePeak (proc, 480, 1000 + n + 4800), 1000 + n);
            }
        }

        beginTest ("Changing FFT size while running stays finite");
        {
            CortexAudioProcessor proc;
            set (proc, limiter, 0.0f);
            proc.prepareToPlay (testSampleRate, 512);
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            juce::Random rng (7);
            bool finite = true;

            for (int block = 0; block < 200; ++block)
            {
                if (block % 25 == 0)
                    set (proc, fftSize, (float) ((block / 25) % 4));
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i)
                        buf.setSample (ch, i, rng.nextFloat() - 0.5f);
                proc.processBlock (buf, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i)
                        finite &= std::isfinite (buf.getSample (ch, i));
            }
            expect (finite);
        }
    }
};

static ProcessorMorphTests processorMorphTests;
