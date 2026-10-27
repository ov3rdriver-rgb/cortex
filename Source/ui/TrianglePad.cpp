#include "TrianglePad.h"

namespace cortex::ui
{
using namespace cortex::dsp;

namespace
{
    constexpr int cornerSource[3] { brain1, brain2, input };   // indexed by triangle corner
}

TrianglePad::TrianglePad (juce::AudioProcessorValueTreeState& state, const char* xParamId, const char* yParamId, juce::String titleText)
    : xParam (*state.getParameter (xParamId)),
      yParam (*state.getParameter (yParamId)),
      xAttachment (xParam, [this] (float v) { x = v; repaint(); }),
      yAttachment (yParam, [this] (float v) { y = v; repaint(); }),
      title (std::move (titleText))
{
    xAttachment.sendInitialUpdate();
    yAttachment.sendInitialUpdate();
    liveWeights = triangleWeights (x, y);
}

void TrianglePad::setLiveWeights (const std::array<float, numSources>& w)
{
    liveWeights = w;
    repaint();
}

void TrianglePad::setFollowing (bool shouldFollow)
{
    if (following != shouldFollow)
    {
        following = shouldFollow;
        repaint();
    }
}

juce::Rectangle<float> TrianglePad::triangleBounds() const
{
    // Equilateral-ish: width = height * 1.15, centred, with room for labels.
    auto area = getLocalBounds().toFloat().reduced (14.0f).withTrimmedTop (22.0f).withTrimmedBottom (20.0f);
    const float h = juce::jmin (area.getHeight(), area.getWidth() / 1.15f);
    return area.withSizeKeepingCentre (h * 1.15f, h);
}

juce::Point<float> TrianglePad::toScreen (TrianglePoint p) const
{
    const auto b = triangleBounds();
    return { b.getX() + p.x * b.getWidth(), b.getBottom() - p.y * b.getHeight() };
}

TrianglePoint TrianglePad::fromScreen (juce::Point<float> s) const
{
    const auto b = triangleBounds();
    const float px = (s.x - b.getX()) / b.getWidth();
    const float py = (b.getBottom() - s.y) / b.getHeight();

    // Clamp onto the triangle by rebuilding the point from clamped weights.
    const auto w = triangleWeights (px, py);
    TrianglePoint out { 0.0f, 0.0f };
    for (int c = 0; c < 3; ++c)
    {
        out.x += w[(size_t) cornerSource[c]] * triangleCorner[c].x;
        out.y += w[(size_t) cornerSource[c]] * triangleCorner[c].y;
    }
    return out;
}

void TrianglePad::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());

    drawCaption (g, getLocalBounds(), title, colours::text);

    const auto corner = [this] (int c) { return toScreen (triangleCorner[c]); };

    // Fill: a soft gradient from each corner's colour.
    juce::Path tri;
    tri.addTriangle (corner (0), corner (1), corner (2));
    for (int c = 0; c < 3; ++c)
    {
        const auto mid = (corner ((c + 1) % 3) + corner ((c + 2) % 3)) * 0.5f;
        juce::ColourGradient grad (colours::forSource (cornerSource[c]).withAlpha (following ? 0.08f : 0.22f), corner (c),
                                   colours::forSource (cornerSource[c]).withAlpha (0.0f), mid, false);
        g.setGradientFill (grad);
        g.fillPath (tri);
    }
    g.setColour (colours::rule);
    g.strokePath (tri, juce::PathStrokeType (1.0f));

    // Corner labels with the live weight.
    g.setFont (fonts::title (10.0f, false));
    for (int c = 0; c < 3; ++c)
    {
        const int s = cornerSource[c];
        const auto p = corner (c);
        const auto text = fonts::titleCase (colours::sourceName (s)) + "  " + juce::String (juce::roundToInt (liveWeights[(size_t) s] * 100.0f)) + "%";
        juce::Rectangle<float> box (110.0f, 14.0f);
        if (c == 0)      box = box.withCentre ({ p.x, p.y - 10.0f });
        else if (c == 1) box = box.withPosition (p.x - 4.0f, p.y + 4.0f);
        else             box = box.withPosition (p.x - 106.0f, p.y + 4.0f);
        g.setColour (colours::forSource (s).withAlpha (following ? 0.5f : 1.0f));
        g.drawText (text, box.toNearestInt(), c == 0 ? juce::Justification::centred
                                                     : c == 1 ? juce::Justification::centredLeft : juce::Justification::centredRight);
    }

    // The dot.
    const auto dot = toScreen (fromScreen (toScreen ({ x, y })));
    g.setColour (colours::result.withAlpha (following ? 0.15f : 0.3f));
    g.fillEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (dot));
    g.setColour (following ? colours::dim : colours::text);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (dot));

    if (following)
    {
        g.setFont (fonts::title (10.0f, false));
        g.setColour (colours::dim);
        g.drawText (fonts::titleCase ("linked"), getLocalBounds().reduced (12, 7).removeFromTop (12), juce::Justification::centredRight);
    }
}

void TrianglePad::setFromMouse (juce::Point<float> p)
{
    const auto t = fromScreen (p);
    xAttachment.setValueAsPartOfGesture (xParam.convertFrom0to1 (t.x));
    yAttachment.setValueAsPartOfGesture (yParam.convertFrom0to1 (t.y));
}

void TrianglePad::mouseDown (const juce::MouseEvent& e)
{
    if (following)
        return;
    dragging = true;
    xAttachment.beginGesture();
    yAttachment.beginGesture();
    setFromMouse (e.position);
}

void TrianglePad::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        setFromMouse (e.position);
}

void TrianglePad::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;
    dragging = false;
    xAttachment.endGesture();
    yAttachment.endGesture();
}

void TrianglePad::mouseDoubleClick (const juce::MouseEvent&)
{
    if (following)
        return;
    xAttachment.setValueAsCompleteGesture (xParam.convertFrom0to1 (xParam.getDefaultValue()));
    yAttachment.setValueAsCompleteGesture (yParam.convertFrom0to1 (yParam.getDefaultValue()));
}
} // namespace cortex::ui
