#include "CepstrumView.h"
#include "Parameters.h"

namespace cortex::ui
{
namespace
{
    constexpr float maxQuefrency = 200.0f;   // the cutoff parameters' range
}

CepstrumView::CepstrumView (juce::AudioProcessorValueTreeState& state)
    : cutoffAParam (*state.getParameter (cortex::ParamIDs::cutoffA)),
      cutoffBParam (*state.getParameter (cortex::ParamIDs::cutoffB)),
      cutoffAAttachment (cutoffAParam, [this] (float v) { cutoffA = v; repaint(); }),
      cutoffBAttachment (cutoffBParam, [this] (float v) { cutoffB = v; repaint(); })
{
    cutoffAAttachment.sendInitialUpdate();
    cutoffBAttachment.sendInitialUpdate();
}

void CepstrumView::setSnapshot (const cortex::dsp::MorphSnapshot& s)
{
    snapshot = s;
    repaint();
}

juce::Rectangle<float> CepstrumView::plotArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f).withTrimmedTop (14.0f).withTrimmedBottom (12.0f);
}

float CepstrumView::xForQuefrency (float q) const
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * q / maxQuefrency;
}

float CepstrumView::quefrencyForX (float px) const
{
    const auto a = plotArea();
    return (px - a.getX()) / a.getWidth() * maxQuefrency;
}

void CepstrumView::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    const auto area = plotArea();

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (colours::dim);
    g.drawText ("cepstrum   |   left of a line: envelope, right: detail",
                getLocalBounds().reduced (12, 4).removeFromTop (14), juce::Justification::centredLeft);
    g.drawText ("quefrency 0-200 bins", getLocalBounds().reduced (12, 2).removeFromBottom (12), juce::Justification::centredRight);

    g.setColour (colours::outline);
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    if (snapshot.valid)
    {
        // Skip c0 (overall level); scale to the largest remaining coefficient.
        const int numQ = juce::jmin ((int) maxQuefrency, cortex::dsp::MorphSnapshot::numCoeffs);
        float peak = 1.0e-6f;
        for (int q = 1; q < numQ; ++q)
            peak = juce::jmax (peak, std::abs (snapshot.cepsA[(size_t) q]), std::abs (snapshot.cepsB[(size_t) q]));

        auto plot = [&] (const std::array<float, cortex::dsp::MorphSnapshot::numCoeffs>& c, juce::Colour colour)
        {
            juce::Path p;
            for (int q = 1; q < numQ; ++q)
            {
                const float px = xForQuefrency ((float) q);
                const float py = area.getCentreY() - area.getHeight() * 0.48f * c[(size_t) q] / peak;
                if (q == 1) p.startNewSubPath (px, py);
                else        p.lineTo (px, py);
            }
            g.setColour (colour.withAlpha (0.8f));
            g.strokePath (p, juce::PathStrokeType (1.3f));
        };
        plot (snapshot.cepsB, colours::b);
        plot (snapshot.cepsA, colours::a);
    }

    auto drawCutoff = [&] (float q, juce::Colour colour, const char* label, bool labelBelow)
    {
        const float px = xForQuefrency (q);
        g.setColour (colour);
        g.drawLine (px, area.getY(), px, area.getBottom(), 2.0f);
        g.setFont (juce::FontOptions (11.0f));
        const auto text = juce::String (label) + " " + juce::String (juce::roundToInt (q));
        const auto box = juce::Rectangle<float> (px + 4.0f, labelBelow ? area.getBottom() - 14.0f : area.getY(), 60.0f, 14.0f);
        g.drawText (text, box.toNearestInt(), juce::Justification::centredLeft);
    };
    drawCutoff (cutoffA, colours::a, "A", false);
    drawCutoff (cutoffB, colours::b, "B", true);

    // Where the cutoffs actually are right now (they move under modulation).
    if (snapshot.valid)
        for (auto [q, colour] : { std::pair { snapshot.cutoffA, colours::a }, std::pair { snapshot.cutoffB, colours::b } })
            if (std::abs (q - (colour == colours::a ? cutoffA : cutoffB)) > 0.5f)
            {
                const float px = xForQuefrency (q);
                g.setColour (colour.withAlpha (0.55f));
                const float dashes[] { 4.0f, 3.0f };
                g.drawDashedLine ({ px, area.getY(), px, area.getBottom() }, dashes, 2, 1.5f);
            }
}

juce::ParameterAttachment* CepstrumView::cutoffNear (float px)
{
    const float da = std::abs (px - xForQuefrency (cutoffA));
    const float db = std::abs (px - xForQuefrency (cutoffB));
    if (juce::jmin (da, db) > 8.0f)
        return nullptr;
    return da <= db ? &cutoffAAttachment : &cutoffBAttachment;
}

void CepstrumView::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (cutoffNear (e.position.x) != nullptr ? juce::MouseCursor::LeftRightResizeCursor
                                                         : juce::MouseCursor::NormalCursor);
}

void CepstrumView::mouseDown (const juce::MouseEvent& e)
{
    dragging = cutoffNear (e.position.x);
    if (dragging != nullptr)
        dragging->beginGesture();
}

void CepstrumView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging != nullptr)
        dragging->setValueAsPartOfGesture (juce::jlimit (4.0f, maxQuefrency, quefrencyForX (e.position.x)));
}

void CepstrumView::mouseUp (const juce::MouseEvent&)
{
    if (dragging != nullptr)
        dragging->endGesture();
    dragging = nullptr;
}
} // namespace cortex::ui
