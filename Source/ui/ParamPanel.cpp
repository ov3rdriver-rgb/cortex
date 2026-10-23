#include "ParamPanel.h"

namespace cortex::ui
{
namespace
{
    constexpr int labelHeight = 16;
    constexpr int maxKnobHeight = 92;
}

ParamPanel::ParamPanel (juce::AudioProcessorValueTreeState& state, const juce::StringArray& ids,
                        const juce::String& stripPrefix, int width)
    : cellWidth (width)
{
    for (const auto& id : ids)
    {
        auto* param = state.getParameter (id);
        if (param == nullptr)
            continue;

        auto c = std::make_unique<Control>();
        auto name = param->getName (40);
        if (stripPrefix.isNotEmpty() && name.startsWith (stripPrefix))
            name = name.substring (stripPrefix.length());
        c->label.setText (name, juce::dontSendNotification);
        c->label.setJustificationType (juce::Justification::centred);
        c->label.setFont (juce::FontOptions (11.5f));
        c->label.setColour (juce::Label::textColourId, colours::dim);
        addAndMakeVisible (c->label);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        {
            c->combo = std::make_unique<juce::ComboBox>();
            c->combo->addItemList (choice->choices, 1);
            c->comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, *c->combo);
            addAndMakeVisible (*c->combo);
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
        {
            c->toggle = std::make_unique<juce::ToggleButton> ("on");
            c->buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, *c->toggle);
            addAndMakeVisible (*c->toggle);
        }
        else
        {
            c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, cellWidth - 8, 16);
            c->slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            c->slider->setColour (juce::Slider::textBoxBackgroundColourId, colours::background);
            c->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, *c->slider);

            // The attachment shows the parameter's own text (full precision for
            // plain floats); keep continuous values to two decimals, with the unit.
            if (dynamic_cast<juce::AudioParameterInt*> (param) == nullptr)
            {
                const auto unit = param->getLabel();
                c->slider->textFromValueFunction = [unit] (double v)
                {
                    return juce::String (v, 2) + (unit.isNotEmpty() ? " " + unit : juce::String());
                };
                c->slider->updateText();
            }
            addAndMakeVisible (*c->slider);
        }

        controls.push_back (std::move (c));
    }
}

void ParamPanel::resized()
{
    auto area = getLocalBounds().reduced (6, 4);
    const int perRow = juce::jmax (1, area.getWidth() / cellWidth);
    const int rows = ((int) controls.size() + perRow - 1) / perRow;
    const int rowHeight = rows > 0 ? area.getHeight() / rows : area.getHeight();
    const int width = juce::jmin (cellWidth + 12, area.getWidth() / perRow);

    for (size_t i = 0; i < controls.size(); ++i)
    {
        auto& c = *controls[i];
        const int row = (int) i / perRow, col = (int) i % perRow;
        auto cell = juce::Rectangle<int> (area.getX() + col * width, area.getY() + row * rowHeight, width, rowHeight).reduced (2);

        c.label.setBounds (cell.removeFromTop (labelHeight));
        if (c.slider != nullptr)
            c.slider->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), juce::jmin (cell.getHeight(), maxKnobHeight)));
        else if (c.combo != nullptr)
            c.combo->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 24));
        else if (c.toggle != nullptr)
            c.toggle->setBounds (cell.withSizeKeepingCentre (56, 24));
    }
}
} // namespace cortex::ui
