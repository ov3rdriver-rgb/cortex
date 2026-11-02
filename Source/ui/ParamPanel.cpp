#include "ParamPanel.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "Tooltips.h"

namespace cortex::ui
{
namespace
{
    constexpr int labelHeight = 16;
    constexpr int maxKnobHeight = 92;
    constexpr int linkSize = 14;

    int brainControlIndex (int brain, const juce::String& id)
    {
        if (brain < 0)
            return -1;
        const auto all = cortex::ParamIDs::brainIds[brain].all();
        for (size_t i = 0; i < all.size(); ++i)
            if (id == all[i].id)
                return (int) i;
        return -1;
    }
}

void LinkToggle::paint (juce::Graphics& g)
{
    // Two rings, interlocked when linked, apart when not.
    const auto colour = linked ? accent : isMouseOver() ? colours::text.withAlpha (0.7f) : colours::dim.withAlpha (0.6f);
    const float d = (float) getHeight() - 4.0f;
    const float gapX = linked ? d * 0.45f : d * 0.8f;
    const float x0 = ((float) getWidth() - (d + gapX)) * 0.5f;
    g.setColour (colour);
    g.drawEllipse (x0, 2.0f, d, d, 1.4f);
    g.drawEllipse (x0 + gapX, 2.0f, d, d, 1.4f);
}

ParamPanel::ParamPanel (juce::AudioProcessorValueTreeState& state, const juce::StringArray& ids,
                        const juce::String& stripPrefix, int width, juce::Colour accent,
                        CortexAudioProcessor* host, int brainIndex)
    : cellWidth (width), linkHost (host), brain (brainIndex)
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

        if (linkHost != nullptr)
        {
            c->linkControl = brainControlIndex (brain, id);
            if (c->linkControl >= 0)
            {
                c->link = std::make_unique<LinkToggle>();
                c->link->setAccent (accent);
                c->link->setTooltip (tips::linkToggle (name, brain));
                const int control = c->linkControl;
                c->link->onChange = [this, control] (bool on) { linkHost->setLinked (control, on, brain); refresh(); };
                addAndMakeVisible (*c->link);
            }
        }
        c->label.setText (fonts::titleCase (name), juce::dontSendNotification);
        c->label.setJustificationType (juce::Justification::centred);
        c->label.setFont (fonts::label (9.5f));
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
            // A switch is a button that says what it does and lights up when on
            // (its label row is left empty).
            c->label.setText ({}, juce::dontSendNotification);
            // "Freeze Detail" doesn't fit a brain column's cell; the host still sees the full name.
            c->toggle = std::make_unique<juce::TextButton> (fonts::titleCase (name.replace ("Detail", "Det")));
            c->toggle->setClickingTogglesState (true);
            c->toggle->setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.28f));
            c->toggle->setColour (juce::TextButton::textColourOnId, accent);
            c->toggle->setColour (juce::TextButton::textColourOffId, colours::dim);
            c->buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, *c->toggle);
            addAndMakeVisible (*c->toggle);
        }
        else
        {
            c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, cellWidth - 8, 16);
            c->slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            c->slider->setColour (juce::Slider::textBoxTextColourId, colours::text);   // the box is made before the look-and-feel applies
            c->slider->setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.4f));
            c->slider->setColour (juce::Slider::rotarySliderFillColourId, accent);
            c->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, *c->slider);

            // The attachment shows the parameter's own text (full precision for
            // plain floats); keep continuous values short, with the unit: one
            // decimal for dB / semitones / ms, two for 0..1 amounts.
            if (dynamic_cast<juce::AudioParameterInt*> (param) == nullptr)
            {
                const auto unit = param->getLabel();
                const int decimals = unit.isNotEmpty() ? 1 : 2;
                c->slider->textFromValueFunction = [unit, decimals] (double v)
                {
                    return juce::String (v, decimals) + (unit.isNotEmpty() ? " " + unit : juce::String());
                };
                c->slider->updateText();
            }
            addAndMakeVisible (*c->slider);
        }

        const auto tip = tips::forParam (id);
        c->label.setTooltip (tip);
        if (c->slider != nullptr) c->slider->setTooltip (tip);
        if (c->combo != nullptr)  c->combo->setTooltip (tip);
        if (c->toggle != nullptr) c->toggle->setTooltip (tip);

        controls.push_back (std::move (c));
    }
    refresh();
}

void ParamPanel::refresh()
{
    if (linkHost == nullptr)
        return;
    for (auto& c : controls)
        if (c->link != nullptr)
            c->link->setLinked (linkHost->isLinked (c->linkControl));
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
        {
            auto row = cell.withSizeKeepingCentre (cell.getWidth(), 24);
            if (c.link != nullptr)   // the rings take the end of the menu's row
                c.link->setBounds (row.removeFromRight (linkSize + 2).withSizeKeepingCentre (linkSize, linkSize));
            c.combo->setBounds (row);
        }
        else if (c.toggle != nullptr)
            c.toggle->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), cellWidth - 4), 26));

        // A knob's link rings sit beside its value (short, centred text).
        if (c.link != nullptr && c.slider != nullptr)
        {
            c.link->setBounds (cell.getRight() - linkSize, c.slider->getBottom() - linkSize - 1, linkSize, linkSize);
            c.link->toFront (false);   // over the value box's edge
        }
    }
}
} // namespace cortex::ui
