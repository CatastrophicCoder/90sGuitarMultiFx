# Architecture

State at the end of **Milestone 1**: the plugin (VST3, and AU on macOS) runs the original unit's
full effect chain, with every documented control as a host parameter and a replica of its front
panel in Manual/Edit mode. Every algorithm is a placeholder: the controls and documented values
match the original; the sound does not claim to.

## Layers

```
 Host (DAW)
   │  juce::AudioBuffer, parameters, state blobs
   ▼
 PluginProcessor          src/plugin   JUCE adapter: buses, parameters (APVTS), state, latency, editor
   │  ParameterSnapshot (by value, once per block)       PluginEditor → ui::MainPanel (src/ui)
   │  AudioBufferView   (non-owning channel pointers)
   ▼
 FiveAProcessor           src/core     The chain. Pure C++20, no JUCE.
   input trim → NR → Compressor → Distortion/Overdrive → 3 Band EQ → Chorus/Flanger
              → Reverb/Delay → MASTER → output level
   (the documented order, EV-001; NR and MASTER placed as placeholders, EV-117)
   │
   ▼
 Blocks                   src/dsp      NoiseReduction, Compressor, Drive (+ Oversampler, Waveshapers),
                                       ThreeBandEq (+ Biquad), Modulation (+ DelayLine),
                                       TimeEffects (+ Reverb, DelayLine), BypassCrossfade, Smoothing
```

- **`fivea_engine`** (static library): `src/core` (except the two JUCE files below) and `src/dsp`.
  No JUCE dependency, so the chain is tested without a host and can be wrapped for another plugin
  format (CLAP) without changing DSP code (plan §4).
- **`FiveAPlugin`** (JUCE shared code, plus VST3 and, on macOS, AU wrappers): `src/plugin`,
  `src/ui`, and `src/core/ParameterLayout.cpp` and `PresetState.cpp`, which use JUCE.
- **`FiveAPanelFonts`**: Barlow Condensed, embedded as binary data.

The split between a host-facing controller and an audio engine mirrors, conceptually, the
original's controller CPU and DSP (EV-007). It is a software design choice and models neither chip.

## Hardware values and placeholders

All values that concern the original unit live in `src/core/ModelProfile.h` (plan §14):

- `documented::` — values stated in the owner's manual (CONFIRMED, cited): control ranges, EQ
  frequencies, chorus/flanger delays, delay time steps and maxima, MIX end points.
- `functionalPlaceholderProfile` — every step-to-value mapping and algorithm constant
  (PLACEHOLDER, EV-109 to EV-117). A measured profile replaces it without code changes.

`src/core/StepMapping` turns a documented control step into an algorithm value (plan §7.3's middle
layer), clamping every step to its documented range.

## Programs

`src/core/Program.h` and `ProgramBank` (engine side, no JUCE) model the original's program memory:

- `Program`: what one program stores (EV-024), inline and allocation-free to copy, with a name
  for the host's program list. `programFrom()` and `applyProgram()` move it in and out of a
  `ParameterSnapshot`, leaving input trim, output level and global bypass alone (EV-119).
- `ProgramBank`: the 30 slots; `write()` accepts bank 1 only.
- `ProgramSelection`: the playing program, and the bank the display shows, which differ while a
  bank is pending (EV-025).
- `sameControl()`, `sameEffectSwitches()`, `sameSettings()`: the display's dot (EV-028).
- `ProgramMode`: Program or Manual/Edit; starts in Program (EV-029).
- `ProgramTransition`: the program-change dip and the handshake that keeps the audio thread
  on the old settings until every new parameter is set (EV-122).
- `FactoryPrograms`: the 30 slots a new instance starts with: METAL 1 in 2-1 from
  `documented::Metal1` in the model profile, development presets elsewhere, bank 1 as copies
  (EV-010, EV-120).

## Threading and real-time rules

