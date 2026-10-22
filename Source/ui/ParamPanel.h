#pragma once

#include "Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace cortex::ui
{
// A row of controls for a list of parameter IDs: a knob for continuous
// parameters, a menu for choices, a toggle for switches - each attached to
// the APVTS so host automation and the UI stay in sync.
class ParamPanel final : public juce::Component
{
public:
    ParamPanel (juce::AudioProcessorValueTreeState&, const juce::StringArray& ids);

    void resized() override;

private:
    struct Control
    {
        juce::Label label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
    };

    std::vector<std::unique_ptr<Control>> controls;
};
} // namespace cortex::ui
