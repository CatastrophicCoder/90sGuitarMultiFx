# Parameter specification

Host-visible parameters and their mapping to internal units. IDs are defined once, in
`src/core/ParameterIds.h`, and **must never change after release**: they are stored in plugin
state and host automation. To rename a parameter, change its display name only.

## Mapping layers

The plan (§7.3) requires three layers so that verified value mappings of the original unit can be substituted later
without touching DSP code:

```
DocumentedParameter  →  NormalizedValue (0…1, host)  →  AlgorithmParameter (internal units)
```

Since Milestone 1 every documented control has all three layers (see "How the three layers connect"
at the end). The plugin's own gains (input trim, output level) map directly to dB.

## Milestone 0 parameters

| ID | Display name | Type | Range | Step | Default | Unit | Smoothed | Version hint | Evidence |
|----|--------------|------|-------|------|---------|------|----------|--------------|----------|
| `inputTrim` | Input Trim | float | −24 … +24 | 0.1 | 0 | dB | 20 ms linear | 1 | EV-101, EV-102, EV-103 |
| `outputLevel` | Output Level | float | −24 … +24 | 0.1 | 0 | dB | 20 ms linear | 1 | EV-101, EV-102, EV-103 |
| `globalBypass` | Bypass | bool | off / on | — | off | — | 20 ms ramp to unity | 1 | EV-104 |
| `compressorEnabled` | Compressor On | bool | off / on | — | off | — | no effect in M0 | 1 | EV-002, EV-105 |
| `driveEnabled` | Drive On | bool | off / on | — | off | — | no effect in M0 | 1 | EV-002, EV-105 |
| `equaliserEnabled` | EQ On | bool | off / on | — | off | — | no effect in M0 | 1 | EV-002, EV-105 |
| `modulationEnabled` | Chorus/Flanger On | bool | off / on | — | off | — | no effect in M0 | 1 | EV-002, EV-105 |
| `timeEffectEnabled` | Reverb/Delay On | bool | off / on | — | off | — | no effect in M0 | 1 | EV-002, EV-105 |

All ranges above are plugin design choices, not properties of the original unit.

Not yet present, planned by the plan (§7.2): authenticity mode, mono/stereo routing mode, input
peak indicator.

## Documented hardware parameters (SRC-001)

Transcribed from the original unit's owner's manual, "Effect Parameter List" (SRC-001 pp. 10–12) and
"Utility" (p. 7). Page numbers are the manual's printed numbers; the PDF page is two higher.
The labels, value ranges and the meaning of discrete values are **CONFIRMED**. How each step maps
to an algorithm value (dB per step, Hz per step, and so on) is **not documented** and stays
PLACEHOLDER until measured, unless the manual states it (marked below).

Letters A–E are the five Parameter Editor knobs; a block uses only some of them.

### 1. Compressor [COMPRESSOR] (p. 10)

Described as "a limiter effect which suppresses the high level attack transients … [and]
lengthen[s] the sustain sound by raising the level as a sound decays". Its effect "varies
according to the amount of the input signal" (smaller SENS for hot humbuckers, larger for single
coils).

| Knob | Label | Range | Documented meaning | Mapping status |
|------|-------|-------|--------------------|----------------|
| A | SENS | 0–15 | Degree of compression; larger = more sustain or attack emphasis | PLACEHOLDER |
| B | ATTACK | 0–7 | Attack speed; higher = faster attack | PLACEHOLDER |
| E | LEVEL | 0–15 | Output level of the effect sound | PLACEHOLDER |

No threshold, ratio or release parameter exists on the unit.

### 2. Distortion/Overdrive [DIST/OD] (p. 10)

| Knob | Label | Range | Documented meaning | Mapping status |
|------|-------|-------|--------------------|----------------|
| A | MODE | 1–2 | 1 = Distortion ("a high gain 'fuzz' type effect"), 2 = Overdrive ("a mild saturation effect") | CONFIRMED (discrete) |
| B | DRIVE | 0–15 | Amount of distortion or gain | PLACEHOLDER |
| C | TONE | 0–15 | Larger = brighter | PLACEHOLDER |
| E | LEVEL | 0–15 | Output level of the effect sound | PLACEHOLDER |

