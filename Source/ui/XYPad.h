#pragma once

#include "Theme.h"
#include "dsp/CepstralMorph.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace cortex::ui
{
// The morph's XY pad: x = Morph Envelope (A -> B), y = Morph Detail (A -> B).
// Behind the dot, the live spectral envelopes of A, B and the result.
class XYPad final : public juce::Component
{
public:
    explicit XYPad (juce::AudioProcessorValueTreeState&);

    void setSnapshot (const cortex::dsp::MorphSnapshot&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> padArea() const;
    void setFromMouse (juce::Point<float>);

    juce::RangedAudioParameter& xParam;
    juce::RangedAudioParameter& yParam;
    juce::ParameterAttachment xAttachment, yAttachment;
    float x = 0.0f, y = 0.0f;

    cortex::dsp::MorphSnapshot snapshot;
    bool dragging = false;
};
} // namespace cortex::ui
