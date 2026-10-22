#pragma once

#include "PluginProcessor.h"

// Phase 0 placeholder: JUCE's generic parameter editor plus credits line.
// The XY pad and brain browser arrive in Phase 4.
class CortexAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CortexAudioProcessorEditor (CortexAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::GenericAudioProcessorEditor generic;
    juce::Label credits;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CortexAudioProcessorEditor)
};
