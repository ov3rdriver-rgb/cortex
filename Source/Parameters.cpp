#include "Parameters.h"
#include "dsp/Triangle.h"

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
    using namespace cortex::dsp;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Three-source cepstral morph. Defaults: the envelope from Brain 1, the
    // detail (pitch) from your input - the brain's timbre on your notes.
    layout.add (unit (envX, "Envelope X", triangleCorner[brain1Corner].x),
                unit (envY, "Envelope Y", triangleCorner[brain1Corner].y),
                unit (detailX, "Detail X", triangleCorner[inputCorner].x),
                unit (detailY, "Detail Y", triangleCorner[inputCorner].y),
                std::make_unique<Bool> (pid (linkTriangles), "Link Triangles", false),
                std::make_unique<Choice> (pid (phaseMode), "Phase",
                                          juce::StringArray { "Follow Detail", "Input", "Brain 1", "Brain 2" }, 0),
                std::make_unique<Choice> (pid (fftSize), "FFT Size",
                                          juce::StringArray { "512", "1024", "2048", "4096" }, defaultFftSizeIndex),
                std::make_unique<Bool> (pid (lowLatency), "Low Latency", false));
    if (! instrument)
        layout.add (std::make_unique<Choice> (pid (inputSource), "Input", juce::StringArray { "Track", "Sidechain" }, 0));

    // Per-source envelope/detail split, formant shift and freeze.
    auto cutoff = [] (const char* id, const char* name) { return std::make_unique<Int> (pid (id), name, 4, 200, 25); };
    auto formant = [] (const char* id, const char* name)
    {
        return std::make_unique<Float> (pid (id), name, juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
                                        juce::AudioParameterFloatAttributes().withLabel ("st"));
    };
    auto freeze = [] (const char* id, const char* name) { return std::make_unique<Bool> (pid (id), name, false); };
    layout.add (cutoff (cutoffIn, "Input Cutoff"), cutoff (cutoffB1, "Brain 1 Cutoff"), cutoff (cutoffB2, "Brain 2 Cutoff"),
                formant (formantIn, "Input Formant"), formant (formantB1, "Brain 1 Formant"), formant (formantB2, "Brain 2 Formant"),
                freeze (freezeEnvIn, "Input Freeze Env"), freeze (freezeDetIn, "Input Freeze Detail"),
                freeze (freezeEnvB1, "Brain 1 Freeze Env"), freeze (freezeDetB1, "Brain 1 Freeze Detail"),
                freeze (freezeEnvB2, "Brain 2 Freeze Env"), freeze (freezeDetB2, "Brain 2 Freeze Detail"));

    // Cutoff modulation
    layout.add (std::make_unique<Float> (pid (lfoRate), "LFO Rate", juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.3f), 1.0f,
                                         juce::AudioParameterFloatAttributes().withLabel ("Hz")),
                std::make_unique<Float> (pid (lfoDepth), "LFO Depth", juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f,
                                         juce::AudioParameterFloatAttributes().withLabel ("bins")),
                std::make_unique<Choice> (pid (lfoShape), "LFO Shape",
                                          juce::StringArray { "Sine", "Triangle", "Saw", "Square", "Sample & Hold" }, 0),
                std::make_unique<Choice> (pid (lfoTarget), "LFO Target", juce::StringArray { "Input", "Brain 1", "Brain 2", "All" }, 3),
                std::make_unique<Float> (pid (envDepth), "Env Follow Depth", juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f,
                                         juce::AudioParameterFloatAttributes().withLabel ("bins")),
                std::make_unique<Float> (pid (envAttack), "Env Attack", juce::NormalisableRange<float> (0.1f, 1000.0f, 0.0f, 0.4f), 10.0f,
                                         juce::AudioParameterFloatAttributes().withLabel ("ms")),
                std::make_unique<Float> (pid (envRelease), "Env Release", juce::NormalisableRange<float> (1.0f, 5000.0f, 0.0f, 0.4f), 200.0f,
                                         juce::AudioParameterFloatAttributes().withLabel ("ms")));

    // Search macros
    layout.add (unit (chaos, "Chaos", 0.0f), unit (memory, "Memory", 0.0f), unit (cepstralGlide, "Cepstral Glide", 0.0f));

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
    layout.add (unit (dryWet, "Dry/Wet", 1.0f),
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
