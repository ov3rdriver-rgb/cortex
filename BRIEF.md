# Project brief: "Cortex" (working title): Samplebrain × cepstral morph, as a VST3/AU

Drop this into the root of a new repo (as `BRIEF.md`, or merge it into `CLAUDE.md`) and tell Claude Code: *"Read BRIEF.md and start Phase 0."*

---

## 1. What we're building

A JUCE audio plugin (VST3 + AU + Standalone) that does two things:

1. **Samplebrain engine.** You load a folder of samples into a "brain". The plugin chops them into blocks, analyses each one (FFT magnitudes + MFCCs) and links similar blocks together ("synapses"). The live input on the track is the **target**. Each target block gets swapped for its closest match from the brain, in realtime.
2. **Cepstral morph.** Instead of a hard swap, the matched brain audio and the live input are blended in the **cepstral domain**. The spectral *envelope* (formants/timbre) and the spectral *detail* (pitch/harmonics) are crossfaded separately on an XY pad.

So it's *not* "play the matched sample". It's "take your input's pitch, and wear the brain's timbre", or the reverse, or anywhere in between.

### Reference material
- Samplebrain (Aphex Twin / Then Try This): https://github.com/fxbeaulieu/SampleBrain. The engine lives in `brain/src/` (~1,850 lines of C++: `block`, `brain`, `renderer`, `search_params`, `mfcc`, `fft`). The Qt GUI, portaudio and liblo/OSC are **not** needed.
- `ap_cepstral_morph.amxd` (Alexander Panos), a Max for Live device. The DSP it implements is reconstructed in §3. **Do not copy the patch.** Reimplement the algorithm.

### Licensing: decided, open source
The project is released as **GPL‑3.0** (or AGPL‑3.0) with full source on GitHub, and distributed as pay-what-you-want.
- **Port Samplebrain's engine directly** from `brain/src/`, keeping its copyright headers and crediting Then Try This / Aphex Twin in the README and the plugin's About box.
- JUCE is used under its AGPLv3 option, which is compatible with Samplebrain's GPL‑3 code. FFTW is fine too.
- Add `LICENSE` (GPL‑3.0 or AGPL‑3.0), and `NOTICE`/`CREDITS.md` listing Samplebrain, Aquila (MFCC, MIT) and the cepstral-morph concept credit to Alexander Panos.
- Binaries can be sold or offered as pay-what-you-want. The source must stay public.
- The .amxd is still only a reference. Reimplement its DSP from §3 rather than translating the patch.

---

## 2. Architecture

```
Source/
  PluginProcessor.{h,cpp}     // buses: main stereo in/out + stereo sidechain (Input B)
  PluginEditor.{h,cpp}
  dsp/
    STFT.{h,cpp}              // overlap-add framework, sizes 512/1024/2048/4096, overlap 8 (hop = N/8)
    Cepstrum.{h,cpp}          // log-mag -> real cepstrum -> lifter -> back
    CepstralMorph.{h,cpp}     // the A/B morph core (§3)
    PhaseBlend.{h,cpp}        // 4 phase modes
  brain/
    Block.{h,cpp}             // pcm, windowed pcm, fft mags, mfcc (13–20 coeffs), cepstral envelope
    Brain.{h,cpp}             // block store, synapse graph, nearest-neighbour search
    BrainBuilder.{h,cpp}      // background thread: decode files -> blocks -> analysis -> synapses
    Renderer.{h,cpp}          // realtime: picks blocks for incoming target, OLA playback
    SearchParams.h
  state/
    BrainCache.{h,cpp}        // serialise a built brain to disk (.cortexbrain) so projects reload instantly
```

**Realtime rules (non-negotiable):**
- No allocation, locks, file I/O or logging on the audio thread. Samplebrain's renderer uses `std::list` and allocates. Replace that with a fixed-capacity pool of voices.
- The brain is built on a background thread and swapped in atomically (`std::atomic<std::shared_ptr<const Brain>>` or a double-buffer with a release queue).
- Nearest-neighbour search must fit the block budget. Start with brute force over MFCC vectors (fine up to ~20k blocks with SIMD). Add a k-d tree / HNSW later if needed. Precompute the synapse lists offline.
- FFT: `juce::dsp::FFT` or pffft. Both are lighter to ship than FFTW, and FFTW's license is fine if Samplebrain's code is easier to port with it. Create plans at prepare time, never on the audio thread.
- Report latency via `setLatencySamples()` = FFT size (+ the block size when the brain is in the path). Update it when the FFT size changes.

