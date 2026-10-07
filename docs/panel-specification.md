# Panel specification

The editor is a close visual replica of the original front panel (owner's decision 2026-10-05,
plan §17.2 amended): same layout, proportions, colour scheme, label styling and workflow. Sources:
the front-panel drawing in the owner's manual (SRC-001 p. 2) and a reference photograph (SRC-003).
All artwork is drawn from scratch in code; nothing is traced or cut from the photo or the scan.

**Excluded** (replaced by the plugin's own identity):

| Original | Replica |
|----------|---------|
| Maker's logo, top left | Catastrophic Audio wordmark, same position and size class |
| Model name in the display window | "Five-A", same position and size class |
| Model-variant badge (red pill under the model name) | "MULTIFX" in the same red pill |
| Product-line text under the display | "MULTIFX PROCESSOR", same styling |

No brand or model name of the original appears anywhere in the UI.

## Overall

- Footprint 430 × 205 mm (SRC-001 p. 14). The editor uses the face as seen from above in SRC-003,
  1905 × 883 design units (aspect 2.16 : 1, `src/ui/PanelLayout.h`), scaled as one canvas. Default
  editor size 1290 × 598 px, resizable from 860 to 2580 px wide at the same proportions.
- Two zones: the sloped **upper panel** (≈ 48 % of the height) and the **footswitch deck**
  (≈ 52 %), separated by a **red stripe** that carries the effect names.
- Colours: black housing and panel, a slightly lighter upper panel than deck, red graphics (stripe,
  effect-name labels in the matrix, product pill), white or light-grey printing, a red 2-digit
  LED display behind a dark window, red LEDs. Exact values are chosen when drawing; the photo's
  lighting is unknown, so its pixels are not taken as colour values.

## Layout (fractions of the panel width from the left edge; from SRC-003, ± a few %)

Upper panel, left to right:

| Element | x (centre or span) | Notes |
|---------|--------------------|-------|
| Wordmark (replaces the maker's logo) | 0.03–0.08 | Top left |
| PEAK LED | 0.08 | Above INPUT |
| INPUT knob, "MIN / MAX" | 0.08 | Small black knob |
| BANK/EFFECT slide switch, 6 positions | 0.13–0.20 | Labels "BANK" / "EDIT" and "SELECT" below; "1 USER", "2~6 PRESET" beside |
| Parameter matrix, 6 rows × columns A–E | 0.20–0.62 | Row numbers 1–6 with red row labels (COMP, DIST/OD, 3 BAND EQ, CHORUS/FL, REV/DELAY, UTILITY); each cell "LABEL range" |
| Parameter knobs A–E | 0.28, 0.34, 0.40, 0.46, 0.51 | Below the matrix, with dot scales and letters A–E |
| Display window | 0.63–0.85 | Product name and pill, "BANK/VALUE" label, 2-digit 7-segment LED, product line text |
| MODE LIST | 0.58–0.87 | Below the display: DIST/OD 1–2, CHORUS/FLANGER 1,2 / 3,4 / 5, REVERB/DELAY 1–7 |
| OUTPUT knob, "MIN / MAX" | 0.92 | Top right |
| WRITE and BYPASS keys | 0.89 and 0.93 | Round black buttons |

Red stripe: "▼1 COMPRESSOR", "▼2 DISTORTION/OVERDRIVE", "▼3 3 BAND EQ", "▼4 CHORUS/FLANGER",
"▼5 REVERB/DELAY" above their footswitches, then "PROG ▼" and "▼ MANUAL/EDIT" over the sixth.

Footswitch deck: six identical rectangular footswitches, centres at 0.08, 0.25, 0.42, 0.59, 0.76,
0.92 (evenly spaced), each with a small red LED above; the sixth (mode select) has two LEDs
(PROG, MANUAL/EDIT).

Matrix cell text, as printed (SRC-001 p. 2; SRC-003):

| Row | A | B | C | D | E |
|-----|---|---|---|---|---|
| 1 COMP | SENS 0~15 | ATTACK 0~7 | | | LEVEL 0~15 |
| 2 DIST/OD | MODE 1~2 | DRIVE 0~15 | TONE 0~15 | | LEVEL 0~15 |
| 3 3 BAND EQ | BASS −7~7 | MID FREQ 1~8 | MID −7~7 | TREBLE −7~7 | TRIM 0~15 |
| 4 CHORUS/FL | MODE 1~5 | SPEED 0~15 | DEPTH 0~15 | F. BACK 0~15 | MIX 0~15 |
| 5 REV/DELAY | MODE 1~7 | TIME ×100ms | FINE ×10ms | F. BACK 0~15 | MIX 0~15 |
| 6 UTILITY | | | | NR LEVEL 0~15 | MASTER 0~15 |

## Behaviour (SRC-001 pp. 2–9)

| Control | Original | Plugin | Milestone |
|---------|----------|--------|-----------|
| INPUT, OUTPUT knobs | Analog level pots | `inputTrim`, `outputLevel` | 1 |
| PEAK LED | Lights on input peaks; set INPUT so it lights occasionally | Lights at −3 dBFS after the input trim, held 100 ms (threshold a placeholder, EV-118) | 1 |
| Slide switch | Program mode: shows bank 1–6, entered when a program is picked. Edit mode: selects the row knobs A–E edit | Same (EV-025) | 1 / 2 |
| Knobs A–E | Edit the selected row's parameters | Same in Edit mode; inactive in Program mode (EV-124). Difference: the hardware pots are absolute and shared across rows; the plugin's knobs show the selected row's current values (no jump) | 1 / 2 |
| Display | Bank in Program mode; value of the knob being turned in Edit mode; "--" on edit stand-by; the dot (EV-028); flashing "1" during WRITE | Same. Back in Program mode it shows the playing bank wherever the switch is (EV-025) | 1 / 2 |
| Footswitches 1–5 | Program mode: select program. Edit mode: toggle effects 1–5. During WRITE: pick the bank 1 slot | Same; their LEDs show the selected program (unlit while a bank is pending, EV-124), the effects, or the WRITE destination | 1 / 2 |
| Footswitch 6, mode LEDs | Toggles Program / Manual-Edit mode; cancels WRITE | Same; a new instance starts in Program mode (EV-029), a session reopens in its saved mode (EV-121) | 2 |
| WRITE | Display flashes "1"; a footswitch picks the slot; WRITE again stores it; both mode LEDs light about a second | Same (EV-027); WRITE before a slot is picked does nothing; the written slot becomes the selected program (EV-123) | 2 |
| BYPASS | Unprocessed sound; mode LEDs blink | `globalBypass`; the lit mode LED blinks | 1 |

## Typography

The panel uses a condensed sans-serif for labels and a 7-segment LED display (owner's decisions
2026-10-06):

- Labels: **Barlow Condensed** (SIL Open Font License 1.1), embedded in the plugin with its licence
  text, as the other Catastrophic Audio projects embed their fonts.
- The 2-digit display: **drawn in code** as seven-segment shapes with a decimal point, lit and unlit
  segments, so no display font is needed.

## Implementation (Milestones 1–2)

`src/ui/MainPanel` paints the fixed artwork and positions the controls in design units;
`PanelLookAndFeel` (colours, lettering, knobs), `PanelControls` (knobs, LEDs, footswitches, keys,
slide switch) and `SevenSegmentDisplay` draw everything in code. The modes and WRITE are in
`MainPanel` too; it works on the processor's `ProgramState` and calls `selectProgram()` and
`writeProgram()`. Render it to PNG files (Edit mode at two sizes, and Program mode) with
`FIVEA_SNAPSHOT_DIR=<dir> build/tests/fivea_plugin_tests "[.snapshot]"`.
