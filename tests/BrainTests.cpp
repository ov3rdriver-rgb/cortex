#include "PluginProcessor.h"
#include "TestHelpers.h"
#include "brain/BrainBuilder.h"
#include "brain/Renderer.h"
#include "state/BrainCache.h"

#include <cmath>
#include <cstring>

using namespace cortex::brain;

namespace
{
constexpr double brainRate = 48000.0;

// Something with enough variety that no two blocks share features:
// a gliding saw-ish tone plus filtered noise with a slow level wobble.
std::vector<float> testSignal (size_t length, int seed)
{
    juce::Random rng (seed);
    std::vector<float> v (length);
    double phase = 0.0;
    float lp = 0.0f;
    for (size_t i = 0; i < length; ++i)
    {
        const double t = (double) i / brainRate;
        phase += (180.0 + 140.0 * std::sin (t * 1.3)) / brainRate;
        const double tone = 2.0 * (phase - std::floor (phase)) - 1.0;
        lp += 0.15f * ((rng.nextFloat() * 2.0f - 1.0f) - lp);
        v[i] = (float) (0.25 * tone * (0.6 + 0.4 * std::sin (t * 2.1)) + 0.3 * lp);
    }
    return v;
}

std::shared_ptr<Brain> build (const std::vector<float>& audio, int blockSize, float overlap)
{
    BrainSource src;
    src.path = "test";
    src.mono = audio;
    BrainSettings s;
    s.blockSize = blockSize;
    s.overlap = overlap;
    s.sampleRate = brainRate;
    return buildBrain ({ src }, s, SearchParams {}, {}, {});
}

juce::File writeWav (const juce::File& file, const std::vector<float>& audio, double rate)
{
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (file), rate, 1, 24, {}, 0));
    const float* chans[] { audio.data() };
    writer->writeFromFloatArrays (chans, 1, (int) audio.size());
    return file;
}
} // namespace

class BrainEngineTests final : public juce::UnitTest
{
public:
    BrainEngineTests() : juce::UnitTest ("Brain engine", "Cortex") {}

