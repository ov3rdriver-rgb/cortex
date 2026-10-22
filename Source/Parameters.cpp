#include "Parameters.h"

namespace cortex
{
namespace
{
    using Float  = juce::AudioParameterFloat;
    using Choice = juce::AudioParameterChoice;
    using Bool   = juce::AudioParameterBool;
    using Int    = juce::AudioParameterInt;

    constexpr int version = 1;

    juce::ParameterID pid (const char* id) { return { id, version }; }

    std::unique_ptr<Float> unit (const char* id, const char* name, float def)
    {
        return std::make_unique<Float> (pid (id), name, juce::NormalisableRange<float> (0.0f, 1.0f), def);
    }

    std::unique_ptr<Float> decibels (const char* id, const char* name, float lo, float hi, float def)
    {
        return std::make_unique<Float> (pid (id), name, juce::NormalisableRange<float> (lo, hi, 0.01f), def,
                                        juce::AudioParameterFloatAttributes().withLabel ("dB"));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (bool instrument)
{
    using namespace ParamIDs;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Cepstral morph
    layout.add (unit (morphX, "Morph Envelope", 1.0f),
                unit (morphY, "Morph Detail", 0.0f),
                std::make_unique<Int> (pid (cutoffA), "Cutoff Quefrency A", 4, 200, 25),
                std::make_unique<Int> (pid (cutoffB), "Cutoff Quefrency B", 4, 200, 25),
                std::make_unique<Choice> (pid (phaseMode), "Phase Mode",
                                          juce::StringArray { "Phase A", "Phase B", "Interpolate", "Follow Detail" }, 0),
                std::make_unique<Choice> (pid (fftSize), "FFT Size",
                                          juce::StringArray { "512", "1024", "2048", "4096" }, defaultFftSizeIndex),
                std::make_unique<Choice> (pid (bSource), "B Source",
                                          juce::StringArray { "Sidechain", "Brain" }, 1));

    // Brain search
    layout.add (unit (matchTimbre, "Match Timbre", 0.5f),
                unit (matchPitch, "Match Pitch", 0.5f),
                unit (envTransplant, "Envelope Transplant", 0.0f),
                std::make_unique<Int> (pid (blockSize), "Block Size", 256, 4096, 1000),
                std::make_unique<Float> (pid (overlap), "Block Overlap", juce::NormalisableRange<float> (0.0f, 0.75f), 0.0f),
                unit (novelty, "Novelty", 0.3f),
                unit (boredom, "Boredom", 0.5f),
                unit (stickyness, "Stickyness", 0.0f),
                std::make_unique<Choice> (pid (searchMode), "Search Mode",
                                          juce::StringArray { "Basic", "Rev", "Synaptic", "Synaptic Slide" }, 0));

    // Output / utility
    layout.add (decibels (cornerGainTL, "Corner Gain TL", -24.0f, 12.0f, 0.0f),
                decibels (cornerGainBR, "Corner Gain BR", -24.0f, 12.0f, 0.0f),
                unit (dryWet, "Dry/Wet", 1.0f),
                decibels (outGain, "Output Gain", -24.0f, 12.0f, 0.0f),
                std::make_unique<Bool> (pid (limiter), "Limiter", true),
                std::make_unique<Bool> (pid (gateOn), "Gate", false),
                decibels (gateThresh, "Gate Threshold", -96.0f, 0.0f, -60.0f),
                std::make_unique<Bool> (pid (autoMute), "Auto-Mute", true),
                std::make_unique<Float> (pid (autoMuteMs), "Auto-Mute Time",
                                         juce::NormalisableRange<float> (10.0f, 2000.0f, 1.0f, 0.5f), 250.0f,
                                         juce::AudioParameterFloatAttributes().withLabel ("ms")));

    if (instrument)
    {
        auto ms = [] (const char* id, const char* name, float max, float def)
        {
            return std::make_unique<Float> (pid (id), name, juce::NormalisableRange<float> (0.0f, max, 0.1f, 0.4f), def,
                                            juce::AudioParameterFloatAttributes().withLabel ("ms"));
        };
        layout.add (std::make_unique<Choice> (pid (oscWave), "Oscillator", juce::StringArray { "Saw", "Square", "Triangle", "Sine" }, 0),
                    ms (oscGlide, "Glide", 2000.0f, 60.0f),
                    ms (ampAttack, "Attack", 2000.0f, 5.0f),
                    ms (ampRelease, "Release", 5000.0f, 250.0f));
    }

    return layout;
}
} // namespace cortex
