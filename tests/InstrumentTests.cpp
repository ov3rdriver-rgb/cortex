#include "PluginProcessor.h"
#include "Parameters.h"
#include "synth/MonoSynth.h"

#include <cmath>

using cortex::synth::MonoSynth;
using cortex::synth::Waveform;

namespace
{
constexpr double rate = 48000.0;

// Frequency from upward zero crossings (sine waveform).
double measureFrequency (const std::vector<float>& x, size_t from, size_t to)
{
    int crossings = 0;
    double first = -1.0, last = -1.0;
    for (size_t i = from + 1; i < to; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            const double t = (double) (i - 1) + x[i - 1] / (x[i - 1] - x[i]);
            if (first < 0.0)
                first = t;
            last = t;
            ++crossings;
        }
    return crossings > 1 ? (crossings - 1) * rate / (last - first) : 0.0;
}

std::vector<float> renderWith (MonoSynth& s, juce::MidiBuffer& midi, int numSamples)
{
    std::vector<float> out ((size_t) numSamples);
    s.render (midi, out.data(), numSamples);
    midi.clear();
    return out;
}
} // namespace

class MonoSynthTests final : public juce::UnitTest
{
public:
    MonoSynthTests() : juce::UnitTest ("Mono synth", "Cortex") {}

    void runTest() override
    {
        beginTest ("MIDI notes play at their pitch");
        for (int note : { 45, 57, 69, 81 })
        {
            MonoSynth s;
            s.prepare (rate);
            s.setWaveform (Waveform::sine);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
            const auto out = renderWith (s, midi, 48000);
            const double expected = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            expectWithinAbsoluteError (measureFrequency (out, 4800, out.size()), expected, expected * 0.002);
        }

        beginTest ("Legato: no retrigger, glide up and back, release at the end");
        {
            MonoSynth s;
            s.prepare (rate);
            s.setWaveform (Waveform::sine);
            s.setGlide (50.0f);
            s.setEnvelope (5.0f, 100.0f);
            juce::MidiBuffer midi;

            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            renderWith (s, midi, 9600);
            const float envBefore = s.getEnvelope();
            expectWithinAbsoluteError ((double) s.getCurrentFrequency(), 261.63, 0.5);

            // Second note while the first is held: glides, envelope stays up.
            midi.addEvent (juce::MidiMessage::noteOn (1, 67, 0.8f), 0);
            renderWith (s, midi, 480);   // 10 ms into a 50 ms glide
            const float midGlide = s.getCurrentFrequency();
            expectGreaterThan (midGlide, 270.0f);
            expectLessThan (midGlide, 385.0f);
            expectGreaterThan (s.getEnvelope(), envBefore * 0.99f, "legato must not retrigger the envelope");

            renderWith (s, midi, 9600);
            expectWithinAbsoluteError ((double) s.getCurrentFrequency(), 392.0, 1.0);

            // Releasing the top note returns to the one still held.
            midi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
            renderWith (s, midi, 9600);
            expectWithinAbsoluteError ((double) s.getCurrentFrequency(), 261.63, 1.0);
            expect (s.isNoteHeld());

            // Last note off: the envelope releases to silence.
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            renderWith (s, midi, 14400);
            expect (! s.isNoteHeld());
            expectLessThan (s.getEnvelope(), 0.01f);
        }

        beginTest ("A new phrase starts on pitch (no glide from the last one)");
        {
            MonoSynth s;
            s.prepare (rate);
            s.setGlide (500.0f);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.8f), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 48), 100);
            midi.addEvent (juce::MidiMessage::noteOn (1, 72, 0.8f), 200);
            renderWith (s, midi, 210);
            expectWithinAbsoluteError ((double) s.getCurrentFrequency(), 523.25, 0.5);
        }

        beginTest ("All waveforms stay bounded");
        for (auto w : { Waveform::saw, Waveform::square, Waveform::triangle, Waveform::sine })
        {
            MonoSynth s;
            s.prepare (rate);
            s.setWaveform (w);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 100, 1.0f), 0);
            const auto out = renderWith (s, midi, 4800);
            float peak = 0.0f;
            for (auto v : out)
                peak = std::max (peak, std::abs (v));
            expectGreaterThan (peak, 0.1f);
            expectLessThan (peak, 0.75f);
        }
    }
};

static MonoSynthTests monoSynthTests;

class InstrumentProcessorTests final : public juce::UnitTest
{
public:
    InstrumentProcessorTests() : juce::UnitTest ("Instrument processor", "Cortex") {}

