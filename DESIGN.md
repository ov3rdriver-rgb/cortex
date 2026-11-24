---
name: Cortex
description: A session's tape box, drawn in JUCE code; every value typed on a cream label stuck to the box, every "now" mark in grease pencil.
colors:
  dark-background: "#2b323c"
  dark-panel: "#262c35"
  dark-well: "#1f252d"
  dark-rule: "#3a424e"
  dark-outline: "#434b58"
  dark-dim: "#a39d90"
  dark-text: "#e8e2d4"
  dark-neutral-accent: "#cfc8b8"
  dark-label: "#efe6d2"
  dark-ink: "#1d2a44"
  dark-input: "#3fbf9f"
  dark-brain1: "#f08a3e"
  dark-brain2: "#9d7cd8"
  dark-result: "#e3542b"
  dark-now: "#e3542b"
  light-background: "#e9e0cb"
  light-panel: "#f3ecdb"
  light-well: "#fbf7ee"
  light-rule: "#c8bea6"
  light-outline: "#8e866f"
  light-dim: "#6b6555"
  light-text: "#1d2a44"
  light-neutral-accent: "#1d2a44"
  light-label: "#fbf7ee"
  light-ink: "#1d2a44"
  light-input: "#1f8f78"
  light-brain1: "#d9641e"
  light-brain2: "#6b4fb8"
  light-result: "#d8431f"
  light-now: "#d8431f"
typography:
  display:
    fontFamily: "Barlow"
    fontSize: "20px"
    fontWeight: 600
    letterSpacing: "0.22em"
  title:
    fontFamily: "Barlow"
    fontSize: "13px"
    fontWeight: 600
    letterSpacing: "0.14em"
  caption:
    fontFamily: "Barlow"
    fontSize: "9.5px"
    fontWeight: 600
    letterSpacing: "0.14em"
  label:
    fontFamily: "Barlow"
    fontSize: "9.5px"
    fontWeight: 400
    letterSpacing: "0.035em"
  button:
    fontFamily: "Barlow"
    fontSize: "9.5px"
    fontWeight: 600
    letterSpacing: "0.07em"
  value:
    fontFamily: "Courier New, monospace"
    fontSize: "11px"
    fontWeight: 400
  menu:
    fontFamily: "Courier New, monospace"
    fontSize: "11.5px"
    fontWeight: 400
  share:
    fontFamily: "Courier New, monospace"
    fontSize: "19px"
    fontWeight: 400
  share-trailing:
    fontFamily: "Courier New, monospace"
    fontSize: "15px"
    fontWeight: 400
  tag:
    fontFamily: "Courier New, monospace"
    fontSize: "10.5px"
    fontWeight: 400
  fine:
    fontFamily: "Courier New, monospace"
    fontSize: "9.5px"
    fontWeight: 400
  tooltip:
    fontFamily: "Barlow"
    fontSize: "12.5px"
    fontWeight: 400
  prose:
    fontFamily: "Barlow"
    fontSize: "13.5px"
    fontWeight: 400
    lineHeight: 1.22
rounded:
  none: "0px"
spacing:
  hair: "1px"
  rule: "3px"
  cell: "2px"
  inset: "6px"
  gap: "8px"
  column-pad: "10px"
  caption-pad: "12px"
  label-row: "16px"
  menu-row: "24px"
  button-row: "26px"
  header: "40px"
  column: "300px"