| Call | Thread | Allocation | Notes |
|------|--------|------------|-------|
| `FiveAProcessor::prepare` | message / host prepare | yes | Sizes every delay line, reverb network and scratch buffer for the sample rate and block size. |
| `FiveAProcessor::reset` | message / host prepare | none | Clears state, settles every ramp on the current parameters. |
| `FiveAProcessor::setParameters` | audio | none | Plain struct copy; values sanitised; blocks' settings updated (ramps start only on real changes). |
| `FiveAProcessor::process` | audio | none | No locks, I/O, logging or construction; tested with a counting `operator new`. |
| `PluginProcessor::processBlock` | audio | none | Reads parameters from APVTS atomics into a snapshot; records the input peak (atomic). |
| `get/setStateInformation` | message | yes | Touches parameters only; the audio thread sees them through the atomics. |
| `applyOversamplingSetting` | message (timer) | yes | Re-prepares with processing suspended, reports the new latency. |
| `selectProgram`, `writeProgram`, host program calls | message | yes | Load a program into the parameters between `ProgramTransition::post()` and `complete()`; the audio thread fades out, waits for `complete()`, switches. |
| Editor | message | yes | Reads parameters through attachments and atomics; the input peak through an atomic exchange. |

Denormals: the engine sets flush-to-zero for the duration of `process()` (`dsp/DenormalGuard.h`),
independent of the JUCE wrapper's own.

## Switching, bypass and latency

- Each effect has a 10 ms crossfade (EV-108). A switched-off effect is not run, and is reset when
  its fade-out ends. Switching on, the chorus/flanger's and reverb/delay's input fades in as well,
  so a block with memory never replays a step.
- Within a block, MODE changes crossfade (drive: two pipelines; chorus/flanger: a duck of the
  effect and its feedback; reverb/delay: two engines with the old tail fading out, plan §15).
- Stepped controls ramp (20 ms) so a step does not click.
- Global bypass crossfades to the dry input and becomes bit-exact (EV-104).
- A program change dips the output (EV-122): raised-cosine fade-out of the effects' output on the
  old program's settings; at the first silent sample the new settings, an engine reset (tails are
  cut, ramps settle) and a raised-cosine fade-in of the effects' input, so restarted filters and
  delay lines never record a step. `processBlock` splits the host block at the silent sample.
  Under global bypass nothing is faded or reset.
- Oversampling the drive adds 69 (2×) or 76 (4×) samples. The drive's bypass path and the global
  bypass path are delayed to match, so the reported latency is constant.
- The engine processes in chunks of its announced block size, so longer host blocks are safe.

## Signal handling at the boundary

- Supported layouts: mono → mono, mono → stereo, stereo → stereo.
- Mono → stereo: the input is copied to the right output before the engine runs. The chain treats
  both channels alike up to the chorus/flanger, whose right LFO leads by 90°, and the reverb, whose
  left and right taps differ (EV-005).
- Output channels without an input are cleared, never passed through as host garbage.

## Parameters and state

IDs are in `src/core/ParameterIds.h`, ranges in `ParameterLayout.cpp`, listed in
`parameter-specification.md`. Documented controls are stepped integers (MODEs: named choices).
Every `juce::ParameterID` carries version hint 1; parameters added after a release take that
release's number.

State format (schema version 2):

```xml
<FiveAState schemaVersion="2" model="functional-placeholder">
  <Parameters>                                  the edit buffer: every host parameter
    <Parameter id="inputTrim" value="0"/>
    ...
  </Parameters>
  <Programs bank="2" program="1" mode="program">  selected program; mode "program" or "manualEdit"
    <Program slot="1-1" name="METAL 1">           bank 1 only; banks 2–6 always come from the code
      <Value id="driveEnabled" value="1"/>        on/off states, then the 24 controls,
      <Value id="driveMode" value="1"/>           in the documented steps (MODE 1–7, not an index)
      ...
    </Program>
    ...
  </Programs>
</FiveAState>
```

Stored inside JUCE's binary XML wrapper, plain values in layout order, so saving the same settings
always gives the same bytes. Loading refuses a foreign or unversioned document; migrates older
schemas (none yet); reads newer ones for the parameters it knows; parses each value strictly,
clamps it, defaults it if missing; ignores anything unknown. The same rules apply to each
program value; a missing or unreadable program slot keeps its factory copy, and a slot outside
bank 1 is ignored. Schema 1 (Milestones 0–1) has no `<Programs>` and loads as the factory programs
with 1-1 selected in Program mode; `PluginProcessor` owns the `ProgramState` (message thread).

