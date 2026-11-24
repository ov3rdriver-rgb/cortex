#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "CortexFonts.h"
#include "dsp/Cepstrum.h"
#include "dsp/STFT.h"

namespace cortex::ui
{
// The embedded families (Resources/fonts, SIL Open Font License).
enum class Face { none, inter, barlow };

// Holds the loaded typefaces. A DeletedAtShutdown singleton, so JUCE frees
// them while its font system is still up (a static would outlive it and
// crash on macOS at exit) and recreates them if an editor opens later.
class Faces final : private juce::DeletedAtShutdown
{
public:
    juce::Typeface::Ptr regular (Face f) { return f == Face::inter ? inter : f == Face::barlow ? barlow : nullptr; }
    juce::Typeface::Ptr bold (Face f)    { return f == Face::inter ? interBold : f == Face::barlow ? barlowBold : nullptr; }

    Faces();
    ~Faces() override { clearSingletonInstance(); }

    JUCE_DECLARE_SINGLETON (Faces, false)

private:
    juce::Typeface::Ptr inter, interBold, barlow, barlowBold;
};

// A complete look: palette, fonts, label casing. The world is the studio's
// tape box and the track sheet inside it: Dark is the box (blue-grey card,
// chalk text, cream labels stuck on for every typed value), Light is the
// sheet itself (cream, navy ink). Chosen by the Theme parameter;
// `applyStyle` switches the palette and the editor rebuilds its view.
struct Style
{
    juce::String name;
    juce::Colour background, panel, outline, rule, text, dim;
    juce::Colour input, brain1, brain2, result, neutralAccent;
    juce::Colour label, ink;   // the stuck-on label stock and the ink printed on it
    juce::Colour now;          // grease pencil: anything happening right now
    juce::Colour well;         // the ground of a map or plot
    bool uppercaseTitles = true;
    float titleKerning = 0.08f;     // extra letter spacing for titles / labels
    Face face = Face::none;
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

// The typewriter face for typed values: Courier New ships with both OSes.
inline juce::String typewriterFontName()
{
    return "Courier New";
}

// The box: blue-grey card, chalk, cream labels. Source inks are leader-tape
// green, grease-pencil orange and carbon-copy purple; the result and every
// "now" mark are grease-pencil red.
inline Style darkStyle()
{
    Style s;
    s.name = "dark";
    s.background = juce::Colour (0xff2b323c);
    s.panel = juce::Colour (0xff262c35);
    s.outline = juce::Colour (0xff434b58);
    s.rule = juce::Colour (0xff3a424e);
    s.text = juce::Colour (0xffe8e2d4);
    s.dim = juce::Colour (0xffa39d90);
    s.input = juce::Colour (0xff3fbf9f);
    s.brain1 = juce::Colour (0xfff08a3e);
    s.brain2 = juce::Colour (0xff9d7cd8);
    s.result = juce::Colour (0xffe3542b);
    s.neutralAccent = juce::Colour (0xffcfc8b8);
    s.label = juce::Colour (0xffefe6d2);
    s.ink = juce::Colour (0xff1d2a44);
    s.now = juce::Colour (0xffe3542b);
    s.well = juce::Colour (0xff1f252d);
    s.titleKerning = 0.14f;
    s.face = Face::barlow;
    s.monoFontName = typewriterFontName();
    s.plotLineWidth = 1.2f;
    return s;
}

// The track sheet itself: cream stock, navy ink, the same inks a shade deeper.
inline Style lightStyle()
{
    Style s;
    s.name = "light";
    s.background = juce::Colour (0xffe9e0cb);
    s.panel = juce::Colour (0xfff3ecdb);
    s.outline = juce::Colour (0xff8e866f);
    s.rule = juce::Colour (0xffc8bea6);
    s.text = juce::Colour (0xff1d2a44);
    s.dim = juce::Colour (0xff6b6555);
    s.input = juce::Colour (0xff1f8f78);
    s.brain1 = juce::Colour (0xffd9641e);
    s.brain2 = juce::Colour (0xff6b4fb8);
    s.result = juce::Colour (0xffd8431f);
    s.neutralAccent = juce::Colour (0xff1d2a44);
    s.label = juce::Colour (0xfffbf7ee);
    s.ink = juce::Colour (0xff1d2a44);
    s.now = juce::Colour (0xffd8431f);
    s.well = juce::Colour (0xfffbf7ee);
    s.titleKerning = 0.14f;
    s.face = Face::barlow;
    s.monoFontName = typewriterFontName();
    s.plotLineWidth = 1.3f;
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
    inline juce::Colour label  { 0xffefe6d2 };   // stuck-on label stock (value cells, tags, buttons)
    inline juce::Colour ink    { 0xff1d2a44 };   // printed on the label
    inline juce::Colour now    { 0xffe3542b };   // grease pencil: happening right now
    inline juce::Colour well   { 0xff1f252d };   // a map's or plot's ground

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

// The style in use. Starts empty (the colours above carry the dark defaults);
// the editor applies the real style when it opens, which is also when the
// typefaces are first loaded.
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
    colours::label = s.label;
    colours::ink = s.ink;
    colours::now = s.now;
    colours::well = s.well;
}

namespace fonts
{
    inline juce::Font ui (float size, bool bold = false)
    {
        auto* faces = style.face == Face::none ? nullptr : Faces::getInstance();
        const auto face = faces == nullptr ? nullptr : bold ? faces->bold (style.face) : faces->regular (style.face);
        if (face == nullptr)   // before any style is applied, or after shutdown
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
        setColour (juce::Slider::textBoxTextColourId, ink);
        setColour (juce::Slider::textBoxBackgroundColourId, label);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::outlineColourId, outline);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, dim);
        setColour (juce::PopupMenu::backgroundColourId, label);
        setColour (juce::PopupMenu::textColourId, ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, ink.withAlpha (0.14f));
        setColour (juce::PopupMenu::highlightedTextColourId, ink);
        setColour (juce::TextButton::buttonColourId, label);
        setColour (juce::TextButton::textColourOffId, ink);
        setColour (juce::TextButton::buttonOnColourId, now);   // the ink that marks an "on" label
        setColour (juce::TextButton::textColourOnId, ink);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, now);
        setColour (juce::ToggleButton::tickDisabledColourId, dim);
        setColour (juce::TabbedButtonBar::tabTextColourId, dim);
        setColour (juce::TabbedButtonBar::frontTextColourId, ink);
        setColour (juce::TabbedComponent::outlineColourId, outline);
        setColour (juce::ProgressBar::backgroundColourId, well);
        setColour (juce::ProgressBar::foregroundColourId, now);
        setColour (juce::ListBox::backgroundColourId, label);
        setColour (juce::ListBox::outlineColourId, outline);
        setColour (juce::ScrollBar::thumbColourId, rule);
        setColour (juce::TooltipWindow::backgroundColourId, label);
        setColour (juce::TooltipWindow::textColourId, ink);
        setColour (juce::TooltipWindow::outlineColourId, ink.withAlpha (0.4f));
    }

    // Printed caps on a label, tracked less than a title so they fit a cell.
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return fonts::ui (buttonHeight < 30 ? 9.5f : 11.0f, true).withExtraKerningFactor (style.titleKerning * 0.5f);
    }

    juce::Font getLabelFont (juce::Label& label) override { return label.getFont(); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return fonts::mono (11.5f); }
    juce::Font getPopupMenuFont() override { return fonts::mono (12.0f); }
    juce::Font getTabButtonFont (juce::TabBarButton&, float) override { return fonts::title (9.5f); }
    juce::Font getSliderPopupFont (juce::Slider&) override { return fonts::mono (12.0f); }

    // Values typed into a cream cell.
    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (fonts::mono (11.0f));
        label->setColour (juce::Label::backgroundColourId, colours::label);
        label->setColour (juce::Label::textColourId, colours::ink);
        label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::TextEditor::backgroundColourId, colours::label);
        label->setColour (juce::TextEditor::textColourId, colours::ink);
        label->setColour (juce::TextEditor::highlightColourId, colours::now.withAlpha (0.35f));
        return label;
    }

    // A stuck-on label: cream stock with a hairline of shadow along its bottom
    // edge. Switched on, it keeps its stock and navy print and is marked the
    // way the front tab is: ruled round in its source's ink, with the ink
    // thick along the top.
    static void drawLabelStock (juce::Graphics& g, juce::Rectangle<float> r, bool lifted, juce::Colour onInk = juce::Colours::transparentBlack)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (r.withTrimmedTop (r.getHeight() - 1.0f).translated (0.0f, 1.0f));
        g.setColour (lifted ? colours::label.brighter (0.06f) : colours::label);
        g.fillRect (r);
        if (onInk.isTransparent())
        {
            g.setColour (colours::ink.withAlpha (0.18f));
            g.drawRect (r, 1.0f);
        }
        else
        {
            g.setColour (onInk);
            g.drawRect (r, 1.0f);
            g.fillRect (r.withHeight (3.0f));
        }
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        if (! button.isEnabled())
        {
            g.setColour (colours::outline);
            g.drawRect (bounds, 1.0f);
            return;
        }
        // When toggled on, JUCE hands the on-colour as the background: it is the marking ink.
        drawLabelStock (g, bounds, highlighted && ! down, button.getToggleState() ? backgroundColour : juce::Colours::transparentBlack);
    }

    // The button text: printed caps on the label, ink or the on-colour.
    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setFont (getTextButtonFont (button, button.getHeight()));
        const auto on = button.getToggleState();
        auto colour = button.findColour (on ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
        if (! button.isEnabled())
            colour = colours::dim;
        g.setColour (colour);
        g.drawText (fonts::titleCase (button.getButtonText()), button.getLocalBounds().reduced (4, 0), juce::Justification::centred, true);
    }

    // A menu is a ruled cell: the typed value, a chevron, a hairline round it.
    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRect (bounds);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRect (bounds, 1.0f);

        const auto arrow = juce::Rectangle<float> ((float) width - 16.0f, (float) height * 0.5f - 2.0f, 7.0f, 4.0f);
        juce::Path p;
        p.startNewSubPath (arrow.getX(), arrow.getY());
        p.lineTo (arrow.getCentreX(), arrow.getBottom());
        p.lineTo (arrow.getRight(), arrow.getY());
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }

    // A square tick box, ticked in grease pencil; its text typed.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down) override
    {
        const float boxSize = 13.0f;
        auto bounds = button.getLocalBounds().toFloat();
        auto box = juce::Rectangle<float> (boxSize, boxSize).withCentre ({ bounds.getX() + boxSize * 0.5f + 2.0f, bounds.getCentreY() });
        g.setColour (colours::well.contrasting (down ? 0.12f : highlighted ? 0.05f : 0.0f));
        g.fillRect (box);
        g.setColour (button.getToggleState() ? button.findColour (juce::ToggleButton::tickColourId) : colours::outline);
        g.drawRect (box, 1.0f);
        if (button.getToggleState())
            g.fillRect (box.reduced (3.0f));

        g.setFont (fonts::mono (11.5f));
        g.setColour (button.findColour (juce::ToggleButton::textColourId).withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
        g.drawText (button.getButtonText(), bounds.withTrimmedLeft (boxSize + 10.0f).toNearestInt(), juce::Justification::centredLeft);
    }

    // Tabs are label tabs: the front one is cream stock with its source's ink
    // ruled along the top, the rest printed on the box.
    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool highlighted, bool) override
    {
        const auto area = button.getActiveArea().toFloat();
        const bool front = button.isFrontTab();
        const auto text = button.getButtonText();
        const auto accent = text.startsWith ("Brain 1") ? colours::brain1 : text.startsWith ("Brain 2") ? colours::brain2 : colours::neutralAccent;
        if (front)
        {
            g.setColour (colours::label);
            g.fillRect (area);
            g.setColour (accent);
            g.fillRect (area.withHeight (2.0f));
            g.setColour (colours::ink.withAlpha (0.18f));
            g.drawRect (area, 1.0f);
        }
        else
        {
            g.setColour (colours::outline);
            g.fillRect (area.withTrimmedTop (area.getHeight() - 1.0f));
        }
        g.setFont (fonts::title (9.5f));
        g.setColour (front ? colours::ink : highlighted ? colours::text : colours::dim);
        g.drawText (fonts::titleCase (text), area.toNearestInt(), juce::Justification::centred);
    }

    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}

    // Tooltips: a note on label stock, body text wrapped at a readable width.
    static juce::TextLayout tooltipLayout (const juce::String& text, float maxWidth)
    {
        juce::AttributedString s;
        s.setJustification (juce::Justification::topLeft);
        s.append (text, fonts::ui (12.5f), colours::ink);
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
        g.fillAll (colours::label);
        g.setColour (colours::ink.withAlpha (0.4f));
        g.drawRect (0, 0, width, height, 1);
        tooltipLayout (text, (float) width - 20.0f).draw (g, juce::Rectangle<float> (10.0f, 7.0f, (float) width - 20.0f, (float) height - 14.0f));
    }
    int getTabButtonBestWidth (juce::TabBarButton& button, int) override
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (fonts::title (9.5f), fonts::titleCase (button.getButtonText()), 0.0f, 0.0f);
        return juce::roundToInt (glyphs.getBoundingBox (0, -1, true).getWidth()) + 30;
    }

    // A knob is a thin ring, inked up to its value, with a pencil pointer; the
    // one under the hand lifts to full ink while the rest sit a step back.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float stroke = juce::jmax (1.5f, radius * 0.09f);
        const bool lifted = slider.isMouseOverOrDragging() || slider.hasKeyboardFocus (true);
        const float inkAlpha = lifted ? 1.0f : 0.78f;

        juce::Path track, value;
        track.addCentredArc (centre.x, centre.y, radius - stroke, radius - stroke, 0.0f, startAngle, endAngle, true);
        value.addCentredArc (centre.x, centre.y, radius - stroke, radius - stroke, 0.0f, startAngle, angle, true);

        g.setColour (colours::outline);   // the printed ring, readable even with nothing inked on it
        g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId).withMultipliedAlpha (inkAlpha));
        g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // Printed scale marks at the ends and the middle.
        g.setColour (colours::dim.withAlpha (0.7f));
        for (float a : { startAngle, (startAngle + endAngle) * 0.5f, endAngle })
            g.drawLine ({ centre.getPointOnCircumference (radius + 1.0f, a), centre.getPointOnCircumference (radius - stroke * 2.0f - 1.0f, a) }, 1.0f);

        const auto tip = centre.getPointOnCircumference (radius - stroke * 2.6f, angle);
        g.setColour (slider.findColour (juce::Slider::thumbColourId).withMultipliedAlpha (inkAlpha));
        g.drawLine ({ centre.getPointOnCircumference (radius * 0.2f, angle), tip }, lifted ? 2.0f : 1.6f);
    }
};

