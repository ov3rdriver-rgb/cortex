#pragma once

#include "Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace cortex::ui
{
// A row of controls for a list of parameter IDs: a knob for continuous
// parameters, a menu for choices, a lit button for switches - each attached to
// the APVTS so host automation and the UI stay in sync.
class ParamPanel final : public juce::Component
{
public:
    // `stripPrefix` is removed from labels (e.g. "Brain 1 " inside the Brain 1
    // column); `cellWidth` sets how wide each control is; `accent` colours the
    // knob arcs and lit switches (a source's colour, or neutral for the tabs).
    ParamPanel (juce::AudioProcessorValueTreeState&, const juce::StringArray& ids,
                const juce::String& stripPrefix = {}, int cellWidth = 84,
                juce::Colour accent = colours::neutralAccent);

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
    };

    std::vector<std::unique_ptr<Control>> controls;
    int cellWidth = 84;
};
} // namespace cortex::ui
