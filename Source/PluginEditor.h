#pragma once

#include "PluginProcessor.h"
#include "ui/MainView.h"
#include "ui/Theme.h"

// The window: owns the look-and-feel and the view, and rebuilds the view
// when the Theme parameter changes. Files dropped on a brain column load into
// that brain; anywhere else, Brain 1.
class CortexAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         public juce::FileDragAndDropTarget,
                                         private juce::Timer
{
public:
    explicit CortexAudioProcessorEditor (CortexAudioProcessor&);
    ~CortexAudioProcessorEditor() override;

    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;

    // Pull the latest audio-thread state now (the timer does this 30x a second).
    void refresh() { timerCallback(); }

private:
    void timerCallback() override;
    void buildView();

    CortexAudioProcessor& processor;
    cortex::ui::CortexLookAndFeel lookAndFeel;
    std::unique_ptr<cortex::ui::MainView> view;
    std::unique_ptr<juce::ParameterAttachment> themeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CortexAudioProcessorEditor)
};
