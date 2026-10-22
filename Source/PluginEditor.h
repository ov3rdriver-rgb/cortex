#pragma once

#include "PluginProcessor.h"
#include "ui/BrainMap.h"
#include "ui/CepstrumView.h"
#include "ui/ParamPanel.h"
#include "ui/Theme.h"
#include "ui/XYPad.h"

// Layout:
//   header      title, brain status + progress, Load / Rebuild / Clear
//   left        XY pad (with live envelopes), cepstrum view under it
//   right       brain map, sound list, parameter tabs
//   footer      credits, version
// The whole window is a drop target for sample folders.
class CortexAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         public juce::FileDragAndDropTarget,
                                         private juce::Timer
{
public:
    explicit CortexAudioProcessorEditor (CortexAudioProcessor&);
    ~CortexAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragEnter (const juce::StringArray&, int, int) override { setDragHighlight (true); }
    void fileDragExit (const juce::StringArray&) override { setDragHighlight (false); }
    void filesDropped (const juce::StringArray& files, int, int) override;

    // Pull the latest audio-thread state now (the timer does this 30x a second).
    void refresh() { timerCallback(); }

private:
    void timerCallback() override;
    void setDragHighlight (bool);

    CortexAudioProcessor& processor;
    cortex::ui::CortexLookAndFeel lookAndFeel;

    juce::Label title, brainLabel;
    double progress = 0.0;
    juce::ProgressBar progressBar { progress };
    juce::TextButton chooseButton { "Load..." }, rebuildButton { "Rebuild" }, clearButton { "Clear" };
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragHighlight = false;

    cortex::ui::XYPad xyPad;
    cortex::ui::CepstrumView cepstrumView;
    cortex::ui::BrainMap brainMap;
    cortex::ui::SoundList soundList;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    juce::Label credits, versionLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CortexAudioProcessorEditor)
};