### 3. 3 Band EQ [3 BAND EQ] (p. 11)

| Knob | Label | Range | Documented meaning | Mapping status |
|------|-------|-------|--------------------|----------------|
| A | BASS | −7…+7 | Low band cut/boost, **100 Hz** | Frequency CONFIRMED; dB per step PLACEHOLDER |
| B | MID FREQ | 1–8 | Middle centre frequency: 1 = 200 Hz, 2 = 550 Hz, 3 = 800 Hz, 4 = 1 kHz, 5 = 1.25 kHz, 6 = 2 kHz, 7 = 4 kHz, 8 = 5 kHz | CONFIRMED (table) |
| C | MID | −7…+7 | Middle band cut/boost | dB per step PLACEHOLDER; Q not documented |
| D | TREBLE | −7…+7 | High band cut/boost, **3 kHz** | Frequency CONFIRMED; dB per step PLACEHOLDER |
| E | TRIM | 0–15 | "Gain for input signal" (of the EQ); lower it if the EQ clips | PLACEHOLDER |

Filter shapes (shelf or peak for BASS and TREBLE) are not documented.

### 4. Chorus/Flanger [CHORUS/FL] (p. 11)

| Knob | Label | Range | Documented meaning | Mapping status |
|------|-------|-------|--------------------|----------------|
| A | MODE | 1–5 | 1 = Flanger 1 ("modulates high harmonics", delay time 1.8 ms); 2 = Flanger 2 ("modulates low harmonics", 4.0 ms); 3 = Chorus 1 (24 ms); 4 = Chorus 2 (32 ms); 5 = Slapback (75 ms) | CONFIRMED (types and delay times) |
| B | SPEED | 0–15 | Modulation speed | PLACEHOLDER |
| C | DEPTH | 0–15 | Modulation depth | PLACEHOLDER |
| D | F.BACK | 0–15 | Feedback; deeper modulation in modes 1–2, 0 = clearer chorus in modes 3–4, 1–2 = rockabilly slapback in mode 5 | PLACEHOLDER |
| E | MIX | 0–15 | Effect/direct balance: **15 = 50/50, 0 = direct only** | End points CONFIRMED; curve between PLACEHOLDER |

### 5. Reverb/Delay [REV/DELAY] (p. 12)

"A total of 7 effects including various reverb, delay and echoverb effects … The echoverb effect is
a combination of reverb and delay."

| Knob | Label | Range | Documented meaning | Mapping status |
|------|-------|-------|--------------------|----------------|
| A | MODE | 1–7 | 1 = Hall Reverb, 2 = Ensemble Hall Reverb, 3 = Room Reverb, 4 = Plate Reverb, 5 = Live Stage Reverb, 6 = Echoverb, 7 = Delay | CONFIRMED (types) |
| B | TIME | 0–3 (mode 6), 0–4 (mode 7) | Delay time in **100 ms** steps; not settable in modes 1–5 | CONFIRMED |
| C | FINE | 0–9 | Delay time in **10 ms** steps; not settable in modes 1–5 | CONFIRMED |
| D | F.BACK | 0–15 | Delay repeats; not settable in modes 1–5 | PLACEHOLDER |
| E | MIX | 0–15 | **15 = 50/50, 0 = direct only** | End points CONFIRMED; curve between PLACEHOLDER |

So delay time = TIME × 100 ms + FINE × 10 ms: at most **490 ms** in Delay and **390 ms** in
Echoverb (whether a setting of 0 ms is possible is not stated). The reverb types have no decay,
size or damping parameter: each type is fixed.

### 6. Utility [UTILITY] (pp. 7, 12), stored per program

