#pragma once

#include "PluginProcessor.h"
#include "ui/BrainColumn.h"
#include "ui/CepstrumView.h"
#include "ui/EnvelopeView.h"
#include "ui/ParamPanel.h"
#include "ui/Theme.h"
#include "ui/TrianglePad.h"

// Layout:
//   header       title, Rebuild, version
//   left/right   Brain 1 / Brain 2 columns (status, load, map, sounds, their settings)
//   centre       Envelope and Detail triangles (+ Link), live envelopes, cepstrum
//   bottom       Input panel (synth or track/sidechain + its settings), tabs
// Files dropped on a brain column load into that brain; anywhere else, Brain 1.
class CortexAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         public juce::FileDragAndDropTarget,
                                         private juce::Timer
{
public:
    explicit CortexAudioProcessorEditor (CortexAudioProcessor&);
    ~CortexAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;

    // Pull the latest audio-thread state now (the timer does this 30x a second).
    void refresh() { timerCallback(); }

private:
    void timerCallback() override;

    CortexAudioProcessor& processor;
    cortex::ui::CortexLookAndFeel lookAndFeel;

    juce::Label title, versionLabel, credits;
    juce::TextButton rebuildButton { "Rebuild brains" };

    cortex::ui::BrainColumn brain1Column, brain2Column;
    cortex::ui::TrianglePad envelopeTriangle, detailTriangle;
    juce::ToggleButton linkToggle { "Link: detail follows envelope" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachment;
    cortex::ui::EnvelopeView envelopes;
    cortex::ui::CepstrumView cepstrum;

    juce::Label inputTitle;
    cortex::ui::ParamPanel inputPanel;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CortexAudioProcessorEditor)
};
