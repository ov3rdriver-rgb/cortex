#include "BrainCache.h"

namespace cortex::BrainCache
{
namespace
{
    constexpr int magic = 0x42585443;   // "CTXB"
    // Bump whenever the analysis or this layout changes; old caches are then rebuilt.
    constexpr int formatVersion = 1;

    template <typename T>
    void writePod (juce::OutputStream& out, const T& value)
    {
        static_assert (std::is_trivially_copyable_v<T>);
        out.write (&value, sizeof (T));
    }

    template <typename T>
    bool readPod (juce::InputStream& in, T& value)
    {
        static_assert (std::is_trivially_copyable_v<T>);
        return in.read (&value, (int) sizeof (T)) == (int) sizeof (T);
    }
}

juce::File defaultDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Cortex").getChildFile ("BrainCache");
}

juce::String keyFor (const juce::Array<juce::File>& files, const brain::BrainSettings& s)
{
    juce::String desc;
    desc << "v" << formatVersion << "|" << s.blockSize << "|" << s.overlap << "|" << s.sampleRate;
    for (const auto& f : files)
        desc << "|" << f.getFullPathName() << ":" << f.getSize() << ":" << f.getLastModificationTime().toMilliseconds();

    return juce::String::toHexString (desc.hashCode64()) + "-" + juce::String::toHexString (desc.length());
}

juce::File fileFor (const juce::File& directory, const juce::String& key)
{
    return directory.getChildFile (key + ".cortexbrain");
}

bool save (const brain::Brain& b, const juce::File& file)
{
    file.getParentDirectory().createDirectory();
    juce::TemporaryFile temp (file);

    {
        juce::FileOutputStream out (temp.getFile());
        if (! out.openedOk())
            return false;

        out.writeInt (magic);
        out.writeInt (formatVersion);
        out.writeInt (b.settings.blockSize);
        out.writeFloat (b.settings.overlap);
        out.writeDouble (b.settings.sampleRate);

        out.writeInt ((int) b.sounds.size());
        for (const auto& s : b.sounds)
        {
            out.writeString (s.path);
            out.writeInt ((int) s.firstBlock);
            out.writeInt ((int) s.numBlocks);
            out.writeBool (s.enabled);
        }

        out.writeInt ((int) b.blocks.size());
        for (const auto& blk : b.blocks)
        {
            out.write (blk.pcm.data(), blk.pcm.size() * sizeof (float));
            writePod (out, blk.features);
            writePod (out, blk.normalised);
            out.writeInt ((int) blk.synapses.size());
            out.write (blk.synapses.data(), blk.synapses.size() * sizeof (std::uint32_t));
        }

        out.flush();
        if (out.getStatus().failed())
            return false;
    }

    return temp.overwriteTargetFileWithTemporary();
}

std::shared_ptr<brain::Brain> load (const juce::File& file)
{
    juce::MemoryBlock data;
    if (! file.loadFileAsData (data))
        return nullptr;
    file.setLastModificationTime (juce::Time::getCurrentTime());   // LRU for prune()

    juce::MemoryInputStream in (data, false);
    if (in.readInt() != magic || in.readInt() != formatVersion)
        return nullptr;

    auto b = std::make_shared<brain::Brain>();
    b->settings.blockSize = in.readInt();
    b->settings.overlap = in.readFloat();
    b->settings.sampleRate = in.readDouble();
    if (b->settings.blockSize < brain::minBlockSize || b->settings.blockSize > brain::maxBlockSize)
        return nullptr;

    const int numSounds = in.readInt();
    if (numSounds < 0)
        return nullptr;
    for (int i = 0; i < numSounds; ++i)
    {
        brain::SoundInfo s;
        s.path = in.readString();
        s.firstBlock = (std::uint32_t) in.readInt();
        s.numBlocks = (std::uint32_t) in.readInt();
        s.enabled = in.readBool();
        b->sounds.push_back (s);
    }

    const int numBlocks = in.readInt();
    const auto pcmBytes = (int) (b->settings.blockSize * sizeof (float));
    if (numBlocks < 0 || (juce::int64) numBlocks * pcmBytes > in.getNumBytesRemaining())
        return nullptr;

    b->blocks.resize ((size_t) numBlocks);
    for (auto& blk : b->blocks)
    {
        blk.pcm.resize ((size_t) b->settings.blockSize);
        if (in.read (blk.pcm.data(), pcmBytes) != pcmBytes
            || ! readPod (in, blk.features) || ! readPod (in, blk.normalised))
            return nullptr;

        const int numSyn = in.readInt();
        if (numSyn < 0 || numSyn > numBlocks)
            return nullptr;
        blk.synapses.resize ((size_t) numSyn);
        const auto synBytes = (int) (numSyn * sizeof (std::uint32_t));
        if (in.read (blk.synapses.data(), synBytes) != synBytes)
            return nullptr;
        for (auto s : blk.synapses)
            if (s >= (std::uint32_t) numBlocks)
                return nullptr;
    }

    for (const auto& s : b->sounds)
        if ((juce::int64) s.firstBlock + s.numBlocks > numBlocks)
            return nullptr;

    b->finalise();

    return b;
}
} // namespace cortex::BrainCache

namespace cortex::BrainCache
{
void prune (const juce::File& directory, juce::int64 maxBytes, const juce::File& keep)
{
    auto files = directory.findChildFiles (juce::File::findFiles, false, "*.cortexbrain");

    struct OldestFirst
    {
        static int compareElements (const juce::File& a, const juce::File& b)
        {
            const auto ta = a.getLastModificationTime(), tb = b.getLastModificationTime();
            return ta < tb ? -1 : (tb < ta ? 1 : 0);
        }
    } oldestFirst;
    files.sort (oldestFirst);

    juce::int64 total = 0;
    for (const auto& f : files)
        total += f.getSize();

    for (const auto& f : files)
    {
        if (total <= maxBytes)
            break;
        if (f == keep)
            continue;
        total -= f.getSize();
        f.deleteFile();
    }
}
} // namespace cortex::BrainCache
