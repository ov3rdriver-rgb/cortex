#pragma once

#include "PluginProcessor.h"

// Brain panel (drop a folder or files, build progress, rebuild/clear) above
// JUCE's generic parameter editor. The XY pad and brain browser arrive in Phase 4.
class CortexAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         public juce::FileDragAndDropTarget,
                                         private juce::Timer
{
public:
    explicit CortexAudioProcessorEditor (CortexAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragEnter (const juce::StringArray&, int, int) override { setDragHighlight (true); }
    void fileDragExit (const juce::StringArray&) override { setDragHighlight (false); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void setDragHighlight (bool);
    juce::Rectangle<int> brainArea() const;

    CortexAudioProcessor& processor;

    double progress = 0.0;
    juce::ProgressBar progressBar { progress };
    juce::Label brainLabel;
    juce::TextButton chooseButton { "Load..." }, rebuildButton { "Rebuild" }, clearButton { "Clear" };
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragHighlight = false;

    juce::GenericAudioProcessorEditor generic;
    juce::Label credits;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CortexAudioProcessorEditor)
};