| Knob | Label | Range | Documented meaning | Mapping status |
|------|-------|-------|--------------------|----------------|
| D | NR LEVEL | 0–15 | Noise reduction threshold; larger = more reduction | PLACEHOLDER |
| E | MASTER | 0–15 | Master (output) volume of the program | PLACEHOLDER |

### Global controls (p. 2)

| Control | Documented behaviour | Notes |
|---------|----------------------|-------|
| INPUT (knob) and PEAK LED | Input volume; set so the peak LED lights occasionally (p. 4) | Analog or digital stage: see the service manual |
| OUTPUT (knob) | Output volume | |
| BYPASS key | "sends unprocessed sound to the output terminal" | |
| VOLUME pedal jack | Optional volume pedal; allows swells (p. 9) | Position in the chain not documented |

### Factory program example (p. 6)

The manual shows one factory program as an example of a program's settings: **2-1 "METAL 1"**:
Compressor off; DIST/OD Mode 1, Drive 14, Tone 15, Level 11; 3 Band EQ Bass 6, Mid Freq 3,
Mid 5, Treble 1, Trim 12; Chorus/Flanger off; Reverb/Delay Mode 4, Mix 5. (Utility values not
shown.) The full preset table ("attached Effect Parameter List", p. 7) is not part of this PDF.

### The plugin's factory programs (Milestone 2)

From `src/core/FactoryPrograms.cpp`. 2-1 is METAL 1 as documented above; its controls the manual
does not show keep the defaults (PLACEHOLDER). Every other program is this project's own
**development preset**, named "DEV …", and makes no claim about the original's presets. A
switched-off effect's controls are not listed (they keep the defaults); the reverb modes have no
TIME, FINE or F.BACK.

| Slot | Name | COMP SENS/ATTACK/LEVEL | DIST/OD MODE, DRIVE/TONE/LEVEL | 3 BAND EQ BASS/MID FREQ/MID/TREBLE/TRIM | CHORUS/FL MODE, SPEED/DEPTH/F.BACK/MIX | REV/DELAY MODE, TIME/FINE/F.BACK/MIX | NR LEVEL | MASTER |
|---|---|---|---|---|---|---|---|---|
| 2-1 | METAL 1 | off | Distortion, 14/15/11 | 6/3/5/1/12 | off | Plate, mix 5 | 0 | 12 |
| 2-2 | DEV Soft Drive | off | Overdrive, 6/9/12 | off | off | off | 0 | 12 |
| 2-3 | DEV Hard Drive | off | Distortion, 11/8/11 | 2/4/3/1/13 | off | Room, mix 4 | 0 | 12 |
| 2-4 | DEV Crunch | 6/4/12 | Overdrive, 10/10/12 | off | off | off | 0 | 12 |
| 2-5 | DEV Lead Drive | off | Distortion, 13/11/11 | 0/5/4/0/13 | off | Delay, 3/5/5/6 | 4 | 12 |
| 3-1 | DEV Clean Compression | 8/4/12 | off | off | off | off | 0 | 12 |
| 3-2 | DEV Squeeze | 13/6/13 | off | off | off | off | 0 | 12 |
| 3-3 | DEV Clean Bright | 5/4/12 | off | -1/3/0/3/15 | off | off | 0 | 12 |
| 3-4 | DEV Clean Warm | off | off | 3/2/2/-2/15 | off | off | 0 | 12 |
| 3-5 | DEV Clean Slap | 10/7/12 | off | 0/3/0/2/15 | Slapback, 0/0/0/8 | off | 0 | 12 |
| 4-1 | DEV Wide Chorus | off | off | off | Chorus 2, 4/10/0/15 | off | 0 | 12 |
| 4-2 | DEV Light Chorus | off | off | off | Chorus 1, 6/5/0/9 | off | 0 | 12 |
| 4-3 | DEV Jet Flanger | off | off | off | Flanger 1, 2/12/11/15 | off | 0 | 12 |
| 4-4 | DEV Slow Flanger | off | off | off | Flanger 2, 1/9/7/12 | off | 0 | 12 |
| 4-5 | DEV Slapback | off | off | off | Slapback, 0/0/3/10 | off | 0 | 12 |
| 5-1 | DEV Short Ambience | off | off | off | off | Room, mix 6 | 0 | 12 |
| 5-2 | DEV Big Hall | off | off | off | off | Hall, mix 9 | 0 | 12 |
| 5-3 | DEV Plate | off | off | off | off | Plate, mix 8 | 0 | 12 |
| 5-4 | DEV Echo Delay | off | off | off | off | Delay, 3/8/6/9 | 0 | 12 |
| 5-5 | DEV Echoverb | off | off | off | off | Echoverb, 2/5/4/9 | 0 | 12 |
| 6-1 | DEV Drive Chorus Hall | off | Overdrive, 9/9/12 | off | Chorus 1, 5/6/0/8 | Hall, mix 6 | 0 | 12 |
| 6-2 | DEV Ensemble Lead | 7/3/12 | Distortion, 12/10/11 | 1/5/3/1/13 | off | Ensemble Hall, mix 7 | 3 | 12 |
| 6-3 | DEV Flange Drive | off | Distortion, 10/9/11 | off | Flanger 1, 3/10/9/12 | Room, mix 5 | 0 | 12 |
| 6-4 | DEV Live Stage | 6/4/12 | Overdrive, 7/8/12 | off | off | Live Stage, mix 7 | 0 | 12 |
| 6-5 | DEV Full Chain | 8/4/12 | Overdrive, 8/9/12 | 1/3/1/1/14 | Chorus 2, 3/7/0/8 | Echoverb, 2/0/3/7 | 2 | 12 |

