#pragma once

// Shared helpers for the morph tests.

#include "dsp/CepstralMorph.h"
#include "dsp/STFT.h"
#include "dsp/Triangle.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace cortex::test
{
using namespace cortex::dsp;

// The classic two-input morph in the three-source engine: A is the Input, B is
// Brain 1, Brain 2 silent. x moves the envelope A -> B, y the detail A -> B.
inline MorphParams abParams (float x, float y, PhaseSource phase = PhaseSource::followDetail)
{
    MorphParams p;
    p.envWeights = { 1.0f - x, x, 0.0f };
    p.detailWeights = { 1.0f - y, y, 0.0f };
    p.phase = phase;
    return p;
}

inline const float* silentFrame()
{
    static const std::vector<float> zeros ((size_t) maxFftSize, 0.0f);
    return zeros.data();
}

// One morph frame from A (Input) and B (Brain 1).
inline void processAB (CepstralMorph& m, const float* a, const float* b, float* out)
{
    m.processFrame ({ a, b, silentFrame() }, out);
}

// One STFT sample from A (Input) and B (Brain 1) through `m`.
inline float stftAB (STFT& stft, CepstralMorph& m, float a, float b)
{
    return stft.processSample ({ a, b, 0.0f }, [&m] (const STFT::Frames& f, float* fo) { m.processFrame (f, fo); });
}

// Puts a plugin triangle ("env" or "detail") on a source's corner.
inline void setTriangleCorner (juce::AudioProcessorValueTreeState& state, bool envelope, Source s)
{
    const int corner = s == Source::input ? inputCorner : s == Source::brain1 ? brain1Corner : brain2Corner;
    auto set = [&] (const char* id, float v) { auto* p = state.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    set (envelope ? "envX" : "detailX", triangleCorner[corner].x);
    set (envelope ? "envY" : "detailY", triangleCorner[corner].y);
}

// Both triangles on the same source.
inline void setPureSource (juce::AudioProcessorValueTreeState& state, Source s)
{
    setTriangleCorner (state, true, s);
    setTriangleCorner (state, false, s);
}
} // namespace cortex::test
