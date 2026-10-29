#pragma once

#include "Parameters.h"

#include <juce_core/juce_core.h>

#include <map>

namespace cortex::ui::tips
{
// What each parameter does, in the words a tooltip needs: what it changes,
// what the range means, and what to reach for it for.
inline juce::String forParam (const juce::String& id)
{
    using namespace cortex::ParamIDs;
    static const std::map<juce::String, juce::String> texts {
        { envX, "Envelope triangle. Drag the dot to set how much of the spectral envelope (the tone colour) comes from Brain 1 (top), Brain 2 (bottom left) and the Input (bottom right). Double-click to reset." },
        { envY, "Envelope triangle, vertical position." },
        { detailX, "Detail triangle. Drag the dot to set where the fine spectral detail (pitch, harmonics, texture) comes from: Brain 1 (top), Brain 2 (bottom left) or the Input (bottom right). Double-click to reset." },
        { detailY, "Detail triangle, vertical position." },
        { linkTriangles, "Link: the Detail triangle follows the Envelope triangle, so one drag moves both. Turn off to morph envelope and detail separately (e.g. a brain's colour on the Input's notes)." },

        { phaseMode, "Where the output takes its phase from. Follow Detail: whichever sources supply the detail (the natural choice). Input / Brain 1 / Brain 2: pin it to one source. Pinning to the source that carries the pitch gives the most solid result." },
        { fftSize, "Size of the analysis window in samples. Bigger: finer frequency resolution, smoother morphs, more latency and more smearing of fast material. Smaller: tighter transients, coarser tone. 2048 is the all-rounder; 4096 for pads and drones, 1024 for drums." },
        { lowLatency, "Low latency: 512-sample window and no brain alignment, about 11 ms instead of about 85 ms. For playing live. Your input comes straight through; the brains' timbre follows a slice behind." },
        { inputSource, "What the Input is: the track Cortex sits on, or the sidechain input (route another track into Cortex's sidechain in your DAW)." },

        { cutoffIn, "Input cutoff: where the Input's envelope ends and its detail begins, in cepstral bins (4-200). Low values (10-30): only the broad tone colour counts as envelope. High values: more of the harmonic structure is treated as envelope. Also draggable in the cepstrum view." },
        { cutoffB1, "Brain 1 cutoff: where Brain 1's envelope ends and its detail begins, in cepstral bins (4-200). Lower keeps only the broad colour; higher includes more harmonic structure. Also draggable in the cepstrum view." },
        { cutoffB2, "Brain 2 cutoff: where Brain 2's envelope ends and its detail begins, in cepstral bins (4-200). Lower keeps only the broad colour; higher includes more harmonic structure. Also draggable in the cepstrum view." },
        { formantIn, "Input formant shift, in semitones (-24 to +24). Moves the Input's resonances (the vowel, the body) without changing its pitch. Up: smaller, brighter; down: bigger, darker. Costs some CPU while non-zero." },
        { formantB1, "Brain 1 formant shift, in semitones (-24 to +24). Moves Brain 1's resonances without changing its pitch. Up: smaller, brighter; down: bigger, darker. Costs some CPU while non-zero." },
        { formantB2, "Brain 2 formant shift, in semitones (-24 to +24). Moves Brain 2's resonances without changing its pitch. Up: smaller, brighter; down: bigger, darker. Costs some CPU while non-zero." },
        { freezeEnvIn, "Hold the Input's envelope at the moment you switch this on. The colour stays while the detail keeps moving. Switch off to let it go." },
        { freezeDetIn, "Hold the Input's detail at the moment you switch this on. The pitch and texture stay while the colour keeps moving. Switch off to let it go." },
        { freezeEnvB1, "Hold Brain 1's envelope at the moment you switch this on. Its colour stays while the brain keeps choosing slices. Switch off to let it go." },
        { freezeDetB1, "Hold Brain 1's detail at the moment you switch this on. Its pitch and texture stay while the colour keeps moving. Switch off to let it go." },
        { freezeEnvB2, "Hold Brain 2's envelope at the moment you switch this on. Its colour stays while the brain keeps choosing slices. Switch off to let it go." },
        { freezeDetB2, "Hold Brain 2's detail at the moment you switch this on. Its pitch and texture stay while the colour keeps moving. Switch off to let it go." },

        { lfoRate, "LFO rate in Hz (0.01-20). The LFO sweeps the cutoff of the target source, moving the envelope / detail split." },
        { lfoDepth, "LFO depth in cepstral bins (0-100), peak. How far the cutoff sweeps either side of its knob value." },
        { lfoShape, "LFO shape: Sine, Triangle, Saw, Square or Sample & Hold (a new random value each cycle)." },
        { lfoTarget, "Which source's cutoff the LFO and envelope follower move: Input, Brain 1, Brain 2, or all three together." },
        { envDepth, "Envelope follower depth in cepstral bins (-100 to +100). The Input's loudness pushes the target cutoff up (positive) or down (negative): loud notes open the split, quiet ones close it." },
        { envAttack, "Envelope follower attack in ms (0.1-1000): how fast it reacts when the Input gets louder." },
        { envRelease, "Envelope follower release in ms (1-5000): how fast it falls back when the Input gets quieter." },

        { chaos, "Chaos (0-1): makes the brains wander. Raises novelty, lowers stickyness, and adds random jumps (up to 30% of searches at full). Low for faithful matching, high for a brain that never settles." },
        { memory, "Memory (0-1): makes the brains remember. Longer boredom memory and more stickyness, so a brain follows runs in its source sounds instead of re-picking every slice. High values play recognisable phrases from the samples." },
        { cepstralGlide, "Cepstral glide (0-1): each new slice slides in from the previous slice's colour instead of cutting. 0: hard cuts between slices. 1: the colour drifts smoothly, like a filter sweep between grains." },
        { matchTimbre, "Match timbre (0-1): how much a brain cares about tone colour when it picks the closest slice to the Input. With Match Pitch at 0 it picks purely by colour." },
        { matchPitch, "Match pitch (0-1): how much a brain cares about notes (pitch, chroma, voicing) when it picks the closest slice. Turn up to make a brain follow the Input's melody." },
        { envTransplant, "Transplant (0-1): brain slices take on the Input's envelope before the morph, so the brain 'sings with your mouth'. At 1 the brain supplies only texture and pitch under the Input's colour." },
        { blockSize, "Slice length in samples (256-4096) for the brains. Short slices follow the Input closely and sound granular; long slices keep more of each sample's character. Changing it rebuilds the brains (cached, so usually quick)." },
        { overlap, "How much consecutive brain slices overlap (0-0.75). More overlap: smoother, more smeared. Less: choppier, more distinct. Changing it rebuilds the brains." },
        { novelty, "Novelty (0-1): penalises slices the brain used recently, so it does not repeat itself. Chaos adds to this." },
        { boredom, "Boredom (0-1): how long the brain remembers what it played. Higher: recently used slices stay penalised for longer. Memory adds to this." },
        { stickyness, "Stickyness (0-1): prefer the slice that comes next in the source sound over the closest match, so the brain plays through its samples in order. Memory adds to this; Chaos takes from it." },
        { searchMode, "How a brain looks for its next slice. Basic: the closest slice in the whole brain. Rev: the furthest (deliberately wrong). Synaptic: walk to one of the current slice's nearest neighbours, so the brain drifts rather than jumps. Synaptic Slide: the same walk, sliding between slices." },

        { dryWet, "Dry / wet (0-1): mix between the untouched Input (dry) and the morph (wet)." },
        { outGain, "Output gain in dB (-24 to +12), after the mix." },
        { limiter, "Limiter: a brick-wall limiter at -0.3 dB on the output. Keep it on; a morph can spike when the envelope and detail disagree." },
        { gateOn, "Gate: mute the Input when it falls below the gate threshold, so silence stays silent instead of the brains chasing noise. The header light shows when the gate is open." },
        { gateThresh, "Gate threshold in dB (-96 to 0). The Input must be louder than this for the gate to open." },
        { autoMute, "Auto-mute: after the Input has been below the gate threshold for the auto-mute time, stop processing entirely (saves CPU and lets the output fall silent)." },
        { autoMuteMs, "Auto-mute time in ms (10-2000): how long the Input must stay quiet before auto-mute kicks in." },
        { theme, "Dark or Light. A setting for you, not for the project: every Cortex you open uses it, and projects and presets never change it." },

        { oscWave, "Oscillator waveform for the Instrument's Input: Saw, Square, Triangle or Sine. The brains morph against this, so it sets the detail (pitch) you hear." },
        { oscGlide, "Glide (portamento) in ms: how long a held note takes to slide to a new one." },
        { ampAttack, "Amplitude attack in ms for the oscillator." },
        { ampRelease, "Amplitude release in ms for the oscillator after the key is lifted." },
    };
    const auto it = texts.find (id);
    return it != texts.end() ? it->second : juce::String();
}

// The non-parameter controls.
inline const char* envelopeTriangle = "Envelope triangle: who supplies the tone colour. Drag the dot between Brain 1 (top), Brain 2 (bottom left) and the Input (bottom right); the corners show each share. Double-click to reset.";
inline const char* detailTriangle = "Detail triangle: who supplies the pitch and texture. Drag the dot between Brain 1 (top), Brain 2 (bottom left) and the Input (bottom right). Greyed out while Link is on. Double-click to reset.";
inline const char* envelopeView = "Spectral envelopes: the tone colour of each source in play, and the result (the morph's envelope), 30 Hz to 16 kHz.";
inline const char* cepstrumView = "Cepstrum: each source's spectrum of the spectrum. Left of a source's line is its envelope (colour), right of it its detail (pitch, texture). Drag a line to move that source's cutoff. Dashed lines show where modulation has moved it.";
inline const char* brainMap = "Brain map: every slice in this brain as a dot, placed so similar-sounding slices land near each other (PCA of their MFCCs) and coloured by source sound. The ring is the slice playing now; the line is where the search has been.";
inline const char* soundList = "The sounds in this brain and how many slices each gave.";
inline const char* loadButton = "Choose samples or a folder to build this brain from. You can also drop files or folders on the column.";
inline const char* clearButton = "Empty this brain. Its corner of each triangle then falls back to the Input.";
inline const char* rebuildButton = "Re-analyse both brains from their source files with the current slice length and overlap.";
inline const char* meters = "Input and output peak levels, and the gate light (on when the gate is open, dim when closed, off when the gate is off).";
inline const char* status = "Blocks: the slices this brain holds. Building shows progress; large folders take a while the first time and are cached after that.";
inline const char* version = "Version and commit of this build.";
inline const char* searchTab = "How the brains choose slices: the macros, the matching weights, and the slice settings.";
inline const char* modTab = "An LFO and an envelope follower that sweep a source's cutoff.";
inline const char* outputTab = "Phase, window size, latency, mix, gain, safety and the theme.";
} // namespace cortex::ui::tips
