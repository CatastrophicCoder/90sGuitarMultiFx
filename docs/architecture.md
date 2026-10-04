# Architecture

State at **Milestone 0**: a buildable plugin and standalone app that pass audio through with
input trim, output level and a global bypass. No effect algorithm exists yet. Nothing in this
build reproduces, or claims to reproduce, the sound of the original unit.

## Layers

```
 Host (DAW / standalone)
   │  juce::AudioBuffer, parameters, state blobs
   ▼
 PluginProcessor            src/plugin      JUCE adapter: buses, parameters (APVTS), state, editor
   │  ParameterSnapshot (by value, once per block)
   │  AudioBufferView   (non-owning channel pointers)
   ▼
 A5Processor                src/core        Engine façade. Pure C++20, no JUCE.
   input trim → [Compressor → Drive → EQ → Chorus/Flanger → Reverb/Delay] → output level
                 (Milestone 1; the documented order, EV-001)
```

- **`a5_engine`** (static library): `A5Processor`, `AudioBufferView`, `ParameterSnapshot`,
  `ParameterIds.h`, `src/dsp/*`. It has no JUCE dependency so it can be tested without a host and
  wrapped by another plugin format (CLAP) later without changing DSP code (plan §4).
- **`A5Plugin`** (JUCE shared-code target, plus VST3, Standalone and, on macOS, AU wrappers):
  `PluginProcessor`, `PluginEditor`, `ParameterLayout`, `PresetState`, `src/ui/*`.

`ParameterLayout.cpp` and `PresetState.cpp` live in `src/core` as the plan lays out, but depend on
JUCE and are compiled into the plugin target, not the engine.

The split between a host-facing controller and an audio engine mirrors, conceptually, the
original's controller CPU and DSP (EV-007, INFERRED). It is a software design choice and does not
model either chip.

## Threading and real-time rules

| Call | Thread | Allocation | Notes |
|------|--------|------------|-------|
| `A5Processor::prepare`, `reset` | message / host prepare | allowed (none today) | Sets ramp lengths for the sample rate; `reset` settles every ramp on the current parameters. |
| `A5Processor::setParameters` | audio | none | Copies a plain struct, sanitises values (clamp, snap to 0.1 dB, NaN → 0 dB). |
| `A5Processor::process` | audio | none | No locks, I/O, logging or construction. |
| `PluginProcessor::processBlock` | audio | none | Reads parameters from APVTS atomics (relaxed loads) into a snapshot. |
| `get/setStateInformation` | message | yes | Never touches the engine directly; the audio thread sees new values through the atomics. |

Parameters reach the audio thread only through JUCE's per-parameter `std::atomic<float>`; there is
no shared mutable state between the editor and the engine.

## Signal handling at the boundary

- Supported layouts: mono → mono, mono → stereo, stereo → stereo.
- Mono → stereo: the input is copied to the right output before the engine runs. Where the
  original becomes stereo is unknown, so this is PLACEHOLDER (EV-005).
- Output channels without an input are cleared, never passed through as host garbage.
- Gains are computed once per sample and applied to every channel, keeping stereo matched.

## Parameters

IDs are centralised in `src/core/ParameterIds.h` and listed in `parameter-specification.md`.
Each `juce::ParameterID` carries version hint 1; parameters added later take the version of the
release that adds them.

## State format (schema version 1)

```xml
<NinetiesMultiFxState schemaVersion="1" model="functional-placeholder">
  <Parameters>
    <Parameter id="inputTrim" value="0"/>
    ...
  </Parameters>
</NinetiesMultiFxState>
```

Stored inside JUCE's binary XML wrapper (`copyXmlToBinary`). Values are in plain units, written in
parameter-layout order, so saving the same settings always yields the same bytes.

Loading (`a5::state::fromXml`):

1. Root tag must match and `schemaVersion` must be an integer ≥ 1; otherwise the load is refused
   and current settings are kept.
2. Older versions pass through `migrateToCurrentSchema` (empty until version 2 exists).
3. Newer versions are read for the parameters this build knows.
4. Each known parameter: parsed strictly, clamped to range; if missing or unreadable, its default.
5. Unknown elements and attributes are ignored.

The plan's §16 shows the schema as JSON "conceptually". XML is used because JUCE hosts it
natively and it carries the same information; bank, program and preset name will be added as
attributes in Milestone 2 under a schema-version bump only if the change is not additive.

## Build and dependencies

- CMake ≥ 3.25, C++20, Ninja recommended.
- JUCE 9.0.2 and Catch2 3.8.1 are fetched at configure time from pinned GitHub release archives
  verified by SHA-256 (`cmake/Dependencies.cmake`). The repository was not a git repository at
  M0, so submodules were not an option; FetchContent keeps the tree free of vendored code while
  staying reproducible. `FETCHCONTENT_SOURCE_DIR_JUCE` points the build at a local checkout.
- Company: Catastrophic Audio (bundle ID `com.catastrophicaudio.ninetiesmultifx`, manufacturer
  code `Ctcd`, shared with its other plugins). Plugin code `Nmf1`. The product name
  "Nineties Multi-FX" is a working title chosen to avoid the original's name and trade dress
  (plan §17.2). Changing the manufacturer or plugin code after release breaks saved sessions.

## Differences from the plan's file layout (plan §5)

| Item | Reason |
|------|--------|
| Added `src/core/AudioBufferView.h`, `ParameterSnapshot.h`, `ParameterLayout.h` | Types and declarations the plan's interface sketch (§6.1) needs. |
| Not yet created: `Compressor`, `Drive`, `ThreeBandEq`, `Modulation`, `TimeEffects`, `DelayLine`, `FixedPoint`, `DenormalGuard` | Milestone 1+; M0 forbids speculative DSP. `ScopedNoDenormals` in `processBlock` covers denormals for now. |
| Not yet created: `src/ui/ProgramDisplay` | Bank/program workflow is Milestone 2. |
| Not yet created: `tools/*`, `docs/measurement-protocol.md` | Milestone 4. Directories exist. |
| Not created: `LICENSE` | Licence choice is open; see README. JUCE 9 is AGPLv3 or commercial, which constrains it. |
| Added `docs/source-register.md` early | The evidence register needs somewhere to cite sources. |
| Removed the CLion template `main.cpp` | Replaced by the plugin targets. |