    void runTest() override
    {
        const auto audio = testSignal ((size_t) brainRate * 3, 11);

        beginTest ("Block count follows Samplebrain's chop_and_add");
        {
            for (auto [bs, ov] : { std::pair { 1000, 0.0f }, std::pair { 1024, 0.5f }, std::pair { 512, 0.75f } })
            {
                const auto b = build (audio, bs, ov);
                const int hop = b->settings.hop();
                expectEquals ((int) b->numBlocks(), (int) ((audio.size() - (size_t) bs) / (size_t) hop + 1));
                expectEquals ((int) b->sounds[0].numBlocks, (int) b->numBlocks());
            }
        }

        beginTest ("Identical blocks analyse identically; different ones don't");
        {
            BlockAnalyser an;
            an.prepare (1000, 1000, brainRate);
            BlockFeatures f1, f2, f3;
            an.analyse (audio.data() + 5000, nullptr, f1);
            an.analyse (audio.data() + 5000, nullptr, f2);
            an.analyse (audio.data() + 20000, nullptr, f3);
            auto brain = build (audio, 1000, 0.0f);
            SearchParams p;
            const auto q1 = brain->makeQuery (f1), q2 = brain->makeQuery (f2), q3 = brain->makeQuery (f3);
            expect (q1.timbre == q2.timbre && q1.pitch == q2.pitch);
            expect (q1.timbre != q3.timbre);
        }

        for (float overlap : { 0.25f, 0.5f })
        {
            beginTest ("A brain built from the target reconstructs it (overlap " + juce::String (overlap) + ")");

            const int bs = 1024;
            auto brain = build (audio, bs, overlap);
            Renderer r (brain);
            r.setSearch (SearchParams {}, SearchMode::basic);
            r.setBlocking (true);

            // Each match starts a fixed delay after its target block completes;
            // consecutive blocks always differ, so every change is one start.
            std::vector<float> out (audio.size());
            int wrongBlocks = 0, expectedBlock = 0, last = -1;
            for (size_t i = 0; i < audio.size(); ++i)
            {
                out[i] = r.processSample (audio[i]);
                if (r.getLastChosenBlock() != last)
                {
                    last = r.getLastChosenBlock();
                    if (last != expectedBlock)
                        ++wrongBlocks;
                    ++expectedBlock;
                }
            }
            expectGreaterThan (expectedBlock, (int) brain->numBlocks() - 4);
            expectEquals (wrongBlocks, 0, "every target block should match its own brain block");

            // Compare away from the start (ramp-in) and end (no blocks past the last full one).
            double err = 0.0, sig = 0.0;
            const auto lat = (size_t) r.getLatencySamples();
            for (size_t i = lat + (size_t) bs; i + (size_t) bs < audio.size(); ++i)
            {
                const double d = out[i] - audio[i - lat];
                err += d * d;
                sig += (double) audio[i - lat] * audio[i - lat];
            }
            const double db = 10.0 * std::log10 (err / sig + 1.0e-30);
            logMessage ("  reconstruction null: " + juce::String (db, 1) + " dB");
            expectLessThan (db, -60.0);
        }

        beginTest ("Synapses: nearest first, never self, bounded");
        {
            auto brain = build (audio, 1000, 0.0f);
            const SearchParams p;
            for (std::uint32_t i = 0; i < brain->numBlocks(); i += 17)
            {
                const auto& syn = brain->blocks[i].synapses;
                expectEquals ((int) syn.size(), juce::jmin (maxStoredSynapses, (int) brain->numBlocks() - 1));
                double last = -1.0;
                bool sorted = true, noSelf = true;
                for (auto s : syn)
                {
                    const double d = brain->distanceTo (s, brain->makeQuery (brain->blocks[i].features), p);
                    sorted &= d >= last;
                    noSelf &= s != i;
                    last = d;
                }
                expect (sorted && noSelf);
            }
        }

        beginTest ("Every search mode runs and stays finite");
        {
            auto brain = build (audio, 1000, 0.5f);
            const auto other = testSignal ((size_t) brainRate, 99);
            for (auto mode : { SearchMode::basic, SearchMode::reverse, SearchMode::synaptic, SearchMode::synapticSlide })
            {
                Renderer r (brain);
                SearchParams p;
                p.usageImportance = 0.3f;
                p.stickyness = 0.4f;
                r.setSearch (p, mode);
                r.setBlocking (true);   // faster than real time: wait for each search
                bool finite = true;
                for (auto s : other)
                    finite &= std::isfinite (r.processSample (s));
                expect (finite);
                expect (r.getLastChosenBlock() >= 0 && r.getLastChosenBlock() < (int) brain->numBlocks());
            }
        }

        beginTest ("Cache round-trip");
        {
            auto brain = build (audio, 800, 0.25f);
            juce::TemporaryFile temp (".cortexbrain");
            expect (cortex::BrainCache::save (*brain, temp.getFile()));
            auto loaded = cortex::BrainCache::load (temp.getFile());
            expect (loaded != nullptr);
            if (loaded != nullptr)
            {
                expect (loaded->settings == brain->settings);
                expectEquals ((int) loaded->numBlocks(), (int) brain->numBlocks());
                expect (loaded->blocks[7].pcm == brain->blocks[7].pcm);
                expect (loaded->blocks[7].synapses == brain->blocks[7].synapses);
                expect (std::memcmp (&loaded->blocks[7].features, &brain->blocks[7].features, sizeof (BlockFeatures)) == 0);
                expect (loaded->timbreTable == brain->timbreTable && loaded->pitchTable == brain->pitchTable);
            }

            // A truncated file must be rejected, not crash.
            juce::MemoryBlock data;
            temp.getFile().loadFileAsData (data);
            temp.getFile().replaceWithData (data.getData(), data.getSize() / 2);
            expect (cortex::BrainCache::load (temp.getFile()) == nullptr);
        }

        beginTest ("Decoding resamples to the brain rate and mixes to mono");
        {
            juce::TemporaryFile temp (".wav");
            const std::vector<float> one ((size_t) 44100, 0.25f);
            writeWav (temp.getFile(), one, 44100.0);

            juce::AudioFormatManager fm;
            fm.registerBasicFormats();
            BrainSource src;
            expect (decodeToMono (fm, temp.getFile(), brainRate, src));
            expect (std::abs ((int) src.mono.size() - 48000) <= 1);
            expectWithinAbsoluteError (src.mono[24000], 0.25f, 1.0e-3f);
        }

        beginTest ("Builder thread builds, caches, then reloads from cache quickly");
        {
            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cortex-test-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            dir.createDirectory();
            const auto wavA = writeWav (dir.getChildFile ("a.wav"), testSignal (96000, 1), brainRate);
            const auto wavB = writeWav (dir.getChildFile ("b.wav"), testSignal (48000, 2), 44100.0);

            juce::AudioFormatManager fm;
            fm.registerBasicFormats();
            const auto files = collectAudioFiles ({ dir.getFullPathName() }, fm);
            expectEquals (files.size(), 2);

            juce::WaitableEvent done;
            std::shared_ptr<const Brain> result;
            juce::String message;
            BrainBuilder builder ([&] (std::shared_ptr<const Brain> b, const juce::String& m)
            {
                result = std::move (b);
                message = m;
                done.signal();
            });

            BrainSettings s;
            s.blockSize = 1000;
            s.sampleRate = brainRate;
            builder.start ({ files, s, dir.getChildFile ("cache") });
            expect (done.wait (60000), "build finished");
            expect (result != nullptr && result->sounds.size() == 2);
            const auto blocks = result != nullptr ? result->numBlocks() : 0u;

            done.reset();
            result = nullptr;
            const auto start = juce::Time::getMillisecondCounterHiRes();
            builder.start ({ files, s, dir.getChildFile ("cache") });
            expect (done.wait (60000), "cached build finished");
            const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
            logMessage ("  cache reload: " + juce::String (ms, 1) + " ms (" + message + ")");
            expect (message.contains ("cache"));
            expect (result != nullptr && result->numBlocks() == blocks);
            expectLessThan (ms, 2000.0);

            builder.cancel();
            dir.deleteRecursively();
        }
    }
};