The plan's §16 shows the schema as JSON "conceptually"; XML carries the same information and is
native to JUCE.

## The editor

`ui::MainPanel` is the replica panel (`panel-specification.md`): drawn in code in one design space
measured from the reference photo and scaled as a whole. `PanelLayout.h` holds the grid table and
every coordinate; knobs A–E bind to the selected row's parameters through attachments that are
rebuilt when the slide switch moves. Program mode, banks and WRITE are drawn and inactive (the
processor side exists since Milestone 2 step 4; the panel follows in step 5).

## Build and dependencies

- CMake ≥ 3.25, C++20, Ninja recommended.
- JUCE 9.0.2 and Catch2 v3.9.1 are git submodules in `external/`, pinned to release tags (the same
  commits as the other Catastrophic Audio projects). CMake stops with a message if `external/JUCE`
  is empty.
- A Release build installs the AU and VST3 into the user plugin folders; Debug does not
  (`FIVEA_COPY_PLUGIN`).
- Windows links the C++ runtime statically, so a host does not need `VCRUNTIME140.dll` to load it.
- Identity: Catastrophic Audio (bundle ID `com.catastrophicaudio.fivea`, manufacturer code `Ctcd`,
  shared with its other plugins), plugin code `Nmf1`, product "Five-A MultiFX Processor". Changing
  the codes or the bundle ID after release breaks saved sessions.

## Where Milestone 1 differs from the plan

| Plan | Implementation | Why |
|------|----------------|-----|
| §7.3: engineering parameters first, documented ones later in a Hardware mode | The documented controls are the host parameters, stepped | Owner's decision after the owner's manual was catalogued (PROGRESS, 2026-10-05) |
| §3.1: five blocks | Plus the documented noise reduction and master volume (Utility page) | SRC-001 pp. 7, 12; owner's decision |
| §12: reverb with decay, damping, mix; delay with a practical maximum | Five fixed reverb voicings; delay limited to the documented 490 ms | The original has no reverb controls and documents its delay range (EV-019) |
| §11.2: chorus with base delay, stereo phase and wet/dry levels exposed | Only SPEED, DEPTH, F.BACK, MIX; base delays fixed per documented mode | The documented controls (EV-020) |
| §9.1: a `DriveModel` interface | One class configured per mode by profile data | No second structure yet; a measured model would replace it behind the same calls (plan §23) |
| §8.1: `DetectorType` Peak/MeanSquare/QuasiRms; §11.1: four interpolations | Peak only; Linear and Cubic only | The others are added if a measurement calls for them |
| §15: a `TailPolicy` enum | Crossfade behaviour only | The plan's default; the others wait for evidence of what the original does |
| §3.3: L/Mono as a separate routing mode; §7.2: authenticity mode | Not yet | Not in Milestone 1's deliverables |
| §6.2: hardware-rate mode | Not yet | Milestone 3 |
| §13: `FixedPointProfile` | Not created | Disabled until evidence supports a configuration |
| §17.2: no copied trade dress | A close replica of the panel, without the original's names, logo or artwork | Owner's decision, plan §17.2 amended |
| §5 file layout: added | `AudioBufferView.h`, `ParameterSnapshot.h`, `ParameterLayout.h`, `ModelProfile.h`, `StepMapping`, `Program`, `ProgramBank`, `ProgramMode.h`, `FactoryPrograms`, `BypassCrossfade.h`, `DenormalGuard.h`, `Biquad`, `Oversampler`, `Waveshapers.h`, `Reverb`, `NoiseReduction`; the panel's `PanelLayout.h`, `PanelLookAndFeel`, `PanelControls`, `SevenSegmentDisplay`; `docs/source-register.md`, `panel-specification.md` | Pieces the plan's sections or the evidence rules need |
| §5 file layout: not yet | `ProgramDisplay` (Milestone 2), `tools/*` and `docs/measurement-protocol.md` (Milestone 4), `FixedPoint` | — |
