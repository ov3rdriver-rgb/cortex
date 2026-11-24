#include "MeterStrip.h"

namespace cortex::ui
{
namespace
{
    constexpr float floorDb = -60.0f;
    constexpr float fallPerTick = 0.04f;        // in the 0..1 meter range, per timer tick (30 Hz)
    constexpr float holdFallPerTick = 0.008f;

    float toMeter (float linear)
    {
        return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (linear, floorDb) - floorDb) / -floorDb);
    }
}

void MeterStrip::setLevels (float inputPeak, float outputPeak, bool open, bool enabled)
{
    // Peaks are per audio block, so meter ballistics: jump up, fall back.
    auto update = [] (float& level, float& hold, float peak)
    {
        const float m = toMeter (peak);
        level = juce::jmax (m, level - fallPerTick);
        hold = juce::jmax (m, hold - holdFallPerTick);
    };
    update (inLevel, inHold, inputPeak);
    update (outLevel, outHold, outputPeak);
    gateOpen = open;
    gateEnabled = enabled;
    repaint();
}

void MeterStrip::drawMeter (juce::Graphics& g, juce::Rectangle<float> r, const char* name, float level, float hold, juce::Colour colour)
{
    g.setFont (fonts::title (9.0f));
    g.setColour (colours::dim);
    g.drawText (fonts::titleCase (name), r.removeFromLeft (28.0f).toNearestInt(), juce::Justification::centredLeft);

    // A ruled level scale: a well with the level inked in, grease pencil above -3 dB.
    auto bar = r.reduced (0.0f, 6.0f);
    g.setColour (colours::well);
    g.fillRect (bar);
    const float red = 1.0f - 3.0f / -floorDb;
    auto segment = [&] (float from, float to, juce::Colour c)
    {
        const float a = juce::jlimit (0.0f, level, from), b = juce::jlimit (0.0f, level, to);
        if (b > a)
        {
            g.setColour (c);
            g.fillRect (bar.getX() + a * bar.getWidth(), bar.getY(), (b - a) * bar.getWidth(), bar.getHeight());
        }
    };
    segment (0.0f, red, colour);
    segment (red, 1.0f, colours::now);
    g.setColour (colours::outline);
    g.drawRect (bar, 1.0f);
    for (float db : { -48.0f, -36.0f, -24.0f, -12.0f })   // scale marks
        g.fillRect (bar.getX() + (1.0f + db / -floorDb) * bar.getWidth(), bar.getBottom() - 2.0f, 1.0f, 2.0f);

    if (hold > 0.0f)
    {
        g.setColour (colours::text.withAlpha (0.85f));
        g.fillRect (bar.getX() + hold * bar.getWidth() - 1.0f, bar.getY(), 1.5f, bar.getHeight());
    }
}

void MeterStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float rowHeight = r.getHeight() * 0.5f;

    // Gate light on the right, then two meter rows.
    auto gate = r.removeFromRight (50.0f);
    g.setFont (fonts::title (9.0f));
    g.setColour (gateEnabled ? colours::text : colours::dim);
    g.drawText (fonts::titleCase ("gate"), gate.removeFromLeft (30.0f).toNearestInt(), juce::Justification::centredLeft);
    // The gate light: a grease-pencil mark when the gate is open.
    const auto light = juce::Rectangle<float> (9.0f, 9.0f).withCentre (gate.getCentre());
    g.setColour (! gateEnabled ? colours::rule : gateOpen ? colours::now : colours::now.withAlpha (0.2f));
    g.fillEllipse (light);
    g.setColour (colours::outline);
    g.drawEllipse (light, 1.0f);
    r.removeFromRight (8.0f);

    drawMeter (g, r.removeFromTop (rowHeight), "in", inLevel, inHold, colours::input);
    drawMeter (g, r, "out", outLevel, outHold, colours::result);
}
} // namespace cortex::ui
