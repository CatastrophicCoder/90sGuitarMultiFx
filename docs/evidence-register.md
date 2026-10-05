# Evidence register

Every hardware-related behaviour and every decision that could be mistaken for one. The plan
(§2) requires that nothing INFERRED or PLACEHOLDER is presented as a property of the original unit.

## Status meanings

| Status | Meaning |
|--------|---------|
| CONFIRMED | Explicitly documented in an owner manual, service manual, schematic, parameter chart or verified measurement. |
| INFERRED | Strongly suggested by the architecture, but not explicitly documented. |
| PLACEHOLDER | A reasonable initial implementation used until measurements are available. Includes plugin design choices that make no hardware claim. |
| MEASURED | Derived from a recorded measurement of a physical unit, with the capture metadata stored (Milestone 4 onwards). |

The plan's §2 lists three classes but asks the register to record a fourth, MEASURED. It is read
here as a CONFIRMED result whose source is our own measurement, kept separate so it can be traced
to a capture.

**Citation status.** The CONFIRMED rows below are CONFIRMED *according to the plan* (SRC-000 §3),
which says they are documented but does not name the document or page. Until the primary source is
catalogued in `source-register.md` and cited here, treat them as "confirmed per specification,
primary citation pending". No row has yet been checked against a primary document by this project.

## Hardware architecture

| ID | Feature | Status | Source | Implementation consequence | Confidence | Unresolved questions |
|----|---------|--------|--------|----------------------------|------------|----------------------|
| EV-001 | Serial chain: Input → Compressor → Distortion/Overdrive → 3-band EQ → Chorus/Flanger → Reverb/Delay → Output | CONFIRMED (citation pending) | SRC-000 §3.1 | `A5Processor` keeps this order; `EffectBlock` enumerates it; the editor shows sections in this order. M0 has no blocks, so the order is not yet exercised by audio. | High per plan | Primary page reference. |
| EV-002 | Each of the five blocks independently bypassable | CONFIRMED (citation pending) | SRC-000 §3.1 | Five enable parameters exist and persist. They have no audible effect in M0. | High per plan | Whether bypass on the unit is instant, ramped or tail-preserving (plan §15). |
| EV-003 | Original processes audio at 44.1 kHz | CONFIRMED (citation pending) | SRC-000 §3.1 | None in M0. Processing is at the host rate; a 44.1 kHz internal mode is Milestone 3. | High per plan | Running at 44.1 kHz does not by itself reproduce the converters or DSP arithmetic (plan §6.2). |
| EV-004 | Outputs: left/mono, right, stereo headphone | CONFIRMED (citation pending) | SRC-000 §3.3 | Plugin offers a stereo output bus (and mono/mono). | High per plan | Input configuration of the unit is not stated in the plan. Behaviour of L/Mono when R is unplugged. |
| EV-005 | Point in the chain where the signal becomes stereo | PLACEHOLDER | SRC-000 §3.3 | M0: a mono input is copied to both outputs at the plugin boundary. Later: compressor, drive and EQ stay mono-compatible; chorus/flanger and reverb/delay may widen. | Low | Where the unit actually splits to stereo; must be measured. |
| EV-006 | Hardware: M50941-family controller, TMS57002 DSP, MN86081 ADC, µPD6376 DAC, 512 kbit external DRAM, model-specific controller program/data | CONFIRMED (citation pending) | SRC-000 §3.1 | None in M0. No delay time is derived from the DRAM size (plan §12.1). | High per plan | Sample width, memory partitioning and addressing. |
| EV-007 | M50941 acts as host/controller (controls, programs, DSP loading, coefficients), not audio processing | INFERRED | SRC-000 §3.1 | Architecture separates the host-facing adapter (`PluginProcessor`) from the audio engine (`A5Processor`), mirroring this split conceptually. No behaviour depends on it. | Medium | Stronger evidence of the CPU's role. |
| EV-008 | Guitar model has distortion/overdrive where the Multi-FX model has an exciter; both share the broad hardware | CONFIRMED (citation pending) | SRC-000 §3.1 | Block 2 is "Distortion / Overdrive". No exciter is planned. | High per plan | — |
| EV-009 | Programs: 6 banks × 5 programs; bank 1 user-writable, banks 2–6 hold 25 factory programs | CONFIRMED (citation pending) | SRC-000 §3.2 | None in M0 (Milestone 2). The plugin reports one program. | High per plan | — |
| EV-010 | Factory program contents | Unknown | — | No factory presets are shipped. Development presets will be named as such (plan §3.2). | — | Exact values for all 25 programs. |
| EV-011 | Original parameter labels, ranges and value mappings | Unknown | — | Not exposed. `parameter-specification.md` holds a template for transcription. | — | Needs SRC-001 parameter table. |
| EV-012 | The original was used in front of a guitar amplifier | INFERRED | Owner's statement 2026-10-05; no document catalogued yet | Basis for EV-107. Consistent with the chain starting with a compressor and distortion, which act on a guitar's direct signal. | Medium (unsourced) | Confirm from the owner's manual's connection diagrams (SRC-001). The unit also has a headphone output (EV-004), so it may have been used without an amp too. |

## Plugin design decisions (no hardware claim)

| ID | Feature | Status | Source | Implementation consequence | Confidence | Unresolved questions |
|----|---------|--------|--------|----------------------------|------------|----------------------|
| EV-101 | Input trim and output level range −24 to +24 dB | PLACEHOLDER (design choice) | SRC-000 §7.2 | `ParameterRanges` in `ParameterIds.h`; values clamped in the engine. | — | Whether a measured input-level calibration should replace input trim. |
| EV-102 | Gain step 0.1 dB, snapped in the engine | PLACEHOLDER (design choice) | This project | Host-normalised values do not map back exactly (0 dB returns from saved state as 3.6×10⁻⁷ dB). Snapping keeps 0 dB bit-transparent after a reload. | — | — |
| EV-103 | Gain changes ramp linearly over 20 ms | PLACEHOLDER (design choice) | This project | `A5Processor::gainRampSeconds`. Avoids zipper noise. | — | — |
| EV-104 | Global bypass ramps gains to unity over the same 20 ms, then is bit-exact | PLACEHOLDER (design choice) | SRC-000 §15 | Bypass is click-free and exposed to hosts via `getBypassParameter()`. Zero latency, so no compensation is needed in M0. | — | Behaviour once blocks with latency (oversampling) exist. |
| EV-105 | Effect enables default to off | PLACEHOLDER (design choice) | This project | A default of "on" would suggest an effect that does not exist yet. Revisit in Milestone 1. | — | — |
| EV-106 | Processing at host sample rate (native mode) | PLACEHOLDER (design choice) | SRC-000 §6.2 | Engine is sample-rate aware; tested at 44.1, 48, 88.2, 96 and 192 kHz. | — | — |
| EV-107 | Intended placement: in front of an amp (or amp sim): guitar → plugin → amp → cabinet; no standalone | PLACEHOLDER (design choice) | Owner's decision 2026-10-05 | No standalone target. Milestone 1 sets gain staging for an instrument-level guitar input and an output suited to an amp's input; the drive block works like a pedal driving the amp. Into a single amp, the L/Mono output is what is heard, so the L/Mono routing mode (plan §3.3) matters. | — | — |

## Change log

| Date | Change |
|------|--------|
| 2026-10-04 | Register created for Milestone 0. All CONFIRMED rows taken from the plan, pending primary citations. |
