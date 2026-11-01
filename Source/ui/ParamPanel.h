#pragma once

#include "Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

class CortexAudioProcessor;

namespace cortex::ui
{
// A small two-ring "chain" switch in the corner of a per-brain control: on,
// the control moves on both brains together.
class LinkToggle final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    std::function<void (bool)> onChange;
    void setLinked (bool shouldBeLinked)
    {
        if (linked != shouldBeLinked) { linked = shouldBeLinked; repaint(); }
    }
    bool isLinked() const noexcept { return linked; }
    void setAccent (juce::Colour c) { accent = c; }

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && onChange)
            onChange (! linked);
    }

private:
    bool linked = false;
    juce::Colour accent = colours::neutralAccent;
};

// A row of controls for a list of parameter IDs: a knob for continuous
// parameters, a menu for choices, a lit button for switches - each attached to
// the APVTS so host automation and the UI stay in sync.
class ParamPanel final : public juce::Component
{
public:
    // `stripPrefix` is removed from labels (e.g. "Brain 1 " inside the Brain 1
    // column); `cellWidth` sets how wide each control is; `accent` colours the
    // knob arcs and lit switches (a source's colour, or neutral for the tabs).
    // With `linkHost` and `brain`, the controls that belong to that brain
    // (ParamIDs::brainIds) get a link switch to the other brain's control.
    ParamPanel (juce::AudioProcessorValueTreeState&, const juce::StringArray& ids,
                const juce::String& stripPrefix = {}, int cellWidth = 84,
                juce::Colour accent = colours::neutralAccent,
                CortexAudioProcessor* linkHost = nullptr, int brain = -1);

    // Pulls the link states from the processor (editor timer).
    void refresh();

    void resized() override;

private:
    struct Control
    {
        juce::Label label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::TextButton> toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
        std::unique_ptr<LinkToggle> link;
        int linkControl = -1;   // index into BrainIds::all(), when linkable
    };

    std::vector<std::unique_ptr<Control>> controls;
    int cellWidth = 84;
    CortexAudioProcessor* linkHost = nullptr;
    int brain = -1;
};
} // namespace cortex::ui
