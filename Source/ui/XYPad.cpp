#include "XYPad.h"
#include "Parameters.h"

namespace cortex::ui
{
namespace
{
    constexpr int numPoints = 160;
    constexpr double minHz = 30.0, maxHz = 16000.0;
    constexpr float dbPerNeper = 8.685889638f;   // 20 / ln 10
    constexpr float displayRangeDb = 72.0f;
}

XYPad::XYPad (juce::AudioProcessorValueTreeState& state)
    : xParam (*state.getParameter (cortex::ParamIDs::morphX)),
      yParam (*state.getParameter (cortex::ParamIDs::morphY)),
      xAttachment (xParam, [this] (float v) { x = v; repaint(); }),
      yAttachment (yParam, [this] (float v) { y = v; repaint(); })
{
    xAttachment.sendInitialUpdate();
    yAttachment.sendInitialUpdate();
}

juce::Rectangle<float> XYPad::padArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f);
}

void XYPad::setSnapshot (const cortex::dsp::MorphSnapshot& s)
{
    snapshot = s;
    repaint();
}

void XYPad::paint (juce::Graphics& g)
{
    const auto area = padArea();
    drawPanel (g, getLocalBounds().toFloat());

    // Grid.
    g.setColour (colours::outline.withAlpha (0.6f));
    for (int i = 1; i < 4; ++i)
    {
        const float fx = area.getX() + area.getWidth() * (float) i / 4.0f;
        const float fy = area.getY() + area.getHeight() * (float) i / 4.0f;
        g.drawVerticalLine ((int) fx, area.getY(), area.getBottom());
        g.drawHorizontalLine ((int) fy, area.getX(), area.getRight());
    }

    // Spectral envelopes, log frequency across, dB up.
    if (snapshot.valid)
    {
        std::array<std::array<float, numPoints>, 3> curves {};
        float top = -1.0e9f;
        for (int i = 0; i < numPoints; ++i)
        {
            const double hz = minHz * std::pow (maxHz / minHz, (double) i / (numPoints - 1));
            const double omega = juce::MathConstants<double>::twoPi * hz / snapshot.sampleRate;
            const float ea = dbPerNeper * cepstralEnvelope (snapshot.cepsA.data(), (int) snapshot.cepsA.size(), snapshot.cutoffA, omega);
            const float eb = dbPerNeper * cepstralEnvelope (snapshot.cepsB.data(), (int) snapshot.cepsB.size(), snapshot.cutoffB, omega);
            curves[0][(size_t) i] = ea;
            curves[1][(size_t) i] = eb;
            curves[2][(size_t) i] = ea + snapshot.x * (eb - ea);   // the morph's envelope crossfade
            top = juce::jmax (top, ea, eb);
        }
        top += 6.0f;

        const juce::Colour curveColours[] { colours::a, colours::b, colours::result };
        for (int c = 0; c < 3; ++c)
        {
            juce::Path p;
            for (int i = 0; i < numPoints; ++i)
            {
                const float px = area.getX() + area.getWidth() * (float) i / (numPoints - 1);
                const float norm = juce::jlimit (0.0f, 1.0f, (top - curves[(size_t) c][(size_t) i]) / displayRangeDb);
                const float py = area.getY() + area.getHeight() * norm;
                if (i == 0) p.startNewSubPath (px, py);
                else        p.lineTo (px, py);
            }
            g.setColour (curveColours[c].withAlpha (c == 2 ? 0.85f : 0.5f));
            g.strokePath (p, juce::PathStrokeType (c == 2 ? 2.0f : 1.5f));
        }
    }

    // Corner captions.
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (colours::dim);
    const auto inner = area.reduced (6.0f).toNearestInt();
    g.drawText ("A", inner, juce::Justification::bottomLeft);
    g.drawText ("B", inner, juce::Justification::topRight);
    g.drawText ("A envelope + B detail", inner, juce::Justification::topLeft);
    g.drawText ("B envelope + A detail", inner, juce::Justification::bottomRight);

    // Legend.
    auto legend = area.reduced (6.0f).removeFromBottom (14.0f).withTrimmedLeft (area.getWidth() * 0.25f)
                      .withWidth (area.getWidth() * 0.5f).toNearestInt();
    const std::pair<const char*, juce::Colour> keys[] { { "A", colours::a }, { "B", colours::b }, { "result", colours::result } };
    const int keyWidth = legend.getWidth() / 3;
    for (auto& [name, colour] : keys)
    {
        auto k = legend.removeFromLeft (keyWidth);
        g.setColour (colour);
        g.fillRect (k.removeFromLeft (10).withSizeKeepingCentre (10, 3));
        g.setColour (colours::dim);
        g.drawText (name, k.withTrimmedLeft (4), juce::Justification::centredLeft);
    }

    // The dot.
    const juce::Point<float> dot { area.getX() + x * area.getWidth(), area.getBottom() - y * area.getHeight() };
    g.setColour (colours::result.withAlpha (0.25f));
    g.fillEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (dot));
    g.setColour (colours::text);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (dot));

    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("envelope " + juce::String (x, 2) + "   detail " + juce::String (y, 2),
                getLocalBounds().removeFromTop (22).withTrimmedRight (14), juce::Justification::centredRight);
}

void XYPad::setFromMouse (juce::Point<float> p)
{
    const auto area = padArea();
    const float nx = juce::jlimit (0.0f, 1.0f, (p.x - area.getX()) / area.getWidth());
    const float ny = juce::jlimit (0.0f, 1.0f, (area.getBottom() - p.y) / area.getHeight());
    xAttachment.setValueAsPartOfGesture (xParam.convertFrom0to1 (nx));
    yAttachment.setValueAsPartOfGesture (yParam.convertFrom0to1 (ny));
}

void XYPad::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    xAttachment.beginGesture();
    yAttachment.beginGesture();
    setFromMouse (e.position);
}

void XYPad::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        setFromMouse (e.position);
}

void XYPad::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;
    dragging = false;
    xAttachment.endGesture();
    yAttachment.endGesture();
}

void XYPad::mouseDoubleClick (const juce::MouseEvent&)
{
    // Back to the defaults (brain timbre on your detail).
    xAttachment.setValueAsCompleteGesture (xParam.convertFrom0to1 (xParam.getDefaultValue()));
    yAttachment.setValueAsCompleteGesture (yParam.convertFrom0to1 (yParam.getDefaultValue()));
}
} // namespace cortex::ui
