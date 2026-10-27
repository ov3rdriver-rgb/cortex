#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "CortexFonts.h"
#include "dsp/Cepstrum.h"
#include "dsp/STFT.h"

namespace cortex::ui
{
// The embedded faces (Resources/fonts, SIL Open Font License), loaded once.
namespace faces
{
    inline juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    // Inter is the variable build from google/fonts: JUCE draws it at its
    // default instance (Regular), so it serves as both weights for now.
    inline juce::Typeface::Ptr inter()        { static auto f = load (CortexFonts::InterVariable_ttf, CortexFonts::InterVariable_ttfSize); return f; }
    inline juce::Typeface::Ptr barlow()       { static auto f = load (CortexFonts::BarlowRegular_ttf, CortexFonts::BarlowRegular_ttfSize); return f; }
    inline juce::Typeface::Ptr barlowBold()   { static auto f = load (CortexFonts::BarlowSemiBold_ttf, CortexFonts::BarlowSemiBold_ttfSize); return f; }
}

// A complete look: palette, fonts, label casing. Two ship: Dark and Light,
// chosen by the Theme parameter; `applyStyle` switches the palette and the
// editor rebuilds its view.
struct Style
{
    juce::String name;
    juce::Colour background, panel, outline, rule, text, dim;
    juce::Colour input, brain1, brain2, result, neutralAccent;
    bool uppercaseTitles = true;
    float titleKerning = 0.08f;     // extra letter spacing for titles / labels
    juce::Typeface::Ptr regular, bold;
    juce::String monoFontName;      // values: a system monospace face
    float plotLineWidth = 1.4f;
};

inline juce::String systemMonoFontName()
{
#if JUCE_WINDOWS
    return "Cascadia Mono";
#else
    return juce::Font::getDefaultMonospacedFontName();   // Menlo on macOS
#endif
}

inline Style darkStyle()
{
    Style s;
    s.name = "dark";
    s.background = juce::Colour (0xff0e0f12);
    s.panel = juce::Colour (0xff14161b);
    s.outline = juce::Colour (0xff23272f);
    s.rule = juce::Colour (0xff2b303a);
    s.text = juce::Colour (0xffe6e8ee);
    s.dim = juce::Colour (0xff7d8496);
    s.input = juce::Colour (0xff4fd1c5);
    s.brain1 = juce::Colour (0xfff6ad55);
    s.brain2 = juce::Colour (0xffb794f4);
    s.result = juce::Colour (0xfff687b3);
    s.neutralAccent = juce::Colour (0xffc3c9d6);
    s.titleKerning = 0.08f;
    s.regular = faces::inter();
    s.bold = faces::inter();
    s.monoFontName = systemMonoFontName();
    return s;
}

// Paper, ink and three printing colours; the output is black.
inline Style lightStyle()
{
    Style s;
    s.name = "light";
    s.background = juce::Colour (0xffeeece6);
    s.panel = juce::Colour (0xfffbfaf7);
    s.outline = juce::Colour (0xff141414);
    s.rule = juce::Colour (0xffc9c6bd);
    s.text = juce::Colour (0xff141414);
    s.dim = juce::Colour (0xff6c6a64);
    s.input = juce::Colour (0xff0b63c7);
    s.brain1 = juce::Colour (0xffd92b2b);
    s.brain2 = juce::Colour (0xff6a2bd9);
    s.result = juce::Colour (0xff141414);
    s.neutralAccent = juce::Colour (0xff141414);
    s.titleKerning = 0.1f;
    s.regular = faces::barlow();
    s.bold = faces::barlowBold();
    s.monoFontName = systemMonoFontName();
    s.plotLineWidth = 1.6f;
    return s;
}

// Index of the Theme parameter -> style.
inline Style styleForTheme (int index)
{
    return index == 1 ? lightStyle() : darkStyle();
}

// The colours the components read. They start as the dark style and are
// reassigned by applyStyle (the names stay so call sites read naturally).
namespace colours
{
    inline juce::Colour background { 0xff0e0f12 };
    inline juce::Colour panel      { 0xff14161b };
    inline juce::Colour outline    { 0xff23272f };
    inline juce::Colour rule       { 0xff2b303a };   // hairlines between regions
    inline juce::Colour text       { 0xffe6e8ee };
    inline juce::Colour dim        { 0xff7d8496 };
    inline juce::Colour input      { 0xff4fd1c5 };   // the Input
    inline juce::Colour brain1     { 0xfff6ad55 };   // Brain 1
    inline juce::Colour brain2     { 0xffb794f4 };   // Brain 2
    inline juce::Colour result     { 0xfff687b3 };   // the morph (output)
    inline juce::Colour neutralAccent { 0xffc3c9d6 };   // knobs that belong to no source

    inline juce::Colour forSource (int source)
    {
        return source == cortex::dsp::input ? input : source == cortex::dsp::brain1 ? brain1 : brain2;
    }

