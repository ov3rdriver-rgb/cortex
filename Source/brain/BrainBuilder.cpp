// Cortex: portions ported from Samplebrain (brain/src/brain.cpp: load_sound, init, chop_and_add),
// Copyright (C) 2022 Then Try This. GPL-2.0-or-later, distributed here under AGPL-3.0.

#include "BrainBuilder.h"
#include "state/BrainCache.h"

namespace cortex::brain
{
juce::Array<juce::File> collectAudioFiles (const juce::StringArray& filesOrFolders, juce::AudioFormatManager& formats)
{
    const auto wildcard = formats.getWildcardForAllFormats();
    juce::Array<juce::File> result;

    for (const auto& path : filesOrFolders)
    {
        const juce::File f (path);
        if (f.isDirectory())
        {
            for (const auto& entry : juce::RangedDirectoryIterator (f, true, wildcard, juce::File::findFiles))
                result.addIfNotAlreadyThere (entry.getFile());
        }
        else if (f.existsAsFile() && formats.findFormatForFileExtension (f.getFileExtension()) != nullptr)
        {
            result.addIfNotAlreadyThere (f);
        }
    }

    struct ByPath
    {
        static int compareElements (const juce::File& a, const juce::File& b)
        {
            return a.getFullPathName().compareNatural (b.getFullPathName());
        }
    } byPath;
    result.sort (byPath);
    return result;
}

bool decodeToMono (juce::AudioFormatManager& formats, const juce::File& file, double targetRate, BrainSource& out)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0)
        return false;

    const auto length = (int) juce::jmin<juce::int64> (reader->lengthInSamples, std::numeric_limits<int>::max() / 2);
    const int channels = (int) reader->numChannels;
    juce::AudioBuffer<float> buf (channels, length);
    if (! reader->read (&buf, 0, length, 0, true, true))
        return false;

    // Samplebrain's MIX stereo mode: sum the channels.
    std::vector<float> mono ((size_t) length, 0.0f);
    for (int ch = 0; ch < channels; ++ch)
        juce::FloatVectorOperations::add (mono.data(), buf.getReadPointer (ch), length);

    out.path = file.getFullPathName();

    if (std::abs (reader->sampleRate - targetRate) < 0.5)
    {
        out.mono = std::move (mono);
        return true;
    }

    const double ratio = reader->sampleRate / targetRate;
    const auto outLength = (int) std::floor ((double) length / ratio);
    out.mono.assign ((size_t) juce::jmax (0, outLength), 0.0f);

    juce::LagrangeInterpolator interp;
    interp.process (ratio, mono.data(), out.mono.data(), outLength, length, 0);
    return true;
}

std::shared_ptr<Brain> buildBrain (const std::vector<BrainSource>& sources, const BrainSettings& settings,
                                   const SearchParams& params, const ProgressFn& progress, const ShouldExitFn& shouldExit)
{
    auto brain = std::make_shared<Brain>();
    brain->settings = settings;
    brain->settings.blockSize = juce::jlimit (minBlockSize, maxBlockSize, settings.blockSize);
    brain->settings.overlap = juce::jlimit (0.0f, 0.75f, settings.overlap);

    const int bs = brain->settings.blockSize;
    const int hop = brain->settings.hop();

    std::size_t totalBlocks = 0;
    for (const auto& s : sources)
        if ((int) s.mono.size() >= bs)
            totalBlocks += (s.mono.size() - (size_t) bs) / (size_t) hop + 1;
    brain->blocks.reserve (totalBlocks);

    BlockAnalyser analyser;
    analyser.prepare (bs, hop, brain->settings.sampleRate);

    for (const auto& src : sources)
    {
        SoundInfo info;
        info.path = src.path;
        info.firstBlock = brain->numBlocks();

        for (size_t pos = 0; pos + (size_t) bs <= src.mono.size(); pos += (size_t) hop)
        {
            Block b;
            b.pcm.resize ((size_t) bs);
            analyser.analyse (src.mono.data() + pos, b.pcm.data(), b.features, b.normalised);
            brain->blocks.push_back (std::move (b));

            if (brain->blocks.size() % 64 == 0)
            {
                if (shouldExit && shouldExit())
                    return nullptr;
                if (progress && totalBlocks > 0)
                    progress (0.5f * (float) brain->blocks.size() / (float) totalBlocks, "Analysing blocks");
            }
        }

        info.numBlocks = brain->numBlocks() - info.firstBlock;
        brain->sounds.push_back (info);
    }

    auto synapseProgress = [&] (float p, const juce::String& s) { if (progress) progress (0.5f + 0.5f * p, s); };
    if (! buildSynapses (*brain, params, synapseProgress, shouldExit))
        return nullptr;

    return brain;
}

BrainBuilder::BrainBuilder (Callback cb) : juce::Thread ("Cortex brain builder"), callback (std::move (cb)) {}

BrainBuilder::~BrainBuilder()
{
    cancel();
}

void BrainBuilder::cancel()
{
    stopThread (10000);
}

void BrainBuilder::start (Request r)
{
    cancel();
    request = std::move (r);
    setStatus (0.0f, "Starting");
    startThread (juce::Thread::Priority::low);
}

juce::String BrainBuilder::getStatus() const
{
    std::lock_guard<std::mutex> l (statusLock);
    return status;
}

void BrainBuilder::setStatus (float p, const juce::String& s)
{
    progress = p;
    std::lock_guard<std::mutex> l (statusLock);
    status = s;
}

void BrainBuilder::run()
{
    const auto key = BrainCache::keyFor (request.files, request.settings);
    const auto cacheFile = BrainCache::fileFor (request.cacheDirectory, key);

    if (cacheFile.existsAsFile())
    {
        setStatus (0.0f, "Loading cached brain");
        if (auto cached = BrainCache::load (cacheFile))
        {
            setStatus (1.0f, "Loaded from cache");
            callback (std::move (cached), "Loaded from cache");
            return;
        }
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::vector<BrainSource> sources;
    juce::StringArray failed;
    for (int i = 0; i < request.files.size(); ++i)
    {
        if (threadShouldExit())
            return;
        setStatus (0.0f, "Decoding " + request.files[i].getFileName());

        BrainSource src;
        if (decodeToMono (formats, request.files[i], request.settings.sampleRate, src))
            sources.push_back (std::move (src));
        else
            failed.add (request.files[i].getFileName());
    }

    auto brain = buildBrain (sources, request.settings, SearchParams {},
                             [this] (float p, const juce::String& s) { setStatus (p, s); },
                             [this] { return threadShouldExit(); });
    if (brain == nullptr)
        return;

    setStatus (1.0f, "Saving cache");
    BrainCache::save (*brain, cacheFile);

    juce::String message;
    message << (int) brain->numBlocks() << " blocks from " << (int) brain->sounds.size() << " sounds";
    if (! failed.isEmpty())
        message << " (couldn't read: " << failed.joinIntoString (", ") << ")";

    setStatus (1.0f, message);
    callback (std::move (brain), message);
}
} // namespace cortex::brain
