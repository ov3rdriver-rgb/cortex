// Phase 5 (BRIEF.md §4 ideas 4-9) feature tests.

#include "dsp/CepstralMorph.h"
#include "TestHelpers.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <complex>
#include <set>
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
                cortex::test::processAB (m, f.data(), silence.data(), out.data());
                outs.push_back (out);
            }
            return outs;
        };

        beginTest ("Freezing all of A holds its magnitude spectrum while the input changes");
        {
            MorphParams p;
            p.freezeEnv[input] = p.freezeDetail[input] = true;
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
            p.freezeEnv[input] = true;
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
            p.freezeEnv[input] = p.freezeDetail[input] = true;
            m.setTargets (p);
            m.snapToTargets();
            std::vector<float> o1 ((size_t) frameSize), o2 ((size_t) frameSize), ref ((size_t) frameSize);
            cortex::test::processAB (m, a1.data(), silence.data(), o1.data());
            p.freezeEnv[input] = p.freezeDetail[input] = false;
            m.setTargets (p);
            cortex::test::processAB (m, a2.data(), silence.data(), o2.data());

            CepstralMorph plain;
            plain.prepare (48000.0);
            plain.setOrder (order);
            cortex::test::processAB (plain, a2.data(), silence.data(), ref.data());
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
                lo = std::min (lo, o[input]);
                hi = std::max (hi, o[input]);
                if (b > 0 && (o[input] > 0.0f) != (last > 0.0f))
                    ++signChanges;
                last = o[input];
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
            float last = m.process (s, nullptr, block)[input];
            for (int b = 1; b < 100; ++b)
            {
                const float v = m.process (s, nullptr, block)[input];
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
            for (auto [target, wantIn, wantB1, wantB2] : { std::tuple { Mod::Target::input, true, false, false },
                                                           std::tuple { Mod::Target::brain1, false, true, false },
                                                           std::tuple { Mod::Target::brain2, false, false, true },
                                                           std::tuple { Mod::Target::all, true, true, true } })
            {
                Mod m;
                m.prepare (rate);
                s.target = target;
                const auto o = m.process (s, nullptr, block);
                expectEquals (o[input], wantIn ? 20.0f : 0.0f);
                expectEquals (o[brain1], wantB1 ? 20.0f : 0.0f);
                expectEquals (o[brain2], wantB2 ? 20.0f : 0.0f);
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
            logMessage ("  loud: offset " + juce::String (o[input], 1) + " bins (level " + juce::String (m.getEnvelopeLevel(), 2) + ")");
            expectLessThan (o[input], -36.0f);

            for (int b = 0; b < 100; ++b)
                o = m.process (s, quiet.data(), block);
            logMessage ("  after 1 s of silence: offset " + juce::String (o[input], 2) + " bins");
            expectWithinAbsoluteError (o[input], 0.0f, 0.5f);
        }

        beginTest ("In the plugin the live cutoff actually moves");
        {
            CortexAudioProcessor proc (false);
            auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
            set (cortex::ParamIDs::lfoDepth, 40.0f);
            set (cortex::ParamIDs::lfoRate, 4.0f);
            set (cortex::ParamIDs::cutoffIn, 60.0f);
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
                    lo = std::min (lo, snap.cutoff[input]);
                    hi = std::max (hi, snap.cutoff[input]);
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
            p.formant[input] = semitones;
            m.setTargets (p);
            m.snapToTargets();
            std::vector<float> out ((size_t) frameSize);
            cortex::test::processAB (m, voice.data(), silence.data(), out.data());
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
            cortex::test::processAB (plain, voice.data(), silence.data(), b.data());
            expect (a == b);
        }
    }
};

static FormantShiftTests formantShiftTests;

#include "brain/BrainBuilder.h"
#include "brain/Renderer.h"

class SearchMacroTests final : public juce::UnitTest
{
public:
    SearchMacroTests() : juce::UnitTest ("Chaos / Memory macros", "Cortex") {}

    struct Walk
    {
        int distinct = 0;
        float sequential = 0.0f;   // fraction of choices that are the previous block + 1
    };

