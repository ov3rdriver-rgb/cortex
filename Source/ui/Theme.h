#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/Cepstrum.h"
#include "dsp/STFT.h"

namespace cortex::ui
{
namespace colours
{
    inline const juce::Colour background { 0xff101217 };
    inline const juce::Colour panel      { 0xff181b22 };
    inline const juce::Colour outline    { 0xff2b303b };
    inline const juce::Colour text       { 0xffd9dee8 };
    inline const juce::Colour dim        { 0xff7b8496 };
    inline const juce::Colour input      { 0xff4fd1c5 };   // the Input
    inline const juce::Colour brain1     { 0xfff6ad55 };   // Brain 1
    inline const juce::Colour brain2     { 0xffb794f4 };   // Brain 2
    inline const juce::Colour result     { 0xfff687b3 };   // the morph

    inline juce::Colour forSource (int source)
    {
        return source == cortex::dsp::input ? input : source == cortex::dsp::brain1 ? brain1 : brain2;
    }

    inline const char* sourceName (int source)
    {
        return source == cortex::dsp::input ? "Input" : source == cortex::dsp::brain1 ? "Brain 1" : "Brain 2";
    }
}

class CortexLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CortexLookAndFeel()
    {
        using namespace colours;
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::rotarySliderFillColourId, result);
        setColour (juce::Slider::rotarySliderOutlineColourId, outline);
        setColour (juce::Slider::thumbColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::outlineColourId, outline);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, dim);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, outline);
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, result);
        setColour (juce::ToggleButton::tickDisabledColourId, dim);
        setColour (juce::TabbedButtonBar::tabTextColourId, dim);
        setColour (juce::TabbedButtonBar::frontTextColourId, text);
        setColour (juce::TabbedComponent::outlineColourId, outline);
        setColour (juce::ProgressBar::backgroundColourId, panel);
        setColour (juce::ProgressBar::foregroundColourId, brain1);
        setColour (juce::ListBox::backgroundColourId, panel);
        setColour (juce::ListBox::outlineColourId, outline);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float stroke = juce::jmax (2.5f, radius * 0.16f);

        juce::Path track, value;
        track.addCentredArc (centre.x, centre.y, radius - stroke, radius - stroke, 0.0f, startAngle, endAngle, true);
        value.addCentredArc (centre.x, centre.y, radius - stroke, radius - stroke, 0.0f, startAngle, angle, true);

        g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
        g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto tip = centre.getPointOnCircumference (radius - stroke * 2.2f, angle);
        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.drawLine ({ centre.getPointOnCircumference (radius * 0.15f, angle), tip }, 2.0f);
    }
};

// Smooth log-magnitude envelope (natural log) at angular frequency `omega`
// (radians/sample), rebuilt from low-quefrency cepstrum coefficients with the
// same tapered lifter the morph uses.
inline float cepstralEnvelope (const float* ceps, int numCoeffs, float cutoff, double omega) noexcept
{
    float sum = 0.0f;
    const int limit = juce::jmin (numCoeffs, (int) std::ceil (cutoff) + 5);
    for (int q = 0; q < limit; ++q)
    {
        const float w = cortex::dsp::Cepstrum::envelopeWeight (q, cutoff);
        if (w <= 0.0f)
            break;
        sum += (q == 0 ? 1.0f : 2.0f) * w * ceps[q] * (float) std::cos (omega * q);
    }
    return sum;
}

inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}
} // namespace cortex::ui