components:
  region:
    backgroundColor: "{colors.dark-panel}"
    textColor: "{colors.dark-dim}"
    typography: "{typography.caption}"
    rounded: "{rounded.none}"
    padding: "7px 12px"
  well:
    backgroundColor: "{colors.dark-well}"
    rounded: "{rounded.none}"
  button-label:
    backgroundColor: "{colors.dark-label}"
    textColor: "{colors.dark-ink}"
    typography: "{typography.button}"
    rounded: "{rounded.none}"
    padding: "0 4px"
    height: "26px"
  button-label-lifted:
    backgroundColor: "#f4ecda"
    textColor: "{colors.dark-ink}"
  button-label-on:
    backgroundColor: "{colors.dark-label}"
    textColor: "{colors.dark-ink}"
  button-label-disabled:
    backgroundColor: "transparent"
    textColor: "{colors.dark-dim}"
  value-cell:
    backgroundColor: "{colors.dark-label}"
    textColor: "{colors.dark-ink}"
    typography: "{typography.value}"
    rounded: "{rounded.none}"
    height: "16px"
  value-cell-lifted:
    backgroundColor: "{colors.dark-label}"
    textColor: "{colors.dark-ink}"
  tag:
    backgroundColor: "{colors.dark-label}"
    textColor: "{colors.dark-ink}"
    typography: "{typography.tag}"
    rounded: "{rounded.none}"
    padding: "0 5px"
  menu-cell:
    backgroundColor: "{colors.dark-panel}"
    textColor: "{colors.dark-text}"
    typography: "{typography.menu}"
    rounded: "{rounded.none}"
    height: "24px"
  popup-menu:
    backgroundColor: "{colors.dark-label}"
    textColor: "{colors.dark-ink}"
    typography: "{typography.menu}"
    rounded: "{rounded.none}"
  tab-front:
    backgroundColor: "{colors.dark-label}"
    textColor: "{colors.dark-ink}"
    typography: "{typography.caption}"
    rounded: "{rounded.none}"
    height: "26px"
  tab-back:
    backgroundColor: "transparent"
    textColor: "{colors.dark-dim}"
    typography: "{typography.caption}"
    height: "26px"
  tooltip:
    backgroundColor: "{colors.dark-label}"
    textColor: "{colors.dark-ink}"
    typography: "{typography.tooltip}"
    rounded: "{rounded.none}"
    padding: "7px 10px"
    width: "340px"
  help-sheet:
    backgroundColor: "{colors.dark-label}"
    textColor: "{colors.dark-ink}"
    typography: "{typography.prose}"
    rounded: "{rounded.none}"
    padding: "28px"
    width: "760px"
  sound-list:
    backgroundColor: "{colors.dark-label}"
    textColor: "{colors.dark-ink}"
    typography: "{typography.value}"
    height: "18px"
---

# Design System: Cortex

## Overview

**Creative North Star: "The Tape Box and the Track Sheet"**

The plugin window is a studio's session box. The dark theme is the box itself: a matte blue-grey card, chalk-white print, and a cream label stuck on wherever a value is typed. The light theme is the track sheet that lives inside the box: cream stock, navy ink, the same inks a shade deeper. Both themes are one world with one grammar; the Theme parameter swaps the palette and nothing else. Three sources (the Input, Brain 1, Brain 2) each own one ink and keep it everywhere they appear: region rules, tab rules, knob arcs, map dots, plot curves, cutoff lines, cepstrum tags. Anything happening right now (the triangle dot, the slice playing, the gate light, the meter above -3 dB, the capture just made) is marked in grease pencil.

The surface is dense and flat. Hierarchy comes from scale and ink weight, never from decoration: the triangle shares are the largest figures on the sheet, the leading source is inked fully while the others sit a step back, and the control under the hand lifts to full ink while its siblings stay put. Everything is drawn in code with hairlines and square corners; there are no image assets and no rounded forms except the dots, rings and knobs that are circles by nature. Nothing animates: values, rings and dots update instantly, like a typed entry.

**Key Characteristics:**
- Two themes, one world: box (dark) and sheet (light) share every rule and differ only in palette.
- Three source inks, fixed, used for every mark that belongs to a source.
- Grease pencil for "now" and for the result; nothing else is red-orange.
- Cream label stock only for small typed fields: value cells, buttons, tags, menus' popups, the sound list, the version, the tooltip, the help sheet.
- Barlow for print (captions, labels, titles, prose); Courier New for everything typed (values, menus, status, tags, credits).
- Hairline rules, square corners, no fills under knobs, no shadows beyond a 1px shadow line under label stock.
- Focus lift: hover brings a knob and its cell to full ink; no other hover treatment exists.