    // Feeds a static tone through a renderer and records which blocks it plays.
    static Walk walk (std::shared_ptr<const cortex::brain::Brain> brain, float chaos, float memory)
    {
        using namespace cortex::brain;
        Renderer r (brain);
        r.setBlocking (true);
        r.setSearch (applySearchKnobs ({}, {}, chaos, memory), SearchMode::basic);

        std::set<int> seen;
        int sequential = 0, choices = 0, last = -1;
        for (int i = 0; i < 48000 * 6; ++i)
        {
            r.processSample (0.3f * (float) std::sin (i * 0.0294));   // steady ~225 Hz
            const int b = r.getLastChosenBlock();
            if (b != last && b >= 0)
            {
                if (last >= 0)
                {
                    ++choices;
                    sequential += b == last + 1 ? 1 : 0;
                }
                seen.insert (b);
                last = b;
            }
        }
        return { (int) seen.size(), choices > 0 ? (float) sequential / (float) choices : 0.0f };
    }

    void runTest() override
    {
        using namespace cortex::brain;

        beginTest ("At 0 / 0 the macros leave the knobs exactly as set");
        {
            const auto p = applySearchKnobs ({}, { 0.25f, 0.6f, 0.4f }, 0.0f, 0.0f);
            expectEquals (p.usageImportance, 0.25f);
            expectEquals (p.usageFalloff, 0.5f + 0.49f * 0.6f);
            expectEquals (p.stickyness, 0.4f);
            expectEquals (p.randomJump, 0.0f);
        }

        // A varied brain: a gliding saw with noise, 8 seconds.
        std::vector<float> audio ((size_t) 48000 * 8);
        {
            juce::Random rng (5);
            double phase = 0.0;
            for (size_t i = 0; i < audio.size(); ++i)
            {
                phase += (120.0 + 300.0 * (0.5 + 0.5 * std::sin (i * 0.00004))) / 48000.0;
                audio[i] = (float) (0.3 * (2.0 * (phase - std::floor (phase)) - 1.0)) + 0.05f * (rng.nextFloat() - 0.5f);
            }
        }
        BrainSettings s;
        s.blockSize = 1000;
        s.sampleRate = 48000.0;
        auto brain = buildBrain ({ BrainSource { "glide", audio } }, s, SearchParams {}, {}, {});

        beginTest ("Chaos spreads a static input across far more of the brain");
        {
            const auto calm = walk (brain, 0.0f, 0.0f);
            const auto wild = walk (brain, 1.0f, 0.0f);
            logMessage ("  distinct blocks over 6 s: chaos 0 -> " + juce::String (calm.distinct) + ", chaos 1 -> " + juce::String (wild.distinct));
            expectGreaterThan (wild.distinct, calm.distinct * 3);
        }

        beginTest ("Memory makes the brain follow sequences in its source");
        {
            const auto loose = walk (brain, 0.0f, 0.0f);
            const auto sticky = walk (brain, 0.0f, 1.0f);
            logMessage ("  sequential choices: memory 0 -> " + juce::String (loose.sequential * 100.0f, 0) + "%, memory 1 -> "
                        + juce::String (sticky.sequential * 100.0f, 0) + "%");
            expectGreaterThan (sticky.sequential, 0.5f);
            expectGreaterThan (sticky.sequential, loose.sequential + 0.3f);
        }
    }
};

static SearchMacroTests searchMacroTests;

class CepstralGlideTests final : public juce::UnitTest
{
public:
    CepstralGlideTests() : juce::UnitTest ("Cepstral glide", "Cortex") {}

    // Saw at 220 Hz through a gentle (bright) or steep (dark) lowpass.
    static std::vector<float> saw (size_t length, double cornerHz)
    {
        std::vector<float> v (length, 0.0f);
        for (int h = 1; h * 220.0 < 16000.0; ++h)
        {
            const double f = h * 220.0;
            const double amp = 0.3 / h / (1.0 + std::pow (f / cornerHz, 2.0));
            for (size_t i = 0; i < length; ++i)
                v[i] += (float) (amp * std::sin (juce::MathConstants<double>::twoPi * f * (double) i / 48000.0));
        }
        return v;
    }

