#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

namespace cortex::synth
{
enum class Waveform
{
    saw = 0,
    square,
    triangle,
    sine
};

// The instrument's Input source: one oscillator, played monophonically from MIDI.
// Last-note priority; a note played while another is held glides to it without
// retriggering the envelope (legato), releasing it returns to the previous held
// note. Realtime-safe after prepare().
class MonoSynth
{
public:
    void prepare (double sampleRate);
    void reset();

    void setWaveform (Waveform w) noexcept { waveform = w; }
    void setGlide (float milliseconds) noexcept;
    void setEnvelope (float attackMs, float releaseMs) noexcept;
    void setBendRange (float semitones) noexcept { bendRange = semitones; }

    // Renders numSamples into `out` (overwriting), handling MIDI events at
    // their sample positions.
    void render (const juce::MidiBuffer&, float* out, int numSamples) noexcept;

    float getCurrentFrequency() const noexcept { return (float) std::exp2 (logFreq); }
    bool isNoteHeld() const noexcept { return numHeld > 0; }
    float getEnvelope() const noexcept { return env; }

private:
    void handle (const juce::MidiMessage&) noexcept;
    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void setTargetNote (int note) noexcept;
    float nextSample() noexcept;

    double sampleRate = 48000.0;
    Waveform waveform = Waveform::saw;

    // Held notes in press order; the last one sounds.
    std::array<int, 128> held {};
    int numHeld = 0;

    double phase = 0.0;
    double logFreq = std::log2 (440.0), targetLogFreq = std::log2 (440.0);   // log2 Hz
    double glideCoeff = 1.0;
    float bendSemitones = 0.0f, bendRange = 2.0f;

    float env = 0.0f, envTarget = 0.0f, attackCoeff = 1.0f, releaseCoeff = 1.0f;
    float level = 0.0f;
};
} // namespace cortex::synth