## Colors

Two palettes of the same fifteen roles: a blue-grey box with chalk print, or a cream sheet with navy ink; three source inks and one grease pencil on either.

### Primary (the source inks)
- **Leader-Tape Green** (dark `#3fbf9f`, light `#1f8f78`): the Input. Its region rule, meter, triangle corner, plot curve, cutoff line and tag, and the Input panel title.
- **Grease-Pencil Orange** (dark `#f08a3e`, light `#d9641e`): Brain 1. Its column rule, title, knob arcs, link rings, map dots (as shades, see the One Ink rule), tab rule, search-tab controls, and the "on" marking of its switches.
- **Carbon-Copy Purple** (dark `#9d7cd8`, light `#6b4fb8`): Brain 2, used exactly as Brain 1's ink is.

### Secondary (the pencil)
- **Grease Pencil** (dark `#e3542b`, light `#d8431f`): every "now" mark. The triangle dot, the ring round the slice playing, the gate light when open, the meter above -3 dB, the progress bar while a brain builds, the capture chip's rule, the tick in a tick box, the help sheet's top rule, and the on-ink of buttons that belong to no source (Link all). The same value is `result`: the morph's output curve and the Out meter. Also the text-selection highlight in a value cell, at 35% alpha.

### Neutral (the box, dark theme)
- **Box Card** (`#2b323c`): the window ground and the brain columns.
- **Box Panel** (`#262c35`): regions drawn with `drawPanel` (triangles, map, envelopes, cepstrum, the tab pages) and the menu cell.
- **Box Well** (`#1f252d`): the ground a map, plot, triangle, meter or tick box is drawn on, cut into a panel.
- **Box Rule** (`#3a424e`): hairlines between rows and under the header and above the footer, plot gridlines, the knob's empty outline ring in JUCE's own slider colours, the scrollbar thumb, the gate light when the gate is off.
- **Box Outline** (`#434b58`): the 1px edge of every region, well, menu cell and meter; the knob's printed track; the back tabs' underline; the only mark a disabled button keeps.
- **Faded Chalk** (`#a39d90`): captions, control labels, row numbers, axis figures, the legend, credits, the "empty" messages, the menu chevron, the knob scale marks (at 70%), back-tab text, disabled text.
- **Chalk** (`#e8e2d4`): the title, status lines, tick-box text, hover-state back-tab text, the knob pointer, the search trail and the meter hold line.
- **Chalk Ink** (`#cfc8b8`, `neutralAccent`): the arc and front-tab rule of knobs and tabs that belong to no source (Mod, Output, Input panel's gate threshold and the Input's own cutoff/formant get the Input ink instead).
- **Label Stock** (`#efe6d2`): the cream stuck-on label: value cells, buttons, tags, popup menus, the sound list, the version tag, tooltips, the help sheet, front tabs.
- **Navy Ink** (`#1d2a44`): everything printed or typed on label stock; its 18% alpha is the hairline round idle stock; 14% is the popup highlight; 22% rules the sound list; 30% to 40% edges the help sheet and tooltip.

### Neutral (the sheet, light theme)
- **Track Sheet** (`#e9e0cb`), **Sheet Panel** (`#f3ecdb`), **Sheet Well** (`#fbf7ee`), **Sheet Rule** (`#c8bea6`), **Sheet Outline** (`#8e866f`), **Faded Ink** (`#6b6555`): the same six roles on cream. Text, neutral accent and ink all become Navy Ink (`#1d2a44`); label stock becomes the well's cream (`#fbf7ee`), so a label on the sheet is a lighter patch rather than a contrasting one.

### Named Rules
**The One Ink Per Source Rule.** A source has one colour and every mark that belongs to it uses that colour: rules, tabs, arcs, rings, dots, curves, lines, tags, titles. Sounds inside a brain are shades of that brain's ink (`forSound`: hue rotated within ±0.05, brightness 55% to 100%, saturation 70% to 100%), so a map never borrows another source's colour.