    inline const char* sourceName (int source)
    {
        return source == cortex::dsp::input ? "Input" : source == cortex::dsp::brain1 ? "Brain 1" : "Brain 2";
    }

    // Sounds inside a brain are shades of that brain's colour, so a map never
    // borrows another source's colour.
    inline juce::Colour forSound (juce::Colour brainColour, int index)
    {
        const float t = std::fmod (0.618034f * (float) index, 1.0f);
        return brainColour.withRotatedHue (-0.05f + 0.1f * t).withMultipliedBrightness (0.55f + 0.45f * std::fmod (t * 2.7f, 1.0f))
            .withMultipliedSaturation (0.7f + 0.3f * std::fmod (t * 1.9f, 1.0f));
    }
}

// The style in use. Starts empty (the colours above carry the dark defaults)
// so no typeface is loaded while the plugin binary is still being loaded;
// the editor applies the real style when it opens.
inline Style style;

inline void applyStyle (const Style& s)
{
    style = s;
    colours::background = s.background;
    colours::panel = s.panel;
    colours::outline = s.outline;
    colours::rule = s.rule;
    colours::text = s.text;
    colours::dim = s.dim;
    colours::input = s.input;
    colours::brain1 = s.brain1;
    colours::brain2 = s.brain2;
    colours::result = s.result;
    colours::neutralAccent = s.neutralAccent;
}

namespace fonts
{
    inline juce::Font ui (float size, bool bold = false)
    {
        const auto face = bold ? style.bold : style.regular;
        if (face == nullptr)   // before any style is applied
            return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
        return juce::Font (juce::FontOptions (face).withHeight (size));
    }

    // Section titles: small, tracked, in caps if the style says so.
    inline juce::Font title (float size, bool bold = true)
    {
        return ui (size, bold).withExtraKerningFactor (style.titleKerning);
    }

    // Control labels: like a title but tracked less, so long names fit a cell.
    inline juce::Font label (float size)
    {
        return ui (size).withExtraKerningFactor (style.titleKerning * 0.25f);
    }

    inline juce::Font mono (float size)
    {
        return juce::Font (juce::FontOptions (style.monoFontName, size, juce::Font::plain));
    }
    inline juce::String titleCase (const juce::String& s)
    {
        return style.uppercaseTitles ? s.toUpperCase() : s;
    }
}

class CortexLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CortexLookAndFeel() { applyColours(); }

