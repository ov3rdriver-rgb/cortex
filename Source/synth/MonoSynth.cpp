#include "MonoSynth.h"

#include <cmath>

namespace cortex::synth
{
namespace
{
    // One-pole coefficient that covers ~99% of the way in `ms`.
    double onePole (double ms, double sampleRate)
    {
        if (ms <= 0.0)
            return 1.0;
        return 1.0 - std::exp (-4.6 / (ms * 0.001 * sampleRate));
    }

    // PolyBLEP residual for a unit step at phase 0 (t in [0, 1), dt = increment).
    float polyBlep (double t, double dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return (float) (t + t - t * t - 1.0);
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return (float) (t * t + t + t + 1.0);
        }
        return 0.0f;
    }
}

void MonoSynth::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    setGlide (0.0f);
    setEnvelope (5.0f, 200.0f);
    reset();
}

void MonoSynth::reset()
{
    numHeld = 0;
    phase = 0.0;
    env = envTarget = 0.0f;
    bendSemitones = 0.0f;
}

void MonoSynth::setGlide (float milliseconds) noexcept
{
    glideCoeff = onePole (milliseconds, sampleRate);
}

void MonoSynth::setEnvelope (float attackMs, float releaseMs) noexcept
{
    attackCoeff = (float) onePole (attackMs, sampleRate);
    releaseCoeff = (float) onePole (releaseMs, sampleRate);
}

void MonoSynth::render (const juce::MidiBuffer& midi, float* out, int numSamples) noexcept
{
    int pos = 0;
    for (const auto metadata : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
        for (; pos < eventPos; ++pos)
            out[pos] = nextSample();
        handle (metadata.getMessage());
    }
    for (; pos < numSamples; ++pos)
        out[pos] = nextSample();
}

void MonoSynth::handle (const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        numHeld = 0;
        envTarget = 0.0f;
    }
    else if (m.isPitchWheel())
    {
        bendSemitones = (float) (m.getPitchWheelValue() - 8192) / 8192.0f * bendRange;
    }
}

void MonoSynth::noteOn (int note, float velocity) noexcept
{
    // Move the note to the top of the stack.
    noteOff (note);
    const bool legato = numHeld > 0;
    if (numHeld < (int) held.size())
        held[(size_t) numHeld++] = note;

    level = 0.3f + 0.7f * velocity;
    setTargetNote (note);
    if (! legato)
    {
        // A fresh phrase starts on pitch; only legato notes glide.
        logFreq = targetLogFreq;
        envTarget = 1.0f;
    }
}

void MonoSynth::noteOff (int note) noexcept
{
    int w = 0;
    for (int r = 0; r < numHeld; ++r)
        if (held[(size_t) r] != note)
            held[(size_t) w++] = held[(size_t) r];
    numHeld = w;

    if (numHeld > 0)
        setTargetNote (held[(size_t) (numHeld - 1)]);
    else
        envTarget = 0.0f;
}

void MonoSynth::setTargetNote (int note) noexcept
{
    targetLogFreq = std::log2 (440.0) + (note - 69) / 12.0;
}

float MonoSynth::nextSample() noexcept
{
    logFreq += (targetLogFreq - logFreq) * glideCoeff;
    env += (envTarget - env) * (envTarget > env ? attackCoeff : releaseCoeff);

    const double freq = std::exp2 (logFreq + bendSemitones / 12.0);
    const double dt = juce::jmin (0.5, freq / sampleRate);

    float v = 0.0f;
    switch (waveform)
    {
        case Waveform::saw:
            v = (float) (2.0 * phase - 1.0) - polyBlep (phase, dt);
            break;
        case Waveform::square:
        {
            const double shifted = phase + 0.5 - std::floor (phase + 0.5);
            v = (phase < 0.5 ? 1.0f : -1.0f) + polyBlep (phase, dt) - polyBlep (shifted, dt);
            break;
        }
        case Waveform::triangle:
            v = (float) (1.0 - 4.0 * std::abs (phase - 0.5));
            break;
        case Waveform::sine:
            v = (float) std::sin (juce::MathConstants<double>::twoPi * phase);
            break;
    }

    phase += dt;
    if (phase >= 1.0)
        phase -= 1.0;

    return 0.5f * v * env * level;
}
} // namespace cortex::synth
