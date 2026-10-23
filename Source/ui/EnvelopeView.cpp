#include "EnvelopeView.h"

namespace cortex::ui
{
namespace
{
    constexpr int numPoints = 200;
    constexpr double minHz = 30.0, maxHz = 16000.0;
    constexpr float dbPerNeper = 8.685889638f;   // 20 / ln 10
    constexpr float displayRangeDb = 72.0f;
}

void EnvelopeView::paint (juce::Graphics& g)
{
    using namespace cortex::dsp;
    drawPanel (g, getLocalBounds().toFloat());

    auto area = getLocalBounds().toFloat().reduced (12.0f, 8.0f);
    auto header = area.removeFromTop (14.0f);
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (colours::dim);
    g.drawText ("spectral envelopes", header.toNearestInt(), juce::Justification::centredLeft);

    // Legend.
    auto legend = header.removeFromRight (260.0f).toNearestInt();
    for (int s : { (int) input, (int) brain1, (int) brain2, -1 })
    {
        auto key = legend.removeFromLeft (65);
        g.setColour (s < 0 ? colours::result : colours::forSource (s));
        g.fillRect (key.removeFromLeft (10).withSizeKeepingCentre (10, 3));
        g.setColour (colours::dim);
        g.drawText (s < 0 ? "result" : colours::sourceName (s), key.withTrimmedLeft (4), juce::Justification::centredLeft);
    }

    // Grid at 100 Hz, 1 kHz, 10 kHz.
    auto xFor = [&] (double hz) { return area.getX() + area.getWidth() * (float) (std::log (hz / minHz) / std::log (maxHz / minHz)); };
    g.setColour (colours::outline.withAlpha (0.7f));
    for (double hz : { 100.0, 1000.0, 10000.0 })
    {
        g.drawVerticalLine ((int) xFor (hz), area.getY(), area.getBottom());
        g.setColour (colours::dim.withAlpha (0.6f));
        g.drawText (hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (xFor (hz) + 3.0f, area.getBottom() - 12.0f, 30.0f, 12.0f).toNearestInt(),
                    juce::Justification::centredLeft);
        g.setColour (colours::outline.withAlpha (0.7f));
    }

    if (! snapshot.valid)
        return;

    std::array<std::array<float, numPoints>, numSources + 1> curves {};
    float top = -1.0e9f;
    for (int i = 0; i < numPoints; ++i)
    {
        const double hz = minHz * std::pow (maxHz / minHz, (double) i / (numPoints - 1));
        const double omega = juce::MathConstants<double>::twoPi * hz / snapshot.sampleRate;
        float result = 0.0f;
        for (int s = 0; s < numSources; ++s)
        {
            if (! snapshot.used[(size_t) s])
                continue;
            const float e = dbPerNeper * cepstralEnvelope (snapshot.ceps[(size_t) s].data(), MorphSnapshot::numCoeffs,
                                                           snapshot.cutoff[(size_t) s], omega);
            curves[(size_t) s][(size_t) i] = e;
            result += snapshot.envWeights[(size_t) s] * e;   // the morph's envelope blend
            top = juce::jmax (top, e);
        }
        curves[numSources][(size_t) i] = result;
    }
    top += 6.0f;

    for (int c = 0; c <= numSources; ++c)
    {
        if (c < numSources && ! snapshot.used[(size_t) c])
            continue;
        juce::Path p;
        for (int i = 0; i < numPoints; ++i)
        {
            const float px = area.getX() + area.getWidth() * (float) i / (numPoints - 1);
            const float norm = juce::jlimit (0.0f, 1.0f, (top - curves[(size_t) c][(size_t) i]) / displayRangeDb);
            const float py = area.getY() + area.getHeight() * norm;
            if (i == 0) p.startNewSubPath (px, py);
            else        p.lineTo (px, py);
        }
        const bool isResult = c == numSources;
        g.setColour (isResult ? colours::result : colours::forSource (c).withAlpha (0.6f));
        g.strokePath (p, juce::PathStrokeType (isResult ? 2.2f : 1.4f));
    }
}
} // namespace cortex::ui
