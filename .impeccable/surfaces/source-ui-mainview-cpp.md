---
version: 1
slug: "source-ui-mainview-cpp"
primary_target: "Source/ui/MainView.cpp"
related_targets: ["Source/ui/Theme.h","Source/PluginEditor.cpp"]
---

# Surface brief: the Cortex editor window

Scope: the whole plugin window (effect and instrument), an Operate surface. Audience: producers and sound designers at a desk inside a DAW. Task: load libraries into two brains, steer the morph, read what the engine is doing. Constraints: control set, layout regions, parameter ids and names fixed; three sources keep one colour each everywhere; minimum 1120 x 760; drawn in JUCE code with embedded Inter / Barlow and a system face for typed values. Unresolved: none at the direction level.

## Direction contract

THESIS: The window is the session's tape box. A dark box-card shell, and every value typed into a ruled cell on a cream label stuck to it, with anything happening now marked in grease pencil. It refuses the category's glowing knob discs on rounded grey panels with one blue accent.

OWN-WORLD: Box card ground (deep blue-grey, matte) in the dark theme; the track sheet itself (cream, navy ink) in the light theme. Cream label stock only for small fields: captions, value cells, the sound list, the version tag. Print navy ink on cream, chalk white on the box. Grease-pencil red-orange for every "now" mark: the playing-slice ring, the triangle dots, the gate light. Source inks: leader-tape green (Input), grease-pencil orange (Brain 1), carbon-copy purple (Brain 2). Type: Barlow small caps, tracked, for printed captions; Courier New for every typed value and figure; Inter for prose. Hairline rules; knobs as thin rings with a pencil pointer and no fill; buttons as cream labels with navy text; tabs as label tabs. A fixed six-step neutral scale from box to chalk and nothing off it. Raised: hierarchy by scale (the triangle shares are the largest figures on the sheet); the leading source burns brightest, the others a step dimmer; the control under the hand lifts to full contrast while siblings step back.

STORY: A producer opens the window and sees a session box: two track-sheet strips, one per brain, showing what is loaded and which slice is playing; the morph in the middle with its shares typed large; the live plots on the box. They know which source leads because it is inked brightest. They drag a dot or a knob, it lifts, and the value types itself into its cell.

FIRST VIEWPORT: The same shell as today. Header strip: title in chalk, the "?" tag, the Capture label, meters as ruled level scales, the version as a cream tag at the right. Left and right: the two brain strips with numbered ruled rows (status, Listen, map, sound list, cutoff / formant / freezes, block size / overlap). Centre: the two triangles with large typed corner shares, then envelopes and cepstrum as chalk-thin curves on the box with source-ink cutoff lines. Bottom: the Input label sheet, and the tabs as label tabs with their controls. Primary actions: the triangle dots and each brain's Load.

FORM: Tape box and track sheet, candidate 4 of 7 on the ordered grounded list, assigned by the roll; seed:135042ad465aeb1f. Signature interaction: focus lift on the control under the hand, with its value cell going to full ink. Motion grammar: none that animates; values and rings update instantly, like a typed entry.

FINISH: unreviewed and undocumented is unfinished; this build ends with the finish review, the verdict, DESIGN.md, and every shipping raster carrying its provenance
