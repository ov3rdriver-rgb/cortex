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

#include "PluginProcessor.h"
#include "Parameters.h"
#include "dsp/CutoffModulator.h"

class CutoffModulationTests final : public juce::UnitTest
{
public:
    CutoffModulationTests() : juce::UnitTest ("Cutoff modulation", "Cortex") {}

    void runTest() override
    {
        using Mod = CutoffModulator;
        constexpr double rate = 48000.0;
        constexpr int block = 480;   // 10 ms

        beginTest ("LFO sweeps +-depth at its rate, each shape stays in range");
        for (auto shape : { Mod::Shape::sine, Mod::Shape::triangle, Mod::Shape::saw, Mod::Shape::square, Mod::Shape::sampleAndHold })
        {
            Mod m;
            m.prepare (rate);
            Mod::Settings s;
            s.lfoRateHz = 2.0f;
            s.lfoDepth = 30.0f;
            s.shape = shape;
            float lo = 1e9f, hi = -1e9f;
            int signChanges = 0;
            float last = 0.0f;
            for (int b = 0; b < 100; ++b)   // 1 s = 2 cycles
            {
                const auto o = m.process (s, nullptr, block);
                lo = std::min (lo, o.a);
                hi = std::max (hi, o.a);
                if (b > 0 && (o.a > 0.0f) != (last > 0.0f))
                    ++signChanges;
                last = o.a;
            }
            expectGreaterOrEqual (lo, -30.0f);
            expectLessOrEqual (hi, 30.0f);
            if (shape != Mod::Shape::sampleAndHold)
            {
                expectGreaterThan (hi - lo, 50.0f, "should sweep most of +-depth");
                expectWithinAbsoluteError (signChanges, 4, 1);   // 2 Hz for 1 s
            }
        }

        beginTest ("Sample & hold changes once per cycle");
        {
            Mod m;
            m.prepare (rate);
            Mod::Settings s;
            s.lfoRateHz = 5.0f;
            s.lfoDepth = 10.0f;
            s.shape = Mod::Shape::sampleAndHold;
            int changes = 0;
            float last = m.process (s, nullptr, block).a;
            for (int b = 1; b < 100; ++b)
            {
                const float v = m.process (s, nullptr, block).a;
                if (std::abs (v - last) > 1.0e-6f)
                    ++changes;
                last = v;
            }
            expectWithinAbsoluteError (changes, 5, 1);
        }

        beginTest ("LFO target routes to A, B or both");
        {
            Mod::Settings s;
            s.lfoDepth = 20.0f;
            s.shape = Mod::Shape::square;   // +1 at phase 0
            for (auto [target, wantA, wantB] : { std::tuple { Mod::Target::a, true, false },
                                                 std::tuple { Mod::Target::b, false, true },
                                                 std::tuple { Mod::Target::both, true, true } })
            {
                Mod m;
                m.prepare (rate);
                s.target = target;
                const auto o = m.process (s, nullptr, block);
                expectEquals (o.a, wantA ? 20.0f : 0.0f);
                expectEquals (o.b, wantB ? 20.0f : 0.0f);
            }
        }

        beginTest ("Envelope follower: loud input -> full depth, silence -> 0");
        {
            Mod m;
            m.prepare (rate);
            Mod::Settings s;
            s.envDepth = -40.0f;
            s.envAttackMs = 5.0f;
            s.envReleaseMs = 50.0f;
            std::vector<float> loud ((size_t) block), quiet ((size_t) block, 0.0f);
            for (int i = 0; i < block; ++i)
                loud[(size_t) i] = (float) std::sin (i * 0.1);

            Mod::Offsets o;
            for (int b = 0; b < 20; ++b)
                o = m.process (s, loud.data(), block);
            logMessage ("  loud: offset " + juce::String (o.a, 1) + " bins (level " + juce::String (m.getEnvelopeLevel(), 2) + ")");
            expectLessThan (o.a, -36.0f);

            for (int b = 0; b < 100; ++b)
                o = m.process (s, quiet.data(), block);
            logMessage ("  after 1 s of silence: offset " + juce::String (o.a, 2) + " bins");
            expectWithinAbsoluteError (o.a, 0.0f, 0.5f);
        }

        beginTest ("In the plugin the live cutoff actually moves");
        {
            CortexAudioProcessor proc (false);
            auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
            set (cortex::ParamIDs::bSource, 0.0f);
            set (cortex::ParamIDs::lfoDepth, 40.0f);
            set (cortex::ParamIDs::lfoRate, 4.0f);
            set (cortex::ParamIDs::cutoffA, 60.0f);
            proc.prepareToPlay (rate, 512);

            juce::AudioBuffer<float> buf (4, 512);
            juce::MidiBuffer midi;
            juce::Random rng (3);
            float lo = 1e9f, hi = -1e9f;
            for (int b = 0; b < 200; ++b)
            {
                for (int ch = 0; ch < 4; ++ch)
                    for (int i = 0; i < 512; ++i)
                        buf.setSample (ch, i, rng.nextFloat() - 0.5f);
                proc.processBlock (buf, midi);
                cortex::dsp::MorphSnapshot snap;
                if (b > 20 && proc.readMorphSnapshot (snap))
                {
                    lo = std::min (lo, snap.cutoffA);
                    hi = std::max (hi, snap.cutoffA);
                }
            }
            logMessage ("  live cutoff A swept " + juce::String (lo, 1) + " .. " + juce::String (hi, 1) + " (base 60, depth 40)");
            expectLessThan (lo, 35.0f);
            expectGreaterThan (hi, 85.0f);
        }
    }
};

