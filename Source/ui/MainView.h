#pragma once

#include "PluginProcessor.h"
#include "ui/BrainColumn.h"
#include "ui/CepstrumView.h"
#include "ui/EnvelopeView.h"
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
//   header       title, in/out meters + gate light, Rebuild, version
//   left/right   Brain 1 / Brain 2 columns (status, load, map, sounds, their settings)
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

    juce::Label title, versionLabel, credits;
    juce::TextButton rebuildButton { "Rebuild brains" };
    MeterStrip meters;

    BrainColumn brain1Column, brain2Column;
    TrianglePad envelopeTriangle, detailTriangle;
    juce::ToggleButton linkToggle { "Link: detail follows envelope" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachment;
    EnvelopeView envelopes;
    CepstrumView cepstrum;

    juce::Label inputTitle;
    ParamPanel inputPanel;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
} // namespace cortex::ui
