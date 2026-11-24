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
    // Equilateral-ish: width = height * 1.15, centred, with room for the
    // corner names and their typed shares.
    auto area = getLocalBounds().toFloat().reduced (14.0f).withTrimmedTop (36.0f).withTrimmedBottom (30.0f);
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

    // The triangle: a flat well with a hairline, like a diagram printed on the box.
    juce::Path tri;
    tri.addTriangle (corner (0), corner (1), corner (2));
    g.setColour (colours::well.withAlpha (following ? 0.5f : 1.0f));
    g.fillPath (tri);
    g.setColour (colours::outline);
    g.strokePath (tri, juce::PathStrokeType (1.0f));

    // Each corner: its name printed small, its share typed large. The source
    // with the biggest share is inked fully; the others sit a step back.
    int leading = 0;
    for (int s = 1; s < numSources; ++s)
        if (liveWeights[(size_t) s] > liveWeights[(size_t) leading])
            leading = s;
    for (int c = 0; c < 3; ++c)
    {
        const int s = cornerSource[c];
        const auto p = corner (c);
        const auto share = juce::String (juce::roundToInt (liveWeights[(size_t) s] * 100.0f)) + "%";
        const float alpha = following ? 0.45f : s == leading ? 1.0f : 0.85f;
        juce::Rectangle<float> box (120.0f, 30.0f);
        juce::Justification just = juce::Justification::centred;
        if (c == 0)      box = box.withCentre ({ p.x, p.y - 20.0f });
        else if (c == 1) { box = box.withPosition (p.x - 6.0f, p.y + 4.0f); just = juce::Justification::centredLeft; }
        else             { box = box.withPosition (p.x - 114.0f, p.y + 4.0f); just = juce::Justification::centredRight; }

        g.setColour (colours::forSource (s).withAlpha (alpha));
        g.setFont (fonts::title (9.0f));
        g.drawText (fonts::titleCase (colours::sourceName (s)), box.removeFromTop (12.0f).toNearestInt(), just);
        g.setFont (fonts::mono (s == leading && ! following ? 19.0f : 15.0f));
        g.drawText (share, box.toNearestInt(), just);
    }

    // The dot: a flat grease-pencil mark with a chalk ring.
    const auto dot = toScreen (fromScreen (toScreen ({ x, y })));
    g.setColour (following ? colours::dim : colours::now);
    g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (dot));
    g.setColour (colours::text.withAlpha (following ? 0.4f : 1.0f));
    g.drawEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (dot), 1.2f);

    if (following)
    {
        g.setFont (fonts::title (9.5f));
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