static BrainEngineTests brainEngineTests;

namespace
{
// Band-limited saw with harmonics up to `maxHarmonicHz` (low values = darker).
std::vector<float> saw (size_t length, double f0, double maxHarmonicHz)
{
    std::vector<float> v (length, 0.0f);
    for (int h = 1; h * f0 < maxHarmonicHz; ++h)
        for (size_t i = 0; i < length; ++i)
            v[i] += (float) (0.3 / h * std::sin (juce::MathConstants<double>::twoPi * h * f0 * (double) i / brainRate));
    return v;
}
}

class SplitMatchingTests final : public juce::UnitTest
{
public:
    SplitMatchingTests() : juce::UnitTest ("Split-cepstrum matching", "Cortex") {}

    void runTest() override
    {
        beginTest ("Cepstral pitch: f0 and voicing of saws; noise is unvoiced");
        {
            BlockAnalyser an;
            an.prepare (2048, 2048, brainRate);
            BlockFeatures f;

            for (double hz : { 110.0, 220.0, 440.0, 660.0 })
            {
                const auto s = saw (4096, hz, 12000.0);
                an.analyse (s.data() + 1024, nullptr, f);
                logMessage ("  saw " + juce::String (hz) + " Hz -> f0 " + juce::String (f.f0, 1) + ", voicing " + juce::String (f.voicing, 2));
                expectWithinAbsoluteError (f.f0, (float) hz, (float) hz * 0.03f);
                expectGreaterThan (f.voicing, 0.6f);
            }

            an.analyse (testSignal (4096, 3).data(), nullptr, f);   // includes a tone
            juce::Random rng (4);
            std::vector<float> noise (4096);
            for (auto& v : noise)
                v = rng.nextFloat() - 0.5f;
            an.analyse (noise.data(), nullptr, f);
            logMessage ("  white noise -> voicing " + juce::String (f.voicing, 2));
            expectLessThan (f.voicing, 0.3f);

            const auto a = saw (4096, 220.0, 12000.0);
            an.analyse (a.data(), nullptr, f);
            const auto peak = std::max_element (f.chroma.begin(), f.chroma.end()) - f.chroma.begin();
            expectEquals ((int) peak, 0, "220 Hz is an A: chroma should peak at pitch class 0");
        }

        beginTest ("Match Pitch finds same-pitch blocks, Match Timbre finds same-tone blocks");
        {
            // Brain: a bright saw at 330 Hz and a dark saw at 220 Hz.
            // Target: a bright saw at 220 Hz - same tone as the first, same pitch as the second.
            BrainSource bright330 { "bright330", saw (48000, 330.0, 16000.0) };
            BrainSource dark220 { "dark220", saw (48000, 220.0, 900.0) };
            BrainSettings s;
            s.blockSize = 2048;
            s.sampleRate = brainRate;
            auto brain = buildBrain ({ bright330, dark220 }, s, SearchParams {}, {}, {});

            BlockAnalyser an;
            an.prepare (2048, 2048, brainRate);
            BlockFeatures target;
            const auto t = saw (4096, 220.0, 16000.0);
            an.analyse (t.data() + 1000, nullptr, target);
            const auto q = brain->makeQuery (target);

            auto soundOf = [&] (std::uint32_t block) { return block < brain->sounds[1].firstBlock ? juce::String ("bright330") : juce::String ("dark220"); };

            SearchParams pitchOnly;
            pitchOnly.timbreWeight = 0.0f;
            pitchOnly.pitchWeight = 1.0f;
            BrainSearch search;
            search.prepare (*brain);
            const auto byPitch = soundOf (search.search (*brain, q, pitchOnly));

            SearchParams timbreOnly;
            timbreOnly.timbreWeight = 1.0f;
            timbreOnly.pitchWeight = 0.0f;
            search.reset();
            const auto byTimbre = soundOf (search.search (*brain, q, timbreOnly));

            logMessage ("  pitch-only -> " + byPitch + ", timbre-only -> " + byTimbre);
            expectEquals (byPitch, juce::String ("dark220"));
            expectEquals (byTimbre, juce::String ("bright330"));
        }

        beginTest ("Envelope transplant gives brain grains the input's spectral envelope");
        {
            // Brain: a saw through a steep lowpass (harmonics present but dark).
            // Input: the same saw, unfiltered (bright).
            auto darkSaw = [] (size_t length)
            {
                std::vector<float> v (length, 0.0f);
                for (int h = 1; h * 220.0 < 16000.0; ++h)
                {
                    const double f = h * 220.0;
                    const double amp = 0.3 / h / (1.0 + std::pow (f / 400.0, 2.0));
                    for (size_t i = 0; i < length; ++i)
                        v[i] += (float) (amp * std::sin (juce::MathConstants<double>::twoPi * f * (double) i / brainRate));
                }
                return v;
            };

            BrainSettings s;
            s.blockSize = 2048;
            s.overlap = 0.5f;
            s.sampleRate = brainRate;
            auto brain = buildBrain ({ BrainSource { "dark", darkSaw (96000) } }, s, SearchParams {}, {}, {});
            const auto bright = saw (96000, 220.0, 16000.0);

            auto centroidWith = [&] (float amount)
            {
                Renderer r (brain);
                r.setBlocking (true);
                SearchParams p;
                p.envTransplant = amount;
                r.setSearch (p, SearchMode::basic);

                std::vector<float> out (bright.size());
                for (size_t i = 0; i < bright.size(); ++i)
                    out[i] = r.processSample (bright[i]);

                constexpr int order = 14, n = 1 << order;
                juce::dsp::FFT fft (order);
                std::vector<float> spec (2 * n, 0.0f);
                for (int i = 0; i < n; ++i)
                    spec[(size_t) i] = out[out.size() - n + (size_t) i] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / n));
                fft.performFrequencyOnlyForwardTransform (spec.data(), true);
                double num = 0.0, den = 0.0;
                for (int k = 1; k <= n / 2; ++k)
                {
                    const double pw = (double) spec[(size_t) k] * spec[(size_t) k];
                    num += pw * k * brainRate / n;
                    den += pw;
                }
                return num / den;
            };

