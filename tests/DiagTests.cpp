#include "PluginProcessor.h"

#include <set>

// Manual diagnostic (category "CortexDiag", not run by default): a saw through
// the plugin with a brain from CORTEX_DIAG_DIR, rendered to CORTEX_DIAG_OUT.
class DiagTests final : public juce::UnitTest
{
public:
    DiagTests() : juce::UnitTest ("Diag", "CortexDiag") {}

    void runSaw (const char* label, float x, float y, bool brain, float novelty, bool glide)
    {
        CortexAudioProcessor proc;
        auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        // x / y move the envelope / detail along the Input -> Brain 1 edge.
        set ("envX", 1.0f - 0.5f * x);
        set ("envY", x);
        set ("detailX", 1.0f - 0.5f * y);
        set ("detailY", y);
        set ("novelty", novelty);
        proc.prepareToPlay (44100.0, 512);
        if (brain)
        {
            proc.loadBrainFrom ({ juce::SystemStats::getEnvironmentVariable ("CORTEX_DIAG_DIR", {}) });
            for (int i = 0; i < 1200 && proc.getBrain() == nullptr; ++i)
                juce::Thread::sleep (50);
        }

        constexpr int numBlocks = 400;
        juce::AudioBuffer<float> buf (2, 512), all (1, numBlocks * 512);
        juce::MidiBuffer midi;
        std::set<int> chosen;
        double phase = 0.0;
        for (int b = 0; b < numBlocks; ++b)
        {
            const double f = glide ? 110.0 * std::pow (2.0, 2.0 * b / numBlocks) : 110.0;
            for (int i = 0; i < 512; ++i)
            {
                phase += f / 44100.0;
                const auto s = (float) (0.5 * (2.0 * (phase - std::floor (phase)) - 1.0));
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
            }
            proc.processBlock (buf, midi);
            all.copyFrom (0, b * 512, buf, 0, 0, 512);
            chosen.insert (proc.getBrainStatus().playingBlock);
        }

        // RMS and spectral centroid over the second half.
        constexpr int order = 14, n = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> spec (2 * n, 0.0f);
        const int start = all.getNumSamples() - n;
        double rms = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float s = all.getSample (0, start + i);
            rms += s * s;
            spec[(size_t) i] = s * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / n));
        }
        fft.performFrequencyOnlyForwardTransform (spec.data(), true);
        double num = 0.0, den = 0.0;
        for (int k = 1; k <= n / 2; ++k)
        {
            const double p = (double) spec[(size_t) k] * spec[(size_t) k];
            num += p * k * 44100.0 / n;
            den += p;
        }

        std::printf ("  %-28s rms %.3f  centroid %6.0f Hz  distinct blocks %d\n", label,
                     std::sqrt (rms / n), den > 0 ? num / den : 0.0, (int) chosen.size() - (chosen.count (-1) ? 1 : 0));

        const auto out = juce::File (juce::SystemStats::getEnvironmentVariable ("CORTEX_DIAG_OUT", {})).getChildFile (juce::String (label) + ".wav");
        out.deleteFile();
        juce::WavAudioFormat wav;
        if (std::unique_ptr<juce::AudioFormatWriter> w { wav.createWriterFor (new juce::FileOutputStream (out), 44100.0, 1, 24, {}, 0) })
            w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
    }

    void runTest() override
    {
        beginTest ("saw + brain");
        runSaw ("1_dry_passthrough", 0, 0, false, 0, false);
        runSaw ("2_brain_x0_y0", 0, 0, true, 0, false);
        runSaw ("3_brain_x1_y0", 1, 0, true, 0, false);
        runSaw ("4_brain_x1_y1", 1, 1, true, 0, false);
        runSaw ("5_brain_x1_y0_novelty", 1, 0, true, 0.8f, false);
        runSaw ("6_brain_x1_y0_glide", 1, 0, true, 0, true);
        expect (true);
    }
};

static DiagTests diagTests;

#include "brain/Brain.h"

// Manual benchmark: per-search cost on a large synthetic brain.
class SearchBench final : public juce::UnitTest
{
public:
    SearchBench() : juce::UnitTest ("Search bench", "CortexDiag") {}

    void runTest() override
    {
        using namespace cortex::brain;
        beginTest ("search cost");

        for (int numBlocks : { 9000, 50000, 150000 })
        {
            Brain brain;
            juce::Random rng (1);
            brain.blocks.resize ((size_t) numBlocks);
            for (auto& b : brain.blocks)
            {
                for (auto& v : b.features.cepstrum) v = rng.nextFloat() - 0.5f;
                for (auto& v : b.features.chroma) v = rng.nextFloat() / 12.0f;
                b.features.f0 = 100.0f + 400.0f * rng.nextFloat();
                b.features.voicing = rng.nextFloat();
            }
            brain.sounds.push_back ({ "bench", 0, (std::uint32_t) numBlocks, true });
            brain.finalise();

            BrainSearch search;
            search.prepare (brain);
            SearchParams p;
            p.usageImportance = 0.3f;
            const auto target = brain.blocks[123].features;

            constexpr int runs = 40;
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            std::uint32_t sink = 0;
            for (int i = 0; i < runs; ++i)
                sink += search.search (brain, brain.makeQuery (target), p);
            const double ms = (juce::Time::getMillisecondCounterHiRes() - t0) / runs;

            // At block 1000 / 44.1 kHz there are ~44 searches a second.
            std::printf ("  %6d blocks: %.2f ms/search -> %.0f%% of one core at 44 searches/s  [%u]\n",
                         numBlocks, ms, ms * 44.0 / 10.0, sink % 2);
        }
        expect (true);
    }
};

