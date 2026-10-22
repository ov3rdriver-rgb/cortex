#include "PluginProcessor.h"
#include "brain/BrainBuilder.h"
#include "brain/Renderer.h"
#include "state/BrainCache.h"

#include <cmath>

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
            BlockFeatures f1, n1, f2, n2, f3, n3;
            an.analyse (audio.data() + 5000, nullptr, f1, n1);
            an.analyse (audio.data() + 5000, nullptr, f2, n2);
            an.analyse (audio.data() + 20000, nullptr, f3, n3);
            SearchParams p;
            expectEquals (featureDistance (f1, f2, p), 0.0);
            expectGreaterThan (featureDistance (f1, f3, p), 0.0);
        }

        for (float overlap : { 0.25f, 0.5f })
        {
            beginTest ("A brain built from the target reconstructs it (overlap " + juce::String (overlap) + ")");

            const int bs = 1024;
            auto brain = build (audio, bs, overlap);
            Renderer r (brain);
            r.setSearch (SearchParams {}, SearchMode::basic);

            std::vector<float> out (audio.size());
            int wrongBlocks = 0, expectedBlock = 0;
            for (size_t i = 0; i < audio.size(); ++i)
            {
                out[i] = r.processSample (audio[i]);
                const bool blockCompleted = i + 1 >= (size_t) bs && (i + 1 - (size_t) bs) % (size_t) brain->settings.hop() == 0;
                if (blockCompleted)
                {
                    if (r.getLastChosenBlock() != expectedBlock)
                        ++wrongBlocks;
                    ++expectedBlock;
                }
            }
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
                    const double d = featureDistance (brain->blocks[i].features, brain->blocks[s].features, p);
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
                expectEquals (featureDistance (loaded->blocks[7].features, brain->blocks[7].features, SearchParams {}), 0.0);
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
        auto* bSource = proc.apvts.getParameter ("bSource");
        bSource->setValueNotifyingHost (1.0f);
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
