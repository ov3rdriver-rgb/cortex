# Cortex

Samplebrain × cepstral morph, as a VST3 / AU / Standalone plugin. See [BRIEF.md](BRIEF.md) for the design.

**Status:** Phase 2 (brain engine): drop a folder of samples to build a brain (background thread, cached to disk as `.cortexbrain`); set **B Source** to Brain to feed the live-matched brain into the cepstral morph. Phase 3 aligns latency and adds split-cepstrum matching.

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
