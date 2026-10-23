#pragma once

#include "Theme.h"
#include "dsp/Triangle.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace cortex::ui
{
// One morph triangle: Brain 1 at the top, Brain 2 bottom-left, Input
// bottom-right. Drag the dot to set how much each source contributes (to the
// envelope, or to the detail). The corners are tinted by their source's
// colour, and each corner shows its current weight.
class TrianglePad final : public juce::Component
{
public:
    TrianglePad (juce::AudioProcessorValueTreeState&, const char* xParamId, const char* yParamId, juce::String title);

    // Weights the morph is actually using (after missing brains fall back to
    // the Input), shown next to the corners.
    void setLiveWeights (const std::array<float, cortex::dsp::numSources>&);
    // Greyed out (e.g. the detail triangle while Link is on).
    void setFollowing (bool shouldFollow);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    // Screen position of a point in triangle space, and back (clamped onto the triangle).
    juce::Point<float> toScreen (cortex::dsp::TrianglePoint) const;
    cortex::dsp::TrianglePoint fromScreen (juce::Point<float>) const;

private:
    juce::Rectangle<float> triangleBounds() const;
    void setFromMouse (juce::Point<float>);

    juce::RangedAudioParameter& xParam;
    juce::RangedAudioParameter& yParam;
    juce::ParameterAttachment xAttachment, yAttachment;
    juce::String title;
    float x = 0.5f, y = 0.5f;
    std::array<float, cortex::dsp::numSources> liveWeights {};
    bool following = false, dragging = false;
};
} // namespace cortex::ui