// A typed value on a cream tag: the label stock, the figure in the
// typewriter face, and optionally a source's ink ruled along its left edge.
inline void drawTag (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, float fontSize = 10.5f,
                     juce::Colour edge = juce::Colours::transparentBlack, juce::Justification just = juce::Justification::centred,
                     bool fitToText = false)
{
    if (fitToText)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (fonts::mono (fontSize), text, 0.0f, 0.0f);
        r = r.withWidth (std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()) + 14.0f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRect (r.withTrimmedTop (r.getHeight() - 1.0f).translated (0.0f, 1.0f));
    g.setColour (colours::label);
    g.fillRect (r);
    g.setColour (edge.isTransparent() ? colours::ink.withAlpha (0.18f) : edge);   // ruled round in the source's ink
    g.drawRect (r, 1.0f);
    g.setFont (fonts::mono (fontSize));
    g.setColour (colours::ink);
    g.drawText (text, r.reduced (5.0f, 0.0f).toNearestInt(), just, true);
}

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

// A small printed caption at the top-left of a region.
inline void drawCaption (juce::Graphics& g, juce::Rectangle<int> region, const juce::String& text, juce::Colour colour = colours::dim)
{
    g.setFont (fonts::title (9.5f));
    g.setColour (colour);
    g.drawText (fonts::titleCase (text), region.reduced (12, 7).removeFromTop (12), juce::Justification::centredLeft);
}

// The ground a map or plot is drawn on: a darker well cut into the region.
inline void drawWell (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (colours::well);
    g.fillRect (r);
    g.setColour (colours::outline);
    g.drawRect (r, 1.0f);
}
} // namespace cortex::ui
