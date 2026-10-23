#include "CepstrumView.h"
#include "Parameters.h"

namespace cortex::ui
{
using namespace cortex::dsp;

namespace
{
    constexpr float maxQuefrency = 200.0f;   // the cutoff parameters' range
}

CepstrumView::CepstrumView (juce::AudioProcessorValueTreeState& state)
{
    const char* ids[] { cortex::ParamIDs::cutoffIn, cortex::ParamIDs::cutoffB1, cortex::ParamIDs::cutoffB2 };
    for (int s = 0; s < numSources; ++s)
    {
        auto& c = cutoffs[(size_t) s];
        c.param = state.getParameter (ids[s]);
        c.attachment = std::make_unique<juce::ParameterAttachment> (*c.param, [this, s] (float v) { cutoffs[(size_t) s].value = v; repaint(); });
        c.attachment->sendInitialUpdate();
    }
}

void CepstrumView::setSnapshot (const MorphSnapshot& s)
{
    snapshot = s;
    repaint();
}

juce::Rectangle<float> CepstrumView::plotArea() const
{
    return getLocalBounds().toFloat().reduced (12.0f, 8.0f).withTrimmedTop (16.0f).withTrimmedBottom (12.0f);
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
    g.drawText ("cepstrum: drag a line to set that source's envelope / detail split",
                getLocalBounds().reduced (12, 6).removeFromTop (14), juce::Justification::centredLeft);
    g.drawText ("quefrency 0-200 bins", getLocalBounds().reduced (12, 2).removeFromBottom (12), juce::Justification::centredRight);

    g.setColour (colours::outline);
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    if (snapshot.valid)
    {
        // Skip c0 (overall level); scale to the largest remaining coefficient.
        const int numQ = juce::jmin ((int) maxQuefrency, MorphSnapshot::numCoeffs);
        float peak = 1.0e-6f;
        for (int s = 0; s < numSources; ++s)
            if (snapshot.used[(size_t) s])
                for (int q = 1; q < numQ; ++q)
                    peak = juce::jmax (peak, std::abs (snapshot.ceps[(size_t) s][(size_t) q]));

        for (int s = numSources - 1; s >= 0; --s)
        {
            if (! snapshot.used[(size_t) s])
                continue;
            juce::Path p;
            for (int q = 1; q < numQ; ++q)
            {
                const float px = xForQuefrency ((float) q);
                const float py = area.getCentreY() - area.getHeight() * 0.48f * snapshot.ceps[(size_t) s][(size_t) q] / peak;
                if (q == 1) p.startNewSubPath (px, py);
                else        p.lineTo (px, py);
            }
            g.setColour (colours::forSource (s).withAlpha (0.75f));
            g.strokePath (p, juce::PathStrokeType (1.2f));
        }
    }

    for (int s = 0; s < numSources; ++s)
    {
        const float q = cutoffs[(size_t) s].value;
        const float px = xForQuefrency (q);
        const auto colour = colours::forSource (s);
        g.setColour (colour);
        g.drawLine (px, area.getY(), px, area.getBottom(), 2.0f);
        g.setFont (juce::FontOptions (11.0f));
        const auto label = juce::String (colours::sourceName (s)) + " " + juce::String (juce::roundToInt (q));
        g.drawText (label, juce::Rectangle<float> (px + 4.0f, area.getY() + (float) s * 14.0f, 90.0f, 14.0f).toNearestInt(),
                    juce::Justification::centredLeft);

        // Where the cutoff actually is right now (it moves under modulation).
        if (snapshot.valid && std::abs (snapshot.cutoff[(size_t) s] - q) > 0.5f)
        {
            const float live = xForQuefrency (snapshot.cutoff[(size_t) s]);
            g.setColour (colour.withAlpha (0.55f));
            const float dashes[] { 4.0f, 3.0f };
            g.drawDashedLine ({ live, area.getY(), live, area.getBottom() }, dashes, 2, 1.5f);
        }
    }
}

int CepstrumView::cutoffNear (float px) const
{
    int best = -1;
    float bestDistance = 8.0f;
    for (int s = 0; s < numSources; ++s)
    {
        const float d = std::abs (px - xForQuefrency (cutoffs[(size_t) s].value));
        if (d < bestDistance)
        {
            bestDistance = d;
            best = s;
        }
    }
    return best;
}

void CepstrumView::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (cutoffNear (e.position.x) >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void CepstrumView::mouseDown (const juce::MouseEvent& e)
{
    dragging = cutoffNear (e.position.x);
    if (dragging >= 0)
        cutoffs[(size_t) dragging].attachment->beginGesture();
}

void CepstrumView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging >= 0)
        cutoffs[(size_t) dragging].attachment->setValueAsPartOfGesture (juce::jlimit (4.0f, maxQuefrency, quefrencyForX (e.position.x)));
}

void CepstrumView::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        cutoffs[(size_t) dragging].attachment->endGesture();
    dragging = -1;
}
} // namespace cortex::ui
