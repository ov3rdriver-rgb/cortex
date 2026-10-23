// Realtime-safety audit (BRIEF.md §2, Phase 3): no heap allocation or free on
// the audio thread. Global operator new/delete are replaced for the test
// binary; while a thread has tracking switched on, every call is counted.

#include "PluginProcessor.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace
{
thread_local bool trackingThisThread = false;
std::atomic<int> trackedAllocations { 0 }, trackedFrees { 0 };

void* allocate (std::size_t size)
{
    if (trackingThisThread)
        ++trackedAllocations;
    if (void* p = std::malloc (size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}

void* allocateAligned (std::size_t size, std::align_val_t align)
{
    if (trackingThisThread)
        ++trackedAllocations;
   #if defined (_MSC_VER)
    if (void* p = _aligned_malloc (size == 0 ? 1 : size, (std::size_t) align))
        return p;
   #else
    const auto a = (std::size_t) align;
    if (void* p = std::aligned_alloc (a, ((size == 0 ? 1 : size) + a - 1) / a * a))
        return p;
   #endif
    throw std::bad_alloc();
}

void release (void* p) noexcept
{
    if (p != nullptr && trackingThisThread)
        ++trackedFrees;
    std::free (p);
}

void releaseAligned (void* p) noexcept
{
    if (p != nullptr && trackingThisThread)
        ++trackedFrees;
   #if defined (_MSC_VER)
    _aligned_free (p);
   #else
    std::free (p);
   #endif
}

struct ScopedTracking
{
    ScopedTracking()  { trackingThisThread = true; }
    ~ScopedTracking() { trackingThisThread = false; }
};
} // namespace

void* operator new (std::size_t n) { return allocate (n); }
void* operator new[] (std::size_t n) { return allocate (n); }
void* operator new (std::size_t n, const std::nothrow_t&) noexcept { try { return allocate (n); } catch (...) { return nullptr; } }
void* operator new[] (std::size_t n, const std::nothrow_t&) noexcept { try { return allocate (n); } catch (...) { return nullptr; } }
void* operator new (std::size_t n, std::align_val_t a) { return allocateAligned (n, a); }
void* operator new[] (std::size_t n, std::align_val_t a) { return allocateAligned (n, a); }
void operator delete (void* p) noexcept { release (p); }
void operator delete[] (void* p) noexcept { release (p); }
void operator delete (void* p, std::size_t) noexcept { release (p); }
void operator delete[] (void* p, std::size_t) noexcept { release (p); }
void operator delete (void* p, std::align_val_t) noexcept { releaseAligned (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { releaseAligned (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { releaseAligned (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { releaseAligned (p); }

class RealtimeSafetyTests final : public juce::UnitTest
{
public:
    RealtimeSafetyTests() : juce::UnitTest ("Realtime safety", "Cortex") {}

    void runTest() override
    {
        beginTest ("The tracker sees allocations (sanity)");
        {
            trackedAllocations = 0;
            {
                ScopedTracking t;
                auto* v = new std::vector<float> (16);
                delete v;
            }
            expectGreaterThan (trackedAllocations.load(), 0);
        }

        beginTest ("processBlock never allocates or frees: brain, transplant, FFT/brain switches, all search modes");

        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cortex-rt-" + juce::String (juce::Random::getSystemRandom().nextInt()));
        dir.createDirectory();
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (dir.getChildFile ("b.wav")), 48000.0, 1, 24, {}, 0));
            juce::AudioBuffer<float> audio (1, 96000);
            juce::Random rng (9);
            double phase = 0.0;
            for (int i = 0; i < audio.getNumSamples(); ++i)
            {
                phase += (150.0 + 100.0 * std::sin (i * 0.0001)) / 48000.0;
                audio.setSample (0, i, (float) (0.3 * (2.0 * (phase - std::floor (phase)) - 1.0) + 0.05 * (rng.nextFloat() - 0.5)));
            }
            w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
        }

        CortexAudioProcessor proc;
        auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        set ("envTransplant", 1.0f);
        set ("gateOn", 1.0f);
        proc.prepareToPlay (48000.0, 512);
        proc.loadBrainFrom ({ dir.getFullPathName() });
        for (int i = 0; i < 600 && proc.getBrain() == nullptr; ++i)
            juce::Thread::sleep (50);
        expect (proc.getBrain() != nullptr);

        juce::AudioBuffer<float> buf (4, 512);
        juce::MidiBuffer midi;
        juce::Random rng (10);
        double phase = 0.0;
        trackedAllocations = 0;
        trackedFrees = 0;

        for (int block = 0; block < 600; ++block)
        {
            // Parameter changes happen on the "host" thread between blocks.
            if (block % 100 == 50)  set ("fftSize", (float) ((block / 100) % 4));
            if (block % 150 == 75)  set ("inputSource", (block / 150) % 2 == 0 ? 0.0f : 1.0f);
            if (block % 90 == 45)   set ("envX", (float) (block % 7) / 7.0f);
            if (block % 120 == 60)  set ("searchMode", (float) ((block / 120) % 4));
            if (block == 300)       set ("phaseMode", 2.0f);

            for (int i = 0; i < 512; ++i)
            {
                phase += 220.0 / 48000.0;
                const float s = (block % 200 < 180) ? (float) (0.4 * std::sin (juce::MathConstants<double>::twoPi * phase)) : 0.0f;
                for (int ch = 0; ch < 4; ++ch)
                    buf.setSample (ch, i, ch < 2 ? s : 0.2f * (rng.nextFloat() - 0.5f));
            }

            {
                ScopedTracking t;
                proc.processBlock (buf, midi);
            }

            // Let real time pass now and then so the search worker answers.
            if (block % 20 == 0)
                juce::Thread::sleep (2);
        }

        logMessage ("  audio-thread allocations: " + juce::String (trackedAllocations.load())
                    + ", frees: " + juce::String (trackedFrees.load()));
        expectEquals (trackedAllocations.load(), 0);
        expectEquals (trackedFrees.load(), 0);

        beginTest ("Instrument processBlock never allocates or frees (MIDI, legato, brain)");
        {
            CortexAudioProcessor inst (true);
            auto setI = [&] (const char* id, float v) { auto* p = inst.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
            setI ("envTransplant", 0.7f);
            inst.prepareToPlay (48000.0, 512);
            inst.loadBrainFrom ({ dir.getFullPathName() });
            for (int i = 0; i < 600 && inst.getBrain() == nullptr; ++i)
                juce::Thread::sleep (50);

            juce::AudioBuffer<float> ibuf (2, 512);
            trackedAllocations = 0;
            trackedFrees = 0;
            for (int block = 0; block < 300; ++block)
            {
                // MIDI is filled in by the "host" outside the tracked call.
                midi.clear();
                if (block % 30 == 0)  midi.addEvent (juce::MidiMessage::noteOn (1, 48 + (block / 30) % 12, 0.8f), 17);
                if (block % 30 == 10) midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);       // legato
                if (block % 30 == 20) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 3);
                if (block % 30 == 25) midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                if (block % 7 == 0)   midi.addEvent (juce::MidiMessage::pitchWheel (1, 4096 + block * 20), 50);
                ibuf.clear();

                {
                    ScopedTracking t;
                    inst.processBlock (ibuf, midi);
                }
                if (block % 20 == 0)
                    juce::Thread::sleep (2);
            }

            logMessage ("  instrument audio-thread allocations: " + juce::String (trackedAllocations.load())
                        + ", frees: " + juce::String (trackedFrees.load()));
            expectEquals (trackedAllocations.load(), 0);
            expectEquals (trackedFrees.load(), 0);
        }

        dir.deleteRecursively();
    }
};

static RealtimeSafetyTests realtimeSafetyTests;