            const double off = centroidWith (0.0f), half = centroidWith (0.5f), full = centroidWith (1.0f);
            logMessage ("  brain output centroid: transplant 0 -> " + juce::String (off, 0) + " Hz, 0.5 -> "
                        + juce::String (half, 0) + " Hz, 1 -> " + juce::String (full, 0) + " Hz");
            // The transplant is linear in dB, so the centroid (dominated by the
            // strong low harmonics) moves little at 0.5 and a lot near 1.
            expectGreaterThan (half, off);
            expectGreaterThan (full, half);
            expectGreaterThan (full, off * 2.0, "full transplant should take on the bright input's tilt");
        }

        beginTest ("Silent blocks are left out of the brain");
        {
            auto audio = saw (96000, 220.0, 8000.0);
            std::fill (audio.begin() + 48000, audio.end(), 0.0f);
            BrainSettings s;
            s.blockSize = 1000;
            s.sampleRate = brainRate;
            auto brain = buildBrain ({ BrainSource { "half-silent", audio } }, s, SearchParams {}, {}, {});
            expectEquals ((int) brain->numBlocks(), 48);
        }

        beginTest ("Brain grains are played at the input's level");
        {
            // A quiet brain (-20 dB) driven by a full-level input of the same sound.
            // Grains land at random phase against each other, so allow a little
            // overlap cancellation.
            auto quiet = saw (96000, 220.0, 8000.0);
            for (auto& v : quiet)
                v *= 0.1f;
            BrainSettings s;
            s.blockSize = 1024;
            s.overlap = 0.5f;
            s.sampleRate = brainRate;
            auto brain = buildBrain ({ BrainSource { "quiet", quiet } }, s, SearchParams {}, {}, {});

            Renderer r (brain);
            r.setBlocking (true);
            const auto loud = saw (96000, 220.0, 8000.0);
            double inSq = 0.0, outSq = 0.0;
            for (size_t i = 0; i < loud.size(); ++i)
            {
                const float o = r.processSample (loud[i]);
                if (i > 20000)
                {
                    inSq += (double) loud[i] * loud[i];
                    outSq += (double) o * o;
                }
            }
            const double ratioDb = 10.0 * std::log10 (outSq / inSq);
            logMessage ("  brain output vs input level: " + juce::String (ratioDb, 2) + " dB");
            expectWithinAbsoluteError (ratioDb, 0.0, 3.0);
        }
    }
};