**The Grease Pencil Rule.** The `now` colour marks only what is happening at this moment or has just happened, plus the result the engine is producing. It is never a brand accent, a border on a resting control, or a hover colour.

**The Label Stock Rule.** Cream stock is for small typed fields and sheets of print; regions, wells and grounds are never cream in the dark theme. On the sheet (light) the same rule holds with the stock a lighter cream than its ground.

## Typography

**Display / Print Font:** Barlow (embedded, Regular and SemiBold; falls back to the JUCE default sans before the style is applied)
**Typed Font:** Courier New (system face on Windows and macOS)
**Unused but embedded:** Inter (Regular and SemiBold) is compiled in and loaded but no current style selects it.

**Character:** Printed stencil caps in Barlow, tracked wide and small, against figures and words typed in a typewriter face on cream. Nothing is set large; the one large element is a typed figure (a triangle share), not a heading.

### Hierarchy
- **Display** (Barlow SemiBold, 20px, extra kerning 0.22, uppercase): the window title only ("CORTEX" / "CORTEX INSTRUMENT"), in chalk.
- **Title** (Barlow SemiBold, 13px, extra kerning 0.14, uppercase): a brain column's name in its ink, the Input panel's title, the help sheet's heading. The drop-to-build overlay uses the same face at 16px.
- **Caption** (Barlow SemiBold, 9.5px, extra kerning 0.14, uppercase): every region caption (`drawCaption`, top-left, inset 12px by 7px, 12px tall), tab names, the "linked" mark. At 9px: triangle corner names, the envelope legend, meter names. At 11.5px: help-sheet section headings in their source inks.
- **Label** (Barlow Regular, 9.5px, extra kerning 0.035, uppercase): a control's name above its knob or menu, in Faded Chalk. At 8.5px, 60% ink: the "drag to DAW" note on the capture chip.
- **Button** (Barlow SemiBold, 9.5px under 30px tall, else 11px, extra kerning 0.07, uppercase): printed caps on label stock.
- **Value** (Courier New, 11px): every value cell, status line, sound-list row, tick-box text (11.5px), the "empty" message. Continuous values are shown to one decimal with a unit (dB, st, ms) or two decimals without.
- **Menu** (Courier New, 11.5px in the cell, 12px in the popup and the slider popup).
- **Share** (Courier New, 19px leading, 15px the other two): the triangle corner shares, the largest figures on the sheet.
- **Tag** (Courier New, 10.5px; 10px for cepstrum cutoff tags; 10px credits; 9px row numbers; 9.5px axis figures).
- **Tooltip** (Barlow Regular, 12.5px): wrapped at 340px.
- **Prose** (Barlow Regular, 13.5px, line spacing 3px): the help sheet's body, in navy ink.

### Named Rules
**The Typed Value Rule.** Every number, menu choice, status line, filename, tag and the credits line is typed in Courier New. Barlow carries print only: titles, captions, labels, button caps and prose.

**The Tracked Caps Rule.** Printed text is uppercase (`uppercaseTitles`) and tracked: titles and captions by 0.14, buttons by half that, control labels by a quarter, so long names still fit a 66px cell.

## Layout

A fixed shell on a window of minimum 1120 x 760 (default 1280 x 820, up to 2600 x 1800), drawn with an 8px gap as the only rhythm unit. The window is inset 8px; the header strip is 40px with a hairline under it; the footer (credits) is 20px with a hairline above it; the bottom row is 190px, split half Input panel, half tabs. The two brain columns are 300px wide at either edge; the centre takes what remains. In the centre the two triangles share the top (capped at 250px tall, never taller than the width allows nor so tall the plots lose 260px), a 28px Link row sits under them, and the envelope and cepstrum plots split the rest equally.

A brain column is padded 10px, with a 28px header (title left, Load / Rebuild / Clear right), a 22px status row (progress bar 5px along its bottom), then a 12px left margin in which the row numbers 1 to 4 are typed beside a hairline above each row: Listen (50px), map (what remains), sound list (a quarter of the column, 60 to 110px), settings (178px, two rows of three).