    static double centroid (const std::vector<float>& x, size_t from, int length)
    {
        juce::dsp::FFT fft (order);
        std::vector<std::complex<float>> in ((size_t) frameSize), out ((size_t) frameSize);
        for (int i = 0; i < length; ++i)
            in[(size_t) i] = x[from + (size_t) i] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / length));
        fft.perform (in.data(), out.data(), false);
        double num = 0.0, den = 0.0;
        for (int k = 1; k < frameSize / 2; ++k)
        {
            const double p = std::norm (out[(size_t) k]);
            num += p * k * 48000.0 / frameSize;
            den += p;
        }
        return num / den;
    }

    // Renders a dark->bright input and returns (start, end) centroids of the
    // first bright grain played after the dark one.
    std::pair<double, double> firstBrightGrain (float glide)
    {
        using namespace cortex::brain;
        constexpr int bs = 2048;
        BrainSettings s;
        s.blockSize = bs;
        s.sampleRate = 48000.0;
        auto brain = buildBrain ({ BrainSource { "dark", saw (bs, 300.0) }, BrainSource { "bright", saw (bs, 8000.0) } },
                                 s, SearchParams {}, {}, {});
        expectEquals ((int) brain->numBlocks(), 2);

        Renderer r (brain);
        r.setBlocking (true);
        SearchParams p;
        p.cepstralGlide = glide;
        r.setSearch (p, SearchMode::basic);

        const auto dark = saw (bs * 2, 300.0), bright = saw (bs * 10, 8000.0);
        std::vector<float> out;
        size_t brightStart = 0;
        int last = -1;
        auto feed = [&] (const std::vector<float>& x)
        {
            for (auto v : x)
            {
                out.push_back (r.processSample (v));
                if (r.getLastChosenBlock() != last)
                {
                    if (last == 0 && r.getLastChosenBlock() == 1 && brightStart == 0)
                        brightStart = out.size();   // this grain's first sample comes next
                    last = r.getLastChosenBlock();
                }
            }
        };
        feed (dark);
        feed (bright);
        expect (brightStart > 0, "the brain should switch from the dark block to the bright one");

        return { centroid (out, brightStart, bs / 4), centroid (out, brightStart + 3 * bs / 4 - 64, bs / 4) };
    }

    void runTest() override
    {
        beginTest ("With glide the new grain starts from the previous grain's envelope and ends on its own");
        const auto [plainStart, plainEnd] = firstBrightGrain (0.0f);
        const auto [glideStart, glideEnd] = firstBrightGrain (1.0f);
        logMessage ("  first bright grain centroid, start -> end: no glide " + juce::String (plainStart, 0) + " -> "
                    + juce::String (plainEnd, 0) + " Hz, glide " + juce::String (glideStart, 0) + " -> " + juce::String (glideEnd, 0) + " Hz");
        expectGreaterThan (glideEnd, glideStart * 1.4, "glide: dark at the start, bright at the end");
        expectLessThan (std::abs (plainEnd - plainStart), plainEnd * 0.15, "no glide: bright throughout");
        expectWithinAbsoluteError (glideEnd, plainEnd, plainEnd * 0.2, "the glide ends on the grain's own envelope");
    }
};

static CepstralGlideTests cepstralGlideTests;

class FourCornerBrainTests final : public juce::UnitTest
{
public:
    FourCornerBrainTests() : juce::UnitTest ("4-corner brain", "Cortex") {}