static SplitMatchingTests splitMatchingTests;

class ProcessorBrainTests final : public juce::UnitTest
{
public:
    ProcessorBrainTests() : juce::UnitTest ("Processor brain path", "Cortex") {}

    void runTest() override
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cortex-proc-" + juce::String (juce::Random::getSystemRandom().nextInt()));
        dir.createDirectory();
        writeWav (dir.getChildFile ("brain.wav"), testSignal (96000, 5), brainRate);

        beginTest ("Brain feeds input B and its file list survives save/restore");

        CortexAudioProcessor proc;
        proc.prepareToPlay (brainRate, 512);
        proc.loadBrainFrom ({ dir.getFullPathName() });

        for (int i = 0; i < 600 && proc.getBrain() == nullptr; ++i)
            juce::Thread::sleep (50);
        expect (proc.getBrain() != nullptr, "brain built");

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        const auto input = testSignal (512 * 100, 6);
        bool finite = true;
        for (int block = 0; block < 100; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
                buf.copyFrom (ch, 0, input.data() + block * 512, 512);
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    finite &= std::isfinite (buf.getSample (ch, i));
        }
        expect (finite);
        expect (proc.getBrainStatus().playingBlock >= 0, "renderer is choosing blocks");

        juce::MemoryBlock state;
        proc.getStateInformation (state);
        CortexAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        expect (restored.getBrainFiles() == proc.getBrainFiles());

