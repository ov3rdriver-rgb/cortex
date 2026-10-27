#pragma once

#include "Theme.h"
#include "dsp/CepstralMorph.h"

namespace cortex::ui
{
// Live spectral envelopes (log frequency across, dB up) of each source in
// play and of the morph's result envelope.
class EnvelopeView final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    void setSnapshot (const cortex::dsp::MorphSnapshot& s) { snapshot = s; repaint(); }
    void paint (juce::Graphics&) override;

private:
    cortex::dsp::MorphSnapshot snapshot;
};
} // namespace cortex::ui
