#include "PluginProcessor.h"
#include "Parameters.h"
#include "state/Settings.h"

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