        for (int i = 0; i < 600 && restored.getBrain() == nullptr; ++i)
            juce::Thread::sleep (50);
        expect (restored.getBrain() != nullptr, "restored brain built");

        dir.deleteRecursively();
    }
};

static ProcessorBrainTests processorBrainTests;

class AlignmentTests final : public juce::UnitTest
{
public:
    AlignmentTests() : juce::UnitTest ("Brain alignment", "Cortex") {}

    // Lag (0..maxLag) at which `out` best matches `in`, by normalised cross-correlation.
    static int bestLag (const std::vector<float>& in, const std::vector<float>& out, int from, int length, int maxLag)
    {
        double best = -1.0;
        int bestLagFound = -1;
        for (int lag = 0; lag <= maxLag; ++lag)
        {
            double xy = 0.0, yy = 0.0;
            for (int i = from; i < from + length; ++i)
            {
                xy += (double) out[(size_t) i] * in[(size_t) (i - lag)];
                yy += (double) out[(size_t) i] * out[(size_t) i];
            }
            const double score = yy > 0.0 ? xy / std::sqrt (yy) : 0.0;
            if (score > best)
            {
                best = score;
                bestLagFound = lag;
            }
        }
        return bestLagFound;
    }

    void runTest() override
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cortex-align-" + juce::String (juce::Random::getSystemRandom().nextInt()));
        dir.createDirectory();
        const auto signal = testSignal ((size_t) brainRate * 4, 21);
        writeWav (dir.getChildFile ("brain.wav"), signal, brainRate);

        struct Case { const char* name; cortex::dsp::Source source; };
        for (auto c : { Case { "Input corner", cortex::dsp::Source::input }, Case { "Brain 1 corner", cortex::dsp::Source::brain1 } })
        {
            beginTest (juce::String ("Output lines up with reported latency: ") + c.name);

            CortexAudioProcessor proc;
            auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
            set ("blockSize", 1024.0f);
            set ("overlap", 0.5f);
            cortex::test::setPureSource (proc.apvts, c.source);
            set ("phaseMode", (float) c.source + 1.0f);
            set ("novelty", 0.0f);
            set ("dryWet", 1.0f);
            set ("limiter", 0.0f);
            set ("autoMute", 0.0f);
            proc.setNonRealtime (true);   // deterministic brain searches
            proc.prepareToPlay (brainRate, 512);
            proc.loadBrainFrom ({ dir.getFullPathName() });
            for (int i = 0; i < 600 && proc.getBrain() == nullptr; ++i)
                juce::Thread::sleep (50);
            expect (proc.getBrain() != nullptr);

            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            std::vector<float> out (signal.size());
            for (size_t start = 0; start + 512 <= signal.size(); start += 512)
            {
                for (int ch = 0; ch < 2; ++ch)
                    buf.copyFrom (ch, 0, signal.data() + start, 512);
                proc.processBlock (buf, midi);
                std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + 512, out.begin() + (std::ptrdiff_t) start);
            }

            // FFT 2048 + brain (block 1024 + search delay 1024).
            const int expected = 2048 + 1024 + 1024;
            expectEquals (proc.getAlignedLatencySamples(), expected);
            const int lag = bestLag (signal, out, (int) signal.size() / 2, 8192, 8000);
            logMessage ("  best lag " + juce::String (lag) + ", reported " + juce::String (proc.getAlignedLatencySamples()));
            expectEquals (lag, expected);
        }

        dir.deleteRecursively();
    }
};

static AlignmentTests alignmentTests;