    void runTest() override
    {
        beginTest ("Instrument: MIDI in, no audio inputs, its own parameters");
        {
            CortexAudioProcessor inst (true), fx (false);
            expect (inst.isInstrument() && inst.acceptsMidi());
            expectEquals (inst.getBusCount (true), 0);
            expect (inst.apvts.getParameter (cortex::ParamIDs::oscWave) != nullptr);
            expect (! fx.acceptsMidi());
            expect (fx.apvts.getParameter (cortex::ParamIDs::oscWave) == nullptr);
        }

        beginTest ("Without a brain the oscillator comes through at the note's pitch");
        {
            CortexAudioProcessor proc (true);
            auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
            set (cortex::ParamIDs::oscWave, 3.0f);   // sine, for a clean pitch measurement
            set (cortex::ParamIDs::limiter, 0.0f);
            proc.prepareToPlay (rate, 512);

            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            std::vector<float> out;
            for (int block = 0; block < 120; ++block)
            {
                buf.clear();
                midi.clear();
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 69, 0.9f), 0);
                proc.processBlock (buf, midi);
                out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + 512);
            }
            expectWithinAbsoluteError (measureFrequency (out, 20000, out.size()), 440.0, 1.0);
        }

        beginTest ("With a brain the instrument renders brain timbre, finite");
        {
            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cortex-inst-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            dir.createDirectory();
            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (dir.getChildFile ("b.wav")), rate, 1, 24, {}, 0));
                juce::AudioBuffer<float> a (1, 96000);
                juce::Random rng (3);
                for (int i = 0; i < a.getNumSamples(); ++i)
                    a.setSample (0, i, 0.3f * (float) std::sin (i * 0.05) * (0.5f + rng.nextFloat()));
                w->writeFromAudioSampleBuffer (a, 0, a.getNumSamples());
            }

            CortexAudioProcessor proc (true);
            proc.setNonRealtime (true);
            proc.prepareToPlay (rate, 512);
            proc.loadBrainFrom ({ dir.getFullPathName() });
            for (int i = 0; i < 600 && proc.getBrain() == nullptr; ++i)
                juce::Thread::sleep (50);
            expect (proc.getBrain() != nullptr);

            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            double energy = 0.0;
            bool finite = true;
            for (int block = 0; block < 200; ++block)
            {
                buf.clear();
                midi.clear();
                if (block % 40 == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 57 + (block / 40) * 2, 0.9f), 0);
                proc.processBlock (buf, midi);
                for (int i = 0; i < 512; ++i)
                {
                    finite &= std::isfinite (buf.getSample (0, i));
                    energy += buf.getSample (0, i) * buf.getSample (0, i);
                }
            }
            expect (finite);
            expectGreaterThan (energy, 1.0);
            expect (proc.getBrainStatus().playingBlock >= 0);

            dir.deleteRecursively();
        }
    }
};

static InstrumentProcessorTests instrumentProcessorTests;

class LowLatencyTests final : public juce::UnitTest
{
public:
    LowLatencyTests() : juce::UnitTest ("Low latency", "Cortex") {}

    static juce::File brainFolder()
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cortex-ll-" + juce::String (juce::Random::getSystemRandom().nextInt()));
        dir.createDirectory();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (dir.getChildFile ("b.wav")), rate, 1, 24, {}, 0));
        juce::AudioBuffer<float> a (1, 96000);
        juce::Random rng (12);
        for (int i = 0; i < a.getNumSamples(); ++i)
            a.setSample (0, i, 0.3f * (float) std::sin (i * 0.03) + 0.1f * (rng.nextFloat() - 0.5f));
        w->writeFromAudioSampleBuffer (a, 0, a.getNumSamples());
        return dir;
    }

    static void loadBrain (CortexAudioProcessor& proc, const juce::File& dir)
    {
        proc.loadBrainFrom ({ dir.getFullPathName() });
        for (int i = 0; i < 600 && proc.getBrain() == nullptr; ++i)
            juce::Thread::sleep (50);
    }

    // Samples from note-on until the instrument's output first passes `threshold`.
    int noteToSound (bool lowLatency, const juce::File& dir)
    {
        CortexAudioProcessor proc (true);
        auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        set (cortex::ParamIDs::lowLatency, lowLatency ? 1.0f : 0.0f);
        set (cortex::ParamIDs::limiter, 0.0f);
        proc.setNonRealtime (true);
        proc.prepareToPlay (rate, 256);
        loadBrain (proc, dir);
        expect (proc.getBrain() != nullptr);

        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        constexpr int noteBlock = 40;   // let engines settle in silence first
        for (int block = 0; block < 200; ++block)
        {
            buf.clear();
            midi.clear();
            if (block == noteBlock)
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, 1.0f), 0);
            proc.processBlock (buf, midi);
            if (block >= noteBlock)
                for (int i = 0; i < 256; ++i)
                    if (std::abs (buf.getSample (0, i)) > 0.02f)
                        return (block - noteBlock) * 256 + i;
        }
        return -1;
    }

    void runTest() override
    {
        const auto dir = brainFolder();

        beginTest ("Low latency reports the 512-sample FFT and nothing else, with a brain loaded");
        {
            CortexAudioProcessor proc (false);
            auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
            set (cortex::ParamIDs::lowLatency, 1.0f);
            set (cortex::ParamIDs::morphX, 0.0f);
            set (cortex::ParamIDs::limiter, 0.0f);
            set (cortex::ParamIDs::autoMute, 0.0f);
            proc.setNonRealtime (true);
            proc.prepareToPlay (rate, 512);
            loadBrain (proc, dir);

            // Impulse through the pure-A path.
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            int peakAt = -1;
            float peak = 0.0f;
            for (int block = 0; block < 40; ++block)
            {
                buf.clear();
                if (block == 20)
                    for (int ch = 0; ch < 2; ++ch)
                        buf.setSample (ch, 0, 1.0f);
                proc.processBlock (buf, midi);
                for (int i = 0; i < 512; ++i)
                    if (block >= 20 && std::abs (buf.getSample (0, i)) > peak)
                    {
                        peak = std::abs (buf.getSample (0, i));
                        peakAt = (block - 20) * 512 + i;
                    }
            }
            expectEquals (proc.getAlignedLatencySamples(), 512);
            expectEquals (peakAt, 512);
        }

        beginTest ("Instrument: note-on to sound is far shorter in low latency");
        {
            const int normal = noteToSound (false, dir);
            const int low = noteToSound (true, dir);
            logMessage ("  note-on to sound: normal " + juce::String (normal) + " samples ("
                        + juce::String (normal / rate * 1000.0, 1) + " ms), low latency " + juce::String (low)
                        + " samples (" + juce::String (low / rate * 1000.0, 1) + " ms)");
            expectGreaterThan (low, 0);
            expectLessThan (low, 640);           // ~ the 512-sample FFT
            expectGreaterThan (normal, low * 4);
        }

        dir.deleteRecursively();
    }
};

static LowLatencyTests lowLatencyTests;
