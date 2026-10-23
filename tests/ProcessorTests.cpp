#include "PluginProcessor.h"
#include "Parameters.h"

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
                          "matchTimbre", "matchPitch", "envTransplant", "blockSize", "overlap",
                          "novelty", "boredom", "stickyness", "searchMode",
                          "dryWet", "outGain", "limiter", "gateOn", "gateThresh", "autoMute", "autoMuteMs" })
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