Bank 1 starts as copies of 2-1, 3-1, 4-1, 5-1 and 6-1, as the original's factory copied five
presets into its user bank (SRC-001 p. 8); which five the original copied is not documented.

## Placeholder mappings (Milestone 1)

Implemented in `src/core/StepMapping.cpp` from the values in `src/core/ModelProfile.h`
(`functionalPlaceholderProfile`). All PLACEHOLDER (EV-109) except where marked documented.

| Control | Mapping |
|---------|---------|
| Compressor SENS 0–15 | Threshold −6 dBFS at 0, −3 dB per step (−51 dBFS at 15); ratio 8:1, knee 6 dB; feed-forward peak compressor linked across channels (EV-111) |
| Compressor ATTACK 0–7 | 50, 30, 20, 12, 8, 5, 3, 1 ms; release fixed at 250 ms |
| LEVEL, MASTER 0–15 | 0 = silent; otherwise 2 dB per step, unity at 12 (+6 dB at 15) |
| DIST/OD MODE 1–2 | **Documented**: 1 Distortion, 2 Overdrive; each its own placeholder pipeline (EV-112); switching crossfades over 20 ms |
| DIST/OD DRIVE 0–15 | Pre-gain: Overdrive 0 dB + 2 dB/step; Distortion 10 dB + 3 dB/step |
| DIST/OD TONE 0–15 | Butterworth low-pass cutoff 1–8 kHz, log-spaced |
| DIST/OD LEVEL 0–15 | As LEVEL above, after a per-mode output trim (Distortion −3 dB) |
| EQ BASS, MID, TREBLE −7…+7 | 1.5 dB per step (±10.5 dB); BASS low shelf and TREBLE high shelf with slope 1, MID peaking with Q 0.7 (EV-110) |
| EQ MID FREQ 1–8 | **Documented** frequency table |
| EQ TRIM 0–15 | Unity at 15, −1.5 dB per step below |
| Chorus/Flanger base delay | **Documented** per mode |
| Chorus/Flanger MODE 1–5 | **Documented** base delays 1.8, 4.0, 24, 32, 75 ms; a change ducks the effect over 20 ms |
| SPEED 0–15 | Sine LFO, 0.1–10 Hz, log-spaced; right channel 90° ahead (EV-114) |
| DEPTH 0–15 | 0 to a per-mode maximum: 1.6, 3.6, 6, 6, 1 ms |
| F.BACK 0–15 | 0 to 0.9 (Chorus/Flanger) or 0.85 (Delay), linear |
| MIX 0–15 | Dry stays at unity; wet 0 → equal to dry, linear (end points **documented**) |
| Reverb/Delay MODE 1–7 | **Documented** types; modes 1–5 are fixed reverb voicings (EV-115), 6 Echoverb and 7 Delay (EV-116); a change crossfades the tail over 100 ms |
| Delay TIME, FINE | **Documented**: TIME × 100 ms + FINE × 10 ms, TIME ≤ 3 (Echoverb) or 4 (Delay); a change crossfades between read positions over 20 ms |
| NR LEVEL 0–15 | 0 = off; threshold −90 dBFS at 1, +3 dB per step |