static SearchBench searchBench;

#include "TestHelpers.h"
#include "Parameters.h"

// Manual benchmark: audio-callback cost of the plugin with features switched on.
class CallbackBench final : public juce::UnitTest
{
public:
    CallbackBench() : juce::UnitTest ("Callback bench", "CortexDiag") {}

    static juce::File folder (int seed)
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cortex-bench-" + juce::String (seed));
        if (dir.isDirectory())
            return dir;
        dir.createDirectory();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (dir.getChildFile ("b.wav")), 48000.0, 1, 24, {}, 0));
        juce::AudioBuffer<float> a (1, 48000 * 20);
        juce::Random rng (seed);
        double phase = 0.0;
        for (int i = 0; i < a.getNumSamples(); ++i)
        {
            phase += (100.0 + 400.0 * (0.5 + 0.5 * std::sin (i * 0.00003 * seed))) / 48000.0;
            a.setSample (0, i, (float) (0.3 * (2.0 * (phase - std::floor (phase)) - 1.0)) + 0.1f * (rng.nextFloat() - 0.5f));
        }
        w->writeFromAudioSampleBuffer (a, 0, a.getNumSamples());
        return dir;
    }

    double measure (const char* label, std::function<void (CortexAudioProcessor&)> setup, bool brains)
    {
        CortexAudioProcessor proc (false);
        setup (proc);
        proc.prepareToPlay (48000.0, 512);
        if (brains)
        {
            proc.loadBrainFrom ({ folder (3).getFullPathName() }, 0);
            proc.loadBrainFrom ({ folder (7).getFullPathName() }, 1);
            for (int i = 0; i < 1200 && (proc.getBrain (0) == nullptr || proc.getBrain (1) == nullptr); ++i)
                juce::Thread::sleep (50);
        }
        juce::AudioBuffer<float> buf (4, 512);
        juce::MidiBuffer midi;
        juce::Random rng (1);
        auto run = [&] (int blocks)
        {
            for (int b = 0; b < blocks; ++b)
            {
                for (int ch = 0; ch < 4; ++ch)
                    for (int i = 0; i < 512; ++i)
                        buf.setSample (ch, i, 0.3f * (rng.nextFloat() - 0.5f));
                proc.processBlock (buf, midi);
            }
        };
        run (200);   // settle (engine switches, priming)
        const int blocks = 2000;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        run (blocks);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        const double realtime = blocks * 512.0 / 48.0;   // ms of audio
        const double load = 100.0 * ms / realtime;
        std::printf ("  %-48s %5.1f%% of one core (%.3f ms per 512-sample block)\n", label, load, ms / blocks);
        return load;
    }

    void runTest() override
    {
        beginTest ("callback cost");
        using namespace cortex::ParamIDs;
        auto setter = [] (std::initializer_list<std::pair<const char*, float>> values)
        {
            std::vector<std::pair<const char*, float>> v (values);
            return [v] (CortexAudioProcessor& p)
            {
                for (auto [id, value] : v)
                    if (auto* param = p.apvts.getParameter (id))
                        param->setValueNotifyingHost (param->convertTo0to1 (value));
            };
        };

        measure ("defaults, no brains", setter ({}), false);
        measure ("defaults, two brains (envelope at Brain 1)", setter ({}), true);
        measure ("triangles at the centre (all 3 sources)", setter ({ { envX, 0.5f }, { envY, 0.33f }, { detailX, 0.5f }, { detailY, 0.33f } }), true);
        measure ("centre + formant shift on all 3", setter ({ { envX, 0.5f }, { envY, 0.33f }, { detailX, 0.5f }, { detailY, 0.33f },
                                                            { formantIn, 5.0f }, { formantB1, -3.0f }, { formantB2, 7.0f } }), true);
        measure ("centre + formant + FFT 4096", setter ({ { envX, 0.5f }, { envY, 0.33f }, { detailX, 0.5f }, { detailY, 0.33f },
                                                         { formantIn, 5.0f }, { formantB1, -3.0f }, { formantB2, 7.0f }, { fftSize, 3.0f } }), true);
        measure ("centre + transplant + glide", setter ({ { envX, 0.5f }, { envY, 0.33f }, { detailX, 0.5f }, { detailY, 0.33f },
                                                         { envTransplant, 1.0f }, { cepstralGlide, 1.0f } }), true);
        expect (true);
    }
};

static CallbackBench callbackBench;