    // Re-read the palette after applyStyle.
    void applyColours()
    {
        using namespace colours;
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::rotarySliderFillColourId, neutralAccent);
        setColour (juce::Slider::rotarySliderOutlineColourId, rule);
        setColour (juce::Slider::thumbColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxBackgroundColourId, background);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::outlineColourId, outline);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, dim);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, rule);
        setColour (juce::PopupMenu::highlightedTextColourId, text);
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, text);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, neutralAccent);
        setColour (juce::ToggleButton::tickDisabledColourId, dim);
        setColour (juce::TabbedButtonBar::tabTextColourId, dim);
        setColour (juce::TabbedButtonBar::frontTextColourId, text);
        setColour (juce::TabbedComponent::outlineColourId, outline);
        setColour (juce::ProgressBar::backgroundColourId, panel);
        setColour (juce::ProgressBar::foregroundColourId, brain1);
        setColour (juce::ListBox::backgroundColourId, panel);
        setColour (juce::ListBox::outlineColourId, outline);
        setColour (juce::ScrollBar::thumbColourId, rule);
        setColour (juce::TooltipWindow::backgroundColourId, panel);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, outline);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return fonts::title (buttonHeight < 30 ? 10.5f : 12.5f, false);
    }

    juce::Font getLabelFont (juce::Label& label) override { return label.getFont(); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return fonts::ui (12.5f); }
    juce::Font getPopupMenuFont() override { return fonts::ui (13.0f); }
    juce::Font getTabButtonFont (juce::TabBarButton&, float) override { return fonts::title (11.0f); }
    juce::Font getSliderPopupFont (juce::Slider&) override { return fonts::mono (12.0f); }

    // Values in a monospace face, so they don't jiggle as they change.
    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (fonts::mono (11.0f));
        return label;
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        auto base = backgroundColour;
        if (down || highlighted)
            base = base.contrasting (down ? 0.12f : 0.05f);
        g.setColour (base);
        g.fillRect (bounds);
        g.setColour (button.getToggleState() ? button.findColour (juce::TextButton::textColourOnId).withAlpha (0.8f) : colours::outline);
        g.drawRect (bounds, 1.0f);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRect (bounds);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRect (bounds, 1.0f);

        // A small chevron.
        const auto arrow = juce::Rectangle<float> ((float) width - 18.0f, (float) height * 0.5f - 2.0f, 8.0f, 4.0f);
        juce::Path p;
        p.startNewSubPath (arrow.getX(), arrow.getY());
        p.lineTo (arrow.getCentreX(), arrow.getBottom());
        p.lineTo (arrow.getRight(), arrow.getY());
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }

    // A square tick box with a filled square when on.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down) override
    {
        const float boxSize = 14.0f;
        auto bounds = button.getLocalBounds().toFloat();
        auto box = juce::Rectangle<float> (boxSize, boxSize).withCentre ({ bounds.getX() + boxSize * 0.5f + 2.0f, bounds.getCentreY() });
        g.setColour (colours::panel.contrasting (down ? 0.12f : highlighted ? 0.05f : 0.0f));
        g.fillRect (box);
        g.setColour (button.getToggleState() ? button.findColour (juce::ToggleButton::tickColourId) : colours::outline);
        g.drawRect (box, 1.0f);
        if (button.getToggleState())
            g.fillRect (box.reduced (3.5f));

        g.setFont (fonts::ui (12.5f));
        g.setColour (button.findColour (juce::ToggleButton::textColourId).withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
        g.drawText (button.getButtonText(), bounds.withTrimmedLeft (boxSize + 10.0f).toNearestInt(), juce::Justification::centredLeft);
    }

    // Tabs: flat text with a rule under the front one in the accent colour.
    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool highlighted, bool) override
    {
        const auto area = button.getActiveArea();
        const bool front = button.isFrontTab();
        g.setColour (front ? colours::panel : colours::background);
        g.fillRect (area);
        g.setColour (colours::outline);
        g.fillRect (area.getX(), area.getBottom() - 1, area.getWidth(), 1);
        if (front)
        {
            g.setColour (colours::neutralAccent);
            g.fillRect (area.getX(), area.getY(), area.getWidth(), 2);
        }
        g.setFont (fonts::title (11.0f));
        g.setColour (front ? colours::text : highlighted ? colours::text.withAlpha (0.8f) : colours::dim);
        g.drawText (fonts::titleCase (button.getButtonText()), area, juce::Justification::centred);
    }

    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}

    // Tooltips: a flat panel, body text wrapped at a readable width.
    static juce::TextLayout tooltipLayout (const juce::String& text, float maxWidth)
    {
        juce::AttributedString s;
        s.setJustification (juce::Justification::topLeft);
        s.append (text, fonts::ui (12.5f), colours::text);
        juce::TextLayout layout;
        layout.createLayout (s, maxWidth);
        return layout;
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override
    {
        const auto layout = tooltipLayout (text, 340.0f);
        const int w = (int) std::ceil (layout.getWidth()) + 20, h = (int) std::ceil (layout.getHeight()) + 14;
        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
            .constrainedWithin (parentArea);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        g.fillAll (colours::panel);
        g.setColour (colours::outline);
        g.drawRect (0, 0, width, height, 1);
        tooltipLayout (text, (float) width - 20.0f).draw (g, juce::Rectangle<float> (10.0f, 7.0f, (float) width - 20.0f, (float) height - 14.0f));
    }
    int getTabButtonBestWidth (juce::TabBarButton& button, int) override
    {
        return juce::roundToInt (fonts::title (11.0f).getStringWidthFloat (fonts::titleCase (button.getButtonText()))) + 32;
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float stroke = juce::jmax (2.0f, radius * 0.13f);

        juce::Path track, value;
        track.addCentredArc (centre.x, centre.y, radius - stroke, radius - stroke, 0.0f, startAngle, endAngle, true);
        value.addCentredArc (centre.x, centre.y, radius - stroke, radius - stroke, 0.0f, startAngle, angle, true);

        g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
        g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // Tick marks at the ends and the middle, like an instrument dial.
        g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId).contrasting (0.25f));
        for (float a : { startAngle, (startAngle + endAngle) * 0.5f, endAngle })
            g.drawLine ({ centre.getPointOnCircumference (radius + 1.0f, a), centre.getPointOnCircumference (radius - stroke * 2.0f - 1.0f, a) }, 1.0f);

        const auto tip = centre.getPointOnCircumference (radius - stroke * 2.4f, angle);
        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.drawLine ({ centre.getPointOnCircumference (radius * 0.1f, angle), tip }, 2.0f);
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

// A region: flat panel with a hairline edge.
inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (colours::panel);
    g.fillRect (r);
    g.setColour (colours::outline);
    g.drawRect (r, 1.0f);
}

// A small section caption at the top-left of a region.
inline void drawCaption (juce::Graphics& g, juce::Rectangle<int> region, const juce::String& text, juce::Colour colour = colours::dim)
{
    g.setFont (fonts::title (10.0f));
    g.setColour (colour);
    g.drawText (fonts::titleCase (text), region.reduced (12, 7).removeFromTop (12), juce::Justification::centredLeft);
}
} // namespace cortex::ui
