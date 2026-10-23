# Cortex

Two "brains" of samples answer whatever you play, and a cepstral morph blends their answers with your sound. VST3 / AU / Standalone, from one codebase:

- **Cortex** (effect): the Input is your track (or a sidechain).
- **Cortex Instrument** (mono synth): the Input is a MIDI-played oscillator (saw, square, triangle or sine, with legato glide).

## What it does

You load sample folders into two **brains**. Whatever you feed Cortex, both brains listen, and each one keeps finding the short slices (about 20 ms) of *its own* samples that sound closest to it, like two improvisers who can only reply with bits of their own record collection. They don't listen to each other; they both follow your Input.

Cortex then blends the three sources (your **Input**, **Brain 1** and **Brain 2**) with a **cepstral morph**, which splits every sound into two layers:

- **Envelope**: the overall shape of the sound. Its body, colour, vowel-like quality: what makes a violin sound like a violin.
- **Detail**: the fine structure. Which notes and harmonics, and the grit and noise.

The two layers are mixed *separately*, with one triangle each. So you can take the notes from one source and the body of another: your melody with the colour of a drum break, or a breakbeat that sings at your synth's pitch.

**Example:** load a folder of music into Brain 1 and a folder of breaks into Brain 2, and play a synth line in. Brain 1 answers with music snippets, Brain 2 with drum snippets. Move the Envelope dot towards Brain 2 and the Detail dot towards your Input, and your line comes out wearing the drums.

## The window

```
┌──────────────┬──────────────────────────────────┬──────────────┐
│  BRAIN 1     │   Envelope ▲        Detail ▲     │  BRAIN 2     │
│  load, map,  │   triangle         triangle      │  load, map,  │
│  sounds,     │   [Link]                         │  sounds,     │
│  its cutoff, │   live spectral envelopes        │  its cutoff, │
│  formant,    │   cepstrum + cutoff lines        │  formant,    │
│  freeze      │                                  │  freeze      │
├──────────────┴──────────────┬───────────────────┴──────────────┤
│  INPUT  synth or track /    │  Search · Mod · Output tabs      │
│  sidechain, its settings    │                                  │
└─────────────────────────────┴──────────────────────────────────┘
```

- **Brain columns**: drop a folder (or files) on a column, or click Load. The brain is built in the background and cached as a `.cortexbrain`, so projects reopen in a moment. The map places every block by how it sounds and rings the one playing now.
- **Triangles**: each has Brain 1 at the top, Brain 2 bottom-left, Input bottom-right. Drag the dot; each corner shows its share. **Link** makes the detail follow the envelope. A corner whose brain is empty hands its share back to the Input.
- **Cepstrum lines**: each source's envelope/detail split point ("cutoff"). Drag them. Higher means more of that source counts as envelope.

## Controls worth knowing

| Where | Control | What it does |
|---|---|---|
| Brain columns / Input | Cutoff, Formant, Freeze Env / Detail | Per source: where envelope ends and detail begins; shift its formants without changing pitch; hold its envelope or detail still |
| Search | Match Timbre / Match Pitch | What "closest" means when a brain picks a slice: tone colour, or notes |
| Search | Chaos / Memory | Chaos makes the brains wander; Memory makes them remember what they played and follow runs in their source |
| Search | Cepstral Glide | New slices slide in from the previous slice's colour instead of cutting |
| Search | Envelope Transplant | Brain slices take on your Input's envelope before the morph ("the brain sings with your mouth") |
| Mod | LFO, Envelope follower | Sweep the cutoffs |
| Output | Low Latency | ~11 ms instead of ~85 ms, for playing live; the brains' timbre follows a slice behind |

## Build

Requires CMake ≥ 3.22 and a C++20 compiler (Visual Studio 2022 on Windows, Xcode 15+ on macOS). JUCE 8 is fetched automatically.

```bash
git clone --recursive <repo-url>
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release
```

CI builds macOS (universal) and Windows, runs the tests, and checks every plugin with pluginval at strictness 10. The tests include null tests for the morph, latency alignment, and an audit that the audio thread never allocates memory.

See [BRIEF.md](BRIEF.md) for the original design and [CHANGELOG.md](CHANGELOG.md) for what changed when.

## Credits

Brain engine ported from **Samplebrain** by Then Try This / Aphex Twin. Cepstral morph concept by Alexander Panos.
Full notices in [CREDITS.md](CREDITS.md). Licensed AGPL-3.0.