Parameter panels lay controls on a grid: cells `cellWidth` wide (66px in a column, 74px on the Input panel, 84px on a search tab, 400px for Listen), panel inset 6px by 4px, each cell reduced 2px; a 16px label row on top, then the knob (up to 92px) with its 16px value cell below, or a 24px menu row, or a 26px switch. Link rings are 14px square at the cell's bottom right.

Regions carry their caption top-left inset 12px by 7px; wells are the region reduced 12px by 8px and expanded back 4px by 3px (plots) or reduced 16px with 14px trimmed off the top and expanded 6px (map). Tooltips sit 24px right / 6px below the cursor (or 12px left / 6px above when past the window's centre), padded 10px by 7px, no wider than 340px. The help sheet is centred, at most 760px wide, inset at least 24px, with a 40px heading row and 28px page margins.

## Elevation & Depth

Flat. Depth is tonal: a well (`well`) is cut one step darker than its panel, a panel one step darker than the box, and label stock sits on top as the lightest material. The one shadow in the system is the stuck-on label's own: a 1px line of 35% black (30% under a tag) directly beneath the stock, which reads as the label's edge lifting off the box. Hover ("lifted") is a brightness step on the stock (`brighter(0.06)`), not a shadow. The help sheet is the one soft shadow (black 40%, radius 10, offset 0 3) and it is a sheet pulled out of the box over an 82% box scrim, not a control.

### Named Rules
**The Stuck-On Rule.** The only shadow a control may carry is the 1px shadow line under label stock. Regions, wells, knobs, menus and tabs cast nothing.

**The Focus Lift Rule.** The control under the hand goes to full ink: a knob's arc from 78% to 100% alpha and its pointer from 1.6px to 2px; its value cell is ruled 1px in navy ink with its source's ink 2px thick along the bottom. Siblings stay a step back. No glow, no scale, no colour change elsewhere.

## Shapes

Square. Every region, well, cell, button, tag, menu, tab, tooltip and sheet is a rectangle with 0 radius and a 1px edge. Circles exist only where the object is a circle: knobs (a thin ring, stroke `max(1.5, radius * 0.09)`, with three 1px scale marks and a pencil pointer from 20% radius to just inside the ring), the triangle dot (11px, chalk ring 1.2px), the playing-slice ring (15px, 2.2px stroke, on an 18px well halo, with a 6px source-ink centre), the gate light (9px), link rings (two 1.4px ellipses, interlocked when linked, apart when not), and the sound list's 7px square swatches. Thick marks are rules, not borders: a 3px source rule along the top of a brain column, the Input region and a switched-on label; a 2px source rule along the top of the front tab; a 2px accent line along the bottom of a lifted value cell. The menu chevron is a 7 x 4 drawn path, 1.2px. There are no glyph icons, no image assets.

## Components

### Region
A flat panel with a hairline edge, captioned top-left.
- **Shape:** rectangle, 1px `outline` edge, no radius.
- **Fill:** `panel` for plots, triangles, map and tab pages (`drawPanel`); the brain columns and the Input region sit directly on `background` with the same edge.
- **Source rule:** a brain column and the Input region carry their source's ink 3px thick along the top edge.
- **Caption:** Caption style in `dim` (chalk `text` on the triangles), inset 12px by 7px; the cepstrum and map captions are instructions ("drag a line to move that source's split").
- **Rows:** inside a column, a `rule` hairline above each row with its number typed in Courier New 9px, `dim`, in a 12px margin.

### Well
The ground of a map, plot, triangle or meter: `well` fill, 1px `outline` edge. The triangle's well drops to 50% alpha when it is following the other triangle. Plot gridlines are `rule` hairlines with figures in Courier New 9.5px `dim`.

### Label stock (buttons)
A cream label stuck on the box, printed in navy caps.
- **Shape:** rectangle, 0.5px inset; a 1px line of black 35% directly beneath; 1px navy edge at 18% alpha.
- **Idle:** `label` fill, Button type in `ink`, text inset 4px, centred.
- **Lifted (hover, not pressed):** fill `label.brighter(0.06)`; nothing else changes.
- **On:** same stock and ink, edged 1px in the on-ink with the on-ink 3px thick along the top. The on-ink is the control's source (Freeze on a brain or the Input) or `now` for controls that belong to no source (Link all).
- **Disabled:** no stock: a 1px `outline` rectangle with the caps in `dim`.
- **Sizes:** 26px tall in a parameter panel (`cellWidth - 4` wide); 22px square for "?"; header buttons 24px (Load 64, Rebuild 66, Clear 54 wide); Capture 106 x 18; Close 56 x 24.

### Value cell
A typed value on label stock below its knob.
- **Style:** `label` fill, Courier New 11px in `ink`, 16px tall, `cellWidth - 10` wide (`- 28` beside link rings), no outline of its own.
- **Lifted:** the panel rules the cell (expanded 1px) in 1px `ink` with the knob's arc colour 2px along the bottom.
- **Editing:** selection highlight is `now` at 35%.

### Tag
A typed value on a cream tag (`drawTag`): `label` fill, 1px black 30% line beneath, edged 1px in `ink` 18% or in a source's ink; Courier New at 10.5px (10px on the cepstrum) in `ink`, inset 5px; fit-to-text adds 14px. Used for the version (130 x 18, centred, in the header), the three cepstrum cutoff tags (stacked 18px apart, ruled in their source's ink), and the capture chip (ruled in `now`, in `ink` while being dragged, with "drag to DAW" at the right).

