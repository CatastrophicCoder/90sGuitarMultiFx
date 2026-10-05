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

In Milestone 0 there is no documented layer yet; the host value maps directly to an engineering
unit via JUCE's `NormalisableRange`, and the engine sanitises it again (`sanitiseGainDb`).

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

## Mapping template

| Block | Documented label | Documented range / values | Source (SRC-nnn p.) | Status | Plugin ID | Normalised mapping | Algorithm parameter and unit | Mapping status | Notes |
|-------|------------------|---------------------------|---------------------|--------|-----------|--------------------|------------------------------|----------------|-------|
| *e.g. Compressor* | *as printed* | *as printed* | | CONFIRMED / INFERRED | | *linear / table* | | PLACEHOLDER / MEASURED | |

Filled in Milestone 1 once the plugin's parameters follow the documented set.