    static juce::File writeFolder (const juce::String& name, const std::vector<float>& audio)
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("cortex-4c-" + name + "-" + juce::String (juce::Random::getSystemRandom().nextInt()));
        dir.createDirectory();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (dir.getChildFile (name + ".wav")),
                                                                         48000.0, 1, 24, {}, 0));
        const float* chans[] { audio.data() };
        w->writeFromFloatArrays (chans, 1, (int) audio.size());
        return dir;
    }

    static double centroidOf (const std::vector<float>& x)
    {
        constexpr int o = 14, len = 1 << o;
        juce::dsp::FFT fft (o);
        std::vector<float> buf (2 * len, 0.0f);
        for (int i = 0; i < len; ++i)
            buf[(size_t) i] = x[x.size() - len + (size_t) i] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / len));
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        double num = 0.0, den = 0.0;
        for (int k = 1; k <= len / 2; ++k)
        {
            const double p = (double) buf[(size_t) k] * buf[(size_t) k];
            num += p * k * 48000.0 / len;
            den += p;
        }
        return num / den;
    }

    struct Render
    {
        double centroid = 0.0;
        int latency = 0;
    };

    // Renders with both triangles (and the phase) on one source.
    Render render (const juce::File& brain1, const juce::File& brain2, Source pure)
    {
        CortexAudioProcessor proc (false);
        auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        cortex::test::setPureSource (proc.apvts, pure);
        set (cortex::ParamIDs::phaseMode, (float) pure + 1.0f);
        set (cortex::ParamIDs::novelty, 0.0f);
        set (cortex::ParamIDs::limiter, 0.0f);
        set (cortex::ParamIDs::autoMute, 0.0f);
        proc.setNonRealtime (true);
        proc.prepareToPlay (48000.0, 512);
        proc.loadBrainFrom ({ brain1.getFullPathName() }, 0);
        if (brain2 != juce::File())
            proc.loadBrainFrom ({ brain2.getFullPathName() }, 1);
        for (int i = 0; i < 600 && (proc.getBrain (0) == nullptr || (brain2 != juce::File() && proc.getBrain (1) == nullptr)); ++i)
            juce::Thread::sleep (50);

        // Input: a mid-bright saw at 220 Hz.
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        std::vector<float> out;
        double phaseAcc = 0.0;
        for (int b = 0; b < 300; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                phaseAcc += 220.0 / 48000.0;
                const float s = (float) (0.3 * (2.0 * (phaseAcc - std::floor (phaseAcc)) - 1.0));
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
            }
            proc.processBlock (buf, midi);
            out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + 512);
        }
        return { centroidOf (out), proc.getAlignedLatencySamples() };
    }

    void runTest() override
    {
        // Brain 1: dark (a saw through a steep lowpass). Brain 2: bright noise.
        std::vector<float> dark ((size_t) 96000, 0.0f), bright ((size_t) 96000);
        for (int h = 1; h * 220.0 < 12000.0; ++h)
        {
            const double amp = 0.3 / h / (1.0 + std::pow (h * 220.0 / 300.0, 2.0));
            for (size_t i = 0; i < dark.size(); ++i)
                dark[i] += (float) (amp * std::sin (juce::MathConstants<double>::twoPi * h * 220.0 * (double) i / 48000.0));
        }
        juce::Random rng (8);
        float prev = 0.0f;
        for (auto& v : bright)
        {
            const float w = rng.nextFloat() - 0.5f;
            v = 0.4f * (w - prev);   // first difference: tilted towards the highs
            prev = w;
        }
        const auto dir1 = writeFolder ("dark", dark), dir2 = writeFolder ("bright", bright);

        beginTest ("Each brain's corner plays that brain");
        {
            const auto pure2 = render (dir1, dir2, Source::brain2);
            const auto pure1 = render (dir1, dir2, Source::brain1);
            logMessage ("  centroid: Brain 2 corner (bright) " + juce::String (pure2.centroid, 0)
                        + " Hz, Brain 1 corner (dark) " + juce::String (pure1.centroid, 0) + " Hz");
            expectGreaterThan (pure2.centroid, pure1.centroid * 4.0);
            expectEquals (pure2.latency, 2048 + 1000 + 1024, "FFT + one brain's latency, not two");
        }

        beginTest ("A corner whose brain isn't loaded plays the Input instead");
        {
            const auto fallback = render (dir1, {}, Source::brain2);
            const auto input = render (dir1, {}, Source::input);
            logMessage ("  centroid with no Brain 2: " + juce::String (fallback.centroid, 0) + " Hz (input path " + juce::String (input.centroid, 0) + " Hz)");
            expectWithinAbsoluteError (fallback.centroid, input.centroid, input.centroid * 0.02);
        }

        beginTest ("Both brains' file lists survive save / restore");
        {
            CortexAudioProcessor proc (false);
            proc.prepareToPlay (48000.0, 512);
            proc.loadBrainFrom ({ dir1.getFullPathName() }, 0);
            proc.loadBrainFrom ({ dir2.getFullPathName() }, 1);
            juce::MemoryBlock state;
            proc.getStateInformation (state);

            CortexAudioProcessor restored (false);
            restored.setStateInformation (state.getData(), (int) state.getSize());
            expect (restored.getBrainFiles (0) == proc.getBrainFiles (0));
            expect (restored.getBrainFiles (1) == proc.getBrainFiles (1));
            expect (restored.getBrainFiles (0) != restored.getBrainFiles (1));
        }

        dir1.deleteRecursively();
        dir2.deleteRecursively();
    }
};

static FourCornerBrainTests fourCornerBrainTests;