---

## 3. Cepstral morph DSP (reconstructed from the .amxd)

Per STFT frame, per input (A = main input, B = sidechain or brain output):

1. `X = FFT(window * frame)`, then convert to polar: `mag`, `phase`.
2. `L = log(mag + ε)`. The device injects tiny noise (~1e-6) as dither so `log(0)` never happens. Do the same, or use a fixed ε.
3. `c = realFFT(L)`. This is the **real cepstrum** (quefrency domain).
4. **Lifter** at a cutoff quefrency `q` (the device's default is **25 bins**):
   - `c_env = c[0..q)` → spectral **envelope** (formants, timbre)
   - `c_det = c[q..N/2]` → spectral **detail** (pitch, harmonics, noise texture)
   - Use a short raised-cosine taper around `q` rather than a brick wall (the device smooths with `slide~ 25`).
   - Each input gets its own cutoff (`cutoff quefrency a`, `cutoff quefrency b`).
5. **Crossfade:** `n*(b - a) + a`, separately for each part:
   - `env  = lerp(envA, envB, x)` (XY pad x-axis)
   - `det  = lerp(detA, detB, y)` (XY pad y-axis)
6. Recombine: `L' = IFFT(env + det)`, then `mag' = exp(L')`.
7. **Phase**, chosen by a `phaseMode` parameter (4 modes):
   1. Phase from A
   2. Phase from B
   3. Linear interpolation (unwrapped, shortest-arc)
   4. Phase follows whichever input dominates detail (y < 0.5 → A, else B), with a crossfade zone
8. `polar → cartesian`, IFFT, window, overlap-add. DC-block the output.

**XY corners:** bottom-left = pure A, top-right = pure B, top-left = A envelope + B detail, bottom-right = B envelope + A detail. The device has a gain trim for each off-diagonal corner (these corners get loud or hollow), plus an optional "true bypass" at the pure corners.

**Extras the device has, which we keep:** FFT size selector (512–4096), dry/wet, output limiter, input gate with threshold, auto-mute on silence (CPU saver: skip FFT work when both inputs are below the threshold for N ms).

---

## 4. Where the two engines meet: feature ideas, ranked

The key insight is that **Samplebrain already matches in cepstral space**. MFCCs are a mel-warped cepstrum. So the morph and the brain speak the same language.

### MVP (Phases 2–3)
1. **Brain as Input B.** Renderer output → `CepstralMorph` input B. Live input = A. The XY pad now decides: "my pitch + the brain's timbre", "the brain's pitch + my formants", etc. Sidechain stays available as an alternative B source (`B Source: Sidechain | Brain`).
2. **Split-cepstrum matching.** Replace Samplebrain's single `ratio` (FFT vs MFCC distance) with two weights: **Match Timbre** (distance on low-quefrency coefficients) and **Match Pitch** (distance on high-quefrency / f0). This lets the brain find blocks that share your melody but not your tone, or the opposite.
3. **Per-block envelope transplant.** At render time, each matched block's envelope is replaced by the target block's envelope (amount knob). The brain then "sings with your mouth".

### v1.x
4. **Synaptic drift with cepstral glide.** When the renderer walks synapses (`SYNAPTIC` / `SYNAPTIC_SLIDE` modes), interpolate cepstra between consecutive blocks over the hop instead of crossfading audio. You get smooth timbral wandering instead of choppy grains.
5. **Formant shift / envelope warp.** Resample `c_env` along quefrency (or warp the envelope along frequency) to shift formants independently of pitch, per input.
6. **Cepstral freeze.** Hold A's or B's envelope (or detail) indefinitely while the other keeps moving.
7. **Quefrency cutoff as performance control.** Modulatable, with an LFO and envelope follower. Sweeping `q` is a great sound on its own.
8. **4-corner brain.** Two brains (Brain A, Brain B) plus the live input. The XY pad morphs between brain-matched timbres.
9. **Novelty / boredom / stickyness** from Samplebrain, exposed as macro knobs ("Chaos", "Memory").

### Wild
10. **Morph-aware search.** Search the brain for the block that would sound *best after morphing*: minimise distance between the target and the morphed result, not the raw block.
11. **Cepstral "average brain".** The mean envelope of the whole brain as a static filter. A one-click "make it sound like this sample pack" EQ.
12. **MIDI-triggered brains.** A note selects which brain or region to match from (keyboard-mapped sample sets).

---

## 5. Parameters (APVTS IDs)

| ID | Range | Notes |
|---|---|---|
| `morphX` | 0–1 | envelope A→B |
| `morphY` | 0–1 | detail A→B |
| `cutoffA`, `cutoffB` | 4–200 bins | default 25 |
| `phaseMode` | 0–3 | §3.7 |
| `fftSize` | 512/1024/2048/4096 | changes latency |
| `bSource` | Sidechain / Brain | |
| `matchTimbre`, `matchPitch` | 0–1 | split-cepstrum search weights |
| `envTransplant` | 0–1 | idea 3 |
| `blockSize` | 256–4096 | brain block length (Samplebrain default 3000; 500–1000 recommended) |
| `overlap` | 0–0.75 | brain block overlap |
| `novelty`, `boredom`, `stickyness` | 0–1 | search behaviour |
| `searchMode` | Basic / Rev / Synaptic / Synaptic Slide | |
| `cornerGainTL`, `cornerGainBR` | −24…+12 dB | |
| `dryWet`, `outGain`, `limiter` | | |
| `gateOn`, `gateThresh`, `autoMute`, `autoMuteMs` | | |

The brain itself (file list + cached analysis path) is saved in plugin state as a reference, **not** as raw audio.

---

## 6. Phases (Claude Code works through these in order; each ends with passing tests)

- **Phase 0, scaffold.** JUCE CMake project (FetchContent JUCE 8), `LICENSE` + `CREDITS.md`, Samplebrain added as a git submodule under `third_party/`, VST3/AU/Standalone targets, sidechain bus, APVTS with all §5 params, GitHub Actions CI building macOS + Windows. Plus a pluginval run in CI.
- **Phase 1, cepstral morph only.** `STFT`, `Cepstrum`, `CepstralMorph`, `PhaseBlend`. Must work as a sidechain morph with no brain. Unit tests:
  - XY = (0,0) with phase A reproduces A (null test < −60 dB after latency compensation).
  - XY = (1,1) with phase B reproduces B.
  - A sine through A + pink noise through B at top-left gives a sine-pitched, noise-coloured output (check the spectral centroid moves).
  - No NaN/denormals on silence. Latency reported correctly on FFT size change.
- **Phase 2, brain engine.** Port `block`, `brain`, `renderer`, `search_params` from Samplebrain's `brain/src/` into `Block`, `Brain`, `BrainBuilder`, `Renderer`. Drop the portaudio, liblo/OSC and Qt dependencies. Replace libsndfile with JUCE's `AudioFormatManager`. Make the renderer realtime-safe (§2). Drag-drop a folder → build on a background thread with a progress bar → cache to disk. Renderer test: a brain made only of the target file should reconstruct the target closely.
- **Phase 3, integrate.** Brain → input B. Split-cepstrum matching (idea 2). Envelope transplant (idea 3). Realtime-safety audit (run under a `-fsanitize=realtime` / RADSan build, or assert no allocation on the audio thread).
- **Phase 4, UI.** XY pad with live spectral-envelope overlay for A, B and the result. Brain browser (file list, block count, a 2D map of blocks via PCA on MFCCs that shows the currently-playing block lighting up). Quefrency cutoff shown on a cepstrum display.
- **Phase 5, v1.x features** (§4 ideas 4–9), one per PR.

## 7. Definition of done for v1
Loads in Ableton, Logic and Reaper. Passes pluginval at strictness 10. A 5k-block brain stays below ~15% of one core at 48 kHz / 2048 FFT. Projects reopen with the brain intact in < 2 s from cache. No clicks when changing FFT size or swapping brains.
