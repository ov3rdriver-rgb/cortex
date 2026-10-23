#pragma once

#include "Theme.h"
#include "dsp/CepstralMorph.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace cortex::ui
{
// The real cepstra of each source in play, by quefrency, with each source's
// lifter cutoff as a line you can drag. Left of a source's line is its
// envelope, right of it its detail. Dashed lines show where the cutoffs
// actually are while modulation moves them.
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
    int cutoffNear (float px) const;   // source index, or -1

    struct Cutoff
    {
        juce::RangedAudioParameter* param = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        float value = 25.0f;
    };
    std::array<Cutoff, cortex::dsp::numSources> cutoffs;
    int dragging = -1;
    cortex::dsp::MorphSnapshot snapshot;
};
} // namespace cortex::ui