static CutoffModulationTests cutoffModulationTests;

class FormantShiftTests final : public juce::UnitTest
{
public:
    FormantShiftTests() : juce::UnitTest ("Formant shift", "Cortex") {}

    // Frequency of the strongest bin in a frame's spectrum.
    static double peakHz (const std::vector<float>& frame)
    {
        juce::dsp::FFT fft (order);
        std::vector<std::complex<float>> in ((size_t) frameSize), out ((size_t) frameSize);
        for (int i = 0; i < frameSize; ++i)
            in[(size_t) i] = frame[(size_t) i];
        fft.perform (in.data(), out.data(), false);
        int best = 1;
        for (int k = 1; k < frameSize / 2; ++k)
            if (std::abs (out[(size_t) k]) > std::abs (out[(size_t) best]))
                best = k;
        return best * 48000.0 / frameSize;
    }

    void runTest() override
    {
        // 187.5 Hz harmonics (exactly every 8 bins) under one formant at 1 kHz,
        // wide enough for a 25-bin lifter to resolve.
        constexpr double f0 = 187.5;
        std::vector<float> voice ((size_t) frameSize, 0.0f), silence ((size_t) frameSize, 0.0f);
        for (int h = 1; h * f0 < 12000.0; ++h)
        {
            const double f = h * f0;
            const double amp = 0.02 + std::exp (-0.5 * std::pow ((f - 1000.0) / 400.0, 2.0));
            for (int i = 0; i < frameSize; ++i)
                voice[(size_t) i] += (float) (amp * std::sin (juce::MathConstants<double>::twoPi * f * i / 48000.0));
        }
        for (int i = 0; i < frameSize; ++i)
            voice[(size_t) i] *= 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / frameSize);

        auto shifted = [&] (float semitones)
        {
            CepstralMorph m;
            m.prepare (48000.0);
            m.setOrder (order);
            MorphParams p;
            p.formantA = semitones;
            m.setTargets (p);
            m.snapToTargets();
            std::vector<float> out ((size_t) frameSize);
            m.processFrame (voice.data(), silence.data(), out.data());
            return out;
        };

        beginTest ("Formant moves with the shift; harmonics (pitch) stay put");
        for (auto [st, expectedHz] : { std::pair { 0.0f, 1000.0 }, std::pair { 12.0f, 2000.0 }, std::pair { -12.0f, 500.0 } })
        {
            const double hz = peakHz (shifted (st));
            const double harmonicError = std::abs (hz - f0 * std::round (hz / f0));
            logMessage ("  shift " + juce::String (st, 0) + " st -> loudest harmonic " + juce::String (hz, 0) + " Hz");
            expectWithinAbsoluteError (hz, expectedHz, f0 * 0.75);
            expectLessThan (harmonicError, 30.0, "peak must sit on a harmonic of the original pitch");
        }

        beginTest ("Zero shift is bit-identical to no formant processing");
        {
            const auto a = shifted (0.0f);
            CepstralMorph plain;
            plain.prepare (48000.0);
            plain.setOrder (order);
            std::vector<float> b ((size_t) frameSize);
            plain.processFrame (voice.data(), silence.data(), b.data());
            expect (a == b);
        }
    }
};

static FormantShiftTests formantShiftTests;