### Menu cell (combo box)
A ruled cell: `panel` fill, 1px `outline` edge, the choice typed in Courier New 11.5px in `text`, a 7 x 4 chevron in `dim` 16px from the right. 24px tall. Its popup is label stock: `label` fill, Courier New 12px in `ink`, the highlighted row `ink` at 14%.

### Tick box (toggle)
A 13px square cut into the well (`well`, contrasting 5% on hover, 12% pressed), edged 1px `outline`, or edged and filled (reduced 3px) in `now` when ticked; the text typed in Courier New 11.5px `text`, 10px to the right, 50% when disabled. Used for "Link: detail follows envelope".

### Knob
A thin ring, inked up to its value, with a pencil pointer and no fill.
- **Track:** `outline`, stroke `max(1.5, r * 0.09)`, butt ends, inset 4px.
- **Value arc:** the control's source ink (`neutralAccent` for sourceless knobs), 78% alpha at rest, 100% lifted.
- **Marks:** three 1px ticks in `dim` 70% at start, middle and end, from just outside the ring to two strokes inside.
- **Pointer:** `text`, 1.6px (2px lifted), from 20% of the radius to 2.6 strokes inside the ring.
- **Label:** above, Label style in `dim`; value cell below.

### Link rings
Two 1.4px ellipses, 14px square: apart (gap 0.8 of the diameter) in `dim` 60% when unlinked (`text` 70% on hover), interlocked (gap 0.45) in the brain's ink when linked. Sit at the bottom right of a knob cell over the value cell's edge, or at the end of a menu row.

### Tabs
Label tabs on a 26px bar, no outline, text in Caption style (uppercase, 0.14).
- **Front:** `label` fill, the tab's source ink (Brain 1, Brain 2, or `neutralAccent` for Mod and Output) 2px along the top, 1px `ink` 18% edge, text in `ink`.
- **Back:** nothing but a 1px `outline` underline; text in `dim`, `text` on hover.
- **Width:** the text's width plus 30px. Link all sits at the bar's right end (86 x 20).

### Sound list
A ruled cream list on the box: `label` fill, 18px rows, a 1px `ink` 22% rule under each, a 7px square swatch in the sound's shade of its brain's ink at x 11, the filename and block count in Courier New 11px `ink`.

### Meter
A ruled level scale: name in Caption 9px `dim` (28px), a `well` bar inset 6px vertically, the level filled in the source ink (`input` for In, `result` for Out) up to -3 dB and in `now` above it, 1px `outline` edge, 1px scale marks at -48, -36, -24, -12 dB along the bottom, a 1.5px hold line in `text` 85%. The gate light is a 9px circle: `rule` when the gate is off, `now` 20% closed, `now` open, edged 1px `outline`.

