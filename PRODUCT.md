# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

> Not a web product: Cortex is a JUCE audio plugin (VST3 / AU / Standalone) with a custom-drawn C++ editor. Impeccable has no JUCE platform value; `web` is recorded because the plugin window is a resizable, mouse-driven desktop surface whose design concerns (hierarchy, density, type, colour, states) map to the web guidance rather than to iOS or Android. Nothing here is rendered by a browser; verification is by the test suite's rendered PNGs of the editor.

## Users

Electronic and experimental producers and sound designers, at a desk, inside a DAW project, designing sounds and textures. They open the plugin window for minutes at a time while a track loops, with the DAW's own UI around it. Live use (Low Latency mode, on stage) is supported but secondary.

## Product Purpose

Cortex turns folders of samples into two "brains" that answer whatever you play, and blends their answers with your sound through a cepstral morph. You load sample folders into Brain 1 and Brain 2; each brain slices its samples and, while audio runs, keeps picking the slice of its own library that sounds closest to what it is listening to. The morph then mixes three sources (the Input, Brain 1, Brain 2) in two layers, envelope (tone colour, body) and detail (pitch, harmonics, texture), each with its own triangle. Success: a producer gets a sound they could not have made another way, understands why it sounds like that, and can steer it.

Two plugins from one codebase: **Cortex** (effect: the Input is the track or a sidechain) and **Cortex Instrument** (the Input is a MIDI-played mono oscillator).

## Positioning

Concatenative resynthesis from large user libraries (Samplebrain lineage), doubled, and then put through a cepstral morph that mixes envelope and detail separately. Neighbours (Samplebrain, Neutone's neu.grains) stop at the grain stream; Cortex treats two independent grain streams as morph sources alongside the live input, with per-brain listening, slicing and search, links between the brains, and a two-minute capture of the output. The window is as much an explanation of what the engine is doing as a set of controls: the brain maps, live envelopes and cepstrum are live views of the process.

## Operating Context

- Hosted in a DAW (tested so far only in the test harness and standalone; DAW testing pending). The window is resizable, minimum 1120 x 760, default 1280 x 820, up to 2600 x 1800.
- Brains are built in the background from dropped folders and cached as `.cortexbrain`; large folders take minutes the first time.
- The editor refreshes live data 30 times a second: brain maps (slice playing now, search path), triangle weights, spectral envelopes, cepstrum, meters.
- Tester packages ship for Windows and macOS with Quick Start, Known Issues and Feedback text files; feedback asks for the version and commit shown in the window.
- Two themes exist today (Dark, Light), a per-user setting, not part of the project state.

## Capabilities and Constraints

- Sources are fixed at three: Input, Brain 1, Brain 2. Each has a colour used everywhere it appears (maps, triangles, plots, rules on regions, tab rules, knob arcs).
- Regions today: header (title, "?" explainer, Capture, meters, version/commit); two brain columns (status, Load / Rebuild / Clear, Listen, block map, sound list, cutoff / formant / freeze, block size / overlap); centre (Envelope and Detail triangles, Link, live envelopes, cepstrum with draggable cutoff lines); bottom (Input panel; tabs: Brain 1 search, Brain 2 search, Mod, Output, with Link all).
- Every parameter has a tooltip; the "?" opens an in-window explainer of the concept.
- Per-brain controls carry link rings (one control on both brains) and Link all.
- Controls are standard JUCE components under a custom LookAndFeel: rotary knobs with text boxes, combo boxes, text buttons, toggle buttons, a tabbed component, labels. The look is drawn in code (Theme.h): no image assets, two embedded fonts (Inter, Barlow) plus the system monospace face for values.
- Parameter IDs and names are persisted in host sessions and must not change. Control set and function are not in scope of a visual redesign.
- Performance: the editor must not stall the message thread; plots are repainted from snapshots, not computed in paint.
- Constraint from the owner: company, manufacturer and bundle-identity fields stay blank until named.

## Brand Commitments

The name is Cortex (and Cortex Instrument). No label, studio or publisher branding: the name and the plugin's own look are the identity. Credits line: AGPL-3.0, brain engine from Samplebrain (Then Try This / Aphex Twin), cepstral morph concept by Alexander Panos. No logo exists.

## Evidence on Hand

- The product itself, with rendered screenshots from the test suite: `ui_effect.png`, `ui_effect_light.png`, `ui_instrument.png`, `ui_tooltip.png`, `ui_help.png` (written to `CORTEX_DIAG_OUT` by `CortexTests`).
- `README.md`, `BRIEF.md` (original design brief), `CHANGELOG.md`, `packaging/docs/QUICK-START.txt`, the in-window explainer text in `Source/ui/HelpOverlay.cpp`, tooltips in `Source/ui/Tooltips.h`.
- No testimonials, no user quotes, no press, no DAW compatibility claims beyond what CI validates (pluginval at strictness 10 for VST3 and AU). Do not fabricate any.

## Product Principles

- The window explains the engine: live views of what each brain and the morph are doing are first-class, not decoration.
- Three sources, three colours, everywhere: a producer should always know which source a control, line, dot or number belongs to.
- Dense but legible at a desk: many controls on one screen, read at arm's length in a DAW, without hunting.
- Per-brain symmetry: whatever Brain 1 has, Brain 2 has, in the same place, in its own colour.
- Nothing hidden behind modes: the full control set stays reachable from one window; tabs are the only nesting.

## Accessibility & Inclusion

Source colours must stay distinguishable for common colour-vision deficiencies and in both themes; no information is carried by colour alone (names and positions back it up). Tooltips on every control. No specific standard has been set.
