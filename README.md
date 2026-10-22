# Cortex

Samplebrain × cepstral morph, as a VST3 / AU / Standalone plugin. See [BRIEF.md](BRIEF.md) for the design.

**Status:** v0.6. Two plugins from one codebase:

- **Cortex** (effect): your track's audio is matched against a brain of samples, and the matches are cepstrally morphed with it.
- **Cortex Instrument** (mono synth): a MIDI-played oscillator (saw/square/triangle/sine, legato glide) drives the brain instead.

Drop a folder of samples on the plugin to build a brain (cached as `.cortexbrain`). Match Timbre / Match Pitch steer the search, Envelope Transplant gives brain grains your spectral envelope, and the XY morph (Morph Envelope / Morph Detail) blends the two.

## Build

Requires CMake ≥ 3.22 and a C++20 compiler (Visual Studio 2022 on Windows, Xcode 15+ on macOS). JUCE 8 is fetched automatically.

```bash
git clone --recursive <repo-url>
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release
```

## Credits

Brain engine ported from **Samplebrain** by Then Try This / Aphex Twin. Cepstral morph concept by Alexander Panos.
Full notices in [CREDITS.md](CREDITS.md). Licensed AGPL-3.0.