### Triangle pad
A `drawPanel` region captioned in `text`, the triangle a well with a 1px `outline` stroke (width = height x 1.15, inset 14px, 36px for the top corner's figures, 30px below). Each corner prints its source name (Caption 9px) above or beside its share typed in Courier New: 19px for the leading source at full ink, 15px at 85% for the others; everything at 45% and the well at 50% when the pad is following. The dot is `now`, 11px, with a 1.2px `text` ring; `dim` with a 40% ring when following.

### Brain map
A `drawPanel` region with a well; blocks as squares in shades of the brain's ink (3px at 80% alpha, 2px at 45% over 5000 blocks, 1.5px at 25% over 50000); the search trail a 1.2px `text` line fading from 55% with 3px dots; the playing block ringed in `now` (15px, 2.2px) on a `well` 75% halo with a 6px ink centre. Empty: "drop samples or a folder here" typed in `dim`. Drop target: the column fills with its ink at 12%, a 3px ink border, "DROP TO BUILD BRAIN 1" in Title 16px.

### Plots
Envelopes and cepstrum on wells: source curves in their inks at 70% (envelopes) or 75% (cepstrum), `plotLineWidth` (1.2px dark, 1.3px light; cepstrum 0.2px thinner); the result curve in `result` 0.8px thicker. Cutoff lines 1.5px solid in the source ink, the live (modulated) position dashed 4/3 at 55%. The legend is a 12 x 2 swatch with the name in Caption 9px `dim`.

### Tooltip
A note on label stock: `label` fill, 1px `ink` 40% edge, Tooltip type in `ink`, padded 10px by 7px, wrapped at 340px.

### Help sheet
A sheet pulled out of the box: 82% `background` scrim, a black 40% / radius 10 / offset 0 3 shadow, `label` fill, 1px `ink` 30% edge, `now` 3px along the top. Heading in Title 13px `ink` at the left of a 40px row with a Close label at the right; sections headed in Caption 11.5px in the section's ink (`ink`, `result`, `input`, `brain1`, `brain2`, `ink` 60%) with the body in Prose; an 8px scrollbar.

## Do's and Don'ts

### Do:
- **Do** give every mark that belongs to a source that source's ink, and every "now" mark the grease pencil.
- **Do** type every value, menu, status and tag in Courier New on label stock or in chalk; print captions and labels in Barlow caps, tracked 0.14 (titles, captions) / 0.07 (buttons) / 0.035 (labels).
- **Do** draw regions, wells and cells as flat rectangles with a 1px `outline` edge; use 3px only for a source rule along a top edge (2px on a front tab).
- **Do** lift the control under the hand to full ink (arc 100%, pointer 2px, cell ruled in ink with a 2px accent) and leave its siblings a step back.
- **Do** keep both themes on the same roles: a new colour is a new role in both `darkStyle()` and `lightStyle()`.
- **Do** rank by scale and ink, not decoration: the leading figure is larger (19px vs 15px) and fully inked.
- **Do** build a well one step darker than its panel; plots and maps are drawn on wells, never directly on a panel.

### Don't:
- **Don't** fill a knob, round a corner, or add a glow; a knob is a thin ring with a pencil pointer on an `outline` track.
- **Don't** cast shadows: the only shadow on a control is the 1px shadow line beneath label stock; the help sheet's soft shadow is the one exception and stays there.
- **Don't** use `now` as an accent, a border on a resting control, or a hover colour; hover is a brightness step on stock or an alpha step on ink.
- **Don't** use cream stock for a region or ground; it is for small typed fields and sheets of print.
- **Don't** animate; values, rings and dots update instantly.
- **Don't** introduce glyph icons or image assets; the chevron, link rings, dots and rings are drawn paths.
- **Don't** set anything in Inter or in a system sans; the two faces in use are Barlow (print) and Courier New (typed).
