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
    g.setFont (juce::FontOptions (10.5f));
    g.setColour (colours::dim);
    g.drawText (name, r.removeFromLeft (26.0f).toNearestInt(), juce::Justification::centredLeft);

    auto bar = r.reduced (0.0f, 6.0f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bar, 2.0f);

    // Green up to -12 dB, amber to -3 dB, red above (positions in the 0..1 range).
    const float amber = 1.0f - 12.0f / -floorDb, red = 1.0f - 3.0f / -floorDb;
    auto segment = [&] (float from, float to, juce::Colour c)
    {
        const float a = juce::jlimit (0.0f, level, from), b = juce::jlimit (0.0f, level, to);
        if (b > a)
        {
            g.setColour (c);
            g.fillRect (bar.getX() + a * bar.getWidth(), bar.getY(), (b - a) * bar.getWidth(), bar.getHeight());
        }
    };
    segment (0.0f, amber, colour);
    segment (amber, red, juce::Colour (0xfff6c744));
    segment (red, 1.0f, juce::Colour (0xffff5c5c));

    if (hold > 0.0f)
    {
        g.setColour (colours::text.withAlpha (0.8f));
        g.fillRect (bar.getX() + hold * bar.getWidth() - 1.0f, bar.getY(), 2.0f, bar.getHeight());
    }
}

void MeterStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float rowHeight = r.getHeight() * 0.5f;

    // Gate light on the right, then two meter rows.
    auto gate = r.removeFromRight (46.0f);
    g.setFont (juce::FontOptions (10.5f));
    g.setColour (gateEnabled ? colours::text : colours::dim);
    g.drawText ("gate", gate.removeFromLeft (26.0f).toNearestInt(), juce::Justification::centredLeft);
    const auto light = juce::Rectangle<float> (10.0f, 10.0f).withCentre (gate.getCentre());
    g.setColour (! gateEnabled ? colours::outline : gateOpen ? colours::input : colours::input.withAlpha (0.2f));
    g.fillEllipse (light);
    if (gateEnabled && gateOpen)
    {
        g.setColour (colours::input.withAlpha (0.25f));
        g.fillEllipse (light.expanded (3.0f));
    }
    r.removeFromRight (8.0f);

    drawMeter (g, r.removeFromTop (rowHeight), "in", inLevel, inHold, colours::input);
    drawMeter (g, r, "out", outLevel, outHold, colours::result);
}
} // namespace cortex::ui
