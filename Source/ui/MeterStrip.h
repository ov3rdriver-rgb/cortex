#pragma once

#include "Theme.h"

namespace cortex::ui
{
// Input and output peak meters with a gate light: a quick answer to "is it
// getting signal, and is anything coming out?". Fed from the editor timer.
class MeterStrip final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    void setLevels (float inputPeak, float outputPeak, bool gateOpen, bool gateEnabled);

    void paint (juce::Graphics&) override;

private:
    void drawMeter (juce::Graphics&, juce::Rectangle<float>, const char* name, float level, float peakHold, juce::Colour);

    float inLevel = 0.0f, outLevel = 0.0f;       // displayed (with fall-back)
    float inHold = 0.0f, outHold = 0.0f;         // peak hold, decays slower
    bool gateOpen = true, gateEnabled = false;
};
} // namespace cortex::ui
