# Changelog

## Unreleased

- **Two themes:** Dark (near-black, Segoe UI) and Light (paper and ink, Bahnschrift, with blue / red / violet for Input / Brain 1 / Brain 2 and a black result curve). Choose under Output → Theme; the choice is saved with the session and remembered for new instances. The fonts are system fonts for now.
- **New look:** flat regions with hairline edges instead of rounded panels, each source's colour as a rule along the top of its region; small tracked uppercase captions; a monospace face for values; instrument-style tick marks on the knobs; square buttons, menus, tabs and tick boxes drawn to match.
- "Envelope Transplant" is now "Transplant".
- **Meters:** input and output peak meters with a gate light in the header, so you can see whether signal is getting in and coming out.
- **Layout:** the triangles are capped at 250 px, so the spectral envelope and cepstrum plots get real height instead of 90 px each.
- **Colours:** knobs take their panel's colour (orange in Brain 1, violet in Brain 2, turquoise in the Input, neutral in the tabs); pink now means the output only. Each brain map colours its sounds in shades of that brain's colour instead of borrowing the other sources' colours.
- **Brain map:** a bigger marker for the block playing now, with a line through the last blocks the search chose.
- **Switches** (Freeze Env, Freeze Det, Gate, Limiter, Low Latency...) are buttons that say what they do and light up when on.
- Values show one decimal for dB, semitones and ms.

## 1.0.1

- **Fixed:** formant shift used far too much CPU. With formant shift on all three sources, the audio processing needed 167% of a CPU core, so the DAW overloaded and audio dropped out. It now needs about 42%. Each shifted source adds about 7% instead of about 43%. The formant estimate uses 4 passes instead of 8, and its gain curve is reused for 2 frames (recomputed at once when the shift or cutoff moves). Formants land where they did before.
- A manual CPU benchmark of the audio processing, with features switched on (`CORTEX_TEST_CATEGORY=CortexDiag`).

## 1.0.0

First release.

- Cleanup after the three-source redesign: theme colours named after their sources, stale A/B wording removed.
- README rewritten around the three-source model, with a plain-English explanation.

## 0.10.0: equal brains

- **Three-source morph:** the live Input, Brain 1 and Brain 2 feed one cepstral morph. Two triangles (Envelope, Detail) set each source's share, with Link to make the detail follow the envelope. Phase follows the detail, or is pinned to one source.
- **Per-source settings:** cutoff, formant shift and envelope/detail freeze for each of Input, Brain 1 and Brain 2. The LFO can target any one, or all.
- **Input** is the track or the sidechain (effect), or the oscillator (instrument).
- **New layout:** Brain 1 and Brain 2 columns, the triangles with live envelopes and a three-source cepstrum in the middle, the Input panel and tabs below.
- Removed: Morph X/Y, A/B Source, Cutoff/Formant/Freeze A/B, corner gains. Older sessions load with the morph at its defaults.

## 0.9.0: Phase 5 (v1.x features)

- Cepstral freeze.
- Cutoff modulation: LFO and envelope follower.
- Formant shift, using a true envelope.
- Chaos and Memory macros.
- Synaptic drift with cepstral glide.
- A second brain (4-corner brain).

## 0.8.0

- Low Latency mode: about 11 ms from note to sound instead of about 85 ms.

## 0.7.0: Phase 4

- The first real UI: XY pad with live envelopes, cepstrum view with draggable cutoffs, brain map (PCA of MFCCs), sound list, parameter tabs.

## 0.6.0: Phase 3b

- Cortex Instrument: a mono, MIDI-played oscillator drives the brain (legato glide, attack/release, pitch bend).

## 0.5.0: Phase 3

- Envelope transplant.
- Realtime audit: no memory allocation on the audio thread.

## 0.4.0

- Split-cepstrum matching: Match Timbre and Match Pitch. Silent blocks are left out of brains, and brain slices play at the input's level.

## 0.3.0

- The brain lines up with the input; the host is told the full latency.

## 0.2.x: Phase 2

- The Samplebrain brain engine, ported: background builds, the `.cortexbrain` cache, and search off the audio thread for large brains.
- The version shown in the editor.

## 0.1.x: Phases 0–1

- Scaffold, CI and pluginval.
- The cepstral morph engine.
- Crossfading between FFT sizes.
