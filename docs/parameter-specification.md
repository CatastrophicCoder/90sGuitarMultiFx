# Parameter specification

Host-visible parameters and their mapping to internal units. IDs are defined once, in
`src/core/ParameterIds.h`, and **must never change after release**: they are stored in plugin
state and host automation. To rename a parameter, change its display name only.

## Mapping layers

The plan (§7.3) requires three layers so that verified A5 value mappings can be substituted later
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

## Template: documented hardware parameter

Fill one row per parameter once the owner manual's parameter table (SRC-001) has been
transcribed and checked. Until then, nothing here may appear in the Hardware UI mode (plan §7.3).

| Block | Documented label | Documented range / values | Source (SRC-nnn p.) | Status | Plugin ID | Normalised mapping | Algorithm parameter and unit | Mapping status | Notes |
|-------|------------------|---------------------------|---------------------|--------|-----------|--------------------|------------------------------|----------------|-------|
| *e.g. Compressor* | *as printed* | *as printed* | | CONFIRMED / INFERRED | | *linear / table* | | PLACEHOLDER / MEASURED | |
