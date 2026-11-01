#include "PluginProcessor.h"
#include "Parameters.h"
#include "state/Settings.h"

#include <juce_audio_formats/juce_audio_formats.h>

class ProcessorScaffoldTests final : public juce::UnitTest
{
public:
    ProcessorScaffoldTests() : juce::UnitTest ("Processor scaffold", "Cortex") {}

    void runTest() override
    {
        CortexAudioProcessor proc;

        beginTest ("All brief parameters exist");
        for (auto* id : { "envX", "envY", "detailX", "detailY", "linkTriangles", "cutoffIn", "cutoffB1", "cutoffB2",
                          "formantIn", "formantB1", "formantB2", "phaseMode", "fftSize", "inputSource",
                          "matchTimbre", "matchPitch", "envTransplant", "blockSizeB1", "overlapB1", "blockSizeB2", "overlapB2",
                          "novelty", "boredom", "stickyness", "searchMode",
                          "dryWet", "outGain", "limiter", "gateOn", "gateThresh", "autoMute", "autoMuteMs", "captureSeconds" })
            expect (proc.apvts.getParameter (id) != nullptr, juce::String ("missing ") + id);

        beginTest ("Cutoff defaults to 25 bins");
        expectEquals ((int) proc.apvts.getRawParameterValue (cortex::ParamIDs::cutoffIn)->load(), 25);

        beginTest ("Sidechain bus present");
        expectEquals (proc.getBusCount (true), 2);

        beginTest ("Latency tracks FFT size");
        expectEquals (proc.getLatencySamples(), 2048);
        auto* fft = proc.apvts.getParameter (cortex::ParamIDs::fftSize);
        for (int i = 0; i < 4; ++i)
        {
            fft->setValueNotifyingHost (fft->convertTo0to1 ((float) i));
            expectEquals (proc.getLatencySamples(), cortex::fftSizes[i]);
        }

        beginTest ("State round-trips");
        auto* x = proc.apvts.getParameter (cortex::ParamIDs::envX);
        x->setValueNotifyingHost (0.7f);
        juce::MemoryBlock state;
        proc.getStateInformation (state);

        CortexAudioProcessor other;
        other.setStateInformation (state.getData(), (int) state.getSize());
        expectWithinAbsoluteError (other.apvts.getParameter (cortex::ParamIDs::envX)->getValue(), 0.7f, 1e-4f);
        expectEquals (other.getLatencySamples(), 4096);

        beginTest ("The theme is a user setting, not part of the state");
        {
            const int chosen = cortex::settings::loadTheme();
            auto* theme = proc.apvts.getParameter (cortex::ParamIDs::theme);
            theme->setValueNotifyingHost (theme->convertTo0to1 ((float) (1 - chosen)));
            juce::MemoryBlock saved;
            proc.getStateInformation (saved);
            expect (! juce::String::fromUTF8 ((const char*) saved.getData(), (int) saved.getSize()).contains ("\"theme\""));

            // A state that does carry a theme (older versions saved one) is ignored.
            auto state = proc.apvts.copyState();
            state.getChildWithProperty ("id", cortex::ParamIDs::theme).setProperty ("value", 1 - chosen, nullptr);
            juce::MemoryBlock withTheme;
            proc.copyXmlToBinary (*state.createXml(), withTheme);
            CortexAudioProcessor third;
            third.setStateInformation (withTheme.getData(), (int) withTheme.getSize());
            auto* restored = third.apvts.getParameter (cortex::ParamIDs::theme);
            expectEquals (juce::roundToInt (restored->convertFrom0to1 (restored->getValue())), chosen);
            theme->setValueNotifyingHost (theme->convertTo0to1 ((float) chosen));
        }

        beginTest ("A session with one shared block size / overlap sets both brains");
        {
            auto state = proc.apvts.copyState();
            for (auto* id : { "blockSizeB1", "overlapB1", "blockSizeB2", "overlapB2" })
                state.removeChild (state.getChildWithProperty ("id", id), nullptr);
            state.appendChild (juce::ValueTree ("PARAM", { { "id", "blockSize" }, { "value", 2048 } }), nullptr);
            state.appendChild (juce::ValueTree ("PARAM", { { "id", "overlap" }, { "value", 0.5 } }), nullptr);
            juce::MemoryBlock legacy;
            proc.copyXmlToBinary (*state.createXml(), legacy);

            CortexAudioProcessor old;
            old.setStateInformation (legacy.getData(), (int) legacy.getSize());
            for (auto* id : { "blockSizeB1", "blockSizeB2" })
                expectEquals ((int) old.apvts.getRawParameterValue (id)->load(), 2048, id);
            for (auto* id : { "overlapB1", "overlapB2" })
                expectWithinAbsoluteError (old.apvts.getRawParameterValue (id)->load(), 0.5f, 1e-4f, id);
            expect (! old.apvts.state.getChildWithProperty ("id", "blockSize").isValid(), "the legacy entry is dropped");
        }

        beginTest ("Capture saves the most recent output");
        {
            CortexAudioProcessor fx;
            auto set = [&] (const char* id, float v) { auto* p = fx.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
            set ("dryWet", 0.0f);
            set ("limiter", 0.0f);
            set ("autoMute", 0.0f);
            set ("captureSeconds", 5.0f);
            const double rate = 48000.0;
            fx.prepareToPlay (rate, 480);

            juce::String error;
            juce::File none;
            expectEquals (fx.saveCapture (none, error), 0.0);
            expect (error.isNotEmpty(), "nothing to capture before any audio");

            // 7 s of noise through; keep what came out so the last 5 s can be compared.
            juce::Random random (7);
            std::vector<float> played;
            juce::AudioBuffer<float> buf (2, 480);
            juce::MidiBuffer midi;
            for (int block = 0; block < 700; ++block)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 480; ++i)
                        buf.setSample (ch, i, ch == 0 ? random.nextFloat() * 0.5f - 0.25f : 0.0f);
                fx.processBlock (buf, midi);
                played.insert (played.end(), buf.getReadPointer (0), buf.getReadPointer (0) + 480);
            }

            const auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cortex-capture-test.wav");
            const double seconds = fx.saveCapture (wav, error);
            expectWithinAbsoluteError (seconds, 5.0, 1e-6);
            expect (wav.existsAsFile(), error);

            juce::WavAudioFormat format;
            std::unique_ptr<juce::AudioFormatReader> reader (format.createReaderFor (wav.createInputStream().release(), true));
            expect (reader != nullptr);
            if (reader != nullptr)
            {
                expectEquals ((int) reader->lengthInSamples, (int) (5 * rate));
                expectEquals ((int) reader->numChannels, 2);
                expectEquals ((int) reader->sampleRate, (int) rate);
                juce::AudioBuffer<float> back (2, (int) reader->lengthInSamples);
                reader->read (&back, 0, back.getNumSamples(), 0, true, true);
                const auto offset = played.size() - (size_t) back.getNumSamples();
                float worst = 0.0f;
                for (int i = 0; i < back.getNumSamples(); ++i)
                    worst = juce::jmax (worst, std::abs (back.getSample (0, i) - played[offset + (size_t) i]));
                expect (worst < 2e-4f, "24-bit WAV matches the output to within quantisation (worst " + juce::String (worst) + ")");
            }
            wav.deleteFile();
        }

        beginTest ("Silence in, silence out, no NaN");
        proc.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (4, 512);
        buf.clear();
        juce::MidiBuffer midi;
        proc.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buf.getNumSamples(); ++i)
                expect (buf.getSample (ch, i) == 0.0f);
    }
};

static ProcessorScaffoldTests processorScaffoldTests;
