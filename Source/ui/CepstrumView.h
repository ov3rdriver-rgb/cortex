#pragma once

#include "Theme.h"
#include "dsp/CepstralMorph.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace cortex::ui
{
// The real cepstra of A and B by quefrency, with each input's lifter cutoff as
// a line you can drag. Left of a cutoff is that input's envelope, right its detail.
class CepstrumView final : public juce::Component
{
public:
    explicit CepstrumView (juce::AudioProcessorValueTreeState&);

    void setSnapshot (const cortex::dsp::MorphSnapshot&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const;
    float xForQuefrency (float q) const;
    float quefrencyForX (float px) const;
    juce::ParameterAttachment* cutoffNear (float px);

    juce::RangedAudioParameter& cutoffAParam;
    juce::RangedAudioParameter& cutoffBParam;
    juce::ParameterAttachment cutoffAAttachment, cutoffBAttachment;
    float cutoffA = 25.0f, cutoffB = 25.0f;

    juce::ParameterAttachment* dragging = nullptr;
    cortex::dsp::MorphSnapshot snapshot;
};
} // namespace cortex::ui