Out-of-range steps are clamped to the documented range.

## Host parameters (Milestone 1)

Every documented control is a stepped host parameter holding the control's own steps
(`src/core/ParameterIds.h`, `src/core/ParameterLayout.cpp`). MODE controls are choice lists named
as in the manual; their index counts from 0, the documented mode from 1. All at version hint 1
(nothing released yet).

| ID | Control | Range | Default |
|----|---------|-------|---------|
| `compSens`, `compAttack`, `compLevel` | Compressor SENS, ATTACK, LEVEL | 0–15, 0–7, 0–15 | 8, 4, 12 |
| `driveMode` | Dist/OD MODE | Distortion, Overdrive | Overdrive |
| `driveDrive`, `driveTone`, `driveLevel` | Dist/OD DRIVE, TONE, LEVEL | 0–15 | 8, 8, 12 |
| `eqBass`, `eqMid`, `eqTreble` | EQ BASS, MID, TREBLE | −7…+7 | 0 |
| `eqMidFreq` | EQ MID FREQ (shown in Hz) | 1–8 | 3 (800 Hz) |
| `eqTrim` | EQ TRIM | 0–15 | 15 |
| `modMode` | Chorus/FL MODE | Flanger 1, Flanger 2, Chorus 1, Chorus 2, Slapback | Chorus 1 |
| `modSpeed`, `modDepth`, `modFeedback`, `modMix` | Chorus/FL SPEED, DEPTH, F.BACK, MIX | 0–15 | 5, 8, 0, 8 |
| `revMode` | Rev/Delay MODE | Hall, Ensemble Hall, Room, Plate, Live Stage Reverb, Echoverb, Delay | Hall Reverb |
| `revTime`, `revFine` | Rev/Delay TIME, FINE | 0–4, 0–9 (TIME clamped to 3 in Echoverb) | 3, 0 |
| `revFeedback`, `revMix` | Rev/Delay F.BACK, MIX | 0–15 | 4, 8 |
| `nrLevel`, `master` | Utility NR LEVEL, MASTER | 0–15 | 0 (off), 12 (unity) |
| `driveOversampling` | Plugin setting, not automatable | Off, 2x, 4x | Off |

Defaults are placeholders: the manual documents no "initial" values.

## How the three layers connect

Plan §7.3's layers, as implemented:

| Layer | Where | Status |
|-------|-------|--------|
| Documented parameter: the control and its steps | "Documented hardware parameters" above; `documented::` in `src/core/ModelProfile.h` | CONFIRMED (SRC-001) |
| Normalised value: what the host stores and automates | "Host parameters" above; `src/core/ParameterLayout.cpp`. A stepped integer or choice holding the documented step itself, so the host value maps 1:1 to the step | Design choice |
| Algorithm parameter: dB, Hz, ms, gain | "Placeholder mappings" above; `src/core/StepMapping.cpp` reading `functionalPlaceholderProfile` | PLACEHOLDER, except values the manual states |

To substitute measured values later, change the profile, not the parameters: the host parameters
and saved sessions stay as they are.
