#pragma once

#include "PluginProcessor.h"
#include "ui/BrainColumn.h"
#include "ui/CaptureStrip.h"
#include "ui/CepstrumView.h"
#include "ui/EnvelopeView.h"
#include "ui/HelpOverlay.h"
#include "ui/MeterStrip.h"
#include "ui/ParamPanel.h"
#include "ui/Theme.h"
#include "ui/TrianglePad.h"

namespace cortex::ui
{
// Everything inside the editor window. Built for the current style; the
// editor makes a fresh one when the theme changes.
//
// Layout:
//   header       title and the "?" explainer, in/out meters + gate light, Capture, version
//   left/right   Brain 1 / Brain 2 columns (status, load, rebuild, map, sounds, their settings and slicing)
//   centre       Envelope and Detail triangles (+ Link), live envelopes, cepstrum
//   bottom       Input panel (synth or track/sidechain + its settings), tabs
class MainView final : public juce::Component
{
public:
    explicit MainView (CortexAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

    // Pull the latest audio-thread state (the editor timer does this 30x a second).
    void refresh();

private:
    CortexAudioProcessor& processor;

public:
    // For tests: the capture strip (what the Capture button does), and the explainer.
    CaptureStrip& getCaptureStrip() { return capture; }
    HelpOverlay& getHelp() { return help; }

private:
    juce::Label title, versionLabel, credits;
    juce::TextButton helpButton { "?" };
    MeterStrip meters;
    CaptureStrip capture;

    BrainColumn brain1Column, brain2Column;
    TrianglePad envelopeTriangle, detailTriangle;
    juce::ToggleButton linkToggle { "Link: detail follows envelope" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachment;
    EnvelopeView envelopes;
    CepstrumView cepstrum;

    juce::Label inputTitle;
    ParamPanel inputPanel;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    std::array<ParamPanel*, CortexAudioProcessor::numBrains> searchPanels {};   // owned by `tabs`
    juce::TextButton linkAllButton { "Link all" };   // every brain control moves on both brains
    HelpOverlay help;   // over everything, hidden until the "?" is clicked

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
} // namespace cortex::ui
